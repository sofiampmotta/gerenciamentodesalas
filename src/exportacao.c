#include "../include/exportacao.h"

#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <string.h>

#define PAGINA_LARGURA 595.28
#define PAGINA_ALTURA 841.89
#define MARGEM 42.0
#define FIM_PAGINA (PAGINA_ALTURA - MARGEM)

static int data_valida(int codigo) {
  static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int ano = codigo / 10000;
  int mes = codigo / 100 % 100;
  int dia = codigo % 100;
  int bissexto = (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0;
  if (ano < 1900 || ano > 9999 || mes < 1 || mes > 12 || dia < 1)
    return 0;
  return dia <= dias[mes - 1] + (mes == 2 && bissexto);
}

static int textos_validos(const ReservaExportacao *reservas,
                          size_t quantidade) {
  if (quantidade > 0 && reservas == NULL)
    return 0;
  for (size_t i = 0; i < quantidade; i++) {
    const ReservaExportacao *r = &reservas[i];
    if (memchr(r->atividade, '\0', sizeof r->atividade) == NULL ||
        memchr(r->tipo, '\0', sizeof r->tipo) == NULL ||
        memchr(r->responsavel, '\0', sizeof r->responsavel) == NULL ||
        memchr(r->sala, '\0', sizeof r->sala) == NULL || r->inicio < 0 ||
        r->fim > 1439 || r->inicio >= r->fim)
      return 0;
  }
  return 1;
}

static void formatar_data(int codigo, char saida[11]) {
  int dia = codigo % 100;
  int mes = codigo / 100 % 100;
  int ano = codigo / 10000;
  saida[0] = (char)('0' + dia / 10);
  saida[1] = (char)('0' + dia % 10);
  saida[2] = '/';
  saida[3] = (char)('0' + mes / 10);
  saida[4] = (char)('0' + mes % 10);
  saida[5] = '/';
  saida[6] = (char)('0' + ano / 1000);
  saida[7] = (char)('0' + ano / 100 % 10);
  saida[8] = (char)('0' + ano / 10 % 10);
  saida[9] = (char)('0' + ano % 10);
  saida[10] = '\0';
}

static void formatar_hora(int minutos, char saida[6]) {
  int hora = minutos / 60;
  int minuto = minutos % 60;
  saida[0] = (char)('0' + hora / 10);
  saida[1] = (char)('0' + hora % 10);
  saida[2] = ':';
  saida[3] = (char)('0' + minuto / 10);
  saida[4] = (char)('0' + minuto % 10);
  saida[5] = '\0';
}

static int escrever_campo_csv(FILE *arquivo, const char *texto, int *primeiro) {
  int aspas = strpbrk(texto, ";\"\r\n") != NULL;
  if (!*primeiro && fputc(';', arquivo) == EOF)
    return 0;
  *primeiro = 0;
  if (aspas && fputc('"', arquivo) == EOF)
    return 0;
  for (const char *p = texto; *p; p++) {
    if (*p == '"' && fputc('"', arquivo) == EOF)
      return 0;
    if (fputc(*p, arquivo) == EOF)
      return 0;
  }
  return !aspas || fputc('"', arquivo) != EOF;
}

int exportar_csv(const char *caminho, int dia, const char *filtro_atividade,
                 const char *filtro_sala, const ReservaExportacao *reservas,
                 size_t quantidade) {
  if (!caminho || !*caminho || !data_valida(dia) ||
      !textos_validos(reservas, quantidade))
    return 0;

  FILE *arquivo = fopen(caminho, "wb");
  if (!arquivo)
    return 0;

  const char *cabecalho[] = {
      "Data exibida", "Filtro de atividade", "Filtro de sala", "Início",
      "Fim",          "Atividade",           "Tipo",           "Responsável",
      "Sala",         "Recorrente"};
  int ok = 1;
  for (size_t i = 0; i < sizeof cabecalho / sizeof cabecalho[0]; i++) {
    int primeiro = i == 0;
    if (!escrever_campo_csv(arquivo, cabecalho[i], &primeiro)) {
      ok = 0;
      break;
    }
  }
  if (ok && fputc('\n', arquivo) == EOF)
    ok = 0;

  char data[11];
  formatar_data(dia, data);
  for (size_t i = 0; ok && i < quantidade; i++) {
    const ReservaExportacao *r = &reservas[i];
    char inicio[6], fim[6];
    formatar_hora(r->inicio, inicio);
    formatar_hora(r->fim, fim);
    const char *campos[] = {data,
                            filtro_atividade ? filtro_atividade : "Todas",
                            filtro_sala ? filtro_sala : "Todas",
                            inicio,
                            fim,
                            r->atividade,
                            r->tipo,
                            r->responsavel,
                            r->sala,
                            r->recorrente ? "Sim" : "Não"};
    for (size_t j = 0; ok && j < sizeof campos / sizeof campos[0]; j++) {
      int primeiro = j == 0;
      ok = escrever_campo_csv(arquivo, campos[j], &primeiro);
    }
    if (ok && fputc('\n', arquivo) == EOF)
      ok = 0;
  }

  if (ferror(arquivo))
    ok = 0;
  if (fclose(arquivo) != 0)
    ok = 0;
  return ok;
}

typedef struct {
  cairo_t *contexto;
  PangoLayout *layout;
  PangoFontDescription *fonte;
  double y;
  int pagina_aberta;
  int codigo_dia;
  const char *filtro_atividade;
  const char *filtro_sala;
} EscritorPDF;

static void desenhar_texto(EscritorPDF *pdf, const char *texto, double x,
                           double largura, double tamanho, int negrito) {
  pango_font_description_set_family(pdf->fonte, "Sans");
  pango_font_description_set_size(pdf->fonte, (int)(tamanho * PANGO_SCALE));
  pango_font_description_set_weight(pdf->fonte, negrito ? PANGO_WEIGHT_BOLD
                                                        : PANGO_WEIGHT_NORMAL);
  pango_layout_set_font_description(pdf->layout, pdf->fonte);
  pango_layout_set_width(pdf->layout, (int)(largura * PANGO_SCALE));
  pango_layout_set_wrap(pdf->layout, PANGO_WRAP_WORD_CHAR);
  pango_layout_set_text(pdf->layout, texto, -1);
  pango_cairo_update_layout(pdf->contexto, pdf->layout);
  int altura = 0;
  pango_layout_get_pixel_size(pdf->layout, NULL, &altura);

  cairo_move_to(pdf->contexto, x, pdf->y);
  pango_cairo_show_layout(pdf->contexto, pdf->layout);
  pdf->y += altura + 7.0;
}

static void iniciar_pagina(EscritorPDF *pdf) {
  if (pdf->pagina_aberta)
    cairo_show_page(pdf->contexto);
  pdf->pagina_aberta = 1;
  pdf->y = MARGEM;

  char data[11], filtros[256];
  formatar_data(pdf->codigo_dia, data);
  snprintf(filtros, sizeof filtros, "Atividade: %s | Sala: %s",
           pdf->filtro_atividade && *pdf->filtro_atividade
               ? pdf->filtro_atividade
               : "Todas",
           pdf->filtro_sala && *pdf->filtro_sala ? pdf->filtro_sala : "Todas");
  desenhar_texto(pdf, "Quadro de horários", MARGEM, PAGINA_LARGURA - 2 * MARGEM,
                 16, 1);
  char titulo_data[64];
  snprintf(titulo_data, sizeof titulo_data, "Data: %s", data);
  desenhar_texto(pdf, titulo_data, MARGEM, PAGINA_LARGURA - 2 * MARGEM, 10, 0);
  desenhar_texto(pdf, filtros, MARGEM, PAGINA_LARGURA - 2 * MARGEM, 10, 0);
  pdf->y += 8.0;
}

int exportar_pdf(const char *caminho, int dia, const char *filtro_atividade,
                 const char *filtro_sala, const ReservaExportacao *reservas,
                 size_t quantidade) {
  if (!caminho || !*caminho || !data_valida(dia) ||
      !textos_validos(reservas, quantidade))
    return 0;

  cairo_surface_t *superficie =
      cairo_pdf_surface_create(caminho, PAGINA_LARGURA, PAGINA_ALTURA);
  if (cairo_surface_status(superficie) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(superficie);
    return 0;
  }

  cairo_t *contexto = cairo_create(superficie);
  PangoLayout *layout = pango_cairo_create_layout(contexto);
  PangoFontDescription *fonte = pango_font_description_new();
  EscritorPDF pdf = {contexto, layout,           fonte,      MARGEM, 0,
                     dia,      filtro_atividade, filtro_sala};
  iniciar_pagina(&pdf);

  if (quantidade == 0) {
    desenhar_texto(&pdf, "Nenhuma ocupação para esses filtros.", MARGEM,
                   PAGINA_LARGURA - 2 * MARGEM, 10, 0);
  }

  for (size_t i = 0; i < quantidade; i++) {
    char inicio[6], fim[6], linha[512];
    formatar_hora(reservas[i].inicio, inicio);
    formatar_hora(reservas[i].fim, fim);
    snprintf(linha, sizeof linha, "%s–%s | %s (%s) | %s | Sala %s%s", inicio,
             fim, reservas[i].tipo, reservas[i].atividade,
             reservas[i].responsavel, reservas[i].sala,
             reservas[i].recorrente ? " | Recorrente" : "");

    pango_layout_set_font_description(layout, fonte);
    pango_layout_set_width(layout,
                           (int)((PAGINA_LARGURA - 2 * MARGEM) * PANGO_SCALE));
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_text(layout, linha, -1);
    pango_cairo_update_layout(contexto, layout);
    int altura = 0;
    pango_layout_get_pixel_size(layout, NULL, &altura);
    if (pdf.y + altura + 7.0 > FIM_PAGINA)
      iniciar_pagina(&pdf);
    desenhar_texto(&pdf, linha, MARGEM, PAGINA_LARGURA - 2 * MARGEM, 10, 0);
  }

  int ok = cairo_status(contexto) == CAIRO_STATUS_SUCCESS;
  pango_font_description_free(fonte);
  g_object_unref(layout);
  cairo_destroy(contexto);
  cairo_surface_finish(superficie);
  ok = ok && cairo_surface_status(superficie) == CAIRO_STATUS_SUCCESS;
  cairo_surface_destroy(superficie);
  return ok;
}
