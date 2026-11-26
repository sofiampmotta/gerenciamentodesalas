#include "interface.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
Estado *e;
GtkWidget *lista;
GtkWidget *total;
GtkWidget *campo_busca;
GtkWidget *caixa_busca;
GtkWidget **rows;
char **chaves_busca;
int n_linhas;
int ultimo_n;
long long ultima_assinatura;
} TelaReservas;

static const char *DIAS_SEMANA[] = {
"Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira",
"Sexta-feira",  "Sábado",       "Domingo"};

static void ao_editar(GtkButton *b, gpointer dados);

static void estilo_reservas(void) {
static gboolean carregado = FALSE;
if (carregado)
return;
carregado = TRUE;

GtkCssProvider *css = gtk_css_provider_new();
gtk_css_provider_load_from_string(
  css,
  ".cab-reservas { padding: 0 16px; }"
  ".cab-reservas label { color: #5b6b8a; font-weight: bold; font-size: "
  "12px; }"
  ".moldura-reservas { border: 1px solid #e1e6ef; border-radius: 6px; }"
  ".tab-reservas { background: #ffffff; }"
  ".tab-reservas row { padding: 10px 16px; border-bottom: 1px solid "
  "#e1e6ef; }"
  ".tab-reservas row:hover { background: #f4f6f9; }"
  ".tab-reservas row:selected { background: #e1ebf8; }"
  ".tab-reservas row:selected label { color: #12274f; }"
  ".negrito { font-weight: bold; }");
gtk_style_context_add_provider_for_display(
  gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
g_object_unref(css);
}

static GtkWidget *celula(const char *texto, int largura, int max_chars) {
GtkWidget *l = gtk_label_new(texto ? texto : "");
gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
gtk_widget_set_halign(l, GTK_ALIGN_START);
gtk_widget_set_valign(l, GTK_ALIGN_CENTER);
gtk_widget_set_hexpand(l, FALSE);
gtk_widget_set_vexpand(l, FALSE);
gtk_label_set_single_line_mode(GTK_LABEL(l), TRUE);
gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
gtk_label_set_width_chars(GTK_LABEL(l), max_chars);
gtk_label_set_max_width_chars(GTK_LABEL(l), max_chars);
gtk_widget_set_size_request(l, largura, -1);
return l;
}

static GtkWidget *linha(const char *a, const char *b, const char *c,
                    const char *d, const char *f, const char *g) {
GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
gtk_widget_set_hexpand(caixa, FALSE);

const char *textos[6] = {a, b, c, d, f, g};
const int larguras[6] = {130, 125, 300, 95, 85, 220};
const int max_chars[6] = {16, 15, 36, 11, 10, 28};

for (int i = 0; i < 6; i++) {
  gtk_box_append(GTK_BOX(caixa), celula(textos[i], larguras[i], max_chars[i]));
}
return caixa;
}

static int comparar(const void *pa, const void *pb) {
const ReservaVista *x = &g_reservas[*(const int *)pa];
const ReservaVista *y = &g_reservas[*(const int *)pb];
if (x->dia != y->dia)
return x->dia - y->dia;
if (x->ini != y->ini)
return x->ini - y->ini;
return strcmp(x->sala, y->sala);
}

static void periodo_reserva(const ReservaVista *r, char *buf, size_t tam) {
if (!r->isRecorrente || !dados_data_valida(r->dia)) {
dados_data_texto(r->dia, buf, tam);
return;
}

GDate *data =
  g_date_new_dmy(r->dia % 100, (r->dia / 100) % 100, r->dia / 10000);
GDateWeekday dia = g_date_get_weekday(data);
g_snprintf(buf, tam, "%s", DIAS_SEMANA[dia - G_DATE_MONDAY]);
g_date_free(data);
}

static void limpar_cache_linhas(TelaReservas *t) {
if (t->chaves_busca) {
for (int i = 0; i < t->n_linhas; i++)
  g_free(t->chaves_busca[i]);
g_free(t->chaves_busca);
t->chaves_busca = NULL;
}
g_free(t->rows);
t->rows = NULL;
t->n_linhas = 0;
}

static void aplicar_filtro_visual(TelaReservas *t) {
if (!t->lista || !t->rows || t->n_linhas <= 0) {
if (t->total) {
  char *txt =
      g_strdup_printf("0 reservas exibidas (de %d salvas)", g_n_reservas);
  gtk_label_set_text(GTK_LABEL(t->total), txt);
  g_free(txt);
}
return;
}

const char *raw =
  t->campo_busca ? gtk_editable_get_text(GTK_EDITABLE(t->campo_busca)) : "";
char *termo = g_utf8_strdown(raw ? raw : "", -1);
g_strstrip(termo);

int visiveis = 0;
GtkListBoxRow *primeira_visivel = NULL;

for (int i = 0; i < t->n_linhas; i++) {
gboolean casou = TRUE;
if (termo[0] != '\0') {
  casou = (t->chaves_busca[i] && strstr(t->chaves_busca[i], termo) != NULL);
}
gtk_widget_set_visible(t->rows[i], casou);
if (casou) {
  visiveis++;
  if (!primeira_visivel)
    primeira_visivel = GTK_LIST_BOX_ROW(t->rows[i]);
}
}

if (primeira_visivel) {
gtk_list_box_select_row(GTK_LIST_BOX(t->lista), primeira_visivel);
} else {
gtk_list_box_unselect_all(GTK_LIST_BOX(t->lista));
}

char *txt = NULL;
if (termo[0] != '\0') {
txt = g_strdup_printf("%d de %d reservas encontradas", visiveis,
                      g_n_reservas);
} else {
txt = g_strdup_printf("%d reservas registradas", g_n_reservas);
}
gtk_label_set_text(GTK_LABEL(t->total), txt);
g_free(txt);
g_free(termo);
}

static long long calcular_assinatura_reservas(void) {
long long s = 0;
for (int i = 0; i < g_n_reservas; i++) {
s += (long long)g_reservas[i].idReserva * 1315423911LL +
     (long long)g_reservas[i].dia * 2654435761LL +
     (long long)g_reservas[i].ini * 97 +
     (long long)g_reservas[i].fim;
}
return s;
}

static void recarregar(TelaReservas *t) {
long long sig = calcular_assinatura_reservas();
if (t->ultimo_n == g_n_reservas && t->ultima_assinatura == sig &&
  t->n_linhas == g_n_reservas && t->rows != NULL) {
aplicar_filtro_visual(t);
return;
}

t->ultimo_n = g_n_reservas;
t->ultima_assinatura = sig;
limpar_cache_linhas(t);

GtkListBoxRow *row;
while ((row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(t->lista), 0)) !=
     NULL)
gtk_list_box_remove(GTK_LIST_BOX(t->lista), GTK_WIDGET(row));

if (g_n_reservas <= 0) {
aplicar_filtro_visual(t);
return;
}

int *idx = g_new(int, g_n_reservas);
for (int i = 0; i < g_n_reservas; i++)
idx[i] = i;
qsort(idx, g_n_reservas, sizeof(int), comparar);

t->rows = g_new0(GtkWidget *, g_n_reservas);
t->chaves_busca = g_new0(char *, g_n_reservas);
t->n_linhas = g_n_reservas;

for (int k = 0; k < g_n_reservas; k++) {
const ReservaVista *r = &g_reservas[idx[k]];
char dia[32], a[8], b[8], hor[24];
periodo_reserva(r, dia, sizeof dia);
dados_hora_texto(r->ini, a, sizeof a);
dados_hora_texto(r->fim, b, sizeof b);
g_snprintf(hor, sizeof hor, "%s às %s", a, b);
GtkWidget *caixa =
    linha(dia, hor, r->atividade, r->sala, r->tipo, r->responsavel);
gtk_widget_add_css_class(gtk_widget_get_first_child(caixa), "negrito");
GtkWidget *item = gtk_list_box_row_new();
g_object_set_data(G_OBJECT(item), "id", GINT_TO_POINTER(r->idReserva));
gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(item), caixa);
gtk_list_box_append(GTK_LIST_BOX(t->lista), item);

t->rows[k] = item;
char *bruta = g_strdup_printf("%s %s %s %s %s %s #%d", dia, hor,
                              r->atividade, r->sala, r->tipo,
                              r->responsavel, r->idReserva);
t->chaves_busca[k] = g_utf8_strdown(bruta, -1);
g_free(bruta);
}
g_free(idx);
aplicar_filtro_visual(t);
}

static void ao_filtro_mudou(GtkEditable *editavel, gpointer dados) {
(void)editavel;
aplicar_filtro_visual((TelaReservas *)dados);
}

static void ao_buscar_reserva(GtkButton *b, gpointer dados) {
(void)b;
TelaReservas *t = (TelaReservas *)dados;
if (!t->caixa_busca || !t->campo_busca)
return;
if (!gtk_widget_get_visible(t->caixa_busca)) {
gtk_widget_set_visible(t->caixa_busca, TRUE);
gtk_widget_grab_focus(t->campo_busca);
} else {
const char *txt = gtk_editable_get_text(GTK_EDITABLE(t->campo_busca));
if (txt && txt[0] != '\0') {
  aplicar_filtro_visual(t);
} else {
  gtk_widget_set_visible(t->caixa_busca, FALSE);
  aplicar_filtro_visual(t);
}
}
}

static void ao_limpar_busca(GtkButton *b, gpointer dados) {
(void)b;
TelaReservas *t = (TelaReservas *)dados;
if (t->campo_busca)
gtk_editable_set_text(GTK_EDITABLE(t->campo_busca), "");
if (t->caixa_busca)
gtk_widget_set_visible(t->caixa_busca, FALSE);
aplicar_filtro_visual(t);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) {
(void)w;
recarregar((TelaReservas *)dados);
}

static void ao_nova(GtkButton *b, gpointer dados) {
(void)b;
interface_ir_para(((TelaReservas *)dados)->e, "nova");
}

GtkWidget *tela_reservas_criar(Estado *e) {
TelaReservas *t = g_new0(TelaReservas, 1);
t->e = e;
t->ultimo_n = -1;
estilo_reservas();

GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
gtk_widget_set_margin_top(pagina, 28);
gtk_widget_set_margin_bottom(pagina, 24);
gtk_widget_set_margin_start(pagina, 32);
gtk_widget_set_margin_end(pagina, 32);

GtkWidget *topo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
gtk_widget_set_hexpand(textos, TRUE);
GtkWidget *titulo = gtk_label_new("Reservas");
gtk_widget_add_css_class(titulo, "titulo");
gtk_label_set_xalign(GTK_LABEL(titulo), 0);
t->total = gtk_label_new("");
gtk_widget_add_css_class(t->total, "subtitulo");
gtk_label_set_xalign(GTK_LABEL(t->total), 0);
gtk_box_append(GTK_BOX(textos), titulo);
gtk_box_append(GTK_BOX(textos), t->total);

GtkWidget *b_buscar = gtk_button_new_with_label("Buscar reserva");
gtk_widget_add_css_class(b_buscar, "secundario");
gtk_widget_set_valign(b_buscar, GTK_ALIGN_START);
g_signal_connect(b_buscar, "clicked", G_CALLBACK(ao_buscar_reserva), t);

GtkWidget *b_ed = gtk_button_new_with_label("Editar reserva");
gtk_widget_add_css_class(b_ed, "secundario");
gtk_widget_set_valign(b_ed, GTK_ALIGN_START);
g_signal_connect(b_ed, "clicked", G_CALLBACK(ao_editar), t);

GtkWidget *b = gtk_button_new_with_label("Nova reserva");
gtk_widget_add_css_class(b, "primario");
gtk_widget_set_valign(b, GTK_ALIGN_START);
g_signal_connect(b, "clicked", G_CALLBACK(ao_nova), t);

gtk_box_append(GTK_BOX(topo), textos);
gtk_box_append(GTK_BOX(topo), b_buscar);
gtk_box_append(GTK_BOX(topo), b_ed);
gtk_box_append(GTK_BOX(topo), b);
gtk_box_append(GTK_BOX(pagina), topo);

t->caixa_busca = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
gtk_widget_set_visible(t->caixa_busca, FALSE);
t->campo_busca = gtk_entry_new();
gtk_entry_set_placeholder_text(
  GTK_ENTRY(t->campo_busca),
  "Buscar reserva salva por atividade, sala, tipo ou responsável...");
gtk_widget_set_hexpand(t->campo_busca, TRUE);
g_signal_connect(t->campo_busca, "changed", G_CALLBACK(ao_filtro_mudou), t);

GtkWidget *b_limpar = gtk_button_new_with_label("Limpar");
gtk_widget_add_css_class(b_limpar, "secundario");
g_signal_connect(b_limpar, "clicked", G_CALLBACK(ao_limpar_busca), t);

gtk_box_append(GTK_BOX(t->caixa_busca), t->campo_busca);
gtk_box_append(GTK_BOX(t->caixa_busca), b_limpar);
gtk_box_append(GTK_BOX(pagina), t->caixa_busca);

GtkWidget *cab =
  linha("Período", "Horário", "Atividade", "Sala", "Tipo", "Responsável");
gtk_widget_add_css_class(cab, "cab-reservas");
gtk_box_append(GTK_BOX(pagina), cab);

t->lista = gtk_list_box_new();
gtk_widget_add_css_class(t->lista, "tab-reservas");
GtkWidget *rolagem = gtk_scrolled_window_new();
gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), t->lista);
gtk_widget_set_vexpand(rolagem, TRUE);
gtk_widget_add_css_class(rolagem, "moldura-reservas");
gtk_box_append(GTK_BOX(pagina), rolagem);

g_signal_connect(pagina, "map", G_CALLBACK(ao_mostrar), t);
recarregar(t);
return pagina;
}

static void ao_editar(GtkButton *b, gpointer dados) {
(void)b;
TelaReservas *t = (TelaReservas *)dados;
GtkListBoxRow *linha = gtk_list_box_get_selected_row(GTK_LIST_BOX(t->lista));
if (!linha) {
interface_mensagem(t->e, "Selecione uma reserva na lista para editar.",
                   TRUE);
return;
}
nova_reserva_editar(
  GPOINTER_TO_INT(g_object_get_data(G_OBJECT(linha), "id")));
interface_ir_para(t->e, "nova");
}
