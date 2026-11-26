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
    if (path[i] == '"' || path[i] == '\\' || path[i] == '$' || path[i] == '`') {
      escaped[j++] = '\\';
    }
    escaped[j++] = path[i];
  }
  escaped[j] = '\0';

  char cmd[MAX_LINE + 64];
  snprintf(cmd, sizeof(cmd), "pdftotext -raw \"%s\" -", escaped);

  *is_pipe = true;
  return popen(cmd, "r");
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

  snprintf(r.descricao, sizeof(r.descricao), // snprintf ao invés de imprimir no
                                             // terminal, escreve em uma string
           "%.31s - %.15s", turma->componente, turma->turma);

  snprintf(r.responsavel, sizeof(r.responsavel), "%.79s", turma->professor);

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
    return 0;
  }

  if (count == 0) {
    free(turmas);
    return 0;
  }
  *resultado = turmas;
  *quantidade = count;

  return 1;
}

int converter_turmas_reservas(const Aplicacao *app, const TurmaPDF *turmas,
                              size_t quantidadeTurmas, Data dataInicio,
                              Data dataFim, Reserva **resultado,
                              size_t *quantidadeReservas) {
  if (!app || !turmas || !resultado || !quantidadeReservas)
    return 0;

  *resultado = NULL;
  *quantidadeReservas = 0;

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

      if (!converter_dia(h->dia, &dia) ||
          !buscar_id_sala(app, h->sala, &idSala)) {
        fprintf(stderr, "Dia ou sala invalida: %s | %s\n", h->dia, h->sala);

        free(reservas);
        return 0;
      }

      int inicioBloco, fimBloco;

      if (h->num_intervalos <= 0 ||
          !converter_intervalo(h->intervalos[0], &inicioBloco, &fimBloco)) {
        free(reservas);
        return 0;
      }

      for (int k = 1; k < h->num_intervalos; k++) {
        int inicio, fim;

        if (!converter_intervalo(h->intervalos[k], &inicio, &fim)) {
          free(reservas);
          return 0;
        }

        if (inicio == fimBloco) {
          fimBloco = fim;
        } else if (inicio > fimBloco) {
          reservas[count++] = criar_reserva(t, idSala, dia, inicioBloco,
                                            fimBloco, dataInicio, dataFim);

          inicioBloco = inicio;
          fimBloco = fim;
        } else {
          fprintf(stderr, "Intervalos sobrepostos ou fora de ordem.\n");
          free(reservas);
          return 0;
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
                size_t *importadas, size_t *duplicadas) {
  if (!app || !caminho || !importadas || !duplicadas)
    return 0;

  *importadas = 0;
  *duplicadas = 0;

  TurmaPDF *turmas = NULL;
  size_t quantidadeTurmas = 0;

  Reserva *reservas = NULL;
  size_t quantidadeReservas = 0;

  if (!lerPDF(caminho, &turmas, &quantidadeTurmas))
    return 0;

  int sucesso = converter_turmas_reservas(app, turmas, quantidadeTurmas, inicio,
                                          fim, &reservas, &quantidadeReservas);

  free(turmas);

  if (!sucesso)
    return 0;

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

    // Compara com outras reservas do mesmo PDF
    for (size_t j = 0; j < validas; j++) {
      if (reservas_iguais(nova, reservas[j]))
        duplicada = true;
      else if (reservasConflitam(nova, reservas[j]))
        conflito = true;
    }

    if (conflito) {
      fprintf(stderr, "Conflito: %s | Sala ID: %d\n", nova.descricao,
              nova.idSala);

      free(reservas);
      return 0;
    }

    if (duplicada) {
      (*duplicadas)++;
      continue;
    }

    reservas[validas++] = nova;
  }

  if (validas > 0) {
    if (!aplicacao_salvar_reservas(app, reservas, validas)) {
      free(reservas);
      return 0;
    }
  }

  *importadas = validas;

  free(reservas);

  return 1;
}
