#include "interface.h"

static const char *NOMES[5] = { "inicio", "visao", "busca", "reservas", "salas" };
static const char *TITULOS[5] = { "Início", "Visão geral", "Buscar salas", "Reservas", "Salas" };
static const char *ICONES[5] = {
    "view-grid-symbolic",
    "view-continuous-symbolic",
    "system-search-symbolic",
    "x-office-calendar-symbolic",
    "go-home-symbolic"
};

static GtkWidget *rotulo_status_global = NULL;

void interface_mensagem(Estado *e, const char *texto, gboolean erro) {
    if (rotulo_status_global)
        gtk_label_set_text(GTK_LABEL(rotulo_status_global), texto);
    else if (GTK_IS_LABEL(e->barra_status))
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
    gtk_widget_set_hexpand(menu, FALSE);

    GtkWidget *faixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(faixa, "faixa-vermelha");
    gtk_widget_set_halign(faixa, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(menu), faixa);

    GtkWidget *cab_logo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(cab_logo, 14);
    gtk_widget_set_margin_end(cab_logo, 10);
    gtk_widget_set_margin_bottom(cab_logo, 22);
    GtkWidget *img_logo = logo_poli_criar();
    gtk_widget_set_valign(img_logo, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(img_logo, 6);
    gtk_box_append(GTK_BOX(cab_logo), img_logo);
    GtkWidget *txt_logo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(txt_logo, GTK_ALIGN_CENTER);

    GtkWidget *logo = gtk_label_new("POLI");
    gtk_widget_add_css_class(logo, "logo");
    gtk_label_set_xalign(GTK_LABEL(logo), 0);
    gtk_box_append(GTK_BOX(txt_logo), logo);

    GtkWidget *sub = gtk_label_new("Gestão de Ambientes");
    gtk_widget_add_css_class(sub, "sublogo");
    gtk_label_set_xalign(GTK_LABEL(sub), 0);
    gtk_box_append(GTK_BOX(txt_logo), sub);
    gtk_box_append(GTK_BOX(cab_logo), txt_logo);
    gtk_box_append(GTK_BOX(menu), cab_logo);

    GtkWidget *primeiro = NULL;
    for (int i = 0; i < 5; i++) {
        GtkWidget *b = gtk_toggle_button_new();
        gtk_widget_add_css_class(b, "menu");

        GtkWidget *conteudo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *ic = gtk_image_new_from_icon_name(ICONES[i]);
        GtkWidget *lbl = gtk_label_new(TITULOS[i]);
        gtk_box_append(GTK_BOX(conteudo), ic);
        gtk_box_append(GTK_BOX(conteudo), lbl);
        gtk_button_set_child(GTK_BUTTON(b), conteudo);

        g_object_set_data(G_OBJECT(b), "tela", (gpointer) NOMES[i]);
        if (primeiro) gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(b), GTK_TOGGLE_BUTTON(primeiro));
        else primeiro = b;
        g_signal_connect(b, "toggled", G_CALLBACK(ao_menu), e);
        e->botoes_menu[i] = b;
        gtk_box_append(GTK_BOX(menu), b);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(primeiro), TRUE);

    GtkWidget *espaco = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(espaco, TRUE);
    gtk_box_append(GTK_BOX(menu), espaco);

    gtk_box_append(GTK_BOX(menu), tema_criar_chave());

    /* Bloco do perfil com avatar */
    GtkWidget *perfil_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(perfil_box, "perfil-box");

    GtkWidget *avatar = gtk_image_new_from_icon_name("avatar-default-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(avatar), 20);
    gtk_widget_add_css_class(avatar, "avatar-perfil");
    gtk_widget_set_valign(avatar, GTK_ALIGN_CENTER);

    GtkWidget *perfil_textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    e->rotulo_nome = gtk_label_new("Leidjane");
    gtk_widget_add_css_class(e->rotulo_nome, "usuario-nome");
    gtk_label_set_xalign(GTK_LABEL(e->rotulo_nome), 0);

    e->rotulo_setor = gtk_label_new("DATP");
    gtk_widget_add_css_class(e->rotulo_setor, "usuario-info");
    gtk_label_set_xalign(GTK_LABEL(e->rotulo_setor), 0);

    GtkWidget *u3 = gtk_label_new("Sistema local");
    gtk_widget_add_css_class(u3, "usuario-info");
    gtk_label_set_xalign(GTK_LABEL(u3), 0);

    gtk_box_append(GTK_BOX(perfil_textos), e->rotulo_nome);
    gtk_box_append(GTK_BOX(perfil_textos), e->rotulo_setor);
    gtk_box_append(GTK_BOX(perfil_textos), u3);

    gtk_box_append(GTK_BOX(perfil_box), avatar);
    gtk_box_append(GTK_BOX(perfil_box), perfil_textos);
    gtk_box_append(GTK_BOX(menu), perfil_box);

    return menu;
}

static void ativar(GtkApplication *app, gpointer dados) {
    Estado *e = g_new0(Estado, 1);
    dados_iniciar();
    estilo_aplicar();
    visual_aplicar();

    e->janela = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(e->janela), "POLI - Gestão de Ambientes");
    gtk_window_set_resizable(GTK_WINDOW(e->janela), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(e->janela), 960, 540);

    e->stack = gtk_stack_new();
    gtk_stack_set_hhomogeneous(GTK_STACK(e->stack), FALSE);
    gtk_stack_set_vhomogeneous(GTK_STACK(e->stack), FALSE);
    gtk_widget_set_hexpand(e->stack, TRUE);
    gtk_widget_set_vexpand(e->stack, TRUE);
    gtk_stack_add_named(GTK_STACK(e->stack), tela_inicio_criar(e), "inicio");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_visao_criar(e), "visao");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_busca_criar(e), "busca");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_reservas_criar(e), "reservas");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_salas_criar(e), "salas");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_nova_reserva_criar(e), "nova");

    /* Barra de status com bolinha verde discreta (sem wi-fi/mensagem) */
    e->barra_status = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(e->barra_status, "barra-status");

    GtkWidget *ponto = gtk_label_new("●");
    gtk_widget_add_css_class(ponto, "status-ponto");

    rotulo_status_global = gtk_label_new("Pronto.");
    gtk_label_set_xalign(GTK_LABEL(rotulo_status_global), 0);
    gtk_widget_set_hexpand(rotulo_status_global, TRUE);

    gtk_box_append(GTK_BOX(e->barra_status), ponto);
    gtk_box_append(GTK_BOX(e->barra_status), rotulo_status_global);

    GtkWidget *direita = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(direita, TRUE);
    gtk_widget_set_vexpand(direita, TRUE);
    gtk_box_append(GTK_BOX(direita), e->stack);
    gtk_box_append(GTK_BOX(direita), e->barra_status);

    GtkWidget *app_raiz = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(app_raiz, TRUE);
    gtk_widget_set_vexpand(app_raiz, TRUE);
    gtk_box_append(GTK_BOX(app_raiz), criar_menu(e));
    gtk_box_append(GTK_BOX(app_raiz), direita);

    gtk_window_set_child(GTK_WINDOW(e->janela), app_raiz);
    gtk_window_get_resizable(GTK_WINDOW(e->janela)), min_w, min_h, nat_w, nat_h);
    gtk_window_present(GTK_WINDOW(e->janela));
}

int interface_iniciar(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("br.upe.datp", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(ativar), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
