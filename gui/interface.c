#include "interface.h"
#ifdef _WIN32
#include <windows.h>
#include <gdk/win32/gdkwin32.h>
#endif


static const char *NOMES[5]   = { "inicio", "visao", "busca", "reservas", "salas" };
static const char *TITULOS[5] = { "Início", "Visão geral", "Buscar salas", "Reservas", "Salas" };

static guint g_timer_status = 0;

static gboolean limpar_mensagem_status(gpointer dados) {
    GtkWidget *barra = GTK_WIDGET(dados);
    if (GTK_IS_LABEL(barra)) {
        gtk_label_set_text(GTK_LABEL(barra), "");
        gtk_widget_remove_css_class(barra, "error");
        gtk_widget_remove_css_class(barra, "success");
        gtk_widget_set_visible(barra, FALSE);
    }
    g_timer_status = 0;
    return G_SOURCE_REMOVE;
}

void interface_mensagem(Estado *e, const char *texto, gboolean erro) {
    if (g_timer_status > 0) {
        g_source_remove(g_timer_status);
        g_timer_status = 0;
    }

    gtk_label_set_text(GTK_LABEL(e->barra_status), texto);
    gtk_widget_remove_css_class(e->barra_status, "error");
    gtk_widget_remove_css_class(e->barra_status, "success");
    gtk_widget_add_css_class(e->barra_status, erro ? "error" : "success");
    gtk_widget_set_visible(e->barra_status, TRUE);

    g_timer_status = g_timeout_add_seconds(3, limpar_mensagem_status, e->barra_status);
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
    gtk_widget_set_hexpand(menu, FALSE);
    gtk_widget_set_size_request(menu, 330, -1);

    GtkWidget *faixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(faixa, "faixa-vermelha");
    gtk_widget_set_halign(faixa, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(menu), faixa);

    gtk_box_append(GTK_BOX(menu), logo_poli_criar());

    GtkWidget *tag_sistema = gtk_label_new("gerenciamento de salas");
    gtk_widget_add_css_class(tag_sistema, "tag-sistema");
    gtk_label_set_xalign(GTK_LABEL(tag_sistema), 0);
    gtk_widget_set_halign(tag_sistema, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(menu), tag_sistema);

    GtkWidget *primeiro = NULL;
    for (int i = 0; i < 5; i++) {
        GtkWidget *b = gtk_toggle_button_new_with_label(TITULOS[i]);
        gtk_widget_add_css_class(b, "menu");
        gtk_widget_set_hexpand(b, TRUE);
        gtk_widget_set_halign(gtk_button_get_child(GTK_BUTTON(b)), GTK_ALIGN_START);
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

    GtkWidget *rodape = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_bottom(rodape, 18);

    GtkWidget *chave_tema = tema_criar_chave();
    gtk_box_append(GTK_BOX(rodape), chave_tema);

    GtkWidget *caixa_usuario = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(caixa_usuario, 14);
    gtk_widget_set_margin_end(caixa_usuario, 14);

    e->rotulo_nome = gtk_label_new("Leidjane");
    gtk_widget_add_css_class(e->rotulo_nome, "usuario-nome");
    gtk_label_set_xalign(GTK_LABEL(e->rotulo_nome), 0);

    e->rotulo_setor = gtk_label_new("DATP");
    gtk_widget_add_css_class(e->rotulo_setor, "usuario-info");
    gtk_label_set_xalign(GTK_LABEL(e->rotulo_setor), 0);

    GtkWidget *u3 = gtk_label_new("Sistema local");
    gtk_widget_add_css_class(u3, "usuario-info");
    gtk_label_set_xalign(GTK_LABEL(u3), 0);

    gtk_box_append(GTK_BOX(caixa_usuario), e->rotulo_nome);
    gtk_box_append(GTK_BOX(caixa_usuario), e->rotulo_setor);
    gtk_box_append(GTK_BOX(caixa_usuario), u3);
    gtk_box_append(GTK_BOX(rodape), caixa_usuario);

    gtk_box_append(GTK_BOX(menu), rodape);

    return menu;
}


static gboolean ao_atalho_minimizar(GtkWidget *widget, GVariant *args, gpointer dados) {
    (void)widget;
    (void)args;
    GtkWindow *win = GTK_WINDOW(dados);
    g_print("Acionando gtk_window_minimize...\n");
    gtk_window_minimize(win);
    return TRUE;
}


static void ao_notificar_minimizado(GObject *obj, GParamSpec *pspec, gpointer dados) {
    (void)pspec;
    (void)dados;
    GtkWindow *win = GTK_WINDOW(obj);
    GtkNative *native = gtk_widget_get_native(GTK_WIDGET(win));
    if (!native) return;
    GdkSurface *surface = gtk_native_get_surface(native);
    if (!surface || !GDK_IS_TOPLEVEL(surface)) return;

    if (gdk_toplevel_get_state(GDK_TOPLEVEL(surface)) & GDK_TOPLEVEL_STATE_MINIMIZED) {
#ifdef _WIN32
        if (GDK_IS_WIN32_SURFACE(surface)) {
            HWND hwnd = (HWND)gdk_win32_surface_get_handle(GDK_WIN32_SURFACE(surface));
            if (hwnd) {
                ShowWindow(hwnd, SW_MINIMIZE);
            }
        }
#endif
    }
}

static void ativar(GtkApplication *app, gpointer dados) {
    Estado *e = g_new0(Estado, 1);
    dados_iniciar();
    estilo_aplicar();

    e->janela = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(e->janela), "POLI - Gestão de Ambientes");
    gtk_window_set_resizable(GTK_WINDOW(e->janela), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(e->janela), 1024, 650);

    e->stack = gtk_stack_new();
    gtk_stack_set_hhomogeneous(GTK_STACK(e->stack), FALSE);
    gtk_stack_set_vhomogeneous(GTK_STACK(e->stack), FALSE);
    gtk_widget_set_hexpand(e->stack, TRUE);
    gtk_widget_set_vexpand(e->stack, TRUE);
    gtk_stack_add_named(GTK_STACK(e->stack), tela_inicio_criar(e),       "inicio");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_visao_criar(e),        "visao");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_busca_criar(e),        "busca");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_reservas_criar(e),     "reservas");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_salas_criar(e),        "salas");
    gtk_stack_add_named(GTK_STACK(e->stack), tela_nova_reserva_criar(e), "nova");

    e->barra_status = gtk_label_new("");
    gtk_widget_add_css_class(e->barra_status, "barra-status");
    gtk_widget_set_visible(e->barra_status, FALSE);
    gtk_label_set_xalign(GTK_LABEL(e->barra_status), 0);

    GtkWidget *rol_stack = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rol_stack),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(rol_stack), 1);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(rol_stack), 1);
    gtk_widget_set_hexpand(rol_stack, TRUE);
    gtk_widget_set_vexpand(rol_stack, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rol_stack), e->stack);

    GtkWidget *direita = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(direita, TRUE);
    gtk_widget_set_vexpand(direita, TRUE);
    gtk_box_append(GTK_BOX(direita), rol_stack);
    gtk_box_append(GTK_BOX(direita), e->barra_status);

    GtkWidget *app_raiz = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(app_raiz), criar_menu(e));
    gtk_box_append(GTK_BOX(app_raiz), direita);

    e->raiz = app_raiz;
    gtk_window_set_child(GTK_WINDOW(e->janela), e->raiz);
    GtkEventController *key_ctrl = gtk_shortcut_controller_new();
    GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string("<Control>m");
    GtkShortcutAction *action = gtk_callback_action_new(ao_atalho_minimizar, e->janela, NULL);
    GtkShortcut *shortcut = gtk_shortcut_new(trigger, action);
    gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(key_ctrl), shortcut);
    gtk_widget_add_controller(e->janela, key_ctrl);
    gtk_window_present(GTK_WINDOW(e->janela));
    g_signal_connect(e->janela, "notify::minimized", G_CALLBACK(ao_notificar_minimizado), NULL);
    interface_mensagem(e, "POLI - Gestão de Ambientes", FALSE);
}

int interface_iniciar(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("br.upe.datp", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(ativar), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
