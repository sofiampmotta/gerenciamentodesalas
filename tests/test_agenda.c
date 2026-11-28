/* test_agenda.c - testes da exportacao para agenda (.ics).
 *
 * Rodar: make test   (a partir da raiz do projeto)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/agenda.h"

static int g_total = 0, g_falhas = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    g_total++;                                                                 \
    if (!(cond)) {                                                             \
      g_falhas++;                                                              \
      printf("    FALHOU (linha %d): %s\n", __LINE__, #cond);                  \
    }                                                                          \
  } while (0)

#define ARQ "agenda_teste.ics"

/* Le o arquivo inteiro para uma string (precisa de free). */
static char *ler_arquivo(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  if (f == NULL)
    return NULL;
  fseek(f, 0, SEEK_END);
  long tamanho = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *texto = malloc((size_t)tamanho + 1);
  if (texto != NULL) {
    size_t lidos = fread(texto, 1, (size_t)tamanho, f);
    texto[lidos] = '\0';
  }
  fclose(f);
  return texto;
}

static Sala sala(int id, const char *nome) {
  Sala s;
  memset(&s, 0, sizeof s);
  s.id = id;
  strcpy(s.nome, nome);
  return s;
}

static void test_aula_e_evento(void) {
  Sala salas[2];
  salas[0] = sala(1, "I-03");
  salas[1] = sala(2, "Auditório");

  Reserva r[2];
  memset(r, 0, sizeof r);

  /* aula toda segunda, das 07:10 as 08:50, ate 05/03/2027 */
  r[0].id = 10;
  r[0].idSala = 1;
  r[0].atividade = AULA;
  r[0].isRecorrente = true;
  r[0].data = (Data){16, 11, 2026};
  r[0].dataFim = (Data){5, 3, 2027};
  r[0].horario = (Horario){SEG, 7 * 60 + 10, 8 * 60 + 50};
  strcpy(r[0].descricao, "DEB CÁLCULO 1 - TC");
  strcpy(r[0].responsavel, "LENIN BEZERRA");

  /* evento de um dia so, com virgula e ponto e virgula no texto */
  r[1].id = 11;
  r[1].idSala = 2;
  r[1].atividade = EVENTO;
  r[1].isRecorrente = false;
  r[1].data = (Data){1, 12, 2026};
  r[1].horario = (Horario){TER, 14 * 60, 15 * 60 + 40};
  strcpy(r[1].descricao, "Defesa de TCC; Banca A, B");
  strcpy(r[1].responsavel, "Coordenação");

  /* a aula do dia 30/11 foi cancelada (feriado). O cancelamento do evento
   * e ignorado, porque ele nao se repete. */
  Cancelamento cancelados[2] = {{10, {30, 11, 2026}, "Feriado"},
                                {11, {1, 12, 2026}, ""}};

  remove(ARQ);
  CHECK(exportarAgendaICS(ARQ, r, 2, salas, 2, cancelados, 2) == 1);

  char *ics = ler_arquivo(ARQ);
  CHECK(ics != NULL);
  if (ics == NULL)
    return;

  CHECK(strncmp(ics, "BEGIN:VCALENDAR\r\n", 17) == 0);
  CHECK(strstr(ics, "END:VCALENDAR\r\n") != NULL);

  /* aula: data, horario, repeticao semanal e sala */
  CHECK(strstr(ics, "UID:reserva-10@salas.poli.upe\r\n") != NULL);
  CHECK(strstr(ics, "DTSTART:20261116T071000\r\n") != NULL);
  CHECK(strstr(ics, "DTEND:20261116T085000\r\n") != NULL);
  CHECK(strstr(ics, "RRULE:FREQ=WEEKLY;UNTIL=20270305T235959\r\n") != NULL);
  CHECK(strstr(ics, "SUMMARY:DEB CÁLCULO 1 - TC\r\n") != NULL);
  CHECK(strstr(ics, "LOCATION:I-03\r\n") != NULL);

  /* evento: sem repeticao e com os caracteres especiais escapados */
  CHECK(strstr(ics, "DTSTART:20261201T140000\r\n") != NULL);
  CHECK(strstr(ics, "SUMMARY:Defesa de TCC\\; Banca A\\, B\r\n") != NULL);
  CHECK(strstr(ics, "LOCATION:Auditório\r\n") != NULL);

  /* a semana cancelada sai da repeticao, no mesmo horario da aula */
  char *exdate = strstr(ics, "EXDATE:20261130T071000\r\n");
  CHECK(exdate != NULL);
  CHECK(exdate == NULL || strstr(exdate + 1, "EXDATE") == NULL);

  /* so uma regra de repeticao (a da aula) */
  char *primeira = strstr(ics, "RRULE");
  CHECK(primeira != NULL && strstr(primeira + 1, "RRULE") == NULL);

  free(ics);
  remove(ARQ);
}

static void test_linha_longa(void) {
  Sala salas[1];
  salas[0] = sala(1, "I-03");

  Reserva r;
  memset(&r, 0, sizeof r);
  r.id = 1;
  r.idSala = 1;
  r.atividade = AULA;
  r.isRecorrente = false;
  r.data = (Data){20, 11, 2026};
  r.horario = (Horario){SEX, 8 * 60, 9 * 60};
  strcpy(r.descricao, "DEB CÁLCULO DIF. INT. EM UMA VARIÁVEL - TC");
  /* "DESCRIPTION:Responsável: " + este nome passa de 75 bytes */
  strcpy(r.responsavel,
         "MICAELLA RAISSA FALCÃO DE MOURA, ANDREA BENICIO E VANESSA MARIA");

  remove(ARQ);
  CHECK(exportarAgendaICS(ARQ, &r, 1, salas, 1, NULL, 0) == 1);
  char *ics = ler_arquivo(ARQ);
  CHECK(ics != NULL);
  if (ics == NULL)
    return;

  /* nenhuma linha passa de 75 bytes (sem contar o \r\n) */
  int maior = 0, atual = 0;
  for (char *p = ics; *p; p++) {
    if (*p == '\r' && p[1] == '\n') {
      if (atual > maior)
        maior = atual;
      atual = 0;
      p++;
    } else {
      atual++;
    }
  }
  CHECK(maior <= 75);

  /* a linha quebrada continua na de baixo, comecando com espaco */
  CHECK(strstr(ics, "\r\n ") != NULL);

  free(ics);
  remove(ARQ);
}

static void test_erros(void) {
  CHECK(exportarAgendaICS(NULL, NULL, 0, NULL, 0, NULL, 0) == 0);
  CHECK(exportarAgendaICS("pasta_que_nao_existe/agenda.ics", NULL, 0, NULL,
                          0, NULL, 0) == 0);
  /* lista vazia gera um calendario vazio, mas valido */
  CHECK(exportarAgendaICS(ARQ, NULL, 0, NULL, 0, NULL, 0) == 1);
  remove(ARQ);
}

int main(void) {
  printf("Testes da exportacao para agenda:\n");
  printf("- test_aula_e_evento\n");
  test_aula_e_evento();
  printf("- test_linha_longa\n");
  test_linha_longa();
  printf("- test_erros\n");
  test_erros();
  printf("\n%d verificacoes, %d falha(s)\n", g_total, g_falhas);
  return g_falhas == 0 ? 0 : 1;
}
