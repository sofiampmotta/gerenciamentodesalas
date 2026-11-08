#include "interface.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    Estado *e;
    GtkWidget *lista;
    GtkWidget *total;
} TelaReservas;

static void estilo_reservas(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".cab-reservas { padding: 0 16px; }"
        ".cab-reservas label { color: #5b6b8a; font-weight: bold; font-size: 12px; }"
        ".moldura-reservas { border: 1px solid #e1e6ef; border-radius: 6px; }"
        ".tab-reservas { background: #ffffff; }"
        ".tab-reservas row { padding: 10px 16px; border-bottom: 1px solid #e1e6ef; }"
        ".tab-reservas row:hover { background: #f4f6f9; }"
        ".tab-reservas row:selected { background: #e1ebf8; }"
        ".tab-reservas row:selected label { color: #12274f; }"
        ".negrito { font-weight: bold; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

static GtkWidget *celula(const char *texto, int largura) {
    GtkWidget *l = gtk_label_new(texto);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(l, largura, -1);
    return l;
}

static GtkWidget *linha(const char *a, const char *b, const char *c,
                        const char *d, const char *f, const char *g) {
    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(caixa), celula(a, 100));
    gtk_box_append(GTK_BOX(caixa), celula(b, 120));
    gtk_box_append(GTK_BOX(caixa), celula(c, 230));
    gtk_box_append(GTK_BOX(caixa), celula(d, 100));
    gtk_box_append(GTK_BOX(caixa), celula(f, 100));
    gtk_box_append(GTK_BOX(caixa), celula(g, 160));
    return caixa;
}

static int comparar(const void *pa, const void *pb) {
    const ReservaTeste *x = &g_reservas[*(const int *) pa];
    const ReservaTeste *y = &g_reservas[*(const int *) pb];
    if (x->dia != y->dia) return x->dia - y->dia;
    if (x->ini != y->ini) return x->ini - y->ini;
    return strcmp(x->sala, y->sala);
}

static void recarregar(TelaReservas *t) {
    GtkListBoxRow *row;
    while ((row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(t->lista), 0)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(t->lista), GTK_WIDGET(row));

    int idx[MAX_RESERVAS];
    for (int i = 0; i < g_n_reservas; i++) idx[i] = i;
    qsort(idx, g_n_reservas, sizeof(int), comparar);

    for (int k = 0; k < g_n_reservas; k++) {
        const ReservaTeste *r = &g_reservas[idx[k]];
        char dia[16], a[8], b[8], hor[24];
        dados_data_texto(r->dia, dia, sizeof dia);
        dados_hora_texto(r->ini, a, sizeof a);
        dados_hora_texto(r->fim, b, sizeof b);
        g_snprintf(hor, sizeof hor, "%s às %s", a, b);
        GtkWidget *caixa = linha(dia, hor, r->atividade, r->sala, r->tipo, r->responsavel);
        gtk_widget_add_css_class(gtk_widget_get_first_child(caixa), "negrito");
        GtkWidget *item = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(item), caixa);
        gtk_list_box_append(GTK_LIST_BOX(t->lista), item);
    }
    char *txt = g_strdup_printf("%d reservas registradas", g_n_reservas);
    gtk_label_set_text(GTK_LABEL(t->total), txt);
    g_free(txt);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) { recarregar((TelaReservas *) dados); }

static void ao_nova(GtkButton *b, gpointer dados) {
    interface_ir_para(((TelaReservas *) dados)->e, "nova");
}

GtkWidget *tela_reservas_criar(Estado *e) {
    TelaReservas *t = g_new0(TelaReservas, 1);
    t->e = e;
    estilo_reservas();

    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(pagina, 28);
    gtk_widget_set_margin_bottom(pagina, 24);
    gtk_widget_set_margin_start(pagina, 32);
    gtk_widget_set_margin_end(pagina, 32);

    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(textos, TRUE);
    GtkWidget *titulo = gtk_label_new("Reservas");
    gtk_widget_add_css_class(titulo, "titulo");
    gtk_label_set_xalign(GTK_LABEL(titulo), 0);
    t->total = gtk_label_new("");
    gtk_widget_add_css_class(t->total, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(t->total), 0);
    gtk_box_append(GTK_BOX(textos), titulo);
    gtk_box_append(GTK_BOX(textos), t->total);
    GtkWidget *b = gtk_button_new_with_label("Nova reserva");
    gtk_widget_add_css_class(b, "primario");
    gtk_widget_set_valign(b, GTK_ALIGN_START);
    g_signal_connect(b, "clicked", G_CALLBACK(ao_nova), t);
    gtk_box_append(GTK_BOX(topo), textos);
    gtk_box_append(GTK_BOX(topo), b);
    gtk_box_append(GTK_BOX(pagina), topo);

    GtkWidget *cab = linha("Dia", "Horário", "Atividade", "Sala", "Tipo", "Responsável");
    gtk_widget_add_css_class(cab, "cab-reservas");
    gtk_box_append(GTK_BOX(pagina), cab);

    t->lista = gtk_list_box_new();
    gtk_widget_add_css_class(t->lista, "tab-reservas");
    GtkWidget *rolagem = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), t->lista);
    gtk_widget_set_vexpand(rolagem, TRUE);
    gtk_widget_add_css_class(rolagem, "moldura-reservas");
    gtk_box_append(GTK_BOX(pagina), rolagem);

    g_signal_connect(pagina, "map", G_CALLBACK(ao_mostrar), t);
    recarregar(t);
    return pagina;
}
