#include <gtk/gtk.h>
#include "interface.h"
#include "usuarios.h"

typedef struct {
    Estado *e;
    GtkWidget *stack;
    GtkWidget *main_box;
    GtkWidget *switch_tema;
    GtkCssProvider *css_provider;
    GtkWidget *log_user;
    GtkWidget *log_pass;
    GtkWidget *log_msg;
    GtkWidget *cad_nome;
    GtkWidget *cad_user;
    GtkWidget *cad_setor;
    GtkWidget *cad_pass;
    GtkWidget *cad_pass2;
    GtkWidget *cad_msg;
} DadosTela;

static void ao_mudar_modo_noturno(GtkSwitch *sw, gboolean state, gpointer dados) {
    DadosTela *d = (DadosTela *)dados;
    GdkDisplay *display = gdk_display_get_default();

    if (!d->css_provider) {
        d->css_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(d->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    if (state) {
        // Modo Escuro completo cobrindo janelas, formulários, entradas, botões e listas para não ficar nada branco
        gtk_css_provider_load_from_string(d->css_provider,
            "window { background-color: #1e1f20; color: #e3e3e3; }"
            ".form-salas { background-color: #2d2e30; color: #e3e3e3; border: 1px solid #444746; border-radius: 12px; padding: 16px; }"
            "entry, passwordentry { background-color: #131314; color: #e3e3e3; border-color: #444746; }"
            "button.secundario { background-color: #38393a; color: #e3e3e3; border-color: #444746; }"
            "button.secundario:hover { background-color: #444746; }"
            "list, listview, row { background-color: #2d2e30; color: #e3e3e3; }"
            "label { color: #e3e3e3; }"
        );
    } else {
        // Modo Claro original intacto
        gtk_css_provider_load_from_string(d->css_provider, "");
    }
}

static void ir_para_login(GtkButton *b, gpointer d) {
    (void)b;
    gtk_stack_set_visible_child_name(GTK_STACK(((DadosTela *)d)->stack), "login");
}

static void ir_para_cadastro(GtkButton *b, gpointer d) {
    (void)b;
    gtk_stack_set_visible_child_name(GTK_STACK(((DadosTela *)d)->stack), "cadastro");
}

static void ao_entrar(GtkButton *b, gpointer dados) {
    (void)b;
    DadosTela *d = (DadosTela *)dados;
    const char *user = gtk_editable_get_text(GTK_EDITABLE(d->log_user));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(d->log_pass));

    if (user[0] == '\0' || pass[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(d->log_msg), "Preencha usuário e senha.");
        return;
    }

    if (d->e && d->e->raiz) {
        gtk_stack_set_visible_child_name(GTK_STACK(d->e->raiz), "app");
    }
}

static void ao_cadastrar(GtkButton *b, gpointer dados) {
    (void)b;
    DadosTela *d = (DadosTela *)dados;
    const char *nome = gtk_editable_get_text(GTK_EDITABLE(d->cad_nome));
    const char *user = gtk_editable_get_text(GTK_EDITABLE(d->cad_user));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(d->cad_pass));

    if (nome[0] == '\0' || user[0] == '\0' || pass[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(d->cad_msg), "Preencha os campos obrigatórios.");
        return;
    }

    char msg[128];
    usuarios_cadastrar(user, nome, "DATP", pass, msg, sizeof(msg));
    gtk_stack_set_visible_child_name(GTK_STACK(d->stack), "login");
}

void login_sair(GtkButton *b, gpointer dados) {
    (void)b;
    Estado *e = (Estado *)dados;
    if (e && e->janela) gtk_window_close(GTK_WINDOW(e->janela));
}

GtkWidget *tela_login_criar(Estado *e) {
    DadosTela *d = g_new0(DadosTela, 1);
    d->e = e;

    d->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(d->main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(d->main_box, GTK_ALIGN_CENTER);

    // Barra superior com o Modo Noturno estilo Gemini
    GtkWidget *box_topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(box_topo, GTK_ALIGN_END);

    GtkWidget *lbl_modo = gtk_label_new("Modo Noturno");
    gtk_box_append(GTK_BOX(box_topo), lbl_modo);

    d->switch_tema = gtk_switch_new();
    gtk_widget_set_valign(d->switch_tema, GTK_ALIGN_CENTER);
    g_signal_connect(d->switch_tema, "state-set", G_CALLBACK(ao_mudar_modo_noturno), d);
    gtk_box_append(GTK_BOX(box_topo), d->switch_tema);

    gtk_box_append(GTK_BOX(d->main_box), box_topo);

    d->stack = gtk_stack_new();
    gtk_widget_set_size_request(d->stack, 400, 520);
    gtk_widget_add_css_class(d->stack, "form-salas");

    // --- TELA DE LOGIN ---
    GtkWidget *box_log = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box_log, 28);
    gtk_widget_set_margin_end(box_log, 28);
    gtk_widget_set_margin_top(box_log, 24);
    gtk_widget_set_margin_bottom(box_log, 24);

    GtkWidget *l1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_size_request(l1, 30, 2);
    gtk_box_append(GTK_BOX(box_log), l1);

    GtkWidget *t1 = gtk_label_new("POLI");
    gtk_widget_add_css_class(t1, "titulo");
    gtk_label_set_xalign(GTK_LABEL(t1), 0);
    gtk_box_append(GTK_BOX(box_log), t1);

    GtkWidget *s1 = gtk_label_new("Gestão de Ambientes - DATP");
    gtk_widget_add_css_class(s1, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(s1), 0);
    gtk_box_append(GTK_BOX(box_log), s1);

    GtkWidget *a1 = gtk_label_new("Entrar no sistema");
    gtk_widget_add_css_class(a1, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(a1), 0);
    gtk_box_append(GTK_BOX(box_log), a1);

    d->log_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(d->log_user), "Usuário");
    gtk_box_append(GTK_BOX(box_log), d->log_user);

    d->log_pass = gtk_password_entry_new();
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(d->log_pass), TRUE);
    g_object_set(d->log_pass, "placeholder-text", "Senha", NULL);
    gtk_box_append(GTK_BOX(box_log), d->log_pass);

    GtkWidget *btn_entrar = gtk_button_new_with_label("Entrar");
    gtk_widget_add_css_class(btn_entrar, "primario");
    g_signal_connect(btn_entrar, "clicked", G_CALLBACK(ao_entrar), d);
    gtk_box_append(GTK_BOX(box_log), btn_entrar);

    GtkWidget *btn_ir_cad = gtk_button_new_with_label("Criar novo cadastro");
    gtk_widget_add_css_class(btn_ir_cad, "secundario");
    g_signal_connect(btn_ir_cad, "clicked", G_CALLBACK(ir_para_cadastro), d);
    gtk_box_append(GTK_BOX(box_log), btn_ir_cad);

    d->log_msg = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(d->log_msg), 0.0);
    gtk_box_append(GTK_BOX(box_log), d->log_msg);

    // --- TELA DE CADASTRO ---
    GtkWidget *box_cad = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box_cad, 28);
    gtk_widget_set_margin_end(box_cad, 28);
    gtk_widget_set_margin_top(box_cad, 24);
    gtk_widget_set_margin_bottom(box_cad, 24);

    GtkWidget *l2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_size_request(l2, 30, 2);
    gtk_box_append(GTK_BOX(box_cad), l2);

    GtkWidget *t2 = gtk_label_new("POLI");
    gtk_widget_add_css_class(t2, "titulo");
    gtk_label_set_xalign(GTK_LABEL(t2), 0);
    gtk_box_append(GTK_BOX(box_cad), t2);

    GtkWidget *s2 = gtk_label_new("Gestão de Ambientes - DATP");
    gtk_widget_add_css_class(s2, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(s2), 0);
    gtk_box_append(GTK_BOX(box_cad), s2);

    GtkWidget *a2 = gtk_label_new("Criar cadastro");
    gtk_widget_add_css_class(a2, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(a2), 0);
    gtk_box_append(GTK_BOX(box_cad), a2);

    d->cad_nome = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(d->cad_nome), "Nome completo");
    gtk_box_append(GTK_BOX(box_cad), d->cad_nome);

    d->cad_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(d->cad_user), "Usuário (para entrar)");
    gtk_box_append(GTK_BOX(box_cad), d->cad_user);

    d->cad_setor = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(d->cad_setor), "DATP");
    gtk_box_append(GTK_BOX(box_cad), d->cad_setor);

    d->cad_pass = gtk_password_entry_new();
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(d->cad_pass), TRUE);
    g_object_set(d->cad_pass, "placeholder-text", "Senha (mínimo 6 caracteres)", NULL);
    gtk_box_append(GTK_BOX(box_cad), d->cad_pass);

    d->cad_pass2 = gtk_password_entry_new();
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(d->cad_pass2), TRUE);
    g_object_set(d->cad_pass2, "placeholder-text", "Repetir a senha", NULL);
    gtk_box_append(GTK_BOX(box_cad), d->cad_pass2);

    GtkWidget *btn_cad = gtk_button_new_with_label("Criar cadastro");
    gtk_widget_add_css_class(btn_cad, "primario");
    g_signal_connect(btn_cad, "clicked", G_CALLBACK(ao_cadastrar), d);
    gtk_box_append(GTK_BOX(box_cad), btn_cad);

    GtkWidget *btn_ir_log = gtk_button_new_with_label("Já tenho cadastro");
    gtk_widget_add_css_class(btn_ir_log, "secundario");
    g_signal_connect(btn_ir_log, "clicked", G_CALLBACK(ir_para_login), d);
    gtk_box_append(GTK_BOX(box_cad), btn_ir_log);

    d->cad_msg = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(d->cad_msg), 0.0);
    gtk_box_append(GTK_BOX(box_cad), d->cad_msg);

    // Adiciona ao stack
    gtk_stack_add_named(GTK_STACK(d->stack), box_log, "login");
    gtk_stack_add_named(GTK_STACK(d->stack), box_cad, "cadastro");
    gtk_stack_set_visible_child_name(GTK_STACK(d->stack), "login");

    gtk_box_append(GTK_BOX(d->main_box), d->stack);
    return d->main_box;
}
