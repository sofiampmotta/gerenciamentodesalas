#include "interface.h"
#include <string.h>
#include <stdlib.h>

/* PROVISÓRIO: "hoje" e "agora" fixos, como no protótipo (sexta, 13/11, 10:30).
   Na integração, trocar pelo relógio do computador. */
#define HOJE_TESTE   DATA_PADRAO
#define AGORA_TESTE  (10 * 60 + 30)

typedef struct {
    Estado *e;
    GtkWidget *titulo, *sub, *cartao_conf;
    GtkWidget *n_livres, *n_uso, *n_ocup, *n_conf;
    GtkWidget *lista_livres, *lista_prox;
} TelaInicio;

typedef struct { int sala; int ate; } Livre;

static const char *DIAS[]  = { "", "Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado", "Domingo" };
static const char *MESES[] = { "", "janeiro", "fevereiro", "março", "abril", "maio", "junho",
                               "julho", "agosto", "setembro", "outubro", "novembro", "dezembro" };

static void estilo_inicio(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".cartao-info { border: 1px solid #e1e6ef; border-radius: 6px; padding: 14px 18px; min-width: 150px; }"
        ".cartao-conflito { border: 1px solid #c8102e; }"
        ".numero-grande { font-family: Georgia, serif; font-size: 30px; font-weight: bold; }"
        ".secao { font-family: Georgia, serif; font-size: 18px; margin-top: 8px; margin-bottom: 4px; }"
        ".linha-info { padding: 8px 0; border-bottom: 1px solid #e1e6ef; }"
        ".negrito { font-weight: bold; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

static void limpar_caixa(GtkWidget *caixa) {
    GtkWidget *f;
    while ((f = gtk_widget_get_first_child(caixa)) != NULL)
        gtk_box_remove(GTK_BOX(caixa), f);
}

static GtkWidget *cartao(const char *legenda, GtkWidget **numero) {
    GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(c, "cartao-info");
    gtk_widget_set_hexpand(c, TRUE);
    *numero = gtk_label_new("0");
    gtk_widget_add_css_class(*numero, "numero-grande");
    gtk_label_set_xalign(GTK_LABEL(*numero), 0);
    GtkWidget *l = gtk_label_new(legenda);
    gtk_widget_add_css_class(l, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_append(GTK_BOX(c), *numero);
    gtk_box_append(GTK_BOX(c), l);
    return c;
}

static GtkWidget *secao(const char *titulo, GtkWidget **lista) {
    GtkWidget *coluna = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand(coluna, TRUE);
    GtkWidget *t = gtk_label_new(titulo);
    gtk_widget_add_css_class(t, "secao");
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_box_append(GTK_BOX(coluna), t);
    *lista = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(coluna), *lista);
    return coluna;
}

static GtkWidget *linha_info(const char *a, const char *b, const char *c) {
    GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_add_css_class(caixa, "linha-info");

    GtkWidget *la = gtk_label_new(a);
    gtk_label_set_xalign(GTK_LABEL(la), 0);
    gtk_widget_set_size_request(la, 80, -1);
    gtk_widget_add_css_class(la, "negrito");

    GtkWidget *lb = gtk_label_new(b);
    gtk_label_set_xalign(GTK_LABEL(lb), 0);
    gtk_widget_set_hexpand(lb, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(lb), PANGO_ELLIPSIZE_END);

    GtkWidget *lc = gtk_label_new(c);
    gtk_label_set_xalign(GTK_LABEL(lc), 1);
    gtk_widget_add_css_class(lc, "subtitulo");

    gtk_box_append(GTK_BOX(caixa), la);
    gtk_box_append(GTK_BOX(caixa), lb);
    gtk_box_append(GTK_BOX(caixa), lc);
    return caixa;
}

static void numero(GtkWidget *l, int n) {
    char buf[16];
    g_snprintf(buf, sizeof buf, "%d", n);
    gtk_label_set_text(GTK_LABEL(l), buf);
}

static int cmp_livre(const void *pa, const void *pb) {
    const Livre *a = (const Livre *) pa, *b = (const Livre *) pb;
    if (a->ate != b->ate) return b->ate - a->ate;     /* livre por mais tempo primeiro */
    return a->sala - b->sala;
}

static int cmp_prox(const void *pa, const void *pb) {
    int a = *(const int *) pa, b = *(const int *) pb;
    if (g_reservas[a].ini != g_reservas[b].ini) return g_reservas[a].ini - g_reservas[b].ini;
    return a - b;
}

static void atualizar(TelaInicio *t) {
    int hoje = HOJE_TESTE, agora = AGORA_TESTE;

    /* saudação e data */
    const char *nome = t->e->rotulo_nome ? gtk_label_get_text(GTK_LABEL(t->e->rotulo_nome)) : "";
    gchar **partes = g_strsplit(nome, " ", 2);
    const char *primeiro = (partes[0] != NULL) ? partes[0] : "";
    const char *saud = agora < 12 * 60 ? "Bom dia" : (agora < 18 * 60 ? "Boa tarde" : "Boa noite");
    char *tit = primeiro[0] ? g_strdup_printf("%s, %s", saud, primeiro) : g_strdup(saud);
    gtk_label_set_text(GTK_LABEL(t->titulo), tit);
    g_free(tit);
    g_strfreev(partes);

    GDate *d = g_date_new_dmy(hoje % 100, (hoje / 100) % 100, hoje / 10000);
    int wd = g_date_get_weekday(d);
    g_date_free(d);
    char *data = g_strdup_printf("%s, %d de %s, %02d:%02d", DIAS[wd], hoje % 100,
                                 MESES[(hoje / 100) % 100], agora / 60, agora % 60);
    gtk_label_set_text(GTK_LABEL(t->sub), data);
    g_free(data);

    /* salas livres e em uso agora */
    Livre livres[MAX_SALAS];
    int nl = 0, nuso = 0;
    for (int s = 0; s < g_n_salas; s++) {
        int ocupada = 0, ate = 24 * 60;
        for (int i = 0; i < g_n_reservas; i++) {
            const ReservaTeste *r = &g_reservas[i];
            if (r->dia != hoje || strcmp(r->sala, g_salas[s].nome) != 0) continue;
            if (r->ini <= agora && agora < r->fim) ocupada = 1;
            else if (r->ini > agora && r->ini < ate) ate = r->ini;
        }
        if (ocupada) nuso++;
        else { livres[nl].sala = s; livres[nl].ate = ate; nl++; }
    }
    qsort(livres, nl, sizeof(Livre), cmp_livre);

    /* ocupações de hoje e conflitos */
    int ocup = 0, conflitos = 0;
    for (int i = 0; i < g_n_reservas; i++) {
        if (g_reservas[i].dia != hoje) continue;
        ocup++;
        for (int j = i + 1; j < g_n_reservas; j++) {
            if (g_reservas[j].dia != hoje || strcmp(g_reservas[i].sala, g_reservas[j].sala) != 0) continue;
            if (g_reservas[i].ini < g_reservas[j].fim && g_reservas[j].ini < g_reservas[i].fim) conflitos++;
        }
    }

    numero(t->n_livres, nl);
    numero(t->n_uso, nuso);
    numero(t->n_ocup, ocup);
    numero(t->n_conf, conflitos);
    if (conflitos > 0) gtk_widget_add_css_class(t->cartao_conf, "cartao-conflito");
    else gtk_widget_remove_css_class(t->cartao_conf, "cartao-conflito");

    /* lista "Livres agora" */
    limpar_caixa(t->lista_livres);
    if (nl == 0) {
        GtkWidget *l = gtk_label_new("Nenhuma sala livre agora.");
        gtk_widget_add_css_class(l, "subtitulo");
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_box_append(GTK_BOX(t->lista_livres), l);
    }
    for (int k = 0; k < nl; k++) {
        const SalaTeste *sl = &g_salas[livres[k].sala];
        char lug[24], ate[40], h[8];
        g_snprintf(lug, sizeof lug, "%d lugares", sl->lugares);
        if (livres[k].ate >= 24 * 60) {
            g_strlcpy(ate, "Livre o resto do dia", sizeof ate);
        } else {
            dados_hora_texto(livres[k].ate, h, sizeof h);
            g_snprintf(ate, sizeof ate, "Livre até %s", h);
        }
        gtk_box_append(GTK_BOX(t->lista_livres), linha_info(sl->nome, lug, ate));
    }

    /* lista "Próximas ocupações" */
    int prox[MAX_RESERVAS], np = 0;
    for (int i = 0; i < g_n_reservas; i++)
        if (g_reservas[i].dia == hoje && g_reservas[i].ini > agora) prox[np++] = i;
    qsort(prox, np, sizeof(int), cmp_prox);

    limpar_caixa(t->lista_prox);
    if (np == 0) {
        GtkWidget *l = gtk_label_new("Nada mais para hoje.");
        gtk_widget_add_css_class(l, "subtitulo");
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_box_append(GTK_BOX(t->lista_prox), l);
    }
    for (int k = 0; k < np && k < 4; k++) {
        const ReservaTeste *r = &g_reservas[prox[k]];
        char h[8];
        dados_hora_texto(r->ini, h, sizeof h);
        gtk_box_append(GTK_BOX(t->lista_prox), linha_info(h, r->atividade, r->sala));
    }
}

static void ao_mostrar(GtkWidget *w, gpointer dados) { atualizar((TelaInicio *) dados); }
static void ao_nova(GtkButton *b, gpointer d)    { interface_ir_para(((TelaInicio *) d)->e, "nova"); }
static void ao_buscar(GtkButton *b, gpointer d)  { interface_ir_para(((TelaInicio *) d)->e, "busca"); }
static void ao_grade(GtkButton *b, gpointer d)   { interface_ir_para(((TelaInicio *) d)->e, "visao"); }
static void ao_exportar(GtkButton *b, gpointer d) {
    interface_mensagem(((TelaInicio *) d)->e,
        "Exportar o dia: será ligado ao módulo de persistência (Laura).", FALSE);
}

GtkWidget *tela_inicio_criar(Estado *e) {
    TelaInicio *t = g_new0(TelaInicio, 1);
    t->e = e;
    estilo_inicio();

    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(pagina, 28);
    gtk_widget_set_margin_bottom(pagina, 24);
    gtk_widget_set_margin_start(pagina, 32);
    gtk_widget_set_margin_end(pagina, 32);

    /* topo */
    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(textos, TRUE);
    t->titulo = gtk_label_new("");
    gtk_widget_add_css_class(t->titulo, "titulo");
    gtk_label_set_xalign(GTK_LABEL(t->titulo), 0);
    t->sub = gtk_label_new("");
    gtk_widget_add_css_class(t->sub, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(t->sub), 0);
    gtk_box_append(GTK_BOX(textos), t->titulo);
    gtk_box_append(GTK_BOX(textos), t->sub);
    GtkWidget *b_nova = gtk_button_new_with_label("Nova reserva");
    gtk_widget_add_css_class(b_nova, "primario");
    gtk_widget_set_valign(b_nova, GTK_ALIGN_START);
    g_signal_connect(b_nova, "clicked", G_CALLBACK(ao_nova), t);
    gtk_box_append(GTK_BOX(topo), textos);
    gtk_box_append(GTK_BOX(topo), b_nova);
    gtk_box_append(GTK_BOX(pagina), topo);

    /* cartões */
    GtkWidget *cartoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_set_homogeneous(GTK_BOX(cartoes), TRUE);
    gtk_box_append(GTK_BOX(cartoes), cartao("salas livres agora", &t->n_livres));
    gtk_box_append(GTK_BOX(cartoes), cartao("salas em uso", &t->n_uso));
    gtk_box_append(GTK_BOX(cartoes), cartao("ocupações hoje", &t->n_ocup));
    t->cartao_conf = cartao("conflitos a resolver", &t->n_conf);
    gtk_box_append(GTK_BOX(cartoes), t->cartao_conf);
    gtk_box_append(GTK_BOX(pagina), cartoes);

    /* duas listas lado a lado */
    GtkWidget *colunas = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 32);
    gtk_box_set_homogeneous(GTK_BOX(colunas), TRUE);
    gtk_box_append(GTK_BOX(colunas), secao("Livres agora", &t->lista_livres));
    gtk_box_append(GTK_BOX(colunas), secao("Próximas ocupações", &t->lista_prox));
    gtk_box_append(GTK_BOX(pagina), colunas);

    /* ações rápidas */
    GtkWidget *tit_acoes = gtk_label_new("Ações rápidas");
    gtk_widget_add_css_class(tit_acoes, "secao");
    gtk_label_set_xalign(GTK_LABEL(tit_acoes), 0);
    gtk_box_append(GTK_BOX(pagina), tit_acoes);

    GtkWidget *acoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *a1 = gtk_button_new_with_label("Buscar uma sala");
    GtkWidget *a2 = gtk_button_new_with_label("Ver a grade");
    GtkWidget *a3 = gtk_button_new_with_label("Exportar o dia");
    gtk_widget_add_css_class(a1, "secundario");
    gtk_widget_add_css_class(a2, "secundario");
    gtk_widget_add_css_class(a3, "secundario");
    g_signal_connect(a1, "clicked", G_CALLBACK(ao_buscar), t);
    g_signal_connect(a2, "clicked", G_CALLBACK(ao_grade), t);
    g_signal_connect(a3, "clicked", G_CALLBACK(ao_exportar), t);
    gtk_box_append(GTK_BOX(acoes), a1);
    gtk_box_append(GTK_BOX(acoes), a2);
    gtk_box_append(GTK_BOX(acoes), a3);
    gtk_box_append(GTK_BOX(pagina), acoes);

    GtkWidget *rolagem = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rolagem), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), pagina);
    g_signal_connect(rolagem, "map", G_CALLBACK(ao_mostrar), t);

    atualizar(t);
    return rolagem;
}
