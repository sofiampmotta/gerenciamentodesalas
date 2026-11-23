#include "interface.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  Estado *e;
  GtkWidget *titulo, *sub;
  GtkWidget *rotulo_livres;
  GtkWidget *lista_livres, *lista_prox, *lista_dias;
} TelaInicio;

typedef struct {
  int sala;
  int ate;
} Livre;

static const char *DIAS_SEMANA[] = {"Domingo", "Segunda", "Terça", "Quarta",
                                    "Quinta",  "Sexta",   "Sábado"};
static const char *DIAS_CURTOS[] = {"Dom", "Seg", "Ter", "Qua",
                                    "Qui", "Sex", "Sáb"};
static const char *MESES[] = {
    "",      "janeiro", "fevereiro", "março",   "abril",    "maio",    "junho",
    "julho", "agosto",  "setembro",  "outubro", "novembro", "dezembro"};

static void limpar_caixa(GtkWidget *caixa) {
  GtkWidget *f;
  while ((f = gtk_widget_get_first_child(caixa)) != NULL)
    gtk_box_remove(GTK_BOX(caixa), f);
}

static GtkWidget *secao_bloco(const char *titulo_inicial,
                              const char *nome_icone, GCallback ao_ver_todas,
                              gpointer dados, GtkWidget **rotulo_tit,
                              GtkWidget **lista) {
  GtkWidget *painel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_add_css_class(painel, "painel-lista");
  gtk_widget_set_hexpand(painel, TRUE);

  GtkWidget *cab = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  GtkWidget *ic = gtk_image_new_from_icon_name(nome_icone);
  gtk_image_set_pixel_size(GTK_IMAGE(ic), 20);

  GtkWidget *t = gtk_label_new(titulo_inicial);
  gtk_widget_add_css_class(t, "secao");
  gtk_label_set_xalign(GTK_LABEL(t), 0);
  gtk_widget_set_hexpand(t, TRUE);
  if (rotulo_tit)
    *rotulo_tit = t;

  GtkWidget *link = gtk_button_new_with_label("Ver todas →");
  gtk_widget_add_css_class(link, "link-ver-todas");
  g_signal_connect(link, "clicked", ao_ver_todas, dados);

  gtk_box_append(GTK_BOX(cab), ic);
  gtk_box_append(GTK_BOX(cab), t);
  gtk_box_append(GTK_BOX(cab), link);
  gtk_box_append(GTK_BOX(painel), cab);

  *lista = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(painel), *lista);
  return painel;
}

static GtkWidget *linha_livre(const char *sala, const char *lugares,
                              const char *ate) {
  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_add_css_class(caixa, "linha-info");

  GtkWidget *la = gtk_label_new(sala);
  gtk_label_set_xalign(GTK_LABEL(la), 0.5);
  gtk_widget_add_css_class(la, "badge-sala");

  GtkWidget *lb = gtk_label_new(lugares);
  gtk_label_set_xalign(GTK_LABEL(lb), 0);
  gtk_widget_set_hexpand(lb, TRUE);
  gtk_label_set_ellipsize(GTK_LABEL(lb), PANGO_ELLIPSIZE_END);

  GtkWidget *lc = gtk_label_new(ate);
  gtk_label_set_xalign(GTK_LABEL(lc), 1);
  gtk_widget_add_css_class(lc, "texto-destaque");

  gtk_box_append(GTK_BOX(caixa), la);
  gtk_box_append(GTK_BOX(caixa), lb);
  gtk_box_append(GTK_BOX(caixa), lc);
  return caixa;
}

static GtkWidget *linha_ocupacao(const char *hora, const char *atividade,
                                 const char *sala, gboolean cabecalho) {
  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_add_css_class(caixa,
                           cabecalho ? "cabecalho-tabela" : "linha-info");

  GtkWidget *la = gtk_label_new(hora);
  gtk_label_set_xalign(GTK_LABEL(la), 0);
  gtk_widget_set_size_request(la, 75, -1);
  if (!cabecalho)
    gtk_widget_add_css_class(la, "negrito");

  GtkWidget *lb = gtk_label_new(atividade);
  gtk_label_set_xalign(GTK_LABEL(lb), 0);
  gtk_widget_set_hexpand(lb, TRUE);
  gtk_label_set_ellipsize(GTK_LABEL(lb), PANGO_ELLIPSIZE_END);

  GtkWidget *lc = gtk_label_new(sala);
  gtk_label_set_xalign(GTK_LABEL(lc), 1);
  gtk_widget_set_size_request(lc, 85, -1);
  if (!cabecalho)
    gtk_widget_add_css_class(lc, "texto-destaque");

  gtk_box_append(GTK_BOX(caixa), la);
  gtk_box_append(GTK_BOX(caixa), lb);
  gtk_box_append(GTK_BOX(caixa), lc);
  return caixa;
}

static int contar_reservas_dia(int data_yyyymmdd) {
  int total = 0;
  for (int i = 0; i < g_n_reservas; i++) {
    if (dados_reserva_ocorre_em(&g_reservas[i], data_yyyymmdd))
      total++;
  }
  return total;
}

static int avancar_dia(int data_yyyymmdd, int delta_dias) {
  GDate *d = g_date_new_dmy(data_yyyymmdd % 100, (data_yyyymmdd / 100) % 100,
                            data_yyyymmdd / 10000);
  if (!d || !g_date_valid(d)) {
    if (d)
      g_date_free(d);
    return data_yyyymmdd;
  }
  if (delta_dias >= 0)
    g_date_add_days(d, (guint)delta_dias);
  else
    g_date_subtract_days(d, (guint)(-delta_dias));
  int out = g_date_get_year(d) * 10000 + g_date_get_month(d) * 100 +
            g_date_get_day(d);
  g_date_free(d);
  return out;
}

static int cmp_livre(const void *pa, const void *pb) {
  const Livre *a = (const Livre *)pa, *b = (const Livre *)pb;
  if (a->ate != b->ate)
    return b->ate - a->ate;
  return a->sala - b->sala;
}

static int cmp_prox(const void *pa, const void *pb) {
  int a = *(const int *)pa, b = *(const int *)pb;
  if (g_reservas[a].ini != g_reservas[b].ini)
    return g_reservas[a].ini - g_reservas[b].ini;
  return a - b;
}

static void atualizar(TelaInicio *t) {
  time_t agora_t = time(NULL);
  struct tm *tm_local = localtime(&agora_t);
  int hoje_dados =
      tm_local ? (tm_local->tm_year + 1900) * 10000 +
                     (tm_local->tm_mon + 1) * 100 + tm_local->tm_mday
               : dados_data_hoje();
  int agora = tm_local ? tm_local->tm_hour * 60 + tm_local->tm_min : 0;
  int hora_atual = tm_local ? tm_local->tm_hour : 0;

  const char *nome =
      t->e->rotulo_nome ? gtk_label_get_text(GTK_LABEL(t->e->rotulo_nome)) : "";
  gchar **partes = g_strsplit(nome, " ", 2);
  const char *primeiro = (partes[0] != NULL) ? partes[0] : "";
  const char *saud = hora_atual < 12
                         ? "Bom dia"
                         : (hora_atual < 18 ? "Boa tarde" : "Boa noite");
  char *tit =
      primeiro[0] ? g_strdup_printf("%s, %s", saud, primeiro) : g_strdup(saud);
  gtk_label_set_text(GTK_LABEL(t->titulo), tit);
  g_free(tit);
  g_strfreev(partes);

  if (tm_local) {
    int wd = tm_local->tm_wday;
    int dia = tm_local->tm_mday;
    int mes = tm_local->tm_mon + 1;
    int ano = tm_local->tm_year + 1900;
    char *data = g_strdup_printf("%s, %d de %s, %d", DIAS_SEMANA[wd], dia,
                                 MESES[mes], ano);
    gtk_label_set_text(GTK_LABEL(t->sub), data);
    g_free(data);
  }

  /* 1) Livres agora */
  Livre *livres = g_new(Livre, g_n_salas);
  int nl = 0;
  for (int s = 0; s < g_n_salas; s++) {
    int ocupada = 0, ate = 24 * 60;
    for (int i = 0; i < g_n_reservas; i++) {
      const ReservaVista *r = &g_reservas[i];
      if (!dados_reserva_ocorre_em(r, hoje_dados) ||
          strcmp(r->sala, g_salas[s].nome) != 0)
        continue;
      if (r->ini <= agora && agora < r->fim)
        ocupada = 1;
      else if (r->ini > agora && r->ini < ate)
        ate = r->ini;
    }
    if (!ocupada) {
      livres[nl].sala = s;
      livres[nl].ate = ate;
      nl++;
    }
  }
  qsort(livres, nl, sizeof(Livre), cmp_livre);

  char *tit_livres =
      g_strdup_printf("%d %s livre%s agora", nl, nl == 1 ? "sala" : "salas",
                      nl == 1 ? "" : "s");
  gtk_label_set_text(GTK_LABEL(t->rotulo_livres), tit_livres);
  g_free(tit_livres);

  limpar_caixa(t->lista_livres);
  if (nl == 0) {
    GtkWidget *l = gtk_label_new("Nenhuma sala livre agora.");
    gtk_widget_add_css_class(l, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_append(GTK_BOX(t->lista_livres), l);
  }
  for (int k = 0; k < nl; k++) {
    const SalaVista *sl = &g_salas[livres[k].sala];
    char lug[24], ate[40], h[8];
    g_snprintf(lug, sizeof lug, "%d lugares", sl->lugares);
    if (livres[k].ate >= 24 * 60) {
      g_strlcpy(ate, "Livre o resto do dia", sizeof ate);
    } else {
      dados_hora_texto(livres[k].ate, h, sizeof h);
      g_snprintf(ate, sizeof ate, "Livre até %s", h);
    }
    gtk_box_append(GTK_BOX(t->lista_livres), linha_livre(sl->nome, lug, ate));
  }
  g_free(livres);

  /* 2) Próximas ocupações (abaixo de Livres agora) */
  int *prox = g_new(int, g_n_reservas);
  int np = 0;
  for (int i = 0; i < g_n_reservas; i++)
    if (dados_reserva_ocorre_em(&g_reservas[i], hoje_dados) &&
        g_reservas[i].ini > agora)
      prox[np++] = i;
  qsort(prox, np, sizeof(int), cmp_prox);

  limpar_caixa(t->lista_prox);
  if (np == 0) {
    GtkWidget *l = gtk_label_new("Nada mais para hoje.");
    gtk_widget_add_css_class(l, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_append(GTK_BOX(t->lista_prox), l);
  } else {
    gtk_box_append(GTK_BOX(t->lista_prox),
                   linha_ocupacao("HORÁRIO", "ATIVIDADE", "SALA", TRUE));
  }
  for (int k = 0; k < np && k < 4; k++) {
    const ReservaVista *r = &g_reservas[prox[k]];
    char h[8];
    dados_hora_texto(r->ini, h, sizeof h);
    gtk_box_append(GTK_BOX(t->lista_prox),
                   linha_ocupacao(h, r->atividade, r->sala, FALSE));
  }
  g_free(prox);

  /* 3) Faixa compacta de dias com movimento (diferencial sem duplicar
   * seletor/grade) */
  limpar_caixa(t->lista_dias);
  GtkWidget *faixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_set_homogeneous(GTK_BOX(faixa), TRUE);

  for (int offset = 0; offset < 5; offset++) {
    int d_cod = avancar_dia(hoje_dados, offset);
    int qtd_res = contar_reservas_dia(d_cod);

    GDate *gd = g_date_new_dmy(d_cod % 100, (d_cod / 100) % 100, d_cod / 10000);
    int wd = gd ? g_date_get_weekday(gd) : 0; /* GDateWeekday: 1=Seg .. 7=Dom */
    int idx_curto = (wd >= 1 && wd <= 7) ? (wd % 7) : 0;
    if (gd)
      g_date_free(gd);

    GtkWidget *cel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(cel, "cartao-info");
    gtk_widget_set_hexpand(cel, TRUE);

    char topo_dia[32];
    g_snprintf(topo_dia, sizeof topo_dia, "%s · %02d/%02d",
               DIAS_CURTOS[idx_curto], d_cod % 100, (d_cod / 100) % 100);
    GtkWidget *l_dia = gtk_label_new(topo_dia);
    gtk_widget_add_css_class(l_dia, "negrito");
    gtk_label_set_xalign(GTK_LABEL(l_dia), 0);

    char info_res[36];
    if (qtd_res == 0)
      g_strlcpy(info_res, "Sem reservas", sizeof info_res);
    else
      g_snprintf(info_res, sizeof info_res, "%d %s", qtd_res,
                 qtd_res == 1 ? "reserva" : "reservas");
    GtkWidget *l_qtd = gtk_label_new(info_res);
    gtk_widget_add_css_class(l_qtd, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l_qtd), 0);

    gtk_box_append(GTK_BOX(cel), l_dia);
    gtk_box_append(GTK_BOX(cel), l_qtd);
    gtk_box_append(GTK_BOX(faixa), cel);
  }
  gtk_box_append(GTK_BOX(t->lista_dias), faixa);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) {
  atualizar((TelaInicio *)dados);
}
static void ao_buscar(GtkButton *b, gpointer d) {
  interface_ir_para(((TelaInicio *)d)->e, "busca");
}
static void ao_reservas(GtkButton *b, gpointer d) {
  interface_ir_para(((TelaInicio *)d)->e, "reservas");
}
static void ao_visao(GtkButton *b, gpointer d) {
  interface_ir_para(((TelaInicio *)d)->e, "visao");
}

GtkWidget *tela_inicio_criar(Estado *e) {
  TelaInicio *t = g_new0(TelaInicio, 1);
  t->e = e;

  GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
  gtk_widget_set_margin_top(pagina, 28);
  gtk_widget_set_margin_bottom(pagina, 24);
  gtk_widget_set_margin_start(pagina, 36);
  gtk_widget_set_margin_end(pagina, 36);

  GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  t->titulo = gtk_label_new("");
  gtk_widget_add_css_class(t->titulo, "titulo");
  gtk_label_set_xalign(GTK_LABEL(t->titulo), 0);

  t->sub = gtk_label_new("");
  gtk_widget_add_css_class(t->sub, "subtitulo");
  gtk_label_set_xalign(GTK_LABEL(t->sub), 0);

  gtk_box_append(GTK_BOX(topo), t->titulo);
  gtk_box_append(GTK_BOX(topo), t->sub);
  gtk_box_append(GTK_BOX(pagina), topo);

  /* Próximas ocupações embaixo de Livres agora */
  gtk_box_append(GTK_BOX(pagina),
                 secao_bloco("Livres agora", "appointment-soon-symbolic",
                             G_CALLBACK(ao_buscar), t, &t->rotulo_livres,
                             &t->lista_livres));

  gtk_box_append(GTK_BOX(pagina),
                 secao_bloco("Próximas ocupações", "view-list-symbolic",
                             G_CALLBACK(ao_reservas), t, NULL, &t->lista_prox));

  gtk_box_append(GTK_BOX(pagina),
                 secao_bloco("Movimento nos próximos dias",
                             "x-office-calendar-symbolic", G_CALLBACK(ao_visao),
                             t, NULL, &t->lista_dias));

  GtkWidget *rolagem = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rolagem), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), pagina);
  g_signal_connect(rolagem, "map", G_CALLBACK(ao_mostrar), t);
  atualizar(t);
  return rolagem;
}
