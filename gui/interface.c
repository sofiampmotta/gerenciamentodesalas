#include "interface.h"

void interface_mensagem(Estado *e, const char *texto, gboolean erro) {
    gtk_label_set_text(GTK_LABEL(e->barra_status), texto);
    gtk_widget_remove_css_class(e->barra_status, "error");
    gtk_widget_remove_css_class(e->barra_status, "success");
    gtk_widget_add_css_class(e->barra_status, erro ? "error" : "success");
}

static void ao_menu(GtkToggleButton *b, gpointer dados) {
    Estado *e = (Estado *) dados;
    if (gtk_toggle_button_get_active(b)) {
        const char *nome = g_object_get_data(G_OBJECT(b), "tela");
        gtk_stack_set_visible_child_name(GTK_STACK(e->stack), nome);
    }
}

static GtkWidget *criar_menu(Estado *e) {
    GtkWidget *menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(menu, "menu-lateral");
    gtk_widget_set_size_request(menu, 230, -1);

    GtkWidget *faixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(faixa, "faixa-vermelha");
    gtk_widget_set_halign(faixa, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(menu), faixa);

    GtkWidget *logo = gtk_label_new("POLI");
    gtk_widget_add_css_class(logo, "logo");
    gtk_label_set_xalign(GTK_LABEL(logo), 0);
    gtk_box_append(GTK_BOX(menu), logo);

    GtkWidget *sub = gtk_label_new("Gestão de Ambientes");
    gtk_widget_add_css_class(sub, "sublogo");
    gtk_label_set_xalign(GTK_LABEL(sub), 0);
    gtk_box_append(GTK_BOX(menu), sub);

    static const char *nomes[]   = { "inicio", "visao", "busca", "reservas", "salas" };
    static const char *titulos[] = { "Início", "Visão geral", "Buscar salas", "Reservas", "Salas" };

    GtkWidget *primeiro = NULL;
    for (int i = 0; i < 5; i++) {
        GtkWidget *b = gtk_toggle_button_new_with_label(titulos[i]);
        gtk_widget_add_css_class(b, "menu");
        gtk_widget_set_halign(gtk_button_get_child(GTK_BUTTON(b)), GTK_ALIGN_START);
        g_object_set_data(G_OBJECT(b), "tela", (gpointer) nomes[i]);
        if (primeiro) gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(b), GTK_TOGGLE_BUTTON(primeiro));
        else primeiro = b;
        g_signal_connect(b, "toggled", G_CALLBACK(ao_menu), e);
        gtk_box_append(GTK_BOX(menu), b);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(primeiro), TRUE);

    GtkWidget *espaco = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(espaco, TRUE);
    gtk_box_append(GTK_BOX(menu), espaco);

    GtkWidget *u1 = gtk_label_new(""); e->rotulo_nome = u1;
    gtk_widget_add_css_class(u1, "usuario-nome");
    gtk_label_set_xalign(GTK_LABEL(u1), 0);
    GtkWidget *u2 = gtk_label_new(""); e->rotulo_setor = u2;
    gtk_widget_add_css_class(u2, "usuario-info");
    gtk_label_set_xalign(GTK_LABEL(u2), 0);
    GtkWidget *u3 = gtk_label_new("Sistema local");
    gtk_widget_add_css_class(u3, "usuario-info");
    gtk_label_set_xalign(GTK_LABEL(u3), 0);
    gtk_box_append(GTK_BOX(menu), u1);
    gtk_box_append(GTK_BOX(menu), u2);
    gtk_box_append(GTK_BOX(menu), u3);
    GtkWidget *sair = gtk_button_new_with_label("Sair"); gtk_widget_add_css_class(sair, "menu"); gtk_widget_set_halign(gtk_button_get_child(GTK_BUTTON(sair)), GTK_ALIGN_START); g_signal_connect(sair, "clicked", G_CALLBACK(login_sair), e); gtk_box_append(GTK_BOX(menu), sair);

    return menu;
}

static void ativar(GtkApplication *app, gpointer dados) {
    Estado *e = g_new0(Estado, 1);
    estilo_aplicar();

    e->janela = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(e->janela), "POLI - Gestão de Ambientes");
    gtk_window_set_default_size(GTK_WINDOW(e->janela), 1100, 700);

    e->stack = gtk_stack_new();
    gtk_widget_set_hexpand(e->stack, TRUE);
    gtk_widget_set_vexpand(e->stack, TRUE);
    gtk_stack_add_named(GTK_STACK(e->stack), tela_inicio_criar(e),   "inicio");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_visao_criar(e),    "visao");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_busca_criar(e),    "busca");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_reservas_criar(e), "reservas");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_salas_criar(e),    "salas");

    e->barra_status = gtk_label_new("Pronto.");
    gtk_widget_add_css_class(e->barra_status, "barra-status");
    gtk_label_set_xalign(GTK_LABEL(e->barra_status), 0);

    GtkWidget *direita = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(direita), e->stack);
    gtk_box_append(GTK_BOX(direita), e->barra_status);

    GtkWidget *raiz = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(raiz), criar_menu(e));
    gtk_box_append(GTK_BOX(raiz), direita);

    e->raiz = gtk_stack_new(); gtk_stack_add_named(GTK_STACK(e->raiz), tela_login_criar(e), "login"); gtk_stack_add_named(GTK_STACK(e->raiz), raiz, "app"); gtk_window_set_child(GTK_WINDOW(e->janela), e->raiz);
    gtk_window_present(GTK_WINDOW(e->janela));
}

int interface_iniciar(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("br.upe.datp", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(ativar), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
