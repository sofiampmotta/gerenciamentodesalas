#include "../include/agenda.h"
#include "../include/exportacao.h"
#include "../include/leitor_pdf.h"
#include "interface.h"
#include <stdlib.h>
#include <string.h>

#define COL_HORA 110
#define COL_ATIV 160
#define COL_RESP 150
#define COL_SALA 56
#define FAIXA_INI 420
#define FAIXA_TOT 900

static const char *TIPOS[] = {
    "Todas as atividades", "Aula", "Monitoria", "Extensão", "Evento", NULL};
static const char *DIAS[] = {"",
                             "Segunda-feira",
                             "Terça-feira",
                             "Quarta-feira",
                             "Quinta-feira",
                             "Sexta-feira",
                             "Sábado",
                             "Domingo"};
static const char *MESES[] = {
    "",      "janeiro", "fevereiro", "março",   "abril",    "maio",    "junho",
    "julho", "agosto",  "setembro",  "outubro", "novembro", "dezembro"};

typedef struct {
  Estado *e;
  GtkWidget *data, *tipo, *sala, *popover;
  GtkWidget *titulo_dia, *quadro, *ocupacao;
  GtkWidget *r_uso, *r_ocup, *r_conf, *r_salas;
  GtkWidget *btn_conf_toggle, *revealer_conf, *lista_conf;
  int ult_n_salas;
  int ult_n_reservas;
  int ult_dia;
  char ult_tipo[16];
  char ult_sala[64];
  gboolean renderizado;
} TelaVisao;

typedef struct {
  int dia;
  char tipo[16];
  char sala[64];
} Filtro;
typedef struct {
  char sala[64];
  char tipo[16];
  int dia;
} FaixaOc;

typedef struct {
  TelaVisao *tela;
  ReservaExportacao *reservas;
  size_t quantidade;
  int dia;
  char tipo[16];
  char sala[64];
  gboolean pdf;
} ExportacaoPendente;

static void estilo_visao(void) {
  static gboolean carregado = FALSE;
  if (carregado)
    return;
  carregado = TRUE;
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_string(
      css,
      ".card-visao { border-radius: 10px; padding: 16px 18px; }"
      ".cab-quadro { border-radius: 6px; }"
      ".cab-quadro label { font-weight: 700; font-size: 12px; padding: 7px 0; }"
      ".grupo-quadro { margin-top: 6px; }"
      ".bloco-hora { border-radius: 6px; font-weight: 700; }"
      ".linha-quadro { padding: 9px 0; }"
      ".linha-ocup { padding: 10px 0; }"
      ".resumo-linha { padding: 8px 0; }"
      ".chip { border-radius: 10px; padding: 1px 9px; font-size: 12px; "
      "font-weight: 600; }"
      ".chip-monitoria { background: #d9f2e6; color: #1b6b45; }"
      ".chip-extensao { background: #ece0fa; color: #5b2f91; }"
      ".chip-evento { background: #fbe0e4; color: #a30f2b; }");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);
}

static GtkWidget *rotulo(const char *txt, int largura, gboolean expande,
                         gboolean negrito) {
  GtkWidget *l = gtk_label_new(txt);
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
  gtk_widget_set_margin_start(l, 12);
  gtk_widget_set_margin_end(l, 6);
  if (largura > 0)
    gtk_widget_set_size_request(l, largura, -1);
  gtk_widget_set_hexpand(l, expande);
  if (negrito)
    gtk_widget_add_css_class(l, "negrito");
  return l;
}

static void limpar(GtkWidget *caixa) {
  GtkWidget *f;
  while ((f = gtk_widget_get_first_child(caixa)) != NULL)
    gtk_box_remove(GTK_BOX(caixa), f);
}

static const char *classe_chip(const char *tipo) {
  if (strcmp(tipo, "Monitoria") == 0)
    return "chip-monitoria";
  if (strcmp(tipo, "Extensão") == 0)
    return "chip-extensao";
  if (strcmp(tipo, "Evento") == 0)
    return "chip-evento";
  return NULL;
}

static int passa(const ReservaVista *r, const Filtro *f) {
  if (!dados_reserva_ocorre_em(r, f->dia))
    return 0;
  if (f->tipo[0] && strcmp(r->tipo, f->tipo) != 0)
    return 0;
  if (f->sala[0] && strcmp(r->sala, f->sala) != 0)
    return 0;
  return 1;
}

static int comparar(const void *pa, const void *pb) {
  const ReservaVista *x = &g_reservas[*(const int *)pa];
  const ReservaVista *y = &g_reservas[*(const int *)pb];
  if (x->ini != y->ini)
    return x->ini - y->ini;
  if (x->fim != y->fim)
    return x->fim - y->fim;
  return strcmp(x->sala, y->sala);
}

static GtkWidget *linha_reserva(const ReservaVista *r) {
  GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(linha, "linha-quadro");

  GtkWidget *ativ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_hexpand(ativ, TRUE);
  gtk_widget_set_size_request(ativ, COL_ATIV, -1);
  gtk_box_append(GTK_BOX(ativ), rotulo(r->atividade, 0, FALSE, TRUE));
  const char *classe = classe_chip(r->tipo);
  if (classe) {
    GtkWidget *chip = gtk_label_new(r->tipo);
    gtk_widget_add_css_class(chip, "chip");
    gtk_widget_add_css_class(chip, classe);
    gtk_widget_set_valign(chip, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(ativ), chip);
  }
  gtk_box_append(GTK_BOX(linha), ativ);
  gtk_box_append(GTK_BOX(linha),
                 rotulo(r->responsavel, COL_RESP, FALSE, FALSE));
  gtk_box_append(GTK_BOX(linha), rotulo(r->sala, COL_SALA, FALSE, FALSE));
  return linha;
}

static GtkWidget *cabecalho(void) {
  GtkWidget *c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(c, "cab-quadro");
  GtkWidget *h = gtk_label_new("Horário");
  gtk_widget_set_size_request(h, COL_HORA, -1);
  gtk_widget_set_margin_end(h, 6);
  gtk_box_append(GTK_BOX(c), h);
  gtk_box_append(GTK_BOX(c), rotulo("Atividade", 0, TRUE, FALSE));
  gtk_box_append(GTK_BOX(c),
                 rotulo("Professor(a) / Responsável", COL_RESP, FALSE, FALSE));
  gtk_box_append(GTK_BOX(c), rotulo("Sala", COL_SALA, FALSE, FALSE));
  return c;
}

static void montar_quadro(TelaVisao *t, const Filtro *f) {
  limpar(t->quadro);
  int *idx = g_new(int, g_n_reservas);
  int n = 0;
  for (int i = 0; i < g_n_reservas; i++)
    if (passa(&g_reservas[i], f))
      idx[n++] = i;
  qsort(idx, n, sizeof(int), comparar);

  if (n == 0) {
    GtkWidget *l = gtk_label_new("Nenhuma ocupação para esses filtros.");
    gtk_widget_add_css_class(l, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_widget_set_margin_top(l, 12);
    gtk_box_append(GTK_BOX(t->quadro), l);
    g_free(idx);
    return;
  }

  int k = 0;
  while (k < n) {
    const ReservaVista *p = &g_reservas[idx[k]];
    int j = k;
    while (j < n && g_reservas[idx[j]].ini == p->ini &&
           g_reservas[idx[j]].fim == p->fim)
      j++;

    char a[8], b[8], h[24];
    dados_hora_texto(p->ini, a, sizeof a);
    dados_hora_texto(p->fim, b, sizeof b);
    g_snprintf(h, sizeof h, "%s – %s", a, b);

    GtkWidget *grupo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(grupo, "grupo-quadro");
    GtkWidget *hora = gtk_label_new(h);
    gtk_widget_add_css_class(hora, "bloco-hora");
    gtk_widget_set_size_request(hora, COL_HORA, -1);
    GtkWidget *lado = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(lado, TRUE);
    for (int m = k; m < j; m++)
      gtk_box_append(GTK_BOX(lado), linha_reserva(&g_reservas[idx[m]]));
    gtk_box_append(GTK_BOX(grupo), hora);
    gtk_box_append(GTK_BOX(grupo), lado);
    gtk_box_append(GTK_BOX(t->quadro), grupo);
    k = j;
  }
  g_free(idx);
}

static double px(int min, int w) {
  double x = (min - FAIXA_INI) * (double)w / FAIXA_TOT;
  if (x < 0)
    x = 0;
  if (x > w)
    x = w;
  return x;
}

static void desenhar_oc(GtkDrawingArea *area, cairo_t *cr, int w, int h,
                        gpointer dados) {
  const FaixaOc *f = (const FaixaOc *)dados;
  int barra = h - 14;

  cairo_set_source_rgb(cr, 0.84, 0.88, 0.94);
  cairo_rectangle(cr, 0, 0, w, barra);
  cairo_fill(cr);

  cairo_set_source_rgb(cr, 0.62, 0.11, 0.14);
  for (int i = 0; i < g_n_reservas; i++) {
    const ReservaVista *o = &g_reservas[i];
    if (!dados_reserva_ocorre_em(o, f->dia) || strcmp(o->sala, f->sala) != 0)
      continue;
    if (f->tipo[0] && strcmp(o->tipo, f->tipo) != 0)
      continue;
    cairo_rectangle(cr, px(o->ini, w), 0, px(o->fim, w) - px(o->ini, w) - 1,
                    barra);
    cairo_fill(cr);
  }

  cairo_set_source_rgb(cr, 0.36, 0.44, 0.56);
  cairo_select_font_face(cr, "Segoe UI", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 13);
  int marcas[4] = {8, 12, 16, 20};
  for (int i = 0; i < 4; i++) {
    char txt[8];
    g_snprintf(txt, sizeof txt, "%dh", marcas[i]);
    cairo_move_to(cr, px(marcas[i] * 60, w), h - 2);
    cairo_show_text(cr, txt);
  }
}

static void montar_ocupacao(TelaVisao *t, const Filtro *f) {
  limpar(t->ocupacao);
  for (int s = 0; s < g_n_salas; s++) {
    const SalaVista *sl = &g_salas[s];
    if (f->sala[0] && strcmp(sl->nome, f->sala) != 0)
      continue;

    int n = 0;
    for (int i = 0; i < g_n_reservas; i++) {
      const ReservaVista *r = &g_reservas[i];
      if (!dados_reserva_ocorre_em(r, f->dia) || strcmp(r->sala, sl->nome) != 0)
        continue;
      if (f->tipo[0] && strcmp(r->tipo, f->tipo) != 0)
        continue;
      n++;
    }

    GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(linha, "linha-ocup");

    GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_size_request(info, 150, -1);
    gtk_box_append(GTK_BOX(info), rotulo(sl->nome, 0, FALSE, TRUE));
    char lug[24];
    if (sl->lugares > 0)
      g_snprintf(lug, sizeof lug, "%d lugares", sl->lugares);
    else
      g_strlcpy(lug, "— lugares", sizeof lug); // capacidade não informada
    GtkWidget *l2 = rotulo(lug, 0, FALSE, FALSE);
    gtk_widget_add_css_class(l2, "subtitulo");
    gtk_box_append(GTK_BOX(info), l2);

    FaixaOc *fo = g_new0(FaixaOc, 1);
    g_strlcpy(fo->sala, sl->nome, sizeof fo->sala);
    g_strlcpy(fo->tipo, f->tipo, sizeof fo->tipo);
    fo->dia = f->dia;
    GtkWidget *faixa = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(faixa), 400);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(faixa), 34);
    gtk_widget_set_hexpand(faixa, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(faixa), desenhar_oc, fo,
                                   g_free);

    char txt[32];
    if (n == 1)
      g_strlcpy(txt, "1 ocupação", sizeof txt);
    else
      g_snprintf(txt, sizeof txt, "%d ocupações", n);
    GtkWidget *qtd = gtk_label_new(txt);
    gtk_label_set_xalign(GTK_LABEL(qtd), 1);
    gtk_widget_set_size_request(qtd, 110, -1);
    gtk_widget_add_css_class(qtd, "subtitulo");

    gtk_box_append(GTK_BOX(linha), info);
    gtk_box_append(GTK_BOX(linha), faixa);
    gtk_box_append(GTK_BOX(linha), qtd);
    gtk_box_append(GTK_BOX(t->ocupacao), linha);
  }
}

static void definir(GtkWidget *l, int n, const char *resto) {
  char *m = g_markup_printf_escaped("<b>%d</b> %s", n, resto);
  gtk_label_set_markup(GTK_LABEL(l), m);
  g_free(m);
}

static void ao_toggle_conf(GtkToggleButton *btn, gpointer dados) {
  TelaVisao *t = (TelaVisao *)dados;
  gboolean ativo = gtk_toggle_button_get_active(btn);
  gtk_button_set_label(GTK_BUTTON(btn), ativo ? "▾" : "▸");
  if (t->revealer_conf)
    gtk_revealer_set_reveal_child(GTK_REVEALER(t->revealer_conf), ativo);
}

static void resumo(TelaVisao *t, int dia) {
  int ocup = 0, conflitos = 0, uso = 0;
  if (t->lista_conf)
    limpar(t->lista_conf);
  GDateTime *agora = g_date_time_new_now_local();
  int hoje = g_date_time_get_year(agora) * 10000 +
             g_date_time_get_month(agora) * 100 +
             g_date_time_get_day_of_month(agora);
  int minuto_atual =
      g_date_time_get_hour(agora) * 60 + g_date_time_get_minute(agora);
  g_date_time_unref(agora);
  for (int i = 0; i < g_n_reservas; i++) {
    if (!dados_reserva_ocorre_em(&g_reservas[i], dia))
      continue;
    ocup++;
    for (int j = i + 1; j < g_n_reservas; j++) {
      if (!dados_reserva_ocorre_em(&g_reservas[j], dia) ||
          strcmp(g_reservas[i].sala, g_reservas[j].sala) != 0)
        continue;
      if (g_reservas[i].ini < g_reservas[j].fim &&
          g_reservas[j].ini < g_reservas[i].fim) {
        conflitos++;
        if (t->lista_conf) {
          int c_ini = g_reservas[i].ini > g_reservas[j].ini ? g_reservas[i].ini
                                                            : g_reservas[j].ini;
          int c_fim = g_reservas[i].fim < g_reservas[j].fim ? g_reservas[i].fim
                                                            : g_reservas[j].fim;
          char h1[16], h2[16], c1[16], c2[16];
          dados_hora_texto(c_ini, c1, sizeof c1);
          dados_hora_texto(c_fim, c2, sizeof c2);
          dados_hora_texto(g_reservas[i].ini, h1, sizeof h1);
          dados_hora_texto(g_reservas[j].ini, h2, sizeof h2);

          char *det = g_markup_printf_escaped(
              "<b>%s</b> (%s–%s)\n• %s (%s)\n• %s (%s)", g_reservas[i].sala, c1,
              c2, g_reservas[i].atividade, h1, g_reservas[j].atividade, h2);
          GtkWidget *item = gtk_label_new(NULL);
          gtk_label_set_markup(GTK_LABEL(item), det);
          gtk_label_set_xalign(GTK_LABEL(item), 0);
          gtk_label_set_wrap(GTK_LABEL(item), TRUE);
          gtk_widget_set_margin_top(item, 4);
          gtk_widget_set_margin_bottom(item, 4);
          gtk_box_append(GTK_BOX(t->lista_conf), item);
          g_free(det);
        }
      }
    }
  }
  for (int s = 0; s < g_n_salas; s++) {
    int em_uso = 0, no_dia = 0;
    for (int i = 0; i < g_n_reservas; i++) {
      const ReservaVista *r = &g_reservas[i];
      if (!dados_reserva_ocorre_em(r, dia) ||
          strcmp(r->sala, g_salas[s].nome) != 0)
        continue;
      no_dia = 1;
      if (r->ini <= minuto_atual && minuto_atual < r->fim)
        em_uso = 1;
    }
    if (dia == hoje ? em_uso : no_dia)
      uso++;
  }
  definir(t->r_uso, uso,
          dia == hoje ? "salas em uso agora" : "salas com ocupação no dia");
  definir(t->r_ocup, ocup, ocup == 1 ? "ocupação no dia" : "ocupações no dia");
  definir(t->r_conf, conflitos, conflitos == 1 ? "conflito" : "conflitos");
  if (t->btn_conf_toggle) {
    gtk_widget_set_sensitive(t->btn_conf_toggle, conflitos > 0);
    if (conflitos == 0 &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(t->btn_conf_toggle))) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(t->btn_conf_toggle),
                                   FALSE);
    }
  }
  if (t->lista_conf && conflitos == 0) {
    GtkWidget *vazio = gtk_label_new("Nenhum conflito neste dia.");
    gtk_label_set_xalign(GTK_LABEL(vazio), 0);
    gtk_widget_add_css_class(vazio, "subtitulo");
    gtk_box_append(GTK_BOX(t->lista_conf), vazio);
  }
  definir(t->r_salas, g_n_salas, "salas cadastradas");
}

static void ler_filtro(TelaVisao *t, Filtro *f) {
  f->dia = seletor_data_get(t->data);

  guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->tipo));
  f->tipo[0] = '\0';
  if (i >= 1 && i <= 4)
    g_strlcpy(f->tipo, TIPOS[i], sizeof f->tipo);

  guint s = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->sala));
  f->sala[0] = '\0';
  if (s != GTK_INVALID_LIST_POSITION && s > 0) {
    GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
    if (it)
      g_strlcpy(f->sala, gtk_string_object_get_string(GTK_STRING_OBJECT(it)),
                sizeof f->sala);
  }
}

static void registrar_filtro_renderizado(TelaVisao *t) {
  Filtro f;
  ler_filtro(t, &f);
  t->ult_n_salas = g_n_salas;
  t->ult_n_reservas = g_n_reservas;
  t->ult_dia = f.dia;
  g_strlcpy(t->ult_tipo, f.tipo, sizeof t->ult_tipo);
  g_strlcpy(t->ult_sala, f.sala, sizeof t->ult_sala);
  t->renderizado = TRUE;
}

static void filtro_renderizado(TelaVisao *t, Filtro *f) {
  if (!t->renderizado) {
    ler_filtro(t, f);
    return;
  }
  f->dia = t->ult_dia;
  g_strlcpy(f->tipo, t->ult_tipo, sizeof f->tipo);
  g_strlcpy(f->sala, t->ult_sala, sizeof f->sala);
}

static void atualizar(TelaVisao *t) {
  Filtro f;
  ler_filtro(t, &f);
  if (!dados_data_valida(f.dia))
    return;

  GDate *g = g_date_new_dmy(f.dia % 100, (f.dia / 100) % 100, f.dia / 10000);
  int wd = g_date_get_weekday(g);
  g_date_free(g);
  char *tit = g_strdup_printf("%s — %d de %s de %d", DIAS[wd], f.dia % 100,
                              MESES[(f.dia / 100) % 100], f.dia / 10000);
  gtk_label_set_text(GTK_LABEL(t->titulo_dia), tit);
  g_free(tit);

  montar_quadro(t, &f);
  montar_ocupacao(t, &f);
  resumo(t, f.dia);
}

static void recarregar_salas(TelaVisao *t) {
  if (t->renderizado && t->ult_n_salas == g_n_salas)
    return;
  t->ult_n_salas = g_n_salas;
  char antes[64] = "";
  guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->sala));
  if (sel != GTK_INVALID_LIST_POSITION && sel > 0) {
    GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
    if (it)
      g_strlcpy(antes, gtk_string_object_get_string(GTK_STRING_OBJECT(it)),
                sizeof antes);
  }

  const char **nomes = g_new(const char *, g_n_salas + 2);
  nomes[0] = "Todos os ambientes";
  for (int i = 0; i < g_n_salas; i++)
    nomes[i + 1] = g_salas[i].nome;
  nomes[g_n_salas + 1] = NULL;

  GtkStringList *sl =
      GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(t->sala)));
  gtk_string_list_splice(sl, 0, g_list_model_get_n_items(G_LIST_MODEL(sl)),
                         nomes);
  g_free(nomes);

  guint pos = 0;
  for (int i = 0; i < g_n_salas; i++)
    if (antes[0] && strcmp(g_salas[i].nome, antes) == 0)
      pos = i + 1;
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->sala), pos);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) {
  (void)w;
  TelaVisao *t = (TelaVisao *)dados;
  Filtro f;
  ler_filtro(t, &f);
  if (t->renderizado && t->ult_n_salas == g_n_salas &&
      t->ult_n_reservas == g_n_reservas && t->ult_dia == f.dia &&
      strcmp(t->ult_tipo, f.tipo) == 0 && strcmp(t->ult_sala, f.sala) == 0)
    return;
  recarregar_salas(t);
  atualizar(t);
  registrar_filtro_renderizado(t);
}

static int comparar_exportacao(const void *pa, const void *pb) {
  const ReservaExportacao *a = pa;
  const ReservaExportacao *b = pb;
  if (a->inicio != b->inicio)
    return a->inicio - b->inicio;
  if (a->fim != b->fim)
    return a->fim - b->fim;
  return strcmp(a->sala, b->sala);
}

static char *caminho_com_extensao(const char *caminho, const char *extensao) {
  size_t tamanho = strlen(caminho);
  size_t tamanhoExtensao = strlen(extensao);
  if (tamanho >= tamanhoExtensao &&
      g_ascii_strcasecmp(caminho + tamanho - tamanhoExtensao, extensao) == 0)
    return g_strdup(caminho);

  char *diretorio = g_path_get_dirname(caminho);
  char *base = g_path_get_basename(caminho);
  char *ponto = strrchr(base, '.');
  if (ponto)
    *ponto = '\0';
  char *novoNome = g_strconcat(base, extensao, NULL);
  char *resultado = g_build_filename(diretorio, novoNome, NULL);
  g_free(diretorio);
  g_free(base);
  g_free(novoNome);
  return resultado;
}

static void liberar_exportacao(ExportacaoPendente *pendente) {
  g_free(pendente->reservas);
  g_free(pendente);
}

static void ao_salvar_exportacao(GtkNativeDialog *dialog, int resposta,
                                 gpointer dados) {
  ExportacaoPendente *pendente = dados;
  if (resposta == GTK_RESPONSE_ACCEPT) {
    GFile *arquivo = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
    char *caminho = arquivo ? g_file_get_path(arquivo) : NULL;
    if (!caminho) {
      interface_mensagem(pendente->tela->e,
                         "Não foi possível obter o caminho do arquivo.", TRUE);
    } else {
      const char *extensao = pendente->pdf ? ".pdf" : ".csv";
      char *destino = caminho_com_extensao(caminho, extensao);
      int sucesso =
          pendente->pdf
              ? exportar_pdf(destino, pendente->dia,
                             pendente->tipo[0] ? pendente->tipo : NULL,
                             pendente->sala[0] ? pendente->sala : NULL,
                             pendente->reservas, pendente->quantidade)
              : exportar_csv(destino, pendente->dia,
                             pendente->tipo[0] ? pendente->tipo : NULL,
                             pendente->sala[0] ? pendente->sala : NULL,
                             pendente->reservas, pendente->quantidade);
      if (sucesso) {
        char *mensagem = g_strdup_printf("Arquivo exportado: %s", destino);
        interface_mensagem(pendente->tela->e, mensagem, FALSE);
        g_free(mensagem);
      } else {
        char *mensagem =
            g_strdup_printf("Não foi possível exportar o arquivo: %s", destino);
        interface_mensagem(pendente->tela->e, mensagem, TRUE);
        g_free(mensagem);
      }
      g_free(destino);
      g_free(caminho);
    }
    if (arquivo)
      g_object_unref(arquivo);
  }

  gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(dialog));
  g_object_unref(dialog);
  liberar_exportacao(pendente);
}

static void exportar(TelaVisao *t, gboolean pdf) {
  Filtro f;
  filtro_renderizado(t, &f);
  if (!dados_data_valida(f.dia)) {
    interface_mensagem(t->e, "A data exibida não é válida.", TRUE);
    return;
  }

  ExportacaoPendente *pendente = g_new0(ExportacaoPendente, 1);
  pendente->tela = t;
  pendente->dia = f.dia;
  pendente->pdf = pdf;
  g_strlcpy(pendente->tipo, f.tipo, sizeof pendente->tipo);
  g_strlcpy(pendente->sala, f.sala, sizeof pendente->sala);
  pendente->reservas = g_new0(ReservaExportacao, g_n_reservas);

  for (int i = 0; i < g_n_reservas; i++) {
    const ReservaVista *r = &g_reservas[i];
    if (!passa(r, &f))
      continue;
    ReservaExportacao *saida = &pendente->reservas[pendente->quantidade++];
    g_strlcpy(saida->atividade, r->atividade, sizeof saida->atividade);
    g_strlcpy(saida->tipo, r->tipo, sizeof saida->tipo);
    g_strlcpy(saida->responsavel, r->responsavel, sizeof saida->responsavel);
    g_strlcpy(saida->sala, r->sala, sizeof saida->sala);
    saida->inicio = r->ini;
    saida->fim = r->fim;
    saida->recorrente = r->isRecorrente;
  }
  if (pendente->quantidade > 1)
    qsort(pendente->reservas, pendente->quantidade, sizeof *pendente->reservas,
          comparar_exportacao);

  acao_log("exportar_%s dia:%d tipo:%s sala:%s", pdf ? "pdf" : "csv", f.dia,
           f.tipo[0] ? f.tipo : "(todas)", f.sala[0] ? f.sala : "(todas)");
  if (t->popover)
    gtk_popover_popdown(GTK_POPOVER(t->popover));

  char nome[64];
  g_snprintf(nome, sizeof nome, "quadro-%04d-%02d-%02d.%s", f.dia / 10000,
             f.dia / 100 % 100, f.dia % 100, pdf ? "pdf" : "csv");
  GtkFileChooserNative *seletor = gtk_file_chooser_native_new(
      pdf ? "Exportar quadro para PDF" : "Exportar quadro para CSV",
      (t->e && t->e->janela) ? GTK_WINDOW(t->e->janela) : NULL,
      GTK_FILE_CHOOSER_ACTION_SAVE, "Salvar", "Cancelar");
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(seletor), nome);
  GtkFileFilter *filtro = gtk_file_filter_new();
  gtk_file_filter_set_name(filtro, pdf ? "Arquivos PDF" : "Arquivos CSV");
  gtk_file_filter_add_pattern(filtro, pdf ? "*.pdf" : "*.csv");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(seletor), filtro);
  g_object_unref(filtro);
  g_signal_connect(seletor, "response", G_CALLBACK(ao_salvar_exportacao),
                   pendente);
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(seletor));
}

static void ao_pdf(GtkButton *b, gpointer dados) {
  (void)b;
  exportar((TelaVisao *)dados, TRUE);
}
static void ao_csv(GtkButton *b, gpointer dados) {
  (void)b;
  exportar((TelaVisao *)dados, FALSE);
}

/* ---- exportar agenda (.ics) ----
   Exporta o período inteiro (não só o dia exibido), respeitando os filtros
   de tipo e de sala. Abre no Google Agenda, no Outlook e no celular. */
static void ao_salvar_agenda(GtkNativeDialog *dialog, int resposta,
                             gpointer dados) {
  TelaVisao *t = dados;
  if (resposta == GTK_RESPONSE_ACCEPT) {
    GFile *arquivo = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
    char *caminho = arquivo ? g_file_get_path(arquivo) : NULL;
    Aplicacao *app = aplicacao_padrao();
    if (!caminho || !app) {
      interface_mensagem(t->e, "Não foi possível exportar a agenda.", TRUE);
    } else {
      char *destino = caminho_com_extensao(caminho, ".ics");
      Filtro f;
      filtro_renderizado(t, &f);

      Reserva *lista = g_new0(Reserva, g_n_reservas > 0 ? g_n_reservas : 1);
      size_t quantidade = 0;
      for (int i = 0; i < g_n_reservas; i++) {
        const ReservaVista *rv = &g_reservas[i];
        if (f.tipo[0] && strcmp(rv->tipo, f.tipo) != 0)
          continue;
        if (f.sala[0] && strcmp(rv->sala, f.sala) != 0)
          continue;
        Reserva *r = aplicacao_buscar_reserva(app, rv->idReserva);
        if (r)
          lista[quantidade++] = *r;
      }

      if (exportarAgendaICS(destino, lista, quantidade, app->salas,
                            app->quantidadeSalas)) {
        char *mensagem =
            g_strdup_printf("Agenda exportada com %lu reservas: %s",
                            (unsigned long)quantidade, destino);
        interface_mensagem(t->e, mensagem, FALSE);
        g_free(mensagem);
      } else {
        char *mensagem =
            g_strdup_printf("Não foi possível exportar a agenda: %s", destino);
        interface_mensagem(t->e, mensagem, TRUE);
        g_free(mensagem);
      }
      g_free(lista);
      g_free(destino);
    }
    g_free(caminho);
    if (arquivo)
      g_object_unref(arquivo);
  }

  gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(dialog));
  g_object_unref(dialog);
}

static void ao_agenda(GtkButton *b, gpointer dados) {
  (void)b;
  TelaVisao *t = dados;
  Filtro f;
  filtro_renderizado(t, &f);
  acao_log("exportar_ics tipo:%s sala:%s", f.tipo[0] ? f.tipo : "(todas)",
           f.sala[0] ? f.sala : "(todas)");
  if (t->popover)
    gtk_popover_popdown(GTK_POPOVER(t->popover));

  char nome[96];
  if (f.sala[0])
    g_snprintf(nome, sizeof nome, "agenda-%s.ics", f.sala);
  else
    g_strlcpy(nome, "agenda-salas.ics", sizeof nome);

  GtkFileChooserNative *seletor = gtk_file_chooser_native_new(
      "Exportar agenda", (t->e && t->e->janela) ? GTK_WINDOW(t->e->janela) : NULL,
      GTK_FILE_CHOOSER_ACTION_SAVE, "Salvar", "Cancelar");
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(seletor), nome);
  GtkFileFilter *filtro = gtk_file_filter_new();
  gtk_file_filter_set_name(filtro, "Agenda (.ics)");
  gtk_file_filter_add_pattern(filtro, "*.ics");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(seletor), filtro);
  g_object_unref(filtro);
  g_signal_connect(seletor, "response", G_CALLBACK(ao_salvar_agenda), t);
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(seletor));
}
typedef struct {
  TelaVisao *t;
  char *caminho;
  GtkWidget *inicio;
  GtkWidget *fim;
} ImportacaoPDF;

static void ao_confirmar_importacao(GtkDialog *dialog, int resposta,
                                    gpointer dados) {
  ImportacaoPDF *imp = dados;

  if (resposta == GTK_RESPONSE_ACCEPT) {
    int ini = seletor_data_get(imp->inicio);
    int fim = seletor_data_get(imp->fim);

    if (!dados_data_valida(ini) || !dados_data_valida(fim) || ini > fim) {
      interface_mensagem(imp->t->e, "Período inválido.", TRUE);
      return;
    }

    Data dataInicio = {ini % 100, (ini / 100) % 100, ini / 10000};

    Data dataFim = {fim % 100, (fim / 100) % 100, fim / 10000};

    Aplicacao *app = aplicacao_padrao();

    ResumoImportacao resumo;

    int resultado =
        importarPDF(app, imp->caminho, dataInicio, dataFim, &resumo);

    if (resultado == LEITOR_OK) {
      dados_recarregar();

      imp->t->renderizado = FALSE;
      atualizar(imp->t);
      registrar_filtro_renderizado(imp->t);

      // Ex.: "780 reservas importadas. 14 com conflito e 2 com sala não
      // cadastrada ficaram de fora."
      GString *msg = g_string_new(NULL);
      g_string_append_printf(msg, "%lu reservas importadas.",
                             (unsigned long)resumo.importadas);
      if (resumo.duplicadas > 0)
        g_string_append_printf(msg, " %lu já existiam.",
                               (unsigned long)resumo.duplicadas);
      if (resumo.conflitos > 0 || resumo.semSala > 0) {
        g_string_append(msg, " Ficaram de fora:");
        if (resumo.conflitos > 0)
          g_string_append_printf(msg, " %lu com conflito",
                                 (unsigned long)resumo.conflitos);
        if (resumo.conflitos > 0 && resumo.semSala > 0)
          g_string_append(msg, " e");
        if (resumo.semSala > 0)
          g_string_append_printf(msg, " %lu com sala não cadastrada",
                                 (unsigned long)resumo.semSala);
        g_string_append(msg, ".");
      }

      interface_mensagem(imp->t->e, msg->str, FALSE);
      g_string_free(msg, TRUE);
    } else if (resultado == LEITOR_SEM_PDF) {
      interface_mensagem(imp->t->e,
                         "Não foi possível ler o PDF. Verifique se o "
                         "pdftotext (Poppler) está instalado.",
                         TRUE);
    } else {
      interface_mensagem(imp->t->e,
                         "Falha na importação. O arquivo precisa ser o "
                         "Relatório Turma/Horário da UPE.",
                         TRUE);
    }
  }

  gtk_window_destroy(GTK_WINDOW(dialog));
  g_free(imp->caminho);
  g_free(imp);
}
static void solicitar_periodo_pdf(TelaVisao *t, const char *caminho) {
  ImportacaoPDF *imp = g_new0(ImportacaoPDF, 1);

  imp->t = t;
  imp->caminho = g_strdup(caminho);

  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Período do semestre", GTK_WINDOW(t->e->janela), GTK_DIALOG_MODAL,
      "Cancelar", GTK_RESPONSE_CANCEL, "Importar", GTK_RESPONSE_ACCEPT, NULL);

  GtkWidget *caixa = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  gtk_box_set_spacing(GTK_BOX(caixa), 12);

  gtk_widget_set_margin_top(caixa, 20);
  gtk_widget_set_margin_bottom(caixa, 20);
  gtk_widget_set_margin_start(caixa, 20);
  gtk_widget_set_margin_end(caixa, 20);

  int hoje = dados_data_hoje();
  int ano = hoje / 10000;

  imp->inicio = seletor_data_criar(hoje, NULL, NULL);

  imp->fim = seletor_data_criar(ano * 10000 + 1231, NULL, NULL);

  gtk_box_append(GTK_BOX(caixa), gtk_label_new("Início do semestre"));

  gtk_box_append(GTK_BOX(caixa), imp->inicio);

  gtk_box_append(GTK_BOX(caixa), gtk_label_new("Fim do semestre"));

  gtk_box_append(GTK_BOX(caixa), imp->fim);

  g_signal_connect(dialog, "response", G_CALLBACK(ao_confirmar_importacao),
                   imp);

  gtk_window_present(GTK_WINDOW(dialog));
}

static void ao_selecionar_pdf(GtkNativeDialog *dialog, int resposta,
                              gpointer dados) {
  TelaVisao *t = (TelaVisao *)dados;

  if (resposta == GTK_RESPONSE_ACCEPT) {
    GFile *arquivo = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));

    if (arquivo) {
      char *caminho = g_file_get_path(arquivo);

      if (caminho) {
        acao_log("PDF selecionado: %s", caminho);

        solicitar_periodo_pdf(t, caminho);

        g_free(caminho);
      }

      g_object_unref(arquivo);
    }
  }

  g_object_unref(dialog);
}

static void executar_importacao_pdf(TelaVisao *t) {
  GtkFileChooserNative *seletor = gtk_file_chooser_native_new(
      "Selecionar PDF da grade", GTK_WINDOW(t->e->janela),
      GTK_FILE_CHOOSER_ACTION_OPEN, "Selecionar", "Cancelar");

  GtkFileFilter *filtro = gtk_file_filter_new();

  gtk_file_filter_set_name(filtro, "Arquivos PDF");
  gtk_file_filter_add_pattern(filtro, "*.pdf");

  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(seletor), filtro);

  g_object_unref(filtro);

  g_signal_connect(seletor, "response", G_CALLBACK(ao_selecionar_pdf), t);

  gtk_native_dialog_show(GTK_NATIVE_DIALOG(seletor));
}

#if GTK_CHECK_VERSION(4, 10, 0)
static void ao_resposta_alerta_pdf(GObject *source, GAsyncResult *res,
                                   gpointer dados) {
  GtkAlertDialog *dialog = GTK_ALERT_DIALOG(source);
  TelaVisao *t = (TelaVisao *)dados;
  GError *err = NULL;
  int escolha = gtk_alert_dialog_choose_finish(dialog, res, &err);
  if (err) {
    g_error_free(err);
    return;
  }
  if (escolha == 1) {
    executar_importacao_pdf(t);
  }
}
#else
static void ao_resposta_dialog_pdf(GtkDialog *dialog, int response_id,
                                   gpointer dados) {
  TelaVisao *t = (TelaVisao *)dados;
  gtk_window_destroy(GTK_WINDOW(dialog));
  if (response_id == GTK_RESPONSE_ACCEPT) {
    executar_importacao_pdf(t);
  }
}
#endif

static void ao_importar_pdf(GtkButton *b, gpointer dados) {
  (void)b;
  TelaVisao *t = (TelaVisao *)dados;
  GtkWindow *janela = (t->e && t->e->janela) ? GTK_WINDOW(t->e->janela) : NULL;

  const char *titulo = "Importar PDF da grade do semestre";
  const char *mensagem = "As novas reservas serão adicionadas sem excluir "
                         "as reservas existentes. Deseja continuar?";
#if GTK_CHECK_VERSION(4, 10, 0)
  const char *botoes[] = {"Cancelar", "Continuar", NULL};
  GtkAlertDialog *alerta = gtk_alert_dialog_new("%s", titulo);
  gtk_alert_dialog_set_detail(alerta, mensagem);
  gtk_alert_dialog_set_buttons(alerta, botoes);
  gtk_alert_dialog_set_cancel_button(alerta, 0);
  gtk_alert_dialog_set_default_button(alerta, 0);
  gtk_alert_dialog_choose(alerta, janela, NULL, ao_resposta_alerta_pdf, t);
  g_object_unref(alerta);
#else
  GtkWidget *dialog = gtk_message_dialog_new(
      janela, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s", titulo);
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s",
                                           mensagem);
  gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancelar", GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button(GTK_DIALOG(dialog), "Continuar", GTK_RESPONSE_ACCEPT);
  g_signal_connect(dialog, "response", G_CALLBACK(ao_resposta_dialog_pdf), t);
  gtk_widget_show(dialog);
#endif
}

static void ao_aplicar(GtkButton *b, gpointer dados) {
  TelaVisao *t = (TelaVisao *)dados;
  Filtro f;
  ler_filtro(t, &f);
  acao_log("filtrar_quadro dia:%d tipo:%s sala:%s", f.dia,
           f.tipo[0] ? f.tipo : "(todas)", f.sala[0] ? f.sala : "(todas)");
  atualizar(t);
  registrar_filtro_renderizado(t);
  interface_mensagem(t->e, "Filtros aplicados.", FALSE);
}

static GtkWidget *coluna_filtro(const char *titulo, GtkWidget *campo) {
  GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *l = gtk_label_new(titulo);
  gtk_widget_add_css_class(l, "subtitulo");
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_box_append(GTK_BOX(c), l);
  gtk_box_append(GTK_BOX(c), campo);
  gtk_widget_set_hexpand(c, TRUE);
  return c;
}

static GtkWidget *cartao_titulo(const char *titulo) {
  GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_add_css_class(c, "card-visao");
  GtkWidget *l = gtk_label_new(titulo);
  gtk_widget_add_css_class(l, "secao");
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_box_append(GTK_BOX(c), l);
  return c;
}

GtkWidget *tela_visao_criar(Estado *e) {
  TelaVisao *t = g_new0(TelaVisao, 1);
  t->e = e;
  estilo_visao();

  GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_margin_top(pagina, 28);
  gtk_widget_set_margin_bottom(pagina, 24);
  gtk_widget_set_margin_start(pagina, 32);
  gtk_widget_set_margin_end(pagina, 32);

  /* topo: título e "Exportar grade" à direita */
  GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_hexpand(textos, TRUE);
  GtkWidget *titulo = gtk_label_new("Visão geral");
  gtk_widget_add_css_class(titulo, "titulo");
  gtk_label_set_xalign(GTK_LABEL(titulo), 0);
  GtkWidget *sub =
      gtk_label_new("Quadro de horários da POLI. Visualize a ocupação das "
                    "salas e gere os quadros de impressão.");
  gtk_widget_add_css_class(sub, "subtitulo");
  gtk_label_set_xalign(GTK_LABEL(sub), 0);
  gtk_box_append(GTK_BOX(textos), titulo);
  gtk_box_append(GTK_BOX(textos), sub);

  GtkWidget *menu = gtk_menu_button_new();
  gtk_menu_button_set_label(GTK_MENU_BUTTON(menu), "Exportar grade");
  gtk_menu_button_set_always_show_arrow(GTK_MENU_BUTTON(menu), TRUE);
  gtk_widget_add_css_class(menu, "secundario");
  gtk_widget_set_valign(menu, GTK_ALIGN_START);
  t->popover = gtk_popover_new();
  GtkWidget *op = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_margin_top(op, 6);
  gtk_widget_set_margin_bottom(op, 6);
  gtk_widget_set_margin_start(op, 6);
  gtk_widget_set_margin_end(op, 6);
  GtkWidget *op_pdf = gtk_button_new_with_label("Exportar PDF");
  GtkWidget *op_csv = gtk_button_new_with_label("Exportar CSV");
  GtkWidget *op_ics = gtk_button_new_with_label("Exportar agenda (.ics)");
  g_signal_connect(op_pdf, "clicked", G_CALLBACK(ao_pdf), t);
  g_signal_connect(op_csv, "clicked", G_CALLBACK(ao_csv), t);
  g_signal_connect(op_ics, "clicked", G_CALLBACK(ao_agenda), t);
  gtk_box_append(GTK_BOX(op), op_pdf);
  gtk_box_append(GTK_BOX(op), op_csv);
  gtk_box_append(GTK_BOX(op), op_ics);
  gtk_popover_set_child(GTK_POPOVER(t->popover), op);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu), t->popover);

  GtkWidget *btn_importar_pdf = gtk_button_new_with_label("Importar PDF");
  gtk_widget_add_css_class(btn_importar_pdf, "primario");
  gtk_widget_set_valign(btn_importar_pdf, GTK_ALIGN_START);
  g_signal_connect(btn_importar_pdf, "clicked", G_CALLBACK(ao_importar_pdf), t);

  gtk_box_append(GTK_BOX(topo), textos);
  gtk_box_append(GTK_BOX(topo), menu);
  gtk_box_append(GTK_BOX(topo), btn_importar_pdf);
  gtk_box_append(GTK_BOX(pagina), topo);

  /* filtros */
  t->data = seletor_data_criar(dados_data_hoje(), NULL, NULL);
  t->tipo = gtk_drop_down_new_from_strings(TIPOS);
  const char *so_todos[] = {"Todos os ambientes", NULL};
  t->sala = gtk_drop_down_new_from_strings(so_todos);
  gtk_drop_down_set_enable_search(GTK_DROP_DOWN(t->sala), TRUE);

  GtkWidget *filtros = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
  gtk_widget_add_css_class(filtros, "card-visao");
  GtkWidget *b_aplicar = gtk_button_new_with_label("Aplicar filtros");
  gtk_widget_add_css_class(b_aplicar, "secundario");
  gtk_widget_set_valign(b_aplicar, GTK_ALIGN_END);
  g_signal_connect(b_aplicar, "clicked", G_CALLBACK(ao_aplicar), t);
  gtk_box_append(GTK_BOX(filtros), coluna_filtro("Dia", t->data));
  gtk_box_append(GTK_BOX(filtros), coluna_filtro("Tipo de atividade", t->tipo));
  gtk_box_append(GTK_BOX(filtros), coluna_filtro("Sala/ambiente", t->sala));
  gtk_box_append(GTK_BOX(filtros), b_aplicar);
  gtk_box_append(GTK_BOX(pagina), filtros);

  /* aba 1: quadro de horários + resumo */
  GtkWidget *p1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_margin_top(p1, 12);

  GtkWidget *esq = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(esq, "card-visao");
  gtk_widget_set_hexpand(esq, TRUE);
  t->titulo_dia = gtk_label_new("");
  gtk_widget_add_css_class(t->titulo_dia, "secao");
  gtk_label_set_xalign(GTK_LABEL(t->titulo_dia), 0);
  gtk_box_append(GTK_BOX(esq), t->titulo_dia);
  gtk_box_append(GTK_BOX(esq), cabecalho());
  t->quadro = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *rol = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rol), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rol), t->quadro);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(rol), 200);
  gtk_widget_set_vexpand(rol, TRUE);
  gtk_box_append(GTK_BOX(esq), rol);

  GtkWidget *dir = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_size_request(dir, 250, -1);
  gtk_widget_set_valign(dir, GTK_ALIGN_START);

  GtkWidget *c_res = cartao_titulo("Resumo do dia");
  t->r_uso = rotulo("", 0, TRUE, FALSE);
  t->r_ocup = rotulo("", 0, TRUE, FALSE);
  t->r_conf = rotulo("", 0, TRUE, FALSE);
  t->r_salas = rotulo("", 0, TRUE, FALSE);
  GtkWidget *lin[4] = {t->r_uso, t->r_ocup, t->r_conf, t->r_salas};
  for (int i = 0; i < 4; i++) {
    if (lin[i] == t->r_conf) {
      GtkWidget *linha_c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_widget_set_hexpand(t->r_conf, TRUE);
      gtk_widget_add_css_class(t->r_conf, "resumo-linha");
      t->btn_conf_toggle = gtk_toggle_button_new_with_label("▸");
      gtk_widget_add_css_class(t->btn_conf_toggle, "flat");
      gtk_widget_set_valign(t->btn_conf_toggle, GTK_ALIGN_CENTER);
      g_signal_connect(t->btn_conf_toggle, "toggled",
                       G_CALLBACK(ao_toggle_conf), t);
      gtk_box_append(GTK_BOX(linha_c), t->r_conf);
      gtk_box_append(GTK_BOX(linha_c), t->btn_conf_toggle);
      gtk_box_append(GTK_BOX(c_res), linha_c);

      t->lista_conf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
      gtk_widget_set_margin_start(t->lista_conf, 8);
      gtk_widget_set_margin_top(t->lista_conf, 4);
      gtk_widget_set_margin_bottom(t->lista_conf, 4);

      t->revealer_conf = gtk_revealer_new();
      gtk_revealer_set_transition_type(GTK_REVEALER(t->revealer_conf),
                                       GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
      gtk_revealer_set_child(GTK_REVEALER(t->revealer_conf), t->lista_conf);
      gtk_box_append(GTK_BOX(c_res), t->revealer_conf);
    } else {
      gtk_widget_add_css_class(lin[i], "resumo-linha");
      gtk_box_append(GTK_BOX(c_res), lin[i]);
    }
  }
  gtk_box_append(GTK_BOX(dir), c_res);

  gtk_box_append(GTK_BOX(p1), esq);
  gtk_box_append(GTK_BOX(p1), dir);

  /* aba 2: ocupação por sala */
  t->ocupacao = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(t->ocupacao, "card-visao");
  GtkWidget *rol2 = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rol2), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rol2), t->ocupacao);
  gtk_widget_set_margin_top(rol2, 12);

  GtkWidget *nb = gtk_notebook_new();
  gtk_widget_set_vexpand(nb, TRUE);
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), p1,
                           gtk_label_new("Quadro de horários"));
  gtk_notebook_append_page(GTK_NOTEBOOK(nb), rol2,
                           gtk_label_new("Ocupação por sala"));
  gtk_box_append(GTK_BOX(pagina), nb);

  g_signal_connect(pagina, "map", G_CALLBACK(ao_mostrar), t);
  recarregar_salas(t);
  atualizar(t);

  GtkWidget *rolagem = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rolagem), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), pagina);
  return rolagem;
}
