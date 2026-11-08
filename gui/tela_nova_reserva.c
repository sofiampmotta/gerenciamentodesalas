#include "interface.h"
#include <string.h>

#define PRIMEIRA_HORA 420      /* 07:00 */
#define PASSO         30
#define NUM_HORARIOS  31       /* 07:00 .. 22:00 */
#define FAIXA_INI     420
#define FAIXA_TOT     900      /* 07:00 .. 22:00 */

static const char *TIPOS[]      = { "Monitoria", "Extensão", "Evento", NULL };
static const char *REPETICOES[] = { "Só nesta data", "Toda semana até o fim do semestre", NULL };

typedef struct {
    Estado *e;
    GtkWidget *atividade, *tipo, *responsavel, *descricao, *pessoas, *sala;
    GtkWidget *dia, *mes, *ano, *h_ini, *h_fim, *repeticao;
    GtkWidget *status, *motivo, *l_cabe, *l_proj, *l_hor, *l_dia, *faixa;
    ReservaTeste       r;
    VerificacaoReserva v;
    gboolean montando;
    GtkWidget *b_voltar;
    gboolean da_busca, pend_busca;
} TelaNova;

static TelaNova *g_nova = NULL;

static void estilo_nova(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".painel-verif { border: 1px solid #e1e6ef; border-radius: 6px; padding: 18px; min-width: 320px; }"
        ".status-ok { color: #1e7e4a; font-family: Georgia, serif; font-size: 20px; font-weight: bold; }"
        ".status-erro { color: #c8102e; font-family: Georgia, serif; font-size: 20px; font-weight: bold; }"
        ".check-ok { color: #1e7e4a; }"
        ".check-erro { color: #c8102e; }"
        ".motivo { color: #5b6b8a; }"
        "button.secundario { background: #ffffff; background-image: none; color: #12274f;"
        "  border: 1px solid #1f4e8c; border-radius: 4px; font-weight: bold; padding: 10px 20px; box-shadow: none; }"
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

static guint indice_horario(GtkWidget *d) {
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(d));
    return i >= NUM_HORARIOS ? 0 : i;
}

static void ler_formulario(TelaNova *t, ReservaTeste *r, int *pessoas, int *repetir) {
    memset(r, 0, sizeof *r);
    g_strlcpy(r->atividade,   gtk_editable_get_text(GTK_EDITABLE(t->atividade)),   sizeof r->atividade);
    g_strlcpy(r->responsavel, gtk_editable_get_text(GTK_EDITABLE(t->responsavel)), sizeof r->responsavel);
    g_strlcpy(r->descricao,   gtk_editable_get_text(GTK_EDITABLE(t->descricao)),   sizeof r->descricao);
    g_strstrip(r->atividade); g_strstrip(r->responsavel); g_strstrip(r->descricao);

    guint tp = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->tipo));
    g_strlcpy(r->tipo, TIPOS[tp > 2 ? 0 : tp], sizeof r->tipo);

    GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
    if (it) g_strlcpy(r->sala, gtk_string_object_get_string(GTK_STRING_OBJECT(it)), sizeof r->sala);

    r->dia = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->ano)) * 10000
           + gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->mes)) * 100
           + gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->dia));
    r->ini = PRIMEIRA_HORA + PASSO * (int) indice_horario(t->h_ini);
    r->fim = PRIMEIRA_HORA + PASSO * (int) indice_horario(t->h_fim);

    *pessoas = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(t->pessoas));
    *repetir = (gtk_drop_down_get_selected(GTK_DROP_DOWN(t->repeticao)) == 1);
}

static void definir_linha(GtkWidget *l, gboolean ok, const char *texto) {
    char *t = g_strdup_printf("%s %s", ok ? "✓" : "✗", texto);
    gtk_label_set_text(GTK_LABEL(l), t);
    g_free(t);
    gtk_widget_remove_css_class(l, "check-ok");
    gtk_widget_remove_css_class(l, "check-erro");
    gtk_widget_add_css_class(l, ok ? "check-ok" : "check-erro");
}

static void apagar_linha(GtkWidget *l) { gtk_label_set_text(GTK_LABEL(l), ""); }

static void atualizar(TelaNova *t) {
    if (t->montando) return;
    int pessoas, rep;
    ler_formulario(t, &t->r, &pessoas, &rep);
    VerificacaoReserva *v = &t->v;
    dados_verificar_reserva(&t->r, pessoas, rep, v);

    gtk_label_set_text(GTK_LABEL(t->status), v->titulo);
    gtk_widget_remove_css_class(t->status, "status-ok");
    gtk_widget_remove_css_class(t->status, "status-erro");
    gtk_widget_add_css_class(t->status, v->ok ? "status-ok" : "status-erro");
    gtk_label_set_text(GTK_LABEL(t->motivo), v->motivo);

    if (v->sala_idx < 0) {
        apagar_linha(t->l_cabe); apagar_linha(t->l_proj);
        apagar_linha(t->l_hor);  apagar_linha(t->l_dia);
    } else {
        char txt[160], a[8], b[8], dia[16];
        if (v->cabe) g_snprintf(txt, sizeof txt, "Cabem %d pessoas (%d lugares)", pessoas, v->lugares);
        else g_snprintf(txt, sizeof txt, "Faltam %d lugares (a sala tem %d)", v->faltam, v->lugares);
        definir_linha(t->l_cabe, v->cabe, txt);
        definir_linha(t->l_proj, v->projetor, v->projetor ? "A sala tem projetor" : "A sala não tem projetor");

        if (!dados_data_valida(t->r.dia)) {
            apagar_linha(t->l_hor);
            definir_linha(t->l_dia, FALSE, "Data inválida");
        } else {
            dados_data_texto(t->r.dia, dia, sizeof dia);
            dia[5] = '\0';
            g_snprintf(txt, sizeof txt, "Dia %s na %s", dia, t->r.sala);
            definir_linha(t->l_dia, TRUE, txt);
            if (t->r.fim <= t->r.ini) {
                definir_linha(t->l_hor, FALSE, "Horário final antes do inicial");
            } else if (v->conflito_idx >= 0) {
                g_snprintf(txt, sizeof txt, "Horário em uso por %s", g_reservas[v->conflito_idx].atividade);
                definir_linha(t->l_hor, FALSE, txt);
            } else {
                dados_hora_texto(t->r.ini, a, sizeof a);
                dados_hora_texto(t->r.fim, b, sizeof b);
                g_snprintf(txt, sizeof txt, "Horário livre das %s às %s", a, b);
                definir_linha(t->l_hor, TRUE, txt);
            }
        }
    }
    gtk_widget_queue_draw(t->faixa);
}

static double px(int min, int w) {
    double x = (min - FAIXA_INI) * (double) w / FAIXA_TOT;
    if (x < 0) x = 0;
    if (x > w) x = w;
    return x;
}

static void desenhar_faixa(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer dados) {
    TelaNova *t = (TelaNova *) dados;
    int barra = h - 18;

    cairo_set_source_rgb(cr, 0.882, 0.902, 0.937);
    cairo_rectangle(cr, 0, 0, w, barra);
    cairo_fill(cr);

    if (t->v.sala_idx >= 0 && dados_data_valida(t->r.dia)) {
        for (int i = 0; i < g_n_reservas; i++) {
            const ReservaTeste *o = &g_reservas[i];
            if (o->dia != t->r.dia || strcmp(o->sala, t->r.sala) != 0) continue;
            cairo_set_source_rgb(cr, 0.624, 0.702, 0.831);
            cairo_rectangle(cr, px(o->ini, w), 0, px(o->fim, w) - px(o->ini, w), barra);
            cairo_fill(cr);
        }
        if (t->r.fim > t->r.ini) {
            if (t->v.ok) cairo_set_source_rgb(cr, 0.071, 0.153, 0.310);
            else         cairo_set_source_rgb(cr, 0.784, 0.063, 0.180);
            cairo_rectangle(cr, px(t->r.ini, w), 0, px(t->r.fim, w) - px(t->r.ini, w), barra);
            cairo_fill(cr);
        }
    }

    cairo_set_source_rgb(cr, 0.357, 0.420, 0.541);
    cairo_set_font_size(cr, 11);
    int marcas[4] = { 8, 12, 16, 20 };
    for (int i = 0; i < 4; i++) {
        char txt[8];
        g_snprintf(txt, sizeof txt, "%dh", marcas[i]);
        cairo_move_to(cr, px(marcas[i] * 60, w), h - 4);
        cairo_show_text(cr, txt);
    }
}

/* recarrega a lista de salas (uma sala nova cadastrada em "Salas" aparece aqui) */
static void recarregar_salas(TelaNova *t) {
    char anterior[64] = "";
    GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
    if (it) g_strlcpy(anterior, gtk_string_object_get_string(GTK_STRING_OBJECT(it)), sizeof anterior);

    const char *nomes[MAX_SALAS + 1];
    for (int i = 0; i < g_n_salas; i++) nomes[i] = g_salas[i].nome;
    nomes[g_n_salas] = NULL;

    GtkStringList *sl = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(t->sala)));
    t->montando = TRUE;
    gtk_string_list_splice(sl, 0, g_list_model_get_n_items(G_LIST_MODEL(sl)), nomes);
    guint pos = 0;
    for (int i = 0; i < g_n_salas; i++)
        if (strcmp(g_salas[i].nome, anterior) == 0) pos = i;
    if (g_n_salas > 0) gtk_drop_down_set_selected(GTK_DROP_DOWN(t->sala), pos);
    t->montando = FALSE;
}

static void limpar(TelaNova *t) {
    t->montando = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(t->atividade), "");
    gtk_editable_set_text(GTK_EDITABLE(t->responsavel), "");
    gtk_editable_set_text(GTK_EDITABLE(t->descricao), "");
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->tipo), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->pessoas), 30);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->dia), DATA_PADRAO % 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->mes), (DATA_PADRAO / 100) % 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->ano), DATA_PADRAO / 10000);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_ini), (14 * 60 - PRIMEIRA_HORA) / PASSO);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_fim), (16 * 60 - PRIMEIRA_HORA) / PASSO);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->repeticao), 0);
    t->montando = FALSE;
    atualizar(t);
}

static void ao_mudou(GObject *o, GParamSpec *p, gpointer dados) { atualizar((TelaNova *) dados); }
static void ao_mudou_spin(GtkSpinButton *s, gpointer dados)     { atualizar((TelaNova *) dados); }

static void ao_mostrar(GtkWidget *w, gpointer dados) {
    TelaNova *t = (TelaNova *) dados;
    recarregar_salas(t);
    t->da_busca = t->pend_busca;
    t->pend_busca = FALSE;
    gtk_button_set_label(GTK_BUTTON(t->b_voltar), t->da_busca ? "Voltar para a busca" : "Voltar para Reservas");
    atualizar(t);
}

static void ao_salvar(GtkButton *b, gpointer dados) {
    TelaNova *t = (TelaNova *) dados;
    ReservaTeste r;
    int pessoas, rep;
    char msg[240];
    ler_formulario(t, &r, &pessoas, &rep);
    if (dados_salvar_reserva(&r, pessoas, rep, msg, sizeof msg) < 0) {
        interface_mensagem(t->e, msg, TRUE);
        return;
    }
    interface_mensagem(t->e, msg, FALSE);
    limpar(t);
    interface_ir_para(t->e, "reservas");
}

static void ao_descartar(GtkButton *b, gpointer dados) {
    TelaNova *t = (TelaNova *) dados;
    limpar(t);
    interface_mensagem(t->e, "Reserva descartada. Nada foi salvo.", FALSE);
    interface_ir_para(t->e, "reservas");
}

static void ao_voltar(GtkButton *b, gpointer dados) {
    { TelaNova *t = (TelaNova *) dados; interface_ir_para(t->e, t->da_busca ? "busca" : "reservas"); }
}

static GtkWidget *nova_linha_check(GtkWidget *caixa) {
    GtkWidget *l = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_append(GTK_BOX(caixa), l);
    return l;
}

GtkWidget *tela_nova_reserva_criar(Estado *e) {
    TelaNova *t = g_new0(TelaNova, 1);
    t->e = e;
    estilo_nova();

    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(pagina, 28);
    gtk_widget_set_margin_bottom(pagina, 24);
    gtk_widget_set_margin_start(pagina, 32);
    gtk_widget_set_margin_end(pagina, 32);

    GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *titulo = gtk_label_new("Nova reserva");
    gtk_widget_add_css_class(titulo, "titulo");
    gtk_label_set_xalign(GTK_LABEL(titulo), 0);
    gtk_widget_set_hexpand(titulo, TRUE);
    GtkWidget *voltar = gtk_button_new_with_label("Voltar para Reservas");
    t->b_voltar = voltar;
    g_nova = t;
    gtk_widget_add_css_class(voltar, "secundario");
    g_signal_connect(voltar, "clicked", G_CALLBACK(ao_voltar), t);
    gtk_box_append(GTK_BOX(topo), titulo);
    gtk_box_append(GTK_BOX(topo), voltar);
    gtk_box_append(GTK_BOX(pagina), topo);

    GtkWidget *corpo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_box_append(GTK_BOX(pagina), corpo);

    const char *vazio[] = { NULL };
    t->atividade   = gtk_entry_new();
    t->responsavel = gtk_entry_new();
    t->descricao   = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(t->atividade), 90);
    gtk_entry_set_max_length(GTK_ENTRY(t->responsavel), 60);
    gtk_entry_set_max_length(GTK_ENTRY(t->descricao), 120);
    t->tipo      = gtk_drop_down_new_from_strings(TIPOS);
    t->sala      = gtk_drop_down_new_from_strings(vazio);
    t->pessoas   = spin(1, 1000, 30);
    t->dia       = spin(1, 31, DATA_PADRAO % 100);
    t->mes       = spin(1, 12, (DATA_PADRAO / 100) % 100);
    t->ano       = spin(2026, 2035, DATA_PADRAO / 10000);
    t->h_ini     = lista_horarios((14 * 60 - PRIMEIRA_HORA) / PASSO);
    t->h_fim     = lista_horarios((16 * 60 - PRIMEIRA_HORA) / PASSO);
    t->repeticao = gtk_drop_down_new_from_strings(REPETICOES);

    GtkWidget *data = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(data), t->dia);
    gtk_box_append(GTK_BOX(data), t->mes);
    gtk_box_append(GTK_BOX(data), t->ano);
    GtkWidget *horas = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(horas), t->h_ini);
    gtk_box_append(GTK_BOX(horas), gtk_label_new("às"));
    gtk_box_append(GTK_BOX(horas), t->h_fim);

    GtkWidget *grade = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grade), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grade), 6);
    gtk_widget_set_hexpand(grade, TRUE);
    gtk_widget_set_hexpand(t->atividade, TRUE);
    gtk_widget_set_hexpand(t->responsavel, TRUE);
    gtk_widget_set_hexpand(t->tipo, TRUE);
    gtk_widget_set_hexpand(t->sala, TRUE);

    gtk_grid_attach(GTK_GRID(grade), rotulo("Atividade"),   0, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grade), t->atividade,          0, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Tipo"),        0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Responsável"), 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->tipo,               0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->responsavel,        1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Descrição"),   0, 4, 2, 1);
    gtk_grid_attach(GTK_GRID(grade), t->descricao,          0, 5, 2, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Sala"),        0, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Pessoas previstas"), 1, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->sala,               0, 7, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), t->pessoas,            1, 7, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Dia (dia, mês, ano)"), 0, 8, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Horário"),     1, 8, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), data,                  0, 9, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), horas,                 1, 9, 1, 1);
    gtk_grid_attach(GTK_GRID(grade), rotulo("Repetição"),   0, 10, 2, 1);
    gtk_grid_attach(GTK_GRID(grade), t->repeticao,          0, 11, 2, 1);

    GtkWidget *botoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(botoes, 14);
    GtkWidget *b_salvar = gtk_button_new_with_label("Salvar reserva");
    gtk_widget_add_css_class(b_salvar, "primario");
    g_signal_connect(b_salvar, "clicked", G_CALLBACK(ao_salvar), t);
    GtkWidget *b_desc = gtk_button_new_with_label("Descartar");
    gtk_widget_add_css_class(b_desc, "secundario");
    g_signal_connect(b_desc, "clicked", G_CALLBACK(ao_descartar), t);
    gtk_box_append(GTK_BOX(botoes), b_salvar);
    gtk_box_append(GTK_BOX(botoes), b_desc);
    gtk_grid_attach(GTK_GRID(grade), botoes, 0, 12, 2, 1);
    gtk_box_append(GTK_BOX(corpo), grade);

    /* painel de verificação (lado direito, como no protótipo) */
    GtkWidget *painel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(painel, "painel-verif");
    gtk_widget_set_valign(painel, GTK_ALIGN_START);

    t->status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(t->status), 0);
    t->motivo = gtk_label_new("");
    gtk_widget_add_css_class(t->motivo, "motivo");
    gtk_label_set_xalign(GTK_LABEL(t->motivo), 0);
    gtk_label_set_wrap(GTK_LABEL(t->motivo), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(t->motivo), 38);
    gtk_box_append(GTK_BOX(painel), t->status);
    gtk_box_append(GTK_BOX(painel), t->motivo);

    t->l_cabe = nova_linha_check(painel);
    t->l_proj = nova_linha_check(painel);
    t->l_hor  = nova_linha_check(painel);
    t->l_dia  = nova_linha_check(painel);

    t->faixa = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(t->faixa), 290);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(t->faixa), 48);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(t->faixa), desenhar_faixa, t, NULL);
    gtk_widget_set_margin_top(t->faixa, 10);
    gtk_box_append(GTK_BOX(painel), t->faixa);
    gtk_box_append(GTK_BOX(corpo), painel);

    /* o painel se atualiza a cada mudança no formulário */
    g_signal_connect(t->sala,      "notify::selected", G_CALLBACK(ao_mudou), t);
    g_signal_connect(t->h_ini,     "notify::selected", G_CALLBACK(ao_mudou), t);
    g_signal_connect(t->h_fim,     "notify::selected", G_CALLBACK(ao_mudou), t);
    g_signal_connect(t->repeticao, "notify::selected", G_CALLBACK(ao_mudou), t);
    g_signal_connect(t->pessoas,   "value-changed", G_CALLBACK(ao_mudou_spin), t);
    g_signal_connect(t->dia,       "value-changed", G_CALLBACK(ao_mudou_spin), t);
    g_signal_connect(t->mes,       "value-changed", G_CALLBACK(ao_mudou_spin), t);
    g_signal_connect(t->ano,       "value-changed", G_CALLBACK(ao_mudou_spin), t);

    GtkWidget *rolagem = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rolagem), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), pagina);
    g_signal_connect(rolagem, "map", G_CALLBACK(ao_mostrar), t);

    recarregar_salas(t);
    atualizar(t);
    return rolagem;
}

/* Chamada pela tela "Buscar salas": deixa o formulário já preenchido */
void nova_reserva_preencher(const char *sala, int dia, int ini, int fim, int pessoas) {
    TelaNova *t = g_nova;
    if (!t) return;
    recarregar_salas(t);
    limpar(t);
    t->montando = TRUE;
    for (int i = 0; i < g_n_salas; i++)
        if (strcmp(g_salas[i].nome, sala) == 0)
            gtk_drop_down_set_selected(GTK_DROP_DOWN(t->sala), i);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->dia), dia % 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->mes), (dia / 100) % 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->ano), dia / 10000);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_ini), (ini - PRIMEIRA_HORA) / PASSO);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_fim), (fim - PRIMEIRA_HORA) / PASSO);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t->pessoas), pessoas);
    t->montando = FALSE;
    t->pend_busca = TRUE;
    atualizar(t);
}
