#include "interface.h"
#include <string.h>
#include <stdlib.h>

#define PRIMEIRA_HORA 420      /* 07:00 */
#define ULTIMA_HORA   1320     /* 22:00 */
#define PASSO         30
#define NUM_HORARIOS  31
#define FAIXA_INI     420
#define FAIXA_TOT     900

typedef struct {
    Estado *e;
    GtkWidget *dia, *mes, *ano, *h_ini, *h_fim, *pessoas, *c_proj, *c_ace;
    GtkWidget *resultados;
} TelaBusca;

typedef struct { int dia, ini, fim, pessoas, proj, ace; } Filtros;
typedef struct { TelaBusca *t; char sala[64]; int dia, ini, fim, pessoas; } Pedido;
typedef struct { char sala[64]; int dia, ini, fim; } Faixa;

static void estilo_busca(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".form-busca { border: 1px solid #e1e6ef; border-radius: 6px; padding: 18px; }"
        ".cartao-sala { border: 1px solid #e1e6ef; border-radius: 6px; padding: 14px 18px; }"
        ".nome-sala-busca { font-family: Georgia, serif; font-size: 20px; font-weight: bold; }"
        ".titulo-resultado { font-family: Georgia, serif; font-size: 20px; margin-top: 6px; }"
        ".numero-busca { font-family: Georgia, serif; font-size: 24px; font-weight: bold; }"
        ".nota-busca { color: #5b6b8a; }"
        ".aviso-busca { color: #c8102e; font-weight: bold; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

static GtkWidget *rotulo(const char *txt) {
    GtkWidget *l = gtk_label_new(txt);
    gtk_widget_add_css_class(l, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    return l;
}

static GtkWidget *spin(int min, int max, int valor) {
    GtkWidget *s = gtk_spin_button_new_with_range(min, max, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s), valor);
    return s;
}

static GtkWidget *lista_horarios(int selecionado) {
    static char textos[NUM_HORARIOS][8];
    const char *ptrs[NUM_HORARIOS + 1];
    for (int i = 0; i < NUM_HORARIOS; i++) {
        dados_hora_texto(PRIMEIRA_HORA + PASSO * i, textos[i], sizeof textos[i]);
        ptrs[i] = textos[i];
    }
    ptrs[NUM_HORARIOS] = NULL;
    GtkWidget *d = gtk_drop_down_new_from_strings(ptrs);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(d), selecionado);
    return d;
}

static void limpar_caixa(GtkWidget *caixa) {
    GtkWidget *f;
    while ((f = gtk_widget_get_first_child(caixa)) != NULL)
        gtk_box_remove(GTK_BOX(caixa), f);
}

static void ler_filtros(TelaBusca *t, Filtros *f) {
    f->dia = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->ano)) * 10000
           + gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->mes)) * 100
           + gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->dia));
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->h_ini));
    guint j = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->h_fim));
    f->ini = PRIMEIRA_HORA + PASSO * (int) (i >= NUM_HORARIOS ? 0 : i);
    f->fim = PRIMEIRA_HORA + PASSO * (int) (j >= NUM_HORARIOS ? 0 : j);
    f->pessoas = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->pessoas));
    f->proj = gtk_check_button_get_active(GTK_CHECK_BUTTON(t->c_proj));
    f->ace  = gtk_check_button_get_active(GTK_CHECK_BUTTON(t->c_ace));
}

/* ---- faixa do dia de cada sala ---- */
static double px(int min, int w) {
    double x = (min - FAIXA_INI) * (double) w / FAIXA_TOT;
    if (x < 0) x = 0;
    if (x > w) x = w;
    return x;
}

static void desenhar_faixa(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer dados) {
    const Faixa *f = (const Faixa *) dados;
    int barra = h - 16;

    cairo_set_source_rgb(cr, 0.882, 0.902, 0.937);
    cairo_rectangle(cr, 0, 0, w, barra);
    cairo_fill(cr);

    for (int i = 0; i < g_n_reservas; i++) {
        const ReservaTeste *o = &g_reservas[i];
        if (o->dia != f->dia || strcmp(o->sala, f->sala) != 0) continue;
        cairo_set_source_rgb(cr, 0.624, 0.702, 0.831);
        cairo_rectangle(cr, px(o->ini, w), 0, px(o->fim, w) - px(o->ini, w), barra);
        cairo_fill(cr);
    }
    cairo_set_source_rgb(cr, 0.071, 0.153, 0.310);
    cairo_rectangle(cr, px(f->ini, w), 0, px(f->fim, w) - px(f->ini, w), barra);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.357, 0.420, 0.541);
    cairo_set_font_size(cr, 11);
    int marcas[4] = { 8, 12, 16, 20 };
    for (int i = 0; i < 4; i++) {
        char txt[8];
        g_snprintf(txt, sizeof txt, "%dh", marcas[i]);
        cairo_move_to(cr, px(marcas[i] * 60, w), h - 3);
        cairo_show_text(cr, txt);
    }
}

static GtkWidget *criar_faixa(const char *sala, int dia, int ini, int fim) {
    Faixa *f = g_new0(Faixa, 1);
    g_strlcpy(f->sala, sala, sizeof f->sala);
    f->dia = dia; f->ini = ini; f->fim = fim;
    GtkWidget *a = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(a), 300);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(a), 40);
    gtk_widget_set_hexpand(a, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(a), desenhar_faixa, f, g_free);
    return a;
}

/* ---- botão Reservar: abre o formulário já preenchido ---- */
static void ao_reservar(GtkButton *b, gpointer dados) {
    Pedido *p = (Pedido *) dados;
    nova_reserva_preencher(p->sala, p->dia, p->ini, p->fim, p->pessoas);
    interface_ir_para(p->t->e, "nova");
}

static GtkWidget *cartao_sala(TelaBusca *t, const SalaTeste *sl, const Filtros *f) {
    GtkWidget *cartao = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_add_css_class(cartao, "cartao-sala");

    GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand(info, TRUE);

    GtkWidget *nome = gtk_label_new(sl->nome);
    gtk_widget_add_css_class(nome, "nome-sala-busca");
    gtk_label_set_xalign(GTK_LABEL(nome), 0);

    int sobra = sl->lugares - f->pessoas;
    char folga[40];
    if (sobra == 0) g_strlcpy(folga, "Lotação exata", sizeof folga);
    else if (sobra == 1) g_strlcpy(folga, "Sobra 1 lugar", sizeof folga);
    else g_snprintf(folga, sizeof folga, "Sobram %d lugares", sobra);

    GString *det = g_string_new(NULL);
    g_string_append_printf(det, "%d lugares", sl->lugares);
    if (sl->projetor)  g_string_append(det, "  ·  Projetor");
    if (sl->acessivel) g_string_append(det, "  ·  Acessível");
    g_string_append_printf(det, "  ·  %s", folga);
    GtkWidget *detalhes = gtk_label_new(det->str);
    g_string_free(det, TRUE);
    gtk_widget_add_css_class(detalhes, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(detalhes), 0);

    gtk_box_append(GTK_BOX(info), nome);
    gtk_box_append(GTK_BOX(info), detalhes);
    gtk_box_append(GTK_BOX(info), criar_faixa(sl->nome, f->dia, f->ini, f->fim));
    gtk_box_append(GTK_BOX(cartao), info);

    Pedido *p = g_new0(Pedido, 1);
    p->t = t;
    g_strlcpy(p->sala, sl->nome, sizeof p->sala);
    p->dia = f->dia; p->ini = f->ini; p->fim = f->fim; p->pessoas = f->pessoas;
    GtkWidget *b = gtk_button_new_with_label("Reservar");
    gtk_widget_add_css_class(b, "primario");
    gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
    g_object_set_data_full(G_OBJECT(b), "pedido", p, g_free);
    g_signal_connect(b, "clicked", G_CALLBACK(ao_reservar), p);
    gtk_box_append(GTK_BOX(cartao), b);
    return cartao;
}

/* "B04, C01 e Auditório" */
static char *juntar(GPtrArray *v) {
    GString *s = g_string_new("");
    for (guint i = 0; i < v->len; i++) {
        if (i > 0) g_string_append(s, i == v->len - 1 ? " e " : ", ");
        g_string_append(s, (const char *) g_ptr_array_index(v, i));
    }
    return g_string_free(s, FALSE);
}

static void estatistica(GtkWidget *linha, int n, const char *legenda) {
    GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    char buf[16];
    g_snprintf(buf, sizeof buf, "%d", n);
    GtkWidget *num = gtk_label_new(buf);
    gtk_widget_add_css_class(num, "numero-busca");
    gtk_label_set_xalign(GTK_LABEL(num), 0);
    GtkWidget *leg = gtk_label_new(legenda);
    gtk_widget_add_css_class(leg, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(leg), 0);
    gtk_box_append(GTK_BOX(c), num);
    gtk_box_append(GTK_BOX(c), leg);
    gtk_box_append(GTK_BOX(linha), c);
}

static void aviso(TelaBusca *t, const char *msg) {
    GtkWidget *l = gtk_label_new(msg);
    gtk_widget_add_css_class(l, "aviso-busca");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_append(GTK_BOX(t->resultados), l);
    interface_mensagem(t->e, msg, TRUE);
}

static void buscar(TelaBusca *t) {
    Filtros f;
    ler_filtros(t, &f);
    limpar_caixa(t->resultados);

    if (!dados_data_valida(f.dia)) { aviso(t, "Essa data não existe no calendário."); return; }
    if (f.fim <= f.ini)            { aviso(t, "O horário final precisa ser depois do inicial."); return; }

    int n_cabem = 0, n_rec = 0, achadas[MAX_SALAS], na = 0;
    GPtrArray *ocupadas = g_ptr_array_new();                          /* nomes (não são nossos) */
    GPtrArray *fora_cap = g_ptr_array_new_with_free_func(g_free);
    GPtrArray *fora_rec = g_ptr_array_new_with_free_func(g_free);

    for (int s = 0; s < g_n_salas; s++) {
        const SalaTeste *sl = &g_salas[s];
        if (f.pessoas > sl->lugares) {
            g_ptr_array_add(fora_cap, g_strdup_printf("%s tem %d lugares", sl->nome, sl->lugares));
            continue;
        }
        n_cabem++;
        if ((f.proj && !sl->projetor) || (f.ace && !sl->acessivel)) {
            if (f.proj && !sl->projetor)
                g_ptr_array_add(fora_rec, g_strdup_printf("%s não tem projetor", sl->nome));
            else
                g_ptr_array_add(fora_rec, g_strdup_printf("%s não é acessível", sl->nome));
            continue;
        }
        n_rec++;
        if (dados_conflito(sl->nome, f.dia, f.ini, f.fim, 0) >= 0) {
            g_ptr_array_add(ocupadas, (gpointer) sl->nome);
            continue;
        }
        achadas[na++] = s;
    }

    /* título do resultado */
    char *tit;
    if (na == 0) tit = g_strdup("Nenhuma sala atende ao pedido");
    else if (na == 1) tit = g_strdup("1 sala atende ao pedido");
    else tit = g_strdup_printf("%d salas atendem ao pedido", na);
    GtkWidget *ltit = gtk_label_new(tit);
    g_free(tit);
    gtk_widget_add_css_class(ltit, "titulo-resultado");
    gtk_label_set_xalign(GTK_LABEL(ltit), 0);
    gtk_box_append(GTK_BOX(t->resultados), ltit);

    /* estatísticas */
    GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_set_homogeneous(GTK_BOX(linha), TRUE);
    char leg[64];
    if (f.pessoas == 1) g_strlcpy(leg, "cabe 1 pessoa", sizeof leg);
    else g_snprintf(leg, sizeof leg, "cabem %d pessoas", f.pessoas);
    const char *leg_rec = (f.proj && f.ace) ? "têm projetor e acessibilidade"
                        : f.proj ? "têm projetor"
                        : f.ace  ? "são acessíveis"
                        : "atendem aos recursos";
    estatistica(linha, g_n_salas, "salas");
    estatistica(linha, n_cabem, leg);
    estatistica(linha, n_rec, leg_rec);
    estatistica(linha, na, "estão livres");
    gtk_box_append(GTK_BOX(t->resultados), linha);

    /* salas encontradas */
    if (na == 0) {
        GtkWidget *l = gtk_label_new("Nenhuma sala livre com esses requisitos. Tente \"Encontrar outro horário\".");
        gtk_widget_add_css_class(l, "subtitulo");
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_box_append(GTK_BOX(t->resultados), l);
    }
    for (int k = 0; k < na; k++)
        gtk_box_append(GTK_BOX(t->resultados), cartao_sala(t, &g_salas[achadas[k]], &f));

    /* motivo de cada sala que ficou de fora */
    GPtrArray *fora = g_ptr_array_new();
    for (guint i = 0; i < fora_cap->len; i++) g_ptr_array_add(fora, g_ptr_array_index(fora_cap, i));
    for (guint i = 0; i < fora_rec->len; i++) g_ptr_array_add(fora, g_ptr_array_index(fora_rec, i));

    GString *rodape = g_string_new(NULL);
    if (ocupadas->len > 0) {
        char *s = juntar(ocupadas);
        g_string_append_printf(rodape, "Ocupadas nesse horário: %s. ", s);
        g_free(s);
    }
    if (fora->len > 0) {
        char *s = juntar(fora);
        g_string_append_printf(rodape, "Fora do pedido: %s.", s);
        g_free(s);
    }
    if (rodape->len > 0) {
        GtkWidget *l = gtk_label_new(rodape->str);
        gtk_widget_add_css_class(l, "nota-busca");
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_label_set_wrap(GTK_LABEL(l), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(l), 100);
        gtk_widget_set_margin_top(l, 6);
        gtk_box_append(GTK_BOX(t->resultados), l);
    }
    g_string_free(rodape, TRUE);
    g_ptr_array_free(fora, TRUE);
    g_ptr_array_free(fora_cap, TRUE);
    g_ptr_array_free(fora_rec, TRUE);
    g_ptr_array_free(ocupadas, TRUE);
}

static void ao_buscar(GtkButton *b, gpointer dados) { buscar((TelaBusca *) dados); }

/* ---- "Encontrar outro horário" ---- */
static int contar_livres(int dia, int ini, int fim, int pessoas, int proj, int ace) {
    int n = 0;
    for (int s = 0; s < g_n_salas; s++) {
        const SalaTeste *sl = &g_salas[s];
        if (pessoas > sl->lugares) continue;
        if (proj && !sl->projetor) continue;
        if (ace && !sl->acessivel) continue;
        if (dados_conflito(sl->nome, dia, ini, fim, 0) < 0) n++;
    }
    return n;
}

static void ao_outro_horario(GtkButton *b, gpointer dados) {
    TelaBusca *t = (TelaBusca *) dados;
    Filtros f;
    ler_filtros(t, &f);
    if (!dados_data_valida(f.dia) || f.fim <= f.ini) {
        interface_mensagem(t->e, "Confira o dia e o horário antes de procurar outro.", TRUE);
        return;
    }
    int dur = f.fim - f.ini, melhor = -1, dist_melhor = 100000;
    for (int ini = PRIMEIRA_HORA; ini + dur <= ULTIMA_HORA; ini += PASSO) {
        if (ini == f.ini) continue;
        if (contar_livres(f.dia, ini, ini + dur, f.pessoas, f.proj, f.ace) == 0) continue;
        int dist = abs(ini - f.ini);
        if (dist < dist_melhor) { dist_melhor = dist; melhor = ini; }
    }
    if (melhor < 0) {
        interface_mensagem(t->e, "Não há outro horário livre nesse dia com esses requisitos.", TRUE);
        return;
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_ini), (melhor - PRIMEIRA_HORA) / PASSO);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_fim), (melhor + dur - PRIMEIRA_HORA) / PASSO);
    buscar(t);

    char a[8], c[8];
    dados_hora_texto(melhor, a, sizeof a);
    dados_hora_texto(melhor + dur, c, sizeof c);
    char *msg = g_strdup_printf("Horário sugerido: das %s às %s.", a, c);
    interface_mensagem(t->e, msg, FALSE);
    g_free(msg);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) { buscar((TelaBusca *) dados); }

GtkWidget *tela_busca_criar(Estado *e) {
    TelaBusca *t = g_new0(TelaBusca, 1);
    t->e = e;
    estilo_busca();

    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(pagina, 28);
    gtk_widget_set_margin_bottom(pagina, 24);
    gtk_widget_set_margin_start(pagina, 32);
    gtk_widget_set_margin_end(pagina, 32);

    /* topo */
    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(textos, TRUE);
    GtkWidget *titulo = gtk_label_new("Buscar salas");
    gtk_widget_add_css_class(titulo, "titulo");
    gtk_label_set_xalign(GTK_LABEL(titulo), 0);
    GtkWidget *sub = gtk_label_new("Informe o que precisa e veja só as salas que servem.");
    gtk_widget_add_css_class(sub, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(sub), 0);
    gtk_box_append(GTK_BOX(textos), titulo);
    gtk_box_append(GTK_BOX(textos), sub);
    GtkWidget *b_outro = gtk_button_new_with_label("Encontrar outro horário");
    gtk_widget_add_css_class(b_outro, "secundario");
    gtk_widget_set_valign(b_outro, GTK_ALIGN_START);
    g_signal_connect(b_outro, "clicked", G_CALLBACK(ao_outro_horario), t);
    gtk_box_append(GTK_BOX(topo), textos);
    gtk_box_append(GTK_BOX(topo), b_outro);
    gtk_box_append(GTK_BOX(pagina), topo);

    /* formulário */
    t->dia     = spin(1, 31, DATA_PADRAO % 100);
    t->mes     = spin(1, 12, (DATA_PADRAO / 100) % 100);
    t->ano     = spin(2026, 2035, DATA_PADRAO / 10000);
    t->h_ini   = lista_horarios((14 * 60 - PRIMEIRA_HORA) / PASSO);
    t->h_fim   = lista_horarios((16 * 60 - PRIMEIRA_HORA) / PASSO);
    t->pessoas = spin(1, 1000, 30);
    t->c_proj  = gtk_check_button_new_with_label("Projetor");
    t->c_ace   = gtk_check_button_new_with_label("Acessibilidade (rampa, térreo ou elevador)");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(t->c_proj), TRUE);

    GtkWidget *data = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(data), t->dia);
    gtk_box_append(GTK_BOX(data), t->mes);
    gtk_box_append(GTK_BOX(data), t->ano);

    GtkWidget *recursos = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_box_append(GTK_BOX(recursos), t->c_proj);
    gtk_box_append(GTK_BOX(recursos), t->c_ace);

    GtkWidget *b_buscar = gtk_button_new_with_label("Buscar");
    gtk_widget_add_css_class(b_buscar, "primario");
    gtk_widget_set_halign(b_buscar, GTK_ALIGN_END);
    g_signal_connect(b_buscar, "clicked", G_CALLBACK(ao_buscar), t);

    GtkWidget *grade = gtk_grid_new();
    gtk_widget_add_css_class(grade, "form-busca");
    gtk_grid_set_column_spacing(GTK_GRID(grade), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grade), 6);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Dia (dia, mês, ano)"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Das"),                 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Às"),                  2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Pessoas"),             3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), data,                          0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->h_ini,                      1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->h_fim,                      2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->pessoas,                    3, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Recursos"),            0, 2, 3, 1);
    gtk_grid_attach(GTK_GRID(grade), recursos,                      0, 3, 3, 1);
    gtk_grid_attach(GTK_GRID(grade), b_buscar,                      3, 3, 1, 1);
    gtk_box_append(GTK_BOX(pagina), grade);

    /* resultados */
    t->resultados = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(pagina), t->resultados);

    GtkWidget *rolagem = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rolagem), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), pagina);
    g_signal_connect(rolagem, "map", G_CALLBACK(ao_mostrar), t);
    return rolagem;
}
