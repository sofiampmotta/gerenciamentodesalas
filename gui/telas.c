#include "interface.h"

static void ao_nova_reserva(GtkButton *b, gpointer dados) {
    interface_ir_para((Estado *) dados, "nova");
}

static GtkWidget *pagina(Estado *e, const char *titulo, const char *sub, const char *botao) {
    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(caixa, 28);
    gtk_widget_set_margin_bottom(caixa, 24);
    gtk_widget_set_margin_start(caixa, 32);
    gtk_widget_set_margin_end(caixa, 32);

    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(textos, TRUE);

    GtkWidget *t = gtk_label_new(titulo);
    gtk_widget_add_css_class(t, "titulo");
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_box_append(GTK_BOX(textos), t);

    GtkWidget *s = gtk_label_new(sub);
    gtk_widget_add_css_class(s, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(s), 0);
    gtk_box_append(GTK_BOX(textos), s);
    gtk_box_append(GTK_BOX(topo), textos);

    if (botao) {
        GtkWidget *b = gtk_button_new_with_label(botao);
        gtk_widget_add_css_class(b, "primario");
        gtk_widget_set_valign(b, GTK_ALIGN_START);
        g_signal_connect(b, "clicked", G_CALLBACK(ao_nova_reserva), e);
        gtk_box_append(GTK_BOX(topo), b);
    }
    gtk_box_append(GTK_BOX(caixa), topo);
    return caixa;
}

GtkWidget *tela_visao_criar(Estado *e)  { return pagina(e, "Visão geral", "Grade do dia: salas nas linhas, horários nas colunas.", "Nova reserva"); }
