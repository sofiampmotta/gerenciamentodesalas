/* test_leitor_pdf.c - testes da importacao do relatorio Turma/Horario.
 *
 * Usa um arquivo .txt no mesmo formato que o pdftotext gera a partir do PDF,
 * entao roda mesmo em computadores sem o pdftotext instalado.
 *
 * Rodar: make test   (a partir da raiz do projeto)
 */

#include <stdio.h>
#include <string.h>

#include "../include/leitor_pdf.h"

static int g_total = 0, g_falhas = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    g_total++;                                                                 \
    if (!(cond)) {                                                             \
      g_falhas++;                                                              \
      printf("    FALHOU (linha %d): %s\n", __LINE__, #cond);                  \
    }                                                                          \
  } while (0)

#define ARQ_SAL "leitor_salas.csv"
#define ARQ_RES "leitor_reservas.csv"
#define ARQ_TXT "leitor_relatorio.txt"

static void limpar(void) {
  remove(ARQ_SAL);
  remove(ARQ_SAL ".bak");
  remove(ARQ_RES);
  remove(ARQ_RES ".bak");
  remove(ARQ_TXT);
}

static void escrever_texto(const char *caminho, const char *texto) {
  FILE *f = fopen(caminho, "wb");
  if (f) {
    fputs(texto, f);
    fclose(f);
  }
}

/* Confere se o texto e UTF-8 valido (nenhuma letra cortada ao meio). */
static int utf8_valido(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int extra = 0;
    if (*p < 0x80)
      extra = 0;
    else if ((*p & 0xE0) == 0xC0)
      extra = 1;
    else if ((*p & 0xF0) == 0xE0)
      extra = 2;
    else if ((*p & 0xF8) == 0xF0)
      extra = 3;
    else
      return 0;
    p++;
    for (int i = 0; i < extra; i++, p++)
      if ((*p & 0xC0) != 0x80)
        return 0;
  }
  return 1;
}

static int iniciar_com_salas(Aplicacao *app) {
  const char *nomes[] = {"I-03", "I-05"};
  limpar();
  memset(app, 0, sizeof *app);
  if (!aplicacao_iniciar(app, ARQ_SAL, ARQ_RES))
    return 0;
  for (int i = 0; i < 2; i++) {
    Sala s;
    memset(&s, 0, sizeof s);
    strcpy(s.nome, nomes[i]);
    s.capacidade = 40;
    if (!aplicacao_salvar_sala(app, &s))
      return 0;
  }
  return 1;
}

/* Relatorio com:
 *   TC  -> I-03 seg 07:10-08:50 (dois intervalos seguidos viram um bloco)
 *   CM  -> sala X-99, que nao existe
 *   QM  -> I-03 seg 08:00-08:50, em conflito com a TC
 *   DZ  -> nome bem comprido, com acentos
 *   EE  -> I-05 qui, dois horarios separados (viram duas reservas)
 */
static const char *RELATORIO =
    "FUNDAÇÃO UNIVERSIDADE DE PERNAMBUCO\n"
    "Relatório Turma/Horário\n"
    "TURMA VAGAS COMPONENTE CURRICULAR\n"
    "TC 40 DEB0001 - CÁLCULO DIF. INT. EM UMA VARIÁVEL\n"
    "PROFESSOR CH\n"
    "LENIN ALEXANDRE DE ALMEIDA BEZERRA 60\n"
    "DIA DA SEMANA SALA HORÁRIOS\n"
    "SEG I-03 07:10-08:00 08:00-08:50\n"
    "TURMA VAGAS COMPONENTE CURRICULAR\n"
    "CM 30 DEB0002 - GEOMETRIA ANALÍTICA\n"
    "PROFESSOR CH\n"
    "DEIVSON CESAR SILVA SALES 60\n"
    "DIA DA SEMANA SALA HORÁRIOS\n"
    "TER X-99 07:10-08:00 08:00-08:50\n"
    "TURMA VAGAS COMPONENTE CURRICULAR\n"
    "QM 30 DEC0003 - MECÂNICA DOS SOLOS 2\n"
    "PROFESSOR CH\n"
    "STELA FUCALE 60\n"
    "DIA DA SEMANA SALA HORÁRIOS\n"
    "SEG I-03 08:00-08:50\n"
    "TURMA VAGAS COMPONENTE CURRICULAR\n"
    "DZ 30 DEC0004 - MECÂNICA DOS SOLOS E FUNDAÇÕES APLICADAS À ENGENHARIA\n"
    "PROFESSOR CH\n"
    "ANTONIO CARLOS COSTA 60\n"
    "DIA DA SEMANA SALA HORÁRIOS\n"
    "SEX I-03 13:50-14:40 14:40-15:30\n"
    "TURMA VAGAS COMPONENTE CURRICULAR\n"
    "EE 30 DEE0005 - ELETRÔNICA DIGITAL\n"
    "PROFESSOR CH\n"
    "SERGIO MURILO MACIEL FERNANDES 60\n"
    "DIA DA SEMANA SALA HORÁRIOS\n"
    "QUI I-05 07:10-08:00 10:30-11:20\n";

static void test_importacao_parcial(void) {
  Aplicacao app;
  ResumoImportacao res;
  Data inicio = {3, 8, 2026};
  Data fim = {5, 3, 2027}; /* semestre que termina no ano seguinte */

  CHECK(iniciar_com_salas(&app));
  escrever_texto(ARQ_TXT, RELATORIO);

  CHECK(importarPDF(&app, ARQ_TXT, inicio, fim, &res) == LEITOR_OK);
  /* TC (1) + DZ (1) + EE (2) entram; CM fica sem sala; QM da conflito */
  CHECK(res.importadas == 4);
  CHECK(res.semSala == 1);
  CHECK(res.conflitos == 1);
  CHECK(res.duplicadas == 0);
  CHECK(app.quantidadeReservas == 4);

  /* o bloco da TC juntou 07:10-08:00 e 08:00-08:50 */
  int achou_tc = 0;
  for (size_t i = 0; i < app.quantidadeReservas; i++) {
    const Reserva *r = &app.reservas[i];
    if (strstr(r->descricao, "- TC") != NULL)
      achou_tc = r->horario.comeco == 7 * 60 + 10 &&
                 r->horario.fim == 8 * 60 + 50 && r->dataFim.ano == 2027;
  }
  CHECK(achou_tc);

  /* nome comprido: cortado sem quebrar acento e com a turma no final */
  int achou_dz = 0;
  for (size_t i = 0; i < app.quantidadeReservas; i++) {
    const char *d = app.reservas[i].descricao;
    size_t n = strlen(d);
    if (n >= 4 && strcmp(d + n - 4, "- DZ") == 0)
      achou_dz = utf8_valido(d) && strncmp(d, "MECÂNICA DOS SOLOS", 19) == 0;
  }
  CHECK(achou_dz);

  aplicacao_finalizar(&app);

  /* os dados continuam la depois de "fechar e abrir" */
  memset(&app, 0, sizeof app);
  CHECK(aplicacao_iniciar(&app, ARQ_SAL, ARQ_RES));
  CHECK(app.quantidadeReservas == 4);

  /* importar de novo: nada duplica */
  CHECK(importarPDF(&app, ARQ_TXT, inicio, fim, &res) == LEITOR_OK);
  CHECK(res.importadas == 0 && res.duplicadas == 4);
  CHECK(app.quantidadeReservas == 4);

  aplicacao_finalizar(&app);
  limpar();
}

static void test_formato_desconhecido(void) {
  Aplicacao app;
  ResumoImportacao res;
  Data inicio = {3, 8, 2026}, fim = {31, 12, 2026};

  CHECK(iniciar_com_salas(&app));
  escrever_texto(ARQ_TXT, "isto nao e um relatorio Turma/Horario\n");
  CHECK(importarPDF(&app, ARQ_TXT, inicio, fim, &res) == LEITOR_ERRO);
  CHECK(app.quantidadeReservas == 0);
  aplicacao_finalizar(&app);
  limpar();
}

static void test_pdf_inexistente(void) {
  Aplicacao app;
  ResumoImportacao res;
  Data inicio = {3, 8, 2026}, fim = {31, 12, 2026};

  CHECK(iniciar_com_salas(&app));
  /* PDF passa pelo pdftotext; se ele falhar (arquivo nao existe ou programa
     nao instalado), a resposta e LEITOR_SEM_PDF, e nao "formato errado" */
  CHECK(importarPDF(&app, "arquivo_que_nao_existe.pdf", inicio, fim, &res) ==
        LEITOR_SEM_PDF);
  CHECK(app.quantidadeReservas == 0);
  aplicacao_finalizar(&app);
  limpar();
}

int main(void) {
  printf("Testes do leitor de PDF:\n");
  printf("- test_importacao_parcial\n");
  test_importacao_parcial();
  printf("- test_formato_desconhecido\n");
  test_formato_desconhecido();
  printf("- test_pdf_inexistente\n");
  test_pdf_inexistente();
  printf("\n%d verificacoes, %d falha(s)\n", g_total, g_falhas);
  return g_falhas == 0 ? 0 : 1;
}
