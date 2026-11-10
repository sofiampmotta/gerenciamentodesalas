#include "interface.h"

static GtkCssProvider *g_escuro = NULL;
static gboolean g_ativo = FALSE;

static const char *CSS_ESCURO =
    "window { background: #141a26; color: #e6ebf5; }"
    ".menu-lateral { background: #0b1220; }"

    ".titulo, .titulo-form, .secao, .titulo-resultado, .nome-sala-busca,"
    ".numero-grande, .numero-busca, .negrito { color: #e6ebf5; }"
    ".subtitulo, .nota-busca, .motivo,"
    ".cabecalho-tabela label, .cab-reservas label { color: #9aa7c0; }"

    ".cartao-info, .cartao-sala, .form-busca, .form-salas, .painel-verif"
    "  { background: #1c2434; border-color: #33405a; }"
    ".cartao-conflito { border-color: #e0526b; }"
    ".moldura, .moldura-reservas { border-color: #33405a; }"

    ".tabela-salas, .tab-reservas { background: #1c2434; }"
    ".tabela-salas row, .tab-reservas row { border-bottom-color: #33405a; }"
    ".tabela-salas row:hover, .tab-reservas row:hover { background: #243049; }"
    ".tabela-salas row:selected, .tab-reservas row:selected { background: #2b4a7a; }"
    ".tabela-salas row:selected label, .tab-reservas row:selected label { color: #ffffff; }"
    ".linha-info { border-bottom-color: #33405a; }"

    "entry, spinbutton, passwordentry"
    "  { background: #0f1522; color: #e6ebf5; border-color: #33405a; }"
    "button.secundario { background: #1c2434; background-image: none; color: #e6ebf5;"
    "  border: 1px solid #5b87c7; }"
    "button.secundario:hover { background: #243049; }"

    ".status-ok, .check-ok { color: #4cc38a; }"
    ".status-erro, .check-erro, .aviso-busca { color: #ff6b81; }"
    ".barra-status { color: #9aa7c0; border-top-color: #33405a; }"
    ".barra-status.success { background: #1e7e4a; color: #ffffff; }"
    ".barra-status.error { background: #c8102e; color: #ffffff; }";

static void aplicar(gboolean escuro) {
    GdkDisplay *display = gdk_display_get_default();

    /* tema escuro do próprio GTK: botões, listas, campos, menus */
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", escuro, NULL);

    /* cores escuras das nossas telas */
    if (!g_escuro) {
        g_escuro = gtk_css_provider_new();
        gtk_css_provider_load_from_string(g_escuro, CSS_ESCURO);
    }
    if (escuro && !g_ativo) {
        gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(g_escuro),
                                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
        g_ativo = TRUE;
    } else if (!escuro && g_ativo) {
        gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(g_escuro));
        g_ativo = FALSE;
    }
}

static void ao_trocar(GObject *o, GParamSpec *p, gpointer dados) {
    aplicar(gtk_switch_get_active(GTK_SWITCH(o)));
}

/* Linha "Modo noturno [interruptor]" para o menu lateral */
GtkWidget *tema_criar_chave(void) {
    aplicar(FALSE);   /* o programa sempre abre no modo claro */

    GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(linha, 12);

    GtkWidget *rotulo = gtk_label_new("Modo noturno");
    gtk_widget_add_css_class(rotulo, "usuario-info");
    gtk_label_set_xalign(GTK_LABEL(rotulo), 0);
    gtk_widget_set_hexpand(rotulo, TRUE);

    GtkWidget *chave = gtk_switch_new();
    gtk_widget_set_valign(chave, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end(chave, 24);
    g_signal_connect(chave, "notify::active", G_CALLBACK(ao_trocar), NULL);

    gtk_box_append(GTK_BOX(linha), rotulo);
    gtk_box_append(GTK_BOX(linha), chave);
    return linha;
}
