#include "interface.h"
#include "usuarios.h"
#include <string.h>

typedef struct {
    Estado *e;
    GtkWidget *modos;       /* "entrar" ou "cadastrar" */
    GtkWidget *aviso;
    GtkWidget *l_login, *l_senha;
    GtkWidget *c_nome, *c_login, *c_setor, *c_senha, *c_senha2;
} TelaLogin;

static void estilo_login(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".login-fundo { background: #f4f6f9; }"
        ".cartao-login { background: #ffffff; border: 1px solid #e1e6ef; border-radius: 8px; padding: 32px 36px; min-width: 380px; }"
        ".faixa-cartao { background: #c8102e; min-width: 28px; min-height: 3px; margin-bottom: 10px; }"
        ".logo-cartao { font-family: Georgia, serif; font-size: 30px; color: #12274f; }"
        ".titulo-login { font-family: Georgia, serif; font-size: 22px; color: #12274f; margin-top: 14px; margin-bottom: 4px; }"
        ".aviso-erro { color: #c8102e; font-weight: bold; }"
        ".aviso-ok { color: #1e7e4a; font-weight: bold; }"
        "button.link { background: none; background-image: none; border: none; box-shadow: none; color: #1f4e8c; padding: 4px; }"
        "button.link:hover { text-decoration: underline; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

static void entrar(Estado *e, const Usuario *u) {
    gtk_label_set_text(GTK_LABEL(e->rotulo_nome), u->nome);
    gtk_label_set_text(GTK_LABEL(e->rotulo_setor), u->setor);
    gtk_stack_set_visible_child_name(GTK_STACK(e->raiz), "app");
    char *m = g_strdup_printf("Bem-vindo(a), %s.", u->nome);
    interface_mensagem(e, m, FALSE);
    g_free(m);
}

void login_sair(GtkButton *b, gpointer dados) {
    Estado *e = (Estado *) dados;
    gtk_label_set_text(GTK_LABEL(e->rotulo_nome), "");
    gtk_label_set_text(GTK_LABEL(e->rotulo_setor), "");
    gtk_stack_set_visible_child_name(GTK_STACK(e->raiz), "login");
}

static void aviso(TelaLogin *t, const char *texto, gboolean erro) {
    gtk_label_set_text(GTK_LABEL(t->aviso), texto);
    gtk_widget_remove_css_class(t->aviso, "aviso-erro");
    gtk_widget_remove_css_class(t->aviso, "aviso-ok");
    gtk_widget_add_css_class(t->aviso, erro ? "aviso-erro" : "aviso-ok");
}

static GtkWidget *rotulo(const char *txt) {
    GtkWidget *l = gtk_label_new(txt);
    gtk_widget_add_css_class(l, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    return l;
}

static GtkWidget *campo_senha(void) {
    GtkWidget *p = gtk_password_entry_new();
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(p), TRUE);
    return p;
}

static void limpar_campos(TelaLogin *t) {
    GtkWidget *todos[] = { t->l_login, t->l_senha, t->c_nome, t->c_login, t->c_senha, t->c_senha2 };
    for (int i = 0; i < 6; i++) gtk_editable_set_text(GTK_EDITABLE(todos[i]), "");
    gtk_editable_set_text(GTK_EDITABLE(t->c_setor), "DATP");
}

static void ao_entrar(GtkWidget *w, gpointer dados) {
    TelaLogin *t = (TelaLogin *) dados;
    char msg[160];
    Usuario u;
    const char *login = gtk_editable_get_text(GTK_EDITABLE(t->l_login));
    const char *senha = gtk_editable_get_text(GTK_EDITABLE(t->l_senha));
    if (usuarios_entrar(login, senha, &u, msg, sizeof msg) != 0) {
        aviso(t, msg, TRUE);
        return;
    }
    aviso(t, "", FALSE);
    entrar(t->e, &u);
}

static void ao_cadastrar(GtkWidget *w, gpointer dados) {
    TelaLogin *t = (TelaLogin *) dados;
    char msg[200];
    const char *senha  = gtk_editable_get_text(GTK_EDITABLE(t->c_senha));
    const char *senha2 = gtk_editable_get_text(GTK_EDITABLE(t->c_senha2));
    if (strcmp(senha, senha2) != 0) {
        aviso(t, "As duas senhas não são iguais.", TRUE);
        return;
    }
    if (usuarios_cadastrar(gtk_editable_get_text(GTK_EDITABLE(t->c_login)),
                           gtk_editable_get_text(GTK_EDITABLE(t->c_nome)),
                           gtk_editable_get_text(GTK_EDITABLE(t->c_setor)),
                           senha, msg, sizeof msg) != 0) {
        aviso(t, msg, TRUE);
        return;
    }
    char *login = g_strdup(gtk_editable_get_text(GTK_EDITABLE(t->c_login)));
    limpar_campos(t);
    gtk_editable_set_text(GTK_EDITABLE(t->l_login), login);
    g_free(login);
    gtk_stack_set_visible_child_name(GTK_STACK(t->modos), "entrar");
    gtk_widget_grab_focus(t->l_senha);
    aviso(t, "Cadastro criado. Entre com seu usuário e senha.", FALSE);
}

static void ir_cadastrar(GtkButton *b, gpointer dados) {
    TelaLogin *t = (TelaLogin *) dados;
    aviso(t, "", FALSE);
    gtk_stack_set_visible_child_name(GTK_STACK(t->modos), "cadastrar");
}

static void ir_entrar(GtkButton *b, gpointer dados) {
    TelaLogin *t = (TelaLogin *) dados;
    aviso(t, "", FALSE);
    gtk_stack_set_visible_child_name(GTK_STACK(t->modos), "entrar");
}

/* roda toda vez que o login aparece (abrir o programa ou clicar em Sair) */
static void ao_mostrar(GtkWidget *w, gpointer dados) {
    TelaLogin *t = (TelaLogin *) dados;
    limpar_campos(t);
    if (usuarios_existe_algum()) {
        gtk_stack_set_visible_child_name(GTK_STACK(t->modos), "entrar");
        aviso(t, "", FALSE);
        gtk_widget_grab_focus(t->l_login);
    } else {
        gtk_stack_set_visible_child_name(GTK_STACK(t->modos), "cadastrar");
        aviso(t, "Primeiro acesso: crie o cadastro de quem vai usar o sistema.", FALSE);
    }
}

static GtkWidget *pagina_entrar(TelaLogin *t) {
    GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *h = gtk_label_new("Entrar");
    gtk_widget_add_css_class(h, "titulo-login");
    gtk_label_set_xalign(GTK_LABEL(h), 0);
    gtk_box_append(GTK_BOX(c), h);

    t->l_login = gtk_entry_new();
    t->l_senha = campo_senha();
    g_signal_connect(t->l_login, "activate", G_CALLBACK(ao_entrar), t);
    g_signal_connect(t->l_senha, "activate", G_CALLBACK(ao_entrar), t);

    gtk_box_append(GTK_BOX(c), rotulo("Usuário"));
    gtk_box_append(GTK_BOX(c), t->l_login);
    gtk_box_append(GTK_BOX(c), rotulo("Senha"));
    gtk_box_append(GTK_BOX(c), t->l_senha);

    GtkWidget *b = gtk_button_new_with_label("Entrar");
    gtk_widget_add_css_class(b, "primario");
    gtk_widget_set_margin_top(b, 10);
    g_signal_connect(b, "clicked", G_CALLBACK(ao_entrar), t);
    gtk_box_append(GTK_BOX(c), b);

    GtkWidget *l = gtk_button_new_with_label("Primeiro acesso? Criar cadastro");
    gtk_widget_add_css_class(l, "link");
    g_signal_connect(l, "clicked", G_CALLBACK(ir_cadastrar), t);
    gtk_box_append(GTK_BOX(c), l);
    return c;
}

static GtkWidget *pagina_cadastrar(TelaLogin *t) {
    GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *h = gtk_label_new("Criar cadastro");
    gtk_widget_add_css_class(h, "titulo-login");
    gtk_label_set_xalign(GTK_LABEL(h), 0);
    gtk_box_append(GTK_BOX(c), h);

    t->c_nome   = gtk_entry_new();
    t->c_login  = gtk_entry_new();
    t->c_setor  = gtk_entry_new();
    t->c_senha  = campo_senha();
    t->c_senha2 = campo_senha();
    g_signal_connect(t->c_senha2, "activate", G_CALLBACK(ao_cadastrar), t);

    gtk_box_append(GTK_BOX(c), rotulo("Nome completo"));
    gtk_box_append(GTK_BOX(c), t->c_nome);
    gtk_box_append(GTK_BOX(c), rotulo("Usuário (para entrar)"));
    gtk_box_append(GTK_BOX(c), t->c_login);
    gtk_box_append(GTK_BOX(c), rotulo("Setor"));
    gtk_box_append(GTK_BOX(c), t->c_setor);
    gtk_box_append(GTK_BOX(c), rotulo("Senha (mínimo 6 caracteres)"));
    gtk_box_append(GTK_BOX(c), t->c_senha);
    gtk_box_append(GTK_BOX(c), rotulo("Repita a senha"));
    gtk_box_append(GTK_BOX(c), t->c_senha2);

    GtkWidget *b = gtk_button_new_with_label("Criar cadastro");
    gtk_widget_add_css_class(b, "primario");
    gtk_widget_set_margin_top(b, 10);
    g_signal_connect(b, "clicked", G_CALLBACK(ao_cadastrar), t);
    gtk_box_append(GTK_BOX(c), b);

    GtkWidget *l = gtk_button_new_with_label("Já tenho cadastro");
    gtk_widget_add_css_class(l, "link");
    g_signal_connect(l, "clicked", G_CALLBACK(ir_entrar), t);
    gtk_box_append(GTK_BOX(c), l);
    return c;
}

GtkWidget *tela_login_criar(Estado *e) {
    TelaLogin *t = g_new0(TelaLogin, 1);
    t->e = e;
    estilo_login();

    GtkWidget *fundo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(fundo, "login-fundo");
    gtk_widget_set_hexpand(fundo, TRUE);
    gtk_widget_set_vexpand(fundo, TRUE);

    GtkWidget *cartao = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(cartao, "cartao-login");
    gtk_widget_set_halign(cartao, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(cartao, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(cartao, TRUE);

    GtkWidget *faixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(faixa, "faixa-cartao");
    gtk_widget_set_halign(faixa, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(cartao), faixa);

    GtkWidget *logo = gtk_label_new("POLI");
    gtk_widget_add_css_class(logo, "logo-cartao");
    gtk_label_set_xalign(GTK_LABEL(logo), 0);
    gtk_box_append(GTK_BOX(cartao), logo);

    GtkWidget *sub = gtk_label_new("Gestão de Ambientes · DATP");
    gtk_widget_add_css_class(sub, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(sub), 0);
    gtk_box_append(GTK_BOX(cartao), sub);

    t->modos = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(t->modos), pagina_entrar(t), "entrar");
    gtk_stack_add_named(GTK_STACK(t->modos), pagina_cadastrar(t), "cadastrar");
    gtk_box_append(GTK_BOX(cartao), t->modos);

    t->aviso = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(t->aviso), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(t->aviso), 42);
    gtk_label_set_xalign(GTK_LABEL(t->aviso), 0);
    gtk_widget_set_margin_top(t->aviso, 8);
    gtk_box_append(GTK_BOX(cartao), t->aviso);

    gtk_box_append(GTK_BOX(fundo), cartao);
    g_signal_connect(fundo, "map", G_CALLBACK(ao_mostrar), t);
    return fundo;
}
