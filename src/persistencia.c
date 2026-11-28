// persistencia.c - le e grava salas e reservas em arquivos CSV.
//
// Gravacao segura:
//   1. grava tudo em "<arquivo>.tmp";
//   2. forca os dados a irem para o disco (fflush + fsync/_commit);
//   3. copia a versao anterior para "<arquivo>.bak";
//   4. troca o .tmp pelo arquivo verdadeiro de uma vez so (rename).
// Se o programa fechar no meio, o arquivo original continua inteiro.

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "../include/persistencia.h"
#include "../include/alocacao.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#define CAB_SALAS "#POLI-SALAS;SALAS;v2"
#define CAB_SALAS_LEGADO "#POLI-SALAS;SALAS;v1"
#define CAB_RESERVAS "#POLI-SALAS;RESERVAS;v3"
#define CAB_RESERVAS_LEGADO "#POLI-SALAS;RESERVAS;v2"
#define CAB_CANCELAMENTOS "#POLI-SALAS;CANCELAMENTOS;v1"
#define SEPARADOR ';'
#define LINHA_MAX 1024
// id;nome;capacidade;acessivel;projetor;televisao;computadores;local
#define CAMPOS_SALA 8
// id;id_sala;atividade;recorrente;data_inicio;fim_recorrencia;dia;inicio;fim;descricao;responsavel
#define CAMPOS_RESERVA 11
#define CAMPOS_RESERVA_V2 10
// id_reserva;data;motivo
#define CAMPOS_CANCELAMENTO 3
#define CAMPOS_MAX 16
#define MINUTOS_MAX 1439 // 23:59
#define DATA_REFERENCIA_LEGADA 20261113

// Nomes usados no arquivo para os enums (a posicao e o valor do enum)
static const char *const NOMES_ATIVIDADE[] = {"AULA", "MONITORIA", "EXTENSAO",
                                              "EVENTO", "OUTRO"};
static const char *const NOMES_DIA[] = {"SEG", "TER", "QUA", "QUI",
                                        "SEX", "SAB", "DOM"};

#define QTD_ATIVIDADES                                                         \
  ((int)(sizeof NOMES_ATIVIDADE / sizeof NOMES_ATIVIDADE[0]))
#define QTD_DIAS ((int)(sizeof NOMES_DIA / sizeof NOMES_DIA[0]))

// ---- Validacao ----

// Texto cabe no campo e nao tem separador nem quebra de linha
static int texto_valido(const char *s, size_t tam_campo, int permite_vazio) {
  const char *fim = memchr(s, '\0', tam_campo);
  if (fim == NULL)
    return 0; // sem '\0' no final
  if (!permite_vazio && fim == s)
    return 0;
  for (; s < fim; s++) {
    if (*s == SEPARADOR || *s == '\n' || *s == '\r')
      return 0;
  }
  return 1;
}

static int eh_bissexto(int a) {
  return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0;
}

static int data_vazia(const Data *d) {
  return d->dia == 0 && d->mes == 0 && d->ano == 0;
}

static int data_valida(const Data *d) {
  static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (d->ano < 1900 || d->ano > 9999 || d->mes < 1 || d->mes > 12 || d->dia < 1)
    return 0;
  return d->dia <= dias[d->mes - 1] + (d->mes == 2 && eh_bissexto(d->ano));
}

static Data adicionar_dia(Data data) {
  static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  data.dia++;
  int limite = dias[data.mes - 1];
  if (data.mes == 2 && eh_bissexto(data.ano))
    limite++;
  if (data.dia > limite) {
    data.dia = 1;
    data.mes++;
    if (data.mes > 12) {
      data.mes = 1;
      data.ano++;
    }
  }
  return data;
}

// Confere se o valor do bool e 0 ou 1
static int bool_valido(bool b) {
  unsigned char byte;
  memcpy(&byte, &b, 1);
  return byte == 0 || byte == 1;
}

// Compara dois textos sem diferenciar maiusculas e minusculas
static int iguais_sem_caixa(const char *a, const char *b) {
  for (; *a && *b; a++, b++) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return 0;
  }
  return *a == *b;
}

static int sala_valida(const Sala *s) {
  return s->id > 0 && texto_valido(s->nome, sizeof s->nome, 0) &&
         s->capacidade >= 0 && bool_valido(s->isAcessivel) &&
         bool_valido(s->hasProjetor) && bool_valido(s->hasTelevisao) &&
         s->computador >= 0 && texto_valido(s->local, sizeof s->local, 1);
}

static int reserva_valida(const Reserva *r) {
  if (r->id <= 0 || r->idSala <= 0)
    return 0;
  if ((int)r->atividade < 0 || (int)r->atividade >= QTD_ATIVIDADES)
    return 0;
  if (!bool_valido(r->isRecorrente))
    return 0;

  // Recorrencias valem da data de inicio ate o fim do semestre.
  // O fim pode cair em qualquer dia, ate no ano seguinte.
  if (r->isRecorrente) {
    if (!data_valida(&r->data) || !data_valida(&r->dataFim))
      return 0;

    int inicio = r->data.ano * 10000 + r->data.mes * 100 + r->data.dia;

    int fim = r->dataFim.ano * 10000 + r->dataFim.mes * 100 + r->dataFim.dia;

    if (fim < inicio || diaDaSemana(r->data) != r->horario.dia)
      return 0;
  } else if (!data_valida(&r->data) || !data_vazia(&r->dataFim)) {
    return 0;
  }

  if ((int)r->horario.dia < 0 || (int)r->horario.dia >= QTD_DIAS)
    return 0;
  if (r->horario.comeco < 0 || r->horario.fim > MINUTOS_MAX ||
      r->horario.fim <= r->horario.comeco)
    return 0;

  return texto_valido(r->descricao, sizeof r->descricao, 0) &&
         texto_valido(r->responsavel, sizeof r->responsavel, 0);
}

// Duas salas nao podem ter o mesmo id nem o mesmo nome
static int salas_duplicadas(const void *x, const void *y) {
  const Sala *a = x, *b = y;
  return a->id == b->id || iguais_sem_caixa(a->nome, b->nome);
}

// Duas reservas nao podem ter o mesmo id
static int reservas_duplicadas(const void *x, const void *y) {
  const Reserva *a = x, *b = y;
  return a->id == b->id;
}

static int cancelamento_valido(const Cancelamento *c) {
  return c->idReserva > 0 && !data_vazia(&c->data) && data_valida(&c->data) &&
         texto_valido(c->motivo, sizeof c->motivo, 1);
}

// A mesma reserva nao pode ser cancelada duas vezes no mesmo dia
static int cancelamentos_duplicados(const void *x, const void *y) {
  const Cancelamento *a = x, *b = y;
  return a->idReserva == b->idReserva && a->data.dia == b->data.dia &&
         a->data.mes == b->data.mes && a->data.ano == b->data.ano;
}

// ---- Leitura de uma linha ----

// Le uma linha sem o '\n' (e sem o '\r' que o Windows coloca).
// Retorna 1 = linha lida, 0 = fim do arquivo, -1 = erro de leitura,
//         2 = linha longa demais (o resto da linha e descartado).
static int ler_linha(FILE *f, char *buf, size_t tam) {
  size_t len;
  int c;

  if (fgets(buf, (int)tam, f) == NULL)
    return ferror(f) ? -1 : 0;

  len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n') {
    buf[--len] = '\0';
  } else if (!feof(f)) {
    while ((c = fgetc(f)) != EOF && c != '\n') {
      // descarta o resto da linha
    }
    return ferror(f) ? -1 : 2;
  }
  if (len > 0 && buf[len - 1] == '\r')
    buf[--len] = '\0';
  return 1;
}

// Corta a linha nos ';' (modifica a linha). Retorna quantos campos achou.
static int dividir_campos(char *linha, char *campos[], int max) {
  int n = 0;
  char *p = linha;
  campos[n++] = p;
  for (; *p; p++) {
    if (*p == SEPARADOR) {
      if (n == max)
        return -1;
      *p = '\0';
      campos[n++] = p + 1;
    }
  }
  return n;
}

static int ler_inteiro(const char *s, long min, long max, int *saida) {
  char *fim;
  long v;
  if (*s == '\0' || isspace((unsigned char)*s))
    return 0;
  errno = 0;
  v = strtol(s, &fim, 10);
  if (errno != 0 || *fim != '\0' || v < min || v > max)
    return 0;
  *saida = (int)v;
  return 1;
}

static int ler_bool(const char *s, bool *saida) {
  if (strcmp(s, "0") == 0) {
    *saida = false;
    return 1;
  }
  if (strcmp(s, "1") == 0) {
    *saida = true;
    return 1;
  }
  return 0;
}

// Procura o texto na tabela de nomes. Retorna a posicao ou -1.
static int ler_nome(const char *s, const char *const tabela[], int qtd) {
  int i;
  for (i = 0; i < qtd; i++) {
    if (strcmp(s, tabela[i]) == 0)
      return i;
  }
  return -1;
}

static int copiar_texto(char *dst, size_t tam, const char *src) {
  size_t len = strlen(src);
  if (len >= tam)
    return 0;
  memcpy(dst, src, len + 1);
  return 1;
}

static int digitos(const char *s, int qtd) {
  int i;
  for (i = 0; i < qtd; i++) {
    if (!isdigit((unsigned char)s[i]))
      return 0;
  }
  return 1;
}

// "AAAA-MM-DD" -> Data. Texto vazio vira 0/0/0.
static int ler_data(const char *s, Data *d) {
  if (*s == '\0') {
    d->dia = d->mes = d->ano = 0;
    return 1;
  }
  if (strlen(s) != 10 || !digitos(s, 4) || s[4] != '-' || !digitos(s + 5, 2) ||
      s[7] != '-' || !digitos(s + 8, 2))
    return 0;
  d->ano = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 +
           (s[3] - '0');
  d->mes = (s[5] - '0') * 10 + (s[6] - '0');
  d->dia = (s[8] - '0') * 10 + (s[9] - '0');
  return 1;
}

// "HH:MM" -> minutos desde 00:00
static int ler_hora(const char *s, int *minutos) {
  int h, m;
  if (strlen(s) != 5 || !digitos(s, 2) || s[2] != ':' || !digitos(s + 3, 2))
    return 0;
  h = (s[0] - '0') * 10 + (s[1] - '0');
  m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59)
    return 0;
  *minutos = h * 60 + m;
  return 1;
}

// ---- Linha do arquivo -> struct ----

static int ler_sala(char **c, int n, void *destino) {
  Sala *s = destino;
  if (n != CAMPOS_SALA && n != CAMPOS_SALA - 1)
    return 0;
  memset(s, 0, sizeof *s);
  if (!ler_inteiro(c[0], 1, INT_MAX, &s->id))
    return 0;
  if (!copiar_texto(s->nome, sizeof s->nome, c[1]))
    return 0;
  if (!ler_inteiro(c[2], 0, INT_MAX, &s->capacidade))
    return 0;
  if (!ler_bool(c[3], &s->isAcessivel))
    return 0;
  if (!ler_bool(c[4], &s->hasProjetor))
    return 0;
  if (!ler_bool(c[5], &s->hasTelevisao))
    return 0;
  if (!ler_inteiro(c[6], 0, INT_MAX, &s->computador))
    return 0;
  if (n == CAMPOS_SALA && !copiar_texto(s->local, sizeof s->local, c[7]))
    return 0;
  return sala_valida(s);
}

static Data proxima_data_recorrente(Data data, Dia dia) {
  for (int i = 0; i < 7 && diaDaSemana(data) != dia; i++) {
    if (data.ano == 9999 && data.mes == 12 && data.dia == 31)
      return (Data){0};
    data = adicionar_dia(data);
  }
  return data;
}

static int ler_reserva(char **c, int n, void *destino) {
  Reserva *r = destino;
  int atividade, dia, deslocamento;
  if (n != CAMPOS_RESERVA && n != CAMPOS_RESERVA_V2)
    return 0;
  memset(r, 0, sizeof *r);
  deslocamento = n == CAMPOS_RESERVA ? 1 : 0;
  if (!ler_inteiro(c[0], 1, INT_MAX, &r->id))
    return 0;
  if (!ler_inteiro(c[1], 1, INT_MAX, &r->idSala))
    return 0;
  atividade = ler_nome(c[2], NOMES_ATIVIDADE, QTD_ATIVIDADES);
  if (atividade < 0)
    return 0;
  r->atividade = (Atividade)atividade;
  if (!ler_bool(c[3], &r->isRecorrente))
    return 0;
  if (!ler_data(c[4], &r->data))
    return 0;
  if (deslocamento && !ler_data(c[5], &r->dataFim))
    return 0;
  dia = ler_nome(c[5 + deslocamento], NOMES_DIA, QTD_DIAS);
  if (dia < 0)
    return 0;
  r->horario.dia = (Dia)dia;
  if (!ler_hora(c[6 + deslocamento], &r->horario.comeco))
    return 0;
  if (!ler_hora(c[7 + deslocamento], &r->horario.fim))
    return 0;
  if (!copiar_texto(r->descricao, sizeof r->descricao, c[8 + deslocamento]))
    return 0;
  if (!copiar_texto(r->responsavel, sizeof r->responsavel, c[9 + deslocamento]))
    return 0;

  if (!deslocamento && r->isRecorrente) {
    if (data_vazia(&r->data)) {
      r->data.dia = DATA_REFERENCIA_LEGADA % 100;
      r->data.mes = (DATA_REFERENCIA_LEGADA / 100) % 100;
      r->data.ano = DATA_REFERENCIA_LEGADA / 10000;
    } else if (!data_valida(&r->data))
      return 0;
    r->data = proxima_data_recorrente(r->data, r->horario.dia);
    r->dataFim = (Data){31, 12, r->data.ano};
  }
  return reserva_valida(r);
}

static int ler_cancelamento(char **c, int n, void *destino) {
  Cancelamento *x = destino;
  if (n != CAMPOS_CANCELAMENTO)
    return 0;
  memset(x, 0, sizeof *x);
  return ler_inteiro(c[0], 1, INT_MAX, &x->idReserva) &&
         ler_data(c[1], &x->data) &&
         copiar_texto(x->motivo, sizeof x->motivo, c[2]) &&
         cancelamento_valido(x);
}

// ---- Carregar (serve para salas, reservas e cancelamentos) ----

typedef int (*LerRegistroFn)(char **campos, int n, void *destino);
typedef int (*DuplicadoFn)(const void *a, const void *b);

static int ja_existe(const char *base, size_t n, size_t tam, DuplicadoFn dup,
                     const void *reg) {
  size_t i;
  if (dup == NULL)
    return 0;
  for (i = 0; i < n; i++) {
    if (dup(base + i * tam, reg))
      return 1;
  }
  return 0;
}

static void marcar_invalida(PersRelatorio *rel, size_t num_linha) {
  rel->linhas_invalidas++;
  if (rel->primeira_linha_invalida == 0)
    rel->primeira_linha_invalida = num_linha;
}

static PersStatus carregar(const char *caminho, const char *cabecalho,
                           const char *cabecalhoLegado, size_t tam,
                           LerRegistroFn ler, DuplicadoFn dup, void **saida,
                           size_t *n_saida, PersRelatorio *rel_usuario) {
  PersRelatorio rel_local;
  PersRelatorio *rel = rel_usuario ? rel_usuario : &rel_local;
  FILE *f;
  char linha[LINHA_MAX];
  char *campos[CAMPOS_MAX];
  char *vetor = NULL;
  void *registro = NULL;
  size_t n = 0, cap = 0, num_linha = 0;
  int achou_cabecalho = 0, r;
  PersStatus status = PERS_OK;

  if (saida)
    *saida = NULL;
  if (n_saida)
    *n_saida = 0;
  if (!caminho || !saida || !n_saida)
    return PERS_ERRO_PARAMETRO;
  memset(rel, 0, sizeof *rel);

  f = fopen(caminho, "rb");
  if (f == NULL) {
    if (errno == ENOENT) { // arquivo nao existe: primeira vez, lista vazia
      rel->arquivo_ausente = 1;
      return PERS_OK;
    }
    return PERS_ERRO_ABRIR;
  }

  registro = malloc(tam);
  if (registro == NULL) {
    fclose(f);
    return PERS_ERRO_MEMORIA;
  }

  while ((r = ler_linha(f, linha, sizeof linha)) != 0) {
    char *p = linha;
    num_linha++;
    if (r < 0) {
      status = PERS_ERRO_LEITURA;
      break;
    }

    // Pula os 3 bytes que o Bloco de Notas as vezes coloca no inicio
    if (num_linha == 1 && (unsigned char)p[0] == 0xEF &&
        (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
      p += 3;
    }

    if (!achou_cabecalho) {
      if (r == 1 && p[0] == '\0')
        continue; // linha em branco
      if (r != 1 ||
          (strcmp(p, cabecalho) != 0 &&
           (cabecalhoLegado == NULL || strcmp(p, cabecalhoLegado) != 0))) {
        status = PERS_ERRO_FORMATO;
        break;
      }
      achou_cabecalho = 1;
      continue;
    }

    if (r == 1 && (p[0] == '\0' || p[0] == '#'))
      continue; // linha em branco ou comentario

    if (r == 2 ||
        !ler(campos, dividir_campos(p, campos, CAMPOS_MAX), registro) ||
        ja_existe(vetor, n, tam, dup, registro)) {
      marcar_invalida(rel, num_linha);
      continue;
    }

    // Lista cheia: dobra o tamanho (16, 32, 64...)
    if (n == cap) {
      size_t nova = cap ? cap * 2 : 16;
      char *tmp = realloc(vetor, nova * tam);
      if (tmp == NULL) {
        status = PERS_ERRO_MEMORIA;
        break;
      }
      vetor = tmp;
      cap = nova;
    }
    memcpy(vetor + n * tam, registro, tam);
    n++;
  }

  // Arquivo vazio chega aqui com n == 0 e vira lista vazia
  free(registro);
  fclose(f);

  if (status != PERS_OK) {
    free(vetor);
    rel->registros_lidos = 0;
    return status;
  }

  rel->registros_lidos = n;
  *saida = vetor;
  *n_saida = n;
  return PERS_OK;
}

// ---- Gravar ----

typedef int (*EscreverFn)(FILE *f, const void *registro);

static int escrever_sala(FILE *f, const void *r) {
  const Sala *s = r;
  return fprintf(f, "%d;%s;%d;%d;%d;%d;%d;%s\n", s->id, s->nome, s->capacidade,
                 s->isAcessivel ? 1 : 0, s->hasProjetor ? 1 : 0,
                 s->hasTelevisao ? 1 : 0, s->computador, s->local);
}

static int escrever_reserva(FILE *f, const void *x) {
  const Reserva *r = x;
  char data[16] = "", dataFim[16] = "";
  if (!data_vazia(&r->data))
    snprintf(data, sizeof data, "%04d-%02d-%02d", r->data.ano, r->data.mes,
             r->data.dia);
  if (!data_vazia(&r->dataFim))
    snprintf(dataFim, sizeof dataFim, "%04d-%02d-%02d", r->dataFim.ano,
             r->dataFim.mes, r->dataFim.dia);
  return fprintf(
      f, "%d;%d;%s;%d;%s;%s;%s;%02d:%02d;%02d:%02d;%s;%s\n", r->id, r->idSala,
      NOMES_ATIVIDADE[r->atividade], r->isRecorrente ? 1 : 0, data, dataFim,
      NOMES_DIA[r->horario.dia], r->horario.comeco / 60, r->horario.comeco % 60,
      r->horario.fim / 60, r->horario.fim % 60, r->descricao, r->responsavel);
}

static int escrever_cancelamento(FILE *f, const void *x) {
  const Cancelamento *c = x;
  return fprintf(f, "%d;%04d-%02d-%02d;%s\n", c->idReserva, c->data.ano,
                 c->data.mes, c->data.dia, c->motivo);
}

// Obriga os dados a irem para o disco agora
static int sincronizar_disco(FILE *f) {
#ifdef _WIN32
  return _commit(_fileno(f));
#else
  return fsync(fileno(f));
#endif
}

// Troca 'destino' por 'origem' de uma vez so. Retorna 0 se deu certo.
static int substituir_arquivo(const char *origem, const char *destino) {
#ifdef _WIN32
  return MoveFileExA(origem, destino,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
             ? 0
             : -1;
#else
  return rename(origem, destino);
#endif
}

// Copia um arquivo. Retorna 0 = ok, 1 = origem nao existe, -1 = erro.
static int copiar_arquivo(const char *origem, const char *destino) {
  char buf[4096];
  size_t lidos;
  int ok = 1;
  FILE *in, *out;

  in = fopen(origem, "rb");
  if (in == NULL)
    return errno == ENOENT ? 1 : -1;
  out = fopen(destino, "wb");
  if (out == NULL) {
    fclose(in);
    return -1;
  }

  while ((lidos = fread(buf, 1, sizeof buf, in)) > 0) {
    if (fwrite(buf, 1, lidos, out) != lidos) {
      ok = 0;
      break;
    }
  }
  if (ferror(in))
    ok = 0;
  if (fflush(out) != 0 || sincronizar_disco(out) != 0)
    ok = 0;
  if (fclose(out) != 0)
    ok = 0;
  fclose(in);
  return ok ? 0 : -1;
}

static int montar_caminho(char *dst, const char *caminho, const char *sufixo) {
  int n = snprintf(dst, PERS_CAMINHO_MAX, "%s%s", caminho, sufixo);
  return n > 0 && n < PERS_CAMINHO_MAX;
}

static PersStatus gravar(const char *caminho, const char *cabecalho,
                         const void *dados, size_t n, size_t tam,
                         EscreverFn escrever) {
  char tmp[PERS_CAMINHO_MAX], bak[PERS_CAMINHO_MAX];
  const char *base = dados;
  FILE *f;
  size_t i;
  int ok;

  if (!montar_caminho(tmp, caminho, ".tmp") ||
      !montar_caminho(bak, caminho, ".bak"))
    return PERS_ERRO_PARAMETRO;

  f = fopen(tmp, "wb");
  if (f == NULL)
    return PERS_ERRO_ABRIR;

  ok = fprintf(f, "%s\n", cabecalho) >= 0;
  for (i = 0; ok && i < n; i++) {
    ok = escrever(f, base + i * tam) >= 0;
  }
  if (ok)
    ok = fflush(f) == 0 && sincronizar_disco(f) == 0;
  if (fclose(f) != 0)
    ok = 0;

  if (!ok) {
    remove(tmp); // o arquivo original nao foi mexido
    return PERS_ERRO_ESCRITA;
  }

  // Copia de seguranca da versao anterior
  (void)copiar_arquivo(caminho, bak);

  if (substituir_arquivo(tmp, caminho) != 0) {
    remove(tmp);
    return PERS_ERRO_ESCRITA;
  }
  return PERS_OK;
}

// Confere todos os registros (e se ha repetidos) antes de gravar
static int vetor_valido(const void *dados, size_t n, size_t tam,
                        int (*valido)(const void *), DuplicadoFn dup) {
  const char *base = dados;
  size_t i;
  for (i = 0; i < n; i++) {
    if (!valido(base + i * tam))
      return 0;
    if (ja_existe(base, i, tam, dup, base + i * tam))
      return 0;
  }
  return 1;
}

static int sala_valida_v(const void *r) { return sala_valida(r); }
static int reserva_valida_v(const void *r) { return reserva_valida(r); }
static int cancelamento_valido_v(const void *r) {
  return cancelamento_valido(r);
}

// ---- Funcoes do persistencia.h ----

PersStatus pers_salvar_salas(const char *caminho, const Sala *v, size_t n) {
  if (caminho == NULL || (v == NULL && n > 0))
    return PERS_ERRO_PARAMETRO;
  if (!vetor_valido(v, n, sizeof *v, sala_valida_v, salas_duplicadas))
    return PERS_ERRO_DADO_INVALIDO;
  return gravar(caminho, CAB_SALAS, v, n, sizeof *v, escrever_sala);
}

PersStatus pers_carregar_salas(const char *caminho, Sala **v, size_t *n,
                               PersRelatorio *rel) {
  return carregar(caminho, CAB_SALAS, CAB_SALAS_LEGADO, sizeof(Sala), ler_sala,
                  salas_duplicadas, (void **)v, n, rel);
}

PersStatus pers_salvar_reservas(const char *caminho, const Reserva *v,
                                size_t n) {
  if (caminho == NULL || (v == NULL && n > 0))
    return PERS_ERRO_PARAMETRO;
  if (!vetor_valido(v, n, sizeof *v, reserva_valida_v, reservas_duplicadas))
    return PERS_ERRO_DADO_INVALIDO;
  return gravar(caminho, CAB_RESERVAS, v, n, sizeof *v, escrever_reserva);
}

PersStatus pers_carregar_reservas(const char *caminho, Reserva **v, size_t *n,
                                  PersRelatorio *rel) {
  return carregar(caminho, CAB_RESERVAS, CAB_RESERVAS_LEGADO, sizeof(Reserva),
                  ler_reserva, reservas_duplicadas, (void **)v, n, rel);
}

PersStatus pers_salvar_cancelamentos(const char *caminho, const Cancelamento *v,
                                     size_t n) {
  if (caminho == NULL || (v == NULL && n > 0))
    return PERS_ERRO_PARAMETRO;
  if (!vetor_valido(v, n, sizeof *v, cancelamento_valido_v,
                    cancelamentos_duplicados))
    return PERS_ERRO_DADO_INVALIDO;
  return gravar(caminho, CAB_CANCELAMENTOS, v, n, sizeof *v,
                escrever_cancelamento);
}

PersStatus pers_carregar_cancelamentos(const char *caminho, Cancelamento **v,
                                       size_t *n, PersRelatorio *rel) {
  return carregar(caminho, CAB_CANCELAMENTOS, NULL, sizeof(Cancelamento),
                  ler_cancelamento, cancelamentos_duplicados, (void **)v, n,
                  rel);
}

void pers_liberar(void *vetor) { free(vetor); }

PersStatus pers_restaurar_backup(const char *caminho) {
  char tmp[PERS_CAMINHO_MAX], bak[PERS_CAMINHO_MAX];
  int r;

  if (caminho == NULL)
    return PERS_ERRO_PARAMETRO;
  if (!montar_caminho(tmp, caminho, ".tmp") ||
      !montar_caminho(bak, caminho, ".bak"))
    return PERS_ERRO_PARAMETRO;

  r = copiar_arquivo(bak, tmp);
  if (r == 1)
    return PERS_ERRO_SEM_BACKUP;
  if (r != 0) {
    remove(tmp);
    return PERS_ERRO_ESCRITA;
  }
  if (substituir_arquivo(tmp, caminho) != 0) {
    remove(tmp);
    return PERS_ERRO_ESCRITA;
  }
  return PERS_OK;
}

const char *pers_mensagem(PersStatus status) {
  switch (status) {
  case PERS_OK:
    return "Operacao concluida com sucesso.";
  case PERS_ERRO_PARAMETRO:
    return "Parametro invalido.";
  case PERS_ERRO_ABRIR:
    return "Nao foi possivel abrir o arquivo de dados.";
  case PERS_ERRO_LEITURA:
    return "Erro ao ler o arquivo de dados.";
  case PERS_ERRO_ESCRITA:
    return "Erro ao gravar o arquivo de dados. Os dados anteriores foram "
           "preservados.";
  case PERS_ERRO_FORMATO:
    return "O arquivo de dados esta corrompido ou em formato desconhecido.";
  case PERS_ERRO_MEMORIA:
    return "Memoria insuficiente.";
  case PERS_ERRO_DADO_INVALIDO:
    return "Ha registros invalidos; nada foi gravado.";
  case PERS_ERRO_SEM_BACKUP:
    return "Nao existe copia de seguranca para restaurar.";
  }
  return "Erro desconhecido.";
}
