#include "interface.h"
#include <string.h>
#include <stdlib.h>

/* PROVISÓRIO: "hoje" e "agora" fixos, iguais aos do Início. Na integração, usar o relógio. */
#define VISAO_HOJE   DATA_PADRAO
#define VISAO_AGORA  (10 * 60 + 30)

#define COL_HORA 110
#define COL_ATIV 160
#define COL_RESP 150
#define COL_SALA 56
#define FAIXA_INI 420
#define FAIXA_TOT 900

static const char *TIPOS[] = { "Todas as atividades", "Aula", "Monitoria", "Extensão", "Evento", NULL };
static const char *DIAS[]  = { "", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira",
                               "Sexta-feira", "Sábado", "Domingo" };
static const char *MESES[] = { "", "janeiro", "fevereiro", "março", "abril", "maio", "junho",
                               "julho", "agosto", "setembro", "outubro", "novembro", "dezembro" };

typedef struct {
    Estado *e;
    GtkWidget *data, *tipo, *sala, *popover;
    GtkWidget *titulo_dia, *quadro, *ocupacao;
    GtkWidget *r_uso, *r_ocup, *r_conf, *r_salas;
} TelaVisao;

typedef struct { int dia; char tipo[16]; char sala[64]; } Filtro;
typedef struct { char sala[64]; char tipo[16]; int dia; } FaixaOc;

static void estilo_visao(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".card-visao { border: 1px solid #e1e6ef; border-radius: 6px; padding: 16px; }"
        ".cab-quadro { background: #eef2f9; border-radius: 4px; }"
        ".cab-quadro label { color: #12274f; font-weight: 700; font-size: 12px; padding: 7px 0; }"
        ".grupo-quadro { margin-top: 6px; }"
        ".bloco-hora { background: #eef2f9; border-radius: 4px; color: #12274f; font-weight: 700; }"
        ".linha-quadro { border-bottom: 1px solid #e1e6ef; padding: 9px 0; }"
        ".linha-ocup { border-bottom: 1px solid #e1e6ef; padding: 10px 0; }"
        ".resumo-linha { border-bottom: 1px solid #e1e6ef; padding: 8px 0; }"
        ".chip { border-radius: 10px; padding: 1px 9px; font-size: 12px; font-weight: 600; }"
        ".chip-monitoria { background: #d9f2e6; color: #1b6b45; }"
        ".chip-extensao { background: #ece0fa; color: #5b2f91; }"
        ".chip-evento { background: #fbe0e4; color: #a30f2b; }"
        "button.btn-pdf { background: #ffffff; background-image: none; color: #c8102e;"
        "  border: 1px solid #c8102e; border-radius: 4px; font-weight: 700; padding: 9px 16px; box-shadow: none; }"
        "button.btn-csv { background: #ffffff; background-image: none; color: #1e7e4a;"
        "  border: 1px solid #1e7e4a; border-radius: 4px; font-weight: 700; padding: 9px 16px; box-shadow: none; }"
        "menubutton.secundario > button { background: #ffffff; background-image: none; color: #12274f;"
        "  border: 1px solid #1f4e8c; border-radius: 4px; font-weight: 700; padding: 10px 20px; box-shadow: none; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

static GtkWidget *rotulo(const char *txt, int largura, gboolean expande, gboolean negrito) {
    GtkWidget *l = gtk_label_new(txt);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_start(l, 12);
    gtk_widget_set_margin_end(l, 6);
    if (largura > 0) gtk_widget_set_size_request(l, largura, -1);
    gtk_widget_set_hexpand(l, expande);
    if (negrito) gtk_widget_add_css_class(l, "negrito");
    return l;
}

static void limpar(GtkWidget *caixa) {
    GtkWidget *f;
    while ((f = gtk_widget_get_first_child(caixa)) != NULL)
        gtk_box_remove(GTK_BOX(caixa), f);
}

static const char *classe_chip(const char *tipo) {
    if (strcmp(tipo, "Monitoria") == 0) return "chip-monitoria";
    if (strcmp(tipo, "Extensão") == 0)  return "chip-extensao";
    if (strcmp(tipo, "Evento") == 0)    return "chip-evento";
    return NULL;
}

static int passa(const ReservaTeste *r, const Filtro *f) {
    if (r->dia != f->dia) return 0;
    if (f->tipo[0] && strcmp(r->tipo, f->tipo) != 0) return 0;
    if (f->sala[0] && strcmp(r->sala, f->sala) != 0) return 0;
    return 1;
}

static int comparar(const void *pa, const void *pb) {
    const ReservaTeste *x = &g_reservas[*(const int *) pa];
    const ReservaTeste *y = &g_reservas[*(const int *) pb];
    if (x->ini != y->ini) return x->ini - y->ini;
    if (x->fim != y->fim) return x->fim - y->fim;
    return strcmp(x->sala, y->sala);
}

static GtkWidget *linha_reserva(const ReservaTeste *r) {
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
    gtk_box_append(GTK_BOX(linha), rotulo(r->responsavel, COL_RESP, FALSE, FALSE));
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
    gtk_box_append(GTK_BOX(c), rotulo("Professor(a) / Responsável", COL_RESP, FALSE, FALSE));
    gtk_box_append(GTK_BOX(c), rotulo("Sala", COL_SALA, FALSE, FALSE));
    return c;
}

static void montar_quadro(TelaVisao *t, const Filtro *f) {
    limpar(t->quadro);
    int idx[MAX_RESERVAS], n = 0;
    for (int i = 0; i < g_n_reservas; i++)
        if (passa(&g_reservas[i], f)) idx[n++] = i;
    qsort(idx, n, sizeof(int), comparar);

    if (n == 0) {
        GtkWidget *l = gtk_label_new("Nenhuma ocupação para esses filtros.");
        gtk_widget_add_css_class(l, "subtitulo");
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_widget_set_margin_top(l, 12);
        gtk_box_append(GTK_BOX(t->quadro), l);
        return;
    }

    int k = 0;
    while (k < n) {
        const ReservaTeste *p = &g_reservas[idx[k]];
        int j = k;
        while (j < n && g_reservas[idx[j]].ini == p->ini && g_reservas[idx[j]].fim == p->fim) j++;

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
}

static double px(int min, int w) {
    double x = (min - FAIXA_INI) * (double) w / FAIXA_TOT;
    if (x < 0) x = 0;
    if (x > w) x = w;
    return x;
}

static void desenhar_oc(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer dados) {
    const FaixaOc *f = (const FaixaOc *) dados;
    int barra = h - 14;

    cairo_set_source_rgb(cr, 0.84, 0.88, 0.94);
    cairo_rectangle(cr, 0, 0, w, barra);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.23, 0.51, 0.96);
    for (int i = 0; i < g_n_reservas; i++) {
        const ReservaTeste *o = &g_reservas[i];
        if (o->dia != f->dia || strcmp(o->sala, f->sala) != 0) continue;
        if (f->tipo[0] && strcmp(o->tipo, f->tipo) != 0) continue;
        cairo_rectangle(cr, px(o->ini, w), 0, px(o->fim, w) - px(o->ini, w) - 1, barra);
        cairo_fill(cr);
    }

    cairo_set_source_rgb(cr, 0.36, 0.44, 0.56); cairo_select_font_face(cr, "Segoe UI", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);
    int marcas[4] = { 8, 12, 16, 20 };
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
        const SalaTeste *sl = &g_salas[s];
        if (f->sala[0] && strcmp(sl->nome, f->sala) != 0) continue;

        int n = 0;
        for (int i = 0; i < g_n_reservas; i++) {
            const ReservaTeste *r = &g_reservas[i];
            if (r->dia != f->dia || strcmp(r->sala, sl->nome) != 0) continue;
            if (f->tipo[0] && strcmp(r->tipo, f->tipo) != 0) continue;
            n++;
        }

        GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
        gtk_widget_add_css_class(linha, "linha-ocup");

        GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_size_request(info, 150, -1);
        gtk_box_append(GTK_BOX(info), rotulo(sl->nome, 0, FALSE, TRUE));
        char lug[24];
        g_snprintf(lug, sizeof lug, "%d lugares", sl->lugares);
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
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(faixa), desenhar_oc, fo, g_free);

        char txt[32];
        if (n == 1) g_strlcpy(txt, "1 ocupação", sizeof txt);
        else g_snprintf(txt, sizeof txt, "%d ocupações", n);
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

static void resumo(TelaVisao *t, int dia) {
    int ocup = 0, conflitos = 0, uso = 0;
    for (int i = 0; i < g_n_reservas; i++) {
        if (g_reservas[i].dia != dia) continue;
        ocup++;
        for (int j = i + 1; j < g_n_reservas; j++) {
            if (g_reservas[j].dia != dia || strcmp(g_reservas[i].sala, g_reservas[j].sala) != 0) continue;
            if (g_reservas[i].ini < g_reservas[j].fim && g_reservas[j].ini < g_reservas[i].fim) conflitos++;
        }
    }
    for (int s = 0; s < g_n_salas; s++) {
        int em_uso = 0, no_dia = 0;
        for (int i = 0; i < g_n_reservas; i++) {
            const ReservaTeste *r = &g_reservas[i];
            if (r->dia != dia || strcmp(r->sala, g_salas[s].nome) != 0) continue;
            no_dia = 1;
            if (r->ini <= VISAO_AGORA && VISAO_AGORA < r->fim) em_uso = 1;
        }
        if (dia == VISAO_HOJE ? em_uso : no_dia) uso++;
    }
    definir(t->r_uso, uso, dia == VISAO_HOJE ? "salas em uso agora" : "salas com ocupação no dia");
    definir(t->r_ocup, ocup, ocup == 1 ? "ocupação no dia" : "ocupações no dia");
    definir(t->r_conf, conflitos, conflitos == 1 ? "conflito" : "conflitos");
    definir(t->r_salas, g_n_salas, "salas cadastradas");
}

static void ler_filtro(TelaVisao *t, Filtro *f) {
    f->dia = seletor_data_get(t->data);

    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->tipo));
    f->tipo[0] = '\0';
    if (i >= 1 && i <= 4) g_strlcpy(f->tipo, TIPOS[i], sizeof f->tipo);

    guint s = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->sala));
    f->sala[0] = '\0';
    if (s != GTK_INVALID_LIST_POSITION && s > 0) {
        GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
        if (it) g_strlcpy(f->sala, gtk_string_object_get_string(GTK_STRING_OBJECT(it)), sizeof f->sala);
    }
}

static void atualizar(TelaVisao *t) {
    Filtro f;
    ler_filtro(t, &f);
    if (!dados_data_valida(f.dia)) return;

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
    char antes[64] = "";
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->sala));
    if (sel != GTK_INVALID_LIST_POSITION && sel > 0) {
        GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
        if (it) g_strlcpy(antes, gtk_string_object_get_string(GTK_STRING_OBJECT(it)), sizeof antes);
    }

    const char *nomes[MAX_SALAS + 2];
    nomes[0] = "Todos os ambientes";
    for (int i = 0; i < g_n_salas; i++) nomes[i + 1] = g_salas[i].nome;
    nomes[g_n_salas + 1] = NULL;

    GtkStringList *sl = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(t->sala)));
    gtk_string_list_splice(sl, 0, g_list_model_get_n_items(G_LIST_MODEL(sl)), nomes);

    guint pos = 0;
    for (int i = 0; i < g_n_salas; i++)
        if (antes[0] && strcmp(g_salas[i].nome, antes) == 0) pos = i + 1;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->sala), pos);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) {
    TelaVisao *t = (TelaVisao *) dados;
    recarregar_salas(t);
    atualizar(t);
}

static void exportar(TelaVisao *t, const char *formato) {
    Filtro f;
    ler_filtro(t, &f);
    acao_log("exportar_%s dia:%d tipo:%s sala:%s", formato, f.dia,
             f.tipo[0] ? f.tipo : "(todas)", f.sala[0] ? f.sala : "(todas)");
    char *up = g_ascii_strup(formato, -1);
    char *msg = g_strdup_printf("Exportar %s: parâmetros impressos no terminal (função do backend ainda não ligada).", up);
    interface_mensagem(t->e, msg, FALSE);
    g_free(up);
    g_free(msg);
    if (t->popover) gtk_popover_popdown(GTK_POPOVER(t->popover));
}

static void ao_pdf(GtkButton *b, gpointer dados) { exportar((TelaVisao *) dados, "pdf"); }
static void ao_csv(GtkButton *b, gpointer dados) { exportar((TelaVisao *) dados, "csv"); }

static void ao_aplicar(GtkButton *b, gpointer dados) {
    TelaVisao *t = (TelaVisao *) dados;
    Filtro f;
    ler_filtro(t, &f);
    acao_log("filtrar_quadro dia:%d tipo:%s sala:%s", f.dia,
             f.tipo[0] ? f.tipo : "(todas)", f.sala[0] ? f.sala : "(todas)");
    atualizar(t);
    interface_mensagem(t->e, "Filtros aplicados.", FALSE);
}

static void ao_nova(GtkButton *b, gpointer dados) {
    interface_ir_para(((TelaVisao *) dados)->e, "nova");
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

    /* topo: título, "Exportar grade" e "Nova reserva" */
    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(textos, TRUE);
    GtkWidget *titulo = gtk_label_new("Visão geral");
    gtk_widget_add_css_class(titulo, "titulo");
    gtk_label_set_xalign(GTK_LABEL(titulo), 0);
    GtkWidget *sub = gtk_label_new("Quadro de horários da POLI. Visualize a ocupação das salas e gere os quadros de impressão.");
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
    g_signal_connect(op_pdf, "clicked", G_CALLBACK(ao_pdf), t);
    g_signal_connect(op_csv, "clicked", G_CALLBACK(ao_csv), t);
    gtk_box_append(GTK_BOX(op), op_pdf);
    gtk_box_append(GTK_BOX(op), op_csv);
    gtk_popover_set_child(GTK_POPOVER(t->popover), op);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu), t->popover);

    GtkWidget *b_nova = gtk_button_new_with_label("Nova reserva");
    gtk_widget_add_css_class(b_nova, "primario");
    gtk_widget_set_valign(b_nova, GTK_ALIGN_START);
    g_signal_connect(b_nova, "clicked", G_CALLBACK(ao_nova), t);

    gtk_box_append(GTK_BOX(topo), textos);
    gtk_box_append(GTK_BOX(topo), menu);
    gtk_box_append(GTK_BOX(topo), b_nova);
    gtk_box_append(GTK_BOX(pagina), topo);

    /* filtros */
    t->data = seletor_data_criar(DATA_PADRAO, NULL, NULL);
    t->tipo = gtk_drop_down_new_from_strings(TIPOS);
    const char *so_todos[] = { "Todos os ambientes", NULL };
    t->sala = gtk_drop_down_new_from_strings(so_todos);
    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(t->sala), TRUE);

    GtkWidget *filtros = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_add_css_class(filtros, "card-visao");
    GtkWidget *b_aplicar = gtk_button_new_with_label("Aplicar filtros");
    gtk_widget_add_css_class(b_aplicar, "primario");
    gtk_widget_set_valign(b_aplicar, GTK_ALIGN_END);
    g_signal_connect(b_aplicar, "clicked", G_CALLBACK(ao_aplicar), t);
    gtk_box_append(GTK_BOX(filtros), coluna_filtro("Dia", t->data));
    gtk_box_append(GTK_BOX(filtros), coluna_filtro("Tipo de atividade", t->tipo));
    gtk_box_append(GTK_BOX(filtros), coluna_filtro("Sala/ambiente", t->sala));
    gtk_box_append(GTK_BOX(filtros), b_aplicar);
    gtk_box_append(GTK_BOX(pagina), filtros);

    /* aba 1: quadro de horários + resumo + exportação */
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
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rol), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rol), t->quadro);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(rol), 200);
    gtk_widget_set_vexpand(rol, TRUE);
    gtk_box_append(GTK_BOX(esq), rol);

    GtkWidget *dir = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_size_request(dir, 250, -1);
    gtk_widget_set_valign(dir, GTK_ALIGN_START);

    GtkWidget *c_res = cartao_titulo("Resumo do dia");
    t->r_uso   = rotulo("", 0, TRUE, FALSE);
    t->r_ocup  = rotulo("", 0, TRUE, FALSE);
    t->r_conf  = rotulo("", 0, TRUE, FALSE);
    t->r_salas = rotulo("", 0, TRUE, FALSE);
    GtkWidget *lin[4] = { t->r_uso, t->r_ocup, t->r_conf, t->r_salas };
    for (int i = 0; i < 4; i++) {
        gtk_widget_add_css_class(lin[i], "resumo-linha");
        gtk_box_append(GTK_BOX(c_res), lin[i]);
    }
    gtk_box_append(GTK_BOX(dir), c_res);

    GtkWidget *c_exp = cartao_titulo("Exportação rápida");
    GtkWidget *desc = gtk_label_new("Gerar o quadro de horários deste dia para impressão.");
    gtk_widget_add_css_class(desc, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(desc), 0);
    gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(desc), 26);
    gtk_box_append(GTK_BOX(c_exp), desc);
    GtkWidget *b_pdf = gtk_button_new_with_label("Exportar PDF");
    GtkWidget *b_csv = gtk_button_new_with_label("Exportar CSV");
    gtk_widget_add_css_class(b_pdf, "btn-pdf");
    gtk_widget_add_css_class(b_csv, "btn-csv");
    gtk_widget_set_margin_top(b_pdf, 8);
    g_signal_connect(b_pdf, "clicked", G_CALLBACK(ao_pdf), t);
    g_signal_connect(b_csv, "clicked", G_CALLBACK(ao_csv), t);
    gtk_box_append(GTK_BOX(c_exp), b_pdf);
    gtk_box_append(GTK_BOX(c_exp), b_csv);
    gtk_box_append(GTK_BOX(dir), c_exp);

    gtk_box_append(GTK_BOX(p1), esq);
    gtk_box_append(GTK_BOX(p1), dir);

    /* aba 2: ocupação por sala */
    t->ocupacao = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(t->ocupacao, "card-visao");
    GtkWidget *rol2 = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rol2), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rol2), t->ocupacao);
    gtk_widget_set_margin_top(rol2, 12);

    GtkWidget *nb = gtk_notebook_new();
    gtk_widget_set_vexpand(nb, TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), p1, gtk_label_new("Quadro de horários"));
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), rol2, gtk_label_new("Ocupação por sala"));
    gtk_box_append(GTK_BOX(pagina), nb);

    g_signal_connect(pagina, "map", G_CALLBACK(ao_mostrar), t);
    recarregar_salas(t);
    atualizar(t);

    GtkWidget *rolagem = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rolagem),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), pagina);
    return rolagem;
}

