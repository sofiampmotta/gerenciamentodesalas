#include "interface.h"
#include <string.h>

#define MAX_SALAS 64

typedef struct {
    char nome[64];
    char local[64];
    int  lugares;
    int  projetor;
    int  acessivel;
} SalaTeste;

typedef struct {
    Estado *e;
    SalaTeste salas[MAX_SALAS];
    int n;
    int selecionado;          /* -1 = cadastrando sala nova */

    GtkWidget *lista;
    GtkWidget *rotulo_total;
    GtkWidget *titulo_form;
    GtkWidget *campo_nome;
    GtkWidget *campo_local;
    GtkWidget *spin_lugares;
    GtkWidget *chk_projetor;
    GtkWidget *chk_acessivel;
} TelaSalas;

static const SalaTeste INICIAIS[] = {
    {"I08", "Bloco I", 35, 1, 1},
    {"B04", "Bloco B", 45, 1, 0},
    {"B03", "Bloco B", 40, 0, 0},
    {"C01", "Bloco C", 30, 1, 0},
    {"A02", "Bloco A", 60, 1, 1},
    {"D05", "Bloco D", 25, 1, 1},
    {"Auditório", "Prédio central", 120, 1, 1}
};

static void aplicar_estilo(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".cabecalho-tabela { padding: 0 16px; }"
        ".cabecalho-tabela label { color: #5b6b8a; font-weight: bold; font-size: 12px; }"
        ".moldura { border: 1px solid #e1e6ef; border-radius: 6px; }"
        ".tabela-salas { background: #ffffff; }"
        ".tabela-salas row { padding: 10px 16px; border-bottom: 1px solid #e1e6ef; }"
        ".tabela-salas row:hover { background: #f4f6f9; }"
        ".tabela-salas row:selected { background: #e1ebf8; }"
        ".tabela-salas row:selected label { color: #12274f; }"
        ".nome-sala { font-weight: bold; }"
        ".form-salas { border: 1px solid #e1e6ef; border-radius: 6px; padding: 20px; }"
        ".titulo-form { font-family: Georgia, serif; font-size: 20px; color: #12274f; }"
        "button.secundario { background: #ffffff; background-image: none; color: #12274f;"
        "  border: 1px solid #1f4e8c; border-radius: 4px; font-weight: bold; padding: 10px 20px; box-shadow: none; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

static GtkWidget *celula(const char *texto, int largura) {
    GtkWidget *l = gtk_label_new(texto);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_widget_set_size_request(l, largura, -1);
    return l;
}

static GtkWidget *linha_tabela(const char *a, const char *b, const char *c,
                               const char *d, const char *f) {
    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(caixa), celula(a, 140));
    gtk_box_append(GTK_BOX(caixa), celula(b, 220));
    gtk_box_append(GTK_BOX(caixa), celula(c, 90));
    gtk_box_append(GTK_BOX(caixa), celula(d, 90));
    gtk_box_append(GTK_BOX(caixa), celula(f, 90));
    return caixa;
}

static void preencher_lista(TelaSalas *t) {
    GtkListBoxRow *r;
    while ((r = gtk_list_box_get_row_at_index(GTK_LIST_BOX(t->lista), 0)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(t->lista), GTK_WIDGET(r));

    for (int i = 0; i < t->n; i++) {
        SalaTeste *s = &t->salas[i];
        char lug[16];
        g_snprintf(lug, sizeof lug, "%d", s->lugares);
        GtkWidget *caixa = linha_tabela(s->nome, s->local, lug,
                                        s->projetor ? "Sim" : "Não",
                                        s->acessivel ? "Sim" : "Não");
        gtk_widget_add_css_class(gtk_widget_get_first_child(caixa), "nome-sala");
        GtkWidget *row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), caixa);
        gtk_list_box_append(GTK_LIST_BOX(t->lista), row);
    }
    char *txt = g_strdup_printf("%d ambientes cadastrados", t->n);
    gtk_label_set_text(GTK_LABEL(t->rotulo_total), txt);
    g_free(txt);
}

static void mostrar_sala(TelaSalas *t, int i) {
    SalaTeste *s = &t->salas[i];
    char *tit = g_strdup_printf("Editar %s", s->nome);
    gtk_label_set_text(GTK_LABEL(t->titulo_form), tit);
    g_free(tit);
    gtk_editable_set_text(GTK_EDITABLE(t->campo_nome), s->nome);
    gtk_editable_set_text(GTK_EDITABLE(t->campo_local), s->local);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->spin_lugares), s->lugares);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(t->chk_projetor), s->projetor);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(t->chk_acessivel), s->acessivel);
}

static void formulario_novo(TelaSalas *t) {
    gtk_label_set_text(GTK_LABEL(t->titulo_form), "Nova sala");
    gtk_editable_set_text(GTK_EDITABLE(t->campo_nome), "");
    gtk_editable_set_text(GTK_EDITABLE(t->campo_local), "");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->spin_lugares), 30);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(t->chk_projetor), FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(t->chk_acessivel), FALSE);
}

static void ao_selecionar(GtkListBox *lista, GtkListBoxRow *row, gpointer dados) {
    TelaSalas *t = (TelaSalas *) dados;
    if (!row) return;
    t->selecionado = gtk_list_box_row_get_index(row);
    mostrar_sala(t, t->selecionado);
}

static void ao_nova(GtkButton *b, gpointer dados) {
    TelaSalas *t = (TelaSalas *) dados;
    gtk_list_box_unselect_all(GTK_LIST_BOX(t->lista));
    t->selecionado = -1;
    formulario_novo(t);
    gtk_widget_grab_focus(t->campo_nome);
}

static void ao_descartar(GtkButton *b, gpointer dados) {
    TelaSalas *t = (TelaSalas *) dados;
    if (t->selecionado >= 0) mostrar_sala(t, t->selecionado);
    else formulario_novo(t);
    interface_mensagem(t->e, "Alterações descartadas. Nada foi salvo.", FALSE);
}

static void ao_salvar(GtkButton *b, gpointer dados) {
    TelaSalas *t = (TelaSalas *) dados;
    char *nome  = g_strstrip(g_strdup(gtk_editable_get_text(GTK_EDITABLE(t->campo_nome))));
    char *local = g_strstrip(g_strdup(gtk_editable_get_text(GTK_EDITABLE(t->campo_local))));
    char *erro = NULL;

    if (nome[0] == '\0') {
        erro = g_strdup("Informe o nome da sala.");
    } else if (local[0] == '\0') {
        erro = g_strdup("Informe o local (bloco ou prédio).");
    } else {
        for (int i = 0; i < t->n; i++) {
            if (i != t->selecionado && g_ascii_strcasecmp(t->salas[i].nome, nome) == 0) {
                erro = g_strdup_printf("Já existe uma sala chamada %s.", nome);
                break;
            }
        }
        if (!erro && t->selecionado < 0 && t->n >= MAX_SALAS)
            erro = g_strdup("Limite de salas atingido.");
    }

    if (erro) {
        interface_mensagem(t->e, erro, TRUE);
        g_free(erro); g_free(nome); g_free(local);
        return;
    }

    int destino = (t->selecionado >= 0) ? t->selecionado : t->n++;
    SalaTeste *s = &t->salas[destino];
    g_strlcpy(s->nome, nome, sizeof s->nome);
    g_strlcpy(s->local, local, sizeof s->local);
    s->lugares   = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->spin_lugares));
    s->projetor  = gtk_check_button_get_active(GTK_CHECK_BUTTON(t->chk_projetor));
    s->acessivel = gtk_check_button_get_active(GTK_CHECK_BUTTON(t->chk_acessivel));

    preencher_lista(t);
    t->selecionado = destino;
    gtk_list_box_select_row(GTK_LIST_BOX(t->lista),
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(t->lista), destino));

    char *msg = g_strdup_printf("Sala %s salva. A lista foi atualizada.", s->nome);
    interface_mensagem(t->e, msg, FALSE);
    g_free(msg); g_free(nome); g_free(local);
}

static GtkWidget *rotulo_campo(const char *texto) {
    GtkWidget *l = gtk_label_new(texto);
    gtk_widget_add_css_class(l, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    return l;
}

GtkWidget *tela_salas_criar(Estado *e) {
    TelaSalas *t = g_new0(TelaSalas, 1);
    t->e = e;
    t->selecionado = -1;
    memcpy(t->salas, INICIAIS, sizeof INICIAIS);
    t->n = G_N_ELEMENTS(INICIAIS);
    aplicar_estilo();

    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(pagina, 28);
    gtk_widget_set_margin_bottom(pagina, 24);
    gtk_widget_set_margin_start(pagina, 32);
    gtk_widget_set_margin_end(pagina, 32);

    /* topo: título, total e botão Nova sala */
    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(textos, TRUE);
    GtkWidget *titulo = gtk_label_new("Salas");
    gtk_widget_add_css_class(titulo, "titulo");
    gtk_label_set_xalign(GTK_LABEL(titulo), 0);
    t->rotulo_total = gtk_label_new("");
    gtk_widget_add_css_class(t->rotulo_total, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(t->rotulo_total), 0);
    gtk_box_append(GTK_BOX(textos), titulo);
    gtk_box_append(GTK_BOX(textos), t->rotulo_total);
    GtkWidget *b_nova = gtk_button_new_with_label("Nova sala");
    gtk_widget_add_css_class(b_nova, "primario");
    gtk_widget_set_valign(b_nova, GTK_ALIGN_START);
    g_signal_connect(b_nova, "clicked", G_CALLBACK(ao_nova), t);
    gtk_box_append(GTK_BOX(topo), textos);
    gtk_box_append(GTK_BOX(topo), b_nova);
    gtk_box_append(GTK_BOX(pagina), topo);

    /* tabela */
    GtkWidget *cab = linha_tabela("Sala", "Local", "Lugares", "Projetor", "Acessível");
    gtk_widget_add_css_class(cab, "cabecalho-tabela");
    gtk_box_append(GTK_BOX(pagina), cab);

    t->lista = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(t->lista), GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class(t->lista, "tabela-salas");
    GtkWidget *rolagem = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), t->lista);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(rolagem), 220);
    gtk_widget_set_vexpand(rolagem, TRUE);
    gtk_widget_add_css_class(rolagem, "moldura");
    gtk_box_append(GTK_BOX(pagina), rolagem);

    /* formulário */
    GtkWidget *form = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(form, "form-salas");
    t->titulo_form = gtk_label_new("Nova sala");
    gtk_widget_add_css_class(t->titulo_form, "titulo-form");
    gtk_label_set_xalign(GTK_LABEL(t->titulo_form), 0);
    gtk_box_append(GTK_BOX(form), t->titulo_form);

    t->campo_nome = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(t->campo_nome), 30);
    t->campo_local = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(t->campo_local), 30);
    gtk_widget_set_hexpand(t->campo_local, TRUE);
    t->spin_lugares = gtk_spin_button_new_with_range(1, 1000, 1);
    t->chk_projetor = gtk_check_button_new_with_label("Projetor");
    t->chk_acessivel = gtk_check_button_new_with_label("Acessível (rampa, térreo ou elevador)");

    GtkWidget *grade = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grade), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grade), 6);
    gtk_grid_attach(GTK_GRID(grade), rotulo_campo("Nome"),    0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo_campo("Local"),   1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo_campo("Lugares"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->campo_nome,    0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->campo_local,   1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->spin_lugares,  2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo_campo("Recursos"), 0, 2, 3, 1);
    GtkWidget *recursos = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_box_append(GTK_BOX(recursos), t->chk_projetor);
    gtk_box_append(GTK_BOX(recursos), t->chk_acessivel);
    gtk_grid_attach(GTK_GRID(grade), recursos, 0, 3, 3, 1);
    gtk_box_append(GTK_BOX(form), grade);

    GtkWidget *botoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *b_salvar = gtk_button_new_with_label("Salvar sala");
    gtk_widget_add_css_class(b_salvar, "primario");
    g_signal_connect(b_salvar, "clicked", G_CALLBACK(ao_salvar), t);
    GtkWidget *b_desc = gtk_button_new_with_label("Descartar");
    gtk_widget_add_css_class(b_desc, "secundario");
    g_signal_connect(b_desc, "clicked", G_CALLBACK(ao_descartar), t);
    gtk_box_append(GTK_BOX(botoes), b_salvar);
    gtk_box_append(GTK_BOX(botoes), b_desc);
    gtk_box_append(GTK_BOX(form), botoes);
    gtk_box_append(GTK_BOX(pagina), form);

    g_signal_connect(t->lista, "row-selected", G_CALLBACK(ao_selecionar), t);
    preencher_lista(t);
    gtk_list_box_select_row(GTK_LIST_BOX(t->lista),
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(t->lista), 0));

    return pagina;
}
