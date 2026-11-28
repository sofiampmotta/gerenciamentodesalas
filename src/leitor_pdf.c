#define _POSIX_C_SOURCE 200809L

#include "../include/leitor_pdf.h"
#include "../include/aplicacao.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_LINE 2048
#include "../include/alocacao.h"

#ifdef _WIN32
#include <windows.h>
/* No Windows o caminho vem do GTK em UTF-8, mas o popen comum nao entende
 * acentos (ex.: "TURMA - HORÁRIO 2026.2.pdf"). Por isso o comando e
 * convertido para UTF-16 e aberto com _wpopen. */
static FILE *abrir_comando(const char *cmd) {
  wchar_t wcmd[MAX_LINE + 64];
  if (MultiByteToWideChar(CP_UTF8, 0, cmd, -1, wcmd, MAX_LINE + 64) == 0)
    return NULL;
  return _wpopen(wcmd, L"r");
}
#else
static FILE *abrir_comando(const char *cmd) { return popen(cmd, "r"); }
#endif

typedef enum {
  STATE_WAIT_TURMA,
  STATE_READ_TURMA,
  STATE_WAIT_PROF,
  STATE_READ_PROF,
  STATE_READ_HORARIOS
} ParserState;

static void trim(char *s) {
  char *start = s;
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[--len] = '\0';
  }
}

/* Ignora cabeçalhos e rodapés repetidos em cada quebra de página do PDF */
static bool is_page_header_or_footer(const char *line) {
  if (strstr(line, "FUNDAÇÃO UNIVERSIDADE DE PERNAMBUCO") != NULL)
    return true;
  if (strstr(line, "FUNDACAO UNIVERSIDADE DE PERNAMBUCO") != NULL)
    return true;
  if (strstr(line, "COORD. GERAL DE GRADUAÇÃO") != NULL)
    return true;
  if (strstr(line, "COORD. GERAL DE GRADUACAO") != NULL)
    return true;
  if (strstr(line, "Relatório Turma/Horário") != NULL)
    return true;
  if (strstr(line, "Relatorio Turma/Horario") != NULL)
    return true;
  if (strncmp(line, "DATA:", 5) == 0)
    return true;
  if (strncmp(line, "HORA:", 5) == 0)
    return true;
  if (strncmp(line, "Página ", 7) == 0 || strncmp(line, "Pagina ", 7) == 0)
    return true;
  return false;
}

static bool is_time_slot(const char *t) {
  if (strlen(t) != 11)
    return false;
  return isdigit((unsigned char)t[0]) && isdigit((unsigned char)t[1]) &&
         t[2] == ':' && isdigit((unsigned char)t[3]) &&
         isdigit((unsigned char)t[4]) && t[5] == '-' &&
         isdigit((unsigned char)t[6]) && isdigit((unsigned char)t[7]) &&
         t[8] == ':' && isdigit((unsigned char)t[9]) &&
         isdigit((unsigned char)t[10]);
}

static bool is_all_digits(const char *s) {
  if (!s || !*s)
    return false;
  for (; *s; s++) {
    if (!isdigit((unsigned char)*s))
      return false;
  }
  return true;
}

static void parse_turma_line(const char *line, TurmaPDF *t) {
  const char *p = line;
  int i = 0;

  while (*p && !isspace((unsigned char)*p) && i < (int)sizeof(t->turma) - 1) {
    t->turma[i++] = *p++;
  }
  t->turma[i] = '\0';

  while (*p && isspace((unsigned char)*p))
    p++;

  char *endptr = NULL;
  t->vagas = (int)strtol(p, &endptr, 10);
  p = endptr;

  while (*p && isspace((unsigned char)*p))
    p++;

  const char *sep = strstr(p, " - ");
  if (sep) {
    size_t cod_len = (size_t)(sep - p);
    if (cod_len >= sizeof(t->codigo))
      cod_len = sizeof(t->codigo) - 1;
    strncpy(t->codigo, p, cod_len);
    t->codigo[cod_len] = '\0';
    trim(t->codigo);

    strncpy(t->componente, sep + 3, sizeof(t->componente) - 1);
    t->componente[sizeof(t->componente) - 1] = '\0';
    trim(t->componente);
  } else {
    t->codigo[0] = '\0';
    strncpy(t->componente, p, sizeof(t->componente) - 1);
    t->componente[sizeof(t->componente) - 1] = '\0';
  }
}

static void parse_prof_line(const char *line, TurmaPDF *t) {
  char buf[MAX_LINE];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  trim(buf);

  bool has_ch = false;
  char *last_space = strrchr(buf, ' ');
  if (last_space && is_all_digits(last_space + 1)) {
    t->carga_horaria += atoi(last_space + 1);
    *last_space = '\0';
    trim(buf);
    has_ch = true;
  }

  if (t->professor[0] != '\0') {
    const char *sep = has_ch ? "; " : " ";
    strncat(t->professor, sep, sizeof(t->professor) - strlen(t->professor) - 1);
  }
  strncat(t->professor, buf, sizeof(t->professor) - strlen(t->professor) - 1);
}

static bool parse_horario_line(const char *line, HorarioPDF *h) {
  char buf[MAX_LINE];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  memset(h, 0, sizeof(HorarioPDF));

  char *tok = strtok(buf, " \t");
  if (!tok)
    return false;

  strncpy(h->dia, tok, sizeof(h->dia) - 1);

  bool reading_slots = false;
  while ((tok = strtok(NULL, " \t")) != NULL) {
    if (is_time_slot(tok)) {
      reading_slots = true;
      if (h->num_intervalos < MAX_SLOTS) {
        strncpy(h->intervalos[h->num_intervalos++], tok, 15);
      }
    } else if (!reading_slots) {
      if (h->sala[0] != '\0') {
        strncat(h->sala, " ", sizeof(h->sala) - strlen(h->sala) - 1);
      }
      strncat(h->sala, tok, sizeof(h->sala) - strlen(h->sala) - 1);
    }
  }

  return h->num_intervalos > 0;
}
static bool
has_pdf_extension(const char *path) // vê os últimos 4 dígitos p/ ver se é .pdf
{
  size_t len = strlen(path);

  if (len < 4)
    return false;

  const char *ext = path + len - 4;

  return (tolower((unsigned char)ext[0]) == '.' &&
          tolower((unsigned char)ext[1]) == 'p' &&
          tolower((unsigned char)ext[2]) == 'd' &&
          tolower((unsigned char)ext[3]) == 'f');
}

/* Abre PDF via pdftotext -raw ou lê arquivo .txt normalmente */
static FILE *open_document(const char *path, bool *is_pipe) {
  *is_pipe = false;
  if (!has_pdf_extension(path)) {
    return fopen(path, "r");
  }

  char escaped[MAX_LINE];
  size_t j = 0;
  for (size_t i = 0; path[i] != '\0' && j + 2 < sizeof(escaped); i++) {
#ifdef _WIN32
    // no Windows a barra \ faz parte do caminho e aspas nao existem em
    // nome de arquivo, entao nada precisa ser escapado
    if (path[i] == '"')
      continue;
#else
    if (path[i] == '"' || path[i] == '\\' || path[i] == '$' || path[i] == '`') {
      escaped[j++] = '\\';
    }
#endif
    escaped[j++] = path[i];
  }
  escaped[j] = '\0';

  char cmd[MAX_LINE + 64];
  snprintf(cmd, sizeof(cmd), "pdftotext -raw \"%s\" -", escaped);

  *is_pipe = true;
  return abrir_comando(cmd);
}
static int horario_para_minutos(int hora, int minuto) {
  return hora * 60 + minuto;
}
static bool
converter_intervalo(const char *intervalo, // trasforma pra minutos já que a
                                           // lógica do código é em minuto
                    int *inicio, int *fim) {
  int h1, m1, h2, m2;

  if (intervalo == NULL || inicio == NULL || fim == NULL)
    return false;

  if (!is_time_slot(intervalo))
    return false;

  if (sscanf(intervalo, "%d:%d-%d:%d", // sscanf lê infos da memoria
             &h1, &m1, &h2, &m2) != 4) {
    return false;
  }

  if (h1 < 0 || h1 > 23 || h2 < 0 || h2 > 23 || m1 < 0 || m1 > 59 || m2 < 0 ||
      m2 > 59) {
    return false;
  }

  *inicio = horario_para_minutos(h1, m1);
  *fim = horario_para_minutos(h2, m2);

  return *inicio < *fim;
}
static bool
converter_dia(const char *texto,
              Dia *dia) // converte pro formato que a gente tá utilizando
{
  if (texto == NULL || dia == NULL)
    return false;

  if (strcmp(texto, "SEG") ==
      0) // strcmp compara strings e retorna 0 quando são iguais
    *dia = SEG;
  else if (strcmp(texto, "TER") == 0)
    *dia = TER;
  else if (strcmp(texto, "QUA") == 0)
    *dia = QUA;
  else if (strcmp(texto, "QUI") == 0)
    *dia = QUI;
  else if (strcmp(texto, "SEX") == 0)
    *dia = SEX;
  else if (strcmp(texto, "SAB") == 0)
    *dia = SAB;
  else if (strcmp(texto, "DOM") == 0)
    *dia = DOM;
  else
    return false;

  return true;
}
static void normalizar_sala(const char *nome, char *saida) {
  int j = 0;

  for (int i = 0; nome[i] != '\0' && j < 63; i++) {
    unsigned char c = nome[i];

    if (c != '-' && !isspace(c)) {
      saida[j++] = toupper(c);
    }
  }

  saida[j] = '\0';
}
static bool buscar_id_sala(const Aplicacao *app, const char *nome,
                           int *idSala) {
  if (!app || !nome || !idSala)
    return false;

  char nomePDF[64];
  char nomeSistema[64];

  normalizar_sala(nome, nomePDF);

  bool encontrou = false;

  for (size_t i = 0; i < app->quantidadeSalas; i++) {
    normalizar_sala(app->salas[i].nome, nomeSistema);

    if (strcmp(nomePDF, nomeSistema) == 0) {
      if (encontrou)
        return false;

      *idSala = app->salas[i].id;
      encontrou = true;
    }
  }

  return encontrou;
}

static Data adicionar_dia(Data data) {
  static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int limite = dias[data.mes - 1];
  if (data.mes == 2 &&
      ((data.ano % 4 == 0 && data.ano % 100 != 0) || data.ano % 400 == 0))
    limite++;

  if (++data.dia > limite) {
    data.dia = 1;
    if (++data.mes > 12) {
      data.mes = 1;
      data.ano++;
    }
  }
  return data;
}

static Data primeira_data_do_dia(Data inicio, Dia dia) {
  for (int i = 0; i < 7 && diaDaSemana(inicio) != dia; i++)
    inicio = adicionar_dia(inicio);
  return inicio;
}

// Copia no maximo 'max' bytes de 'origem' sem cortar uma letra acentuada
// no meio (em UTF-8, letras como "Ã" ocupam 2 bytes).
static void copiar_sem_cortar(char *destino, const char *origem, size_t max) {
  size_t n = strlen(origem);
  if (n > max) {
    n = max;
    // volta enquanto o byte seguinte for continuacao de uma letra (10xxxxxx)
    while (n > 0 && ((unsigned char)origem[n] & 0xC0) == 0x80)
      n--;
  }
  memcpy(destino, origem, n);
  destino[n] = '\0';
}

static Reserva criar_reserva(const TurmaPDF *turma, int idSala, Dia dia,
                             int inicio, int fim, Data dataInicio,
                             Data dataFim) {
  Reserva r = {0};

  r.idSala = idSala;
  r.atividade = AULA;
  r.isRecorrente = true;

  r.data = primeira_data_do_dia(dataInicio, dia);
  r.dataFim = dataFim;

  r.horario.dia = dia;
  r.horario.comeco = inicio;
  r.horario.fim = fim;

  // Descricao: "COMPONENTE - TURMA". A turma sempre aparece inteira; o nome
  // da disciplina usa todo o espaco que sobrar.
  char turmaTxt[16];
  copiar_sem_cortar(turmaTxt, turma->turma, 15);
  size_t espaco = sizeof(r.descricao) - 1 - strlen(turmaTxt) - 3;
  copiar_sem_cortar(r.descricao, turma->componente, espaco);
  strcat(r.descricao, " - "); // sempre cabe: o espaco ja foi descontado
  strcat(r.descricao, turmaTxt);

  copiar_sem_cortar(r.responsavel, turma->professor,
                    sizeof(r.responsavel) - 1);

  return r;
}
int lerPDF(const char *caminho, TurmaPDF **resultado, size_t *quantidade) {
  if (caminho == NULL || resultado == NULL || quantidade == NULL) {
    return 0; // 0 de falha
  }
  *resultado = NULL;
  *quantidade = 0;

  bool is_pipe = false;
  FILE *in = open_document(caminho, &is_pipe);
  if (!in) {
    perror("Erro ao abrir documento de entrada");
    return 0;
  }

  size_t cap = 32, count = 0;
  TurmaPDF *turmas = calloc(cap, sizeof(TurmaPDF));
  if (!turmas) {
    if (is_pipe)
      pclose(in);
    else
      fclose(in);
    return 0;
  }

  ParserState state = STATE_WAIT_TURMA;
  TurmaPDF current;
  memset(&current, 0, sizeof(TurmaPDF));
  bool has_current = false;

  char line[MAX_LINE];
  while (fgets(line, sizeof(line), in)) {
    trim(line);
    if (line[0] == '\0')
      continue;

    /* Descarta cabeçalhos de página antes de avaliar o estado atual */
    if (is_page_header_or_footer(line))
      continue;

    if (strncmp(line, "TURMA", 5) == 0 && strstr(line, "VAGAS") != NULL) {
      if (has_current) {
        if (count == cap) // o vetor tá cheio?
        {
          size_t nova_cap = cap * 2; // dobra a capacidade, o tanto de memoria
          TurmaPDF *tmp = realloc(
              turmas, nova_cap * sizeof(TurmaPDF)); // redimensiona a memoria
          if (!tmp) {
            if (is_pipe)
              pclose(in);

            else
              fclose(in);
            free(turmas);
            return 0; // a leitura falhou
          }

          turmas = tmp; // atualiza o ponteiro
          cap = nova_cap;
        }
        turmas[count++] = current; // armazena a turma
      }
      memset(&current, 0, sizeof(TurmaPDF));
      has_current = true;
      state = STATE_READ_TURMA;
      continue;
    }

    switch (state) {
    case STATE_WAIT_TURMA:
      break;

    case STATE_READ_TURMA:
      parse_turma_line(line, &current);
      state = STATE_WAIT_PROF;
      break;

    case STATE_WAIT_PROF:
      if (strncmp(line, "PROFESSOR", 9) == 0) {
        state = STATE_READ_PROF;
      }
      break;

    case STATE_READ_PROF:
      if (strncmp(line, "DIA DA SEMANA", 13) == 0) {
        state = STATE_READ_HORARIOS;
      } else {
        parse_prof_line(line, &current);
      }
      break;

    case STATE_READ_HORARIOS: {
      HorarioPDF h;
      if (parse_horario_line(line, &h) && current.num_horarios < MAX_DIAS) {
        current.horarios[current.num_horarios++] = h;
      }
      break;
    }
    }
  }

  if (has_current) {
    if (count == cap) {
      size_t nova_cap = cap * 2;
      TurmaPDF *tmp = realloc(turmas, nova_cap * sizeof(TurmaPDF));
      if (!tmp) {
        if (is_pipe)
          pclose(in);
        else
          fclose(in);
        free(turmas);
        return 0;
      }
      turmas = tmp;
      cap = nova_cap;
    }
    turmas[count++] = current;
  }

  bool erro_leitura = ferror(in) != 0;

  int status;

  if (is_pipe)
    status = pclose(in);
  else
    status = fclose(in);

  if (erro_leitura || status != 0) {
    free(turmas);
    // Se o PDF passou pelo pdftotext e deu erro, o problema e a leitura do
    // PDF (por exemplo, o pdftotext nao esta instalado), e nao o formato.
    return is_pipe ? LEITOR_SEM_PDF : LEITOR_ERRO;
  }

  if (count == 0) {
    free(turmas);
    return LEITOR_ERRO;
  }
  *resultado = turmas;
  *quantidade = count;

  return 1;
}

int converter_turmas_reservas(const Aplicacao *app, const TurmaPDF *turmas,
                              size_t quantidadeTurmas, Data dataInicio,
                              Data dataFim, Reserva **resultado,
                              size_t *quantidadeReservas, size_t *ignorados) {
  if (!app || !turmas || !resultado || !quantidadeReservas)
    return 0;

  *resultado = NULL;
  *quantidadeReservas = 0;
  if (ignorados)
    *ignorados = 0;

  size_t total = 0;

  // Calcula a quantidade maxima de reservas
  for (size_t i = 0; i < quantidadeTurmas; i++) {
    for (int j = 0; j < turmas[i].num_horarios; j++) {
      total += turmas[i].horarios[j].num_intervalos;
    }
  }

  if (total == 0)
    return 0;

  Reserva *reservas = calloc(total, sizeof(Reserva));

  if (reservas == NULL)
    return 0;

  size_t count = 0;

  for (size_t i = 0; i < quantidadeTurmas; i++) {
    const TurmaPDF *t = &turmas[i];

    for (int j = 0; j < t->num_horarios; j++) {
      const HorarioPDF *h = &t->horarios[j];

      Dia dia;
      int idSala;

      // Sala nao cadastrada ou dia invalido: pula so este horario
      if (!converter_dia(h->dia, &dia) ||
          !buscar_id_sala(app, h->sala, &idSala)) {
        fprintf(stderr, "Ignorado (dia ou sala invalida): %s | %s | %s\n",
                t->componente, h->dia, h->sala);
        if (ignorados)
          (*ignorados)++;
        continue;
      }

      // Primeiro confere todos os intervalos; se algum estiver errado,
      // pula o horario inteiro
      bool intervalos_ok = h->num_intervalos > 0;
      int ultimoFim = -1;
      for (int k = 0; k < h->num_intervalos && intervalos_ok; k++) {
        int inicio, fim;
        if (!converter_intervalo(h->intervalos[k], &inicio, &fim) ||
            inicio < ultimoFim)
          intervalos_ok = false;
        else
          ultimoFim = fim;
      }
      if (!intervalos_ok) {
        fprintf(stderr, "Ignorado (horario invalido): %s | %s\n",
                t->componente, h->dia);
        if (ignorados)
          (*ignorados)++;
        continue;
      }

      // Junta intervalos seguidos (07:10-08:00 + 08:00-08:50) num bloco so
      int inicioBloco, fimBloco;
      converter_intervalo(h->intervalos[0], &inicioBloco, &fimBloco);

      for (int k = 1; k < h->num_intervalos; k++) {
        int inicio, fim;
        converter_intervalo(h->intervalos[k], &inicio, &fim);

        if (inicio == fimBloco) {
          fimBloco = fim;
        } else {
          reservas[count++] = criar_reserva(t, idSala, dia, inicioBloco,
                                            fimBloco, dataInicio, dataFim);
          inicioBloco = inicio;
          fimBloco = fim;
        }
      }

      reservas[count++] = criar_reserva(t, idSala, dia, inicioBloco, fimBloco,
                                        dataInicio, dataFim);
    }
  }

  *resultado = reservas;
  *quantidadeReservas = count;

  return 1;
}

static bool reservas_iguais(Reserva a, Reserva b) {
  return a.idSala == b.idSala && a.atividade == b.atividade &&
         a.isRecorrente == b.isRecorrente && datasIguais(a.data, b.data) &&
         datasIguais(a.dataFim, b.dataFim) && a.horario.dia == b.horario.dia &&
         a.horario.comeco == b.horario.comeco &&
         a.horario.fim == b.horario.fim &&
         strcmp(a.descricao, b.descricao) == 0 &&
         strcmp(a.responsavel, b.responsavel) == 0;
}

int importarPDF(Aplicacao *app, const char *caminho, Data inicio, Data fim,
                ResumoImportacao *resumo) {
  if (!app || !caminho || !resumo)
    return LEITOR_ERRO;

  memset(resumo, 0, sizeof *resumo);

  TurmaPDF *turmas = NULL;
  size_t quantidadeTurmas = 0;

  Reserva *reservas = NULL;
  size_t quantidadeReservas = 0;

  int lido = lerPDF(caminho, &turmas, &quantidadeTurmas);
  if (lido != LEITOR_OK)
    return lido; // LEITOR_ERRO ou LEITOR_SEM_PDF

  int sucesso = converter_turmas_reservas(app, turmas, quantidadeTurmas, inicio,
                                          fim, &reservas, &quantidadeReservas,
                                          &resumo->semSala);

  free(turmas);

  if (!sucesso)
    return LEITOR_ERRO;

  size_t validas = 0;

  for (size_t i = 0; i < quantidadeReservas; i++) {
    Reserva nova = reservas[i];
    bool duplicada = false;
    bool conflito = false;

    // Compara com reservas existentes
    for (size_t j = 0; j < app->quantidadeReservas; j++) {
      Reserva existente = app->reservas[j];

      if (reservas_iguais(nova, existente))
        duplicada = true;
      else if (reservasConflitam(nova, existente))
        conflito = true;
    }

    // Compara com outras reservas do mesmo PDF que ja entraram
    for (size_t j = 0; j < validas; j++) {
      if (reservas_iguais(nova, reservas[j]))
        duplicada = true;
      else if (reservasConflitam(nova, reservas[j]))
        conflito = true;
    }

    if (duplicada) {
      resumo->duplicadas++;
      continue;
    }

    // Conflito: pula so esta reserva e continua com as outras
    if (conflito) {
      fprintf(stderr, "Ignorado (conflito): %s | Sala ID: %d\n",
              nova.descricao, nova.idSala);
      resumo->conflitos++;
      continue;
    }

    reservas[validas++] = nova;
  }

  if (validas > 0) {
    if (!aplicacao_salvar_reservas(app, reservas, validas)) {
      free(reservas);
      return LEITOR_ERRO;
    }
  }

  resumo->importadas = validas;

  free(reservas);

  return LEITOR_OK;
}
