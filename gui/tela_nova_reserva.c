#include "../include/aplicacao.h"
#include "interface.h"
#include <string.h>

#define PRIMEIRA_HORA 420 /* 07:00 */
#define PASSO 30
#define NUM_HORARIOS 31 /* 07:00 .. 22:00 */
#define FAIXA_INI 420
#define FAIXA_TOT 900 /* 07:00 .. 22:00 */

static const char *TIPOS[] = {"Monitoria", "Extensão", "Evento",
                              "Aula",      "Outro",    NULL};
static const char *REPETICOES[] = {"Só nesta data",
                                   "Toda semana até o fim do ano", NULL};

typedef struct {
  Estado *e;
  GtkWidget *atividade, *tipo, *responsavel, *sala;
  GtkWidget *data, *h_ini, *h_fim, *repeticao;
  GtkWidget *status, *motivo, *l_proj, *l_hor, *l_dia, *faixa;
  ReservaVista r;
  VerificacaoReserva v;
  gboolean montando;
  GtkWidget *b_voltar;
  gboolean da_busca, pend_busca;
  int editando_id;
  GtkWidget *titulo_pag, *b_salvar_w;
  GtkWidget *sugestoes, *lista_sugestoes; // nomes ja usados em Responsavel
  GtkWidget *caixa_cancelados, *resumo_cancelados; // so ao editar aula semanal
  GtkWidget *rotulo_dia, *info_repeticao;
} TelaNova;

static const char *DIAS_SEMANA[] = {"domingo", "segunda", "terça", "quarta",
                                    "quinta",  "sexta",   "sábado"};

// "segunda", "terça"... a partir de uma data AAAAMMDD
static const char *dia_da_semana(int dia) {
  GDate *d = g_date_new_dmy(dia % 100, (dia / 100) % 100, dia / 10000);
  int semana = g_date_get_weekday(d) % 7;
  g_date_free(d);
  return DIAS_SEMANA[semana];
}

static TelaNova *g_nova = NULL;

static void estilo_nova(void) {
  static gboolean carregado = FALSE;
  if (carregado)
    return;
  carregado = TRUE;
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_string(
      css, ".painel-verif { border: 1px solid #e1e6ef; border-radius: 6px; "
           "padding: 18px; min-width: 320px; }"
           ".status-ok { color: #1e7e4a; font-family: Georgia, serif; "
           "font-size: 20px; font-weight: bold; }"
           ".status-erro { color: #c8102e; font-family: Georgia, serif; "
           "font-size: 20px; font-weight: bold; }"
           ".check-ok { color: #1e7e4a; }"
           ".check-erro { color: #c8102e; }"
           ".motivo { color: #5b6b8a; }"
           ".caixa-cancelados { border: 1px solid #e1e6ef; border-radius: 6px; "
           "padding: 10px 12px; }"
           "button.secundario { background: #ffffff; background-image: none; "
           "color: #12274f;"
           "  border: 1px solid #1f4e8c; border-radius: 4px; font-weight: "
           "bold; padding: 10px 20px; box-shadow: none; }");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);
}

static GtkWidget *rotulo(const char *txt) {
  GtkWidget *l = gtk_label_new(txt);
  gtk_widget_add_css_class(l, "subtitulo");
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  return l;
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
  lista_ligar_busca(d);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(d), selecionado);
  return d;
}

static guint indice_horario(GtkWidget *d) {
  guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(d));
  return i >= NUM_HORARIOS ? 0 : i;
}

static void ler_formulario(TelaNova *t, ReservaVista *r) {
  memset(r, 0, sizeof *r);
  g_strlcpy(r->atividade, gtk_editable_get_text(GTK_EDITABLE(t->atividade)),
            sizeof r->atividade);
  g_strlcpy(r->responsavel, gtk_editable_get_text(GTK_EDITABLE(t->responsavel)),
            sizeof r->responsavel);
  g_strstrip(r->atividade);
  g_strstrip(r->responsavel);

  guint tp = gtk_drop_down_get_selected(GTK_DROP_DOWN(t->tipo));
  g_strlcpy(r->tipo, TIPOS[tp > 4 ? 0 : tp], sizeof r->tipo);

  GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
  if (it)
    g_strlcpy(r->sala, gtk_string_object_get_string(GTK_STRING_OBJECT(it)),
              sizeof r->sala);

  r->dia = seletor_data_get(t->data);
  r->ini = PRIMEIRA_HORA + PASSO * (int)indice_horario(t->h_ini);
  r->fim = PRIMEIRA_HORA + PASSO * (int)indice_horario(t->h_fim);

  r->isRecorrente =
      gtk_drop_down_get_selected(GTK_DROP_DOWN(t->repeticao)) == 1;
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
  if (t->montando)
    return;
  ler_formulario(t, &t->r);
  t->r.idReserva = t->editando_id;
  VerificacaoReserva *v = &t->v;
  dados_verificar_reserva(&t->r, v);

  // aula semanal: a data escolhida e so o comeco da repeticao
  gtk_label_set_text(GTK_LABEL(t->rotulo_dia),
                     t->r.isRecorrente ? "Começa em" : "Dia");

  gtk_label_set_text(GTK_LABEL(t->status), v->titulo);
  gtk_widget_remove_css_class(t->status, "status-ok");
  gtk_widget_remove_css_class(t->status, "status-erro");
  gtk_widget_add_css_class(t->status, v->ok ? "status-ok" : "status-erro");
  gtk_label_set_text(GTK_LABEL(t->motivo), v->motivo);

  if (v->sala_idx < 0) {
    apagar_linha(t->l_proj);
    apagar_linha(t->l_hor);
    apagar_linha(t->l_dia);
  } else {
    char txt[160], a[8], b[8], dia[16];
    definir_linha(t->l_proj, v->projetor,
                  v->projetor ? "A sala tem projetor"
                              : "A sala não tem projetor");

    if (!dados_data_valida(t->r.dia)) {
      apagar_linha(t->l_hor);
      definir_linha(t->l_dia, FALSE, "Data inválida");
    } else {
      dados_data_texto(t->r.dia, dia, sizeof dia);
      dia[5] = '\0';
      if (t->r.isRecorrente)
        g_snprintf(txt, sizeof txt, "Toda %s na %s, a partir de %s",
                   dia_da_semana(t->r.dia), t->r.sala, dia);
      else
        g_snprintf(txt, sizeof txt, "Dia %s na %s", dia, t->r.sala);
      definir_linha(t->l_dia, TRUE, txt);
      if (t->r.fim <= t->r.ini) {
        definir_linha(t->l_hor, FALSE, "Horário final antes do inicial");
      } else if (v->conflito) {
        g_snprintf(txt, sizeof txt, "Horário em uso por %s",
                   v->atividade_conflito);
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
  double x = (min - FAIXA_INI) * (double)w / FAIXA_TOT;
  if (x < 0)
    x = 0;
  if (x > w)
    x = w;
  return x;
}

static void desenhar_faixa(GtkDrawingArea *area, cairo_t *cr, int w, int h,
                           gpointer dados) {
  TelaNova *t = (TelaNova *)dados;
  int barra = h - 18;

  cairo_set_source_rgb(cr, 0.882, 0.902, 0.937);
  cairo_rectangle(cr, 0, 0, w, barra);
  cairo_fill(cr);

  if (t->v.sala_idx >= 0 && dados_data_valida(t->r.dia)) {
    for (int i = 0; i < g_n_reservas; i++) {
      const ReservaVista *o = &g_reservas[i];
      if (!dados_reserva_ocorre_em(o, t->r.dia) ||
          strcmp(o->sala, t->r.sala) != 0)
        continue;
      cairo_set_source_rgb(cr, 0.624, 0.702, 0.831);
      cairo_rectangle(cr, px(o->ini, w), 0, px(o->fim, w) - px(o->ini, w),
                      barra);
      cairo_fill(cr);
    }
    if (t->r.fim > t->r.ini) {
      if (t->v.ok)
        cairo_set_source_rgb(cr, 0.071, 0.153, 0.310);
      else
        cairo_set_source_rgb(cr, 0.784, 0.063, 0.180);
      cairo_rectangle(cr, px(t->r.ini, w), 0, px(t->r.fim, w) - px(t->r.ini, w),
                      barra);
      cairo_fill(cr);
    }
  }

  cairo_set_source_rgb(cr, 0.357, 0.420, 0.541);
  cairo_set_font_size(cr, 11);
  int marcas[4] = {8, 12, 16, 20};
  for (int i = 0; i < 4; i++) {
    char txt[8];
    g_snprintf(txt, sizeof txt, "%dh", marcas[i]);
    cairo_move_to(cr, px(marcas[i] * 60, w), h - 4);
    cairo_show_text(cr, txt);
  }
}

/* recarrega a lista de salas (uma sala nova cadastrada em "Salas" aparece aqui)
 */
static void recarregar_salas(TelaNova *t) {
  char anterior[64] = "";
  GObject *it = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(t->sala));
  if (it)
    g_strlcpy(anterior, gtk_string_object_get_string(GTK_STRING_OBJECT(it)),
              sizeof anterior);

  const char **nomes = g_new(const char *, g_n_salas + 1);
  for (int i = 0; i < g_n_salas; i++)
    nomes[i] = g_salas[i].nome;
  nomes[g_n_salas] = NULL;

  GtkStringList *sl =
      GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(t->sala)));
  t->montando = TRUE;
  gtk_string_list_splice(sl, 0, g_list_model_get_n_items(G_LIST_MODEL(sl)),
                         nomes);
  g_free(nomes);
  guint pos = 0;
  for (int i = 0; i < g_n_salas; i++)
    if (strcmp(g_salas[i].nome, anterior) == 0)
      pos = i;
  if (g_n_salas > 0)
    gtk_drop_down_set_selected(GTK_DROP_DOWN(t->sala), pos);
  t->montando = FALSE;
}

static void limpar(TelaNova *t) {
  t->editando_id = 0;
  if (t->caixa_cancelados) { // reserva nova: volta a escolher a repeticao
    gtk_widget_set_visible(t->caixa_cancelados, FALSE);
    gtk_widget_set_visible(t->info_repeticao, FALSE);
    gtk_widget_set_visible(t->repeticao, TRUE);
  }
  t->montando = TRUE;
  gtk_widget_set_sensitive(t->data, TRUE);
  gtk_editable_set_text(GTK_EDITABLE(t->atividade), "");
  gtk_editable_set_text(GTK_EDITABLE(t->responsavel), "");
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->tipo), 0);
  seletor_data_set(t->data, dados_data_hoje());
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_ini),
                             (14 * 60 - PRIMEIRA_HORA) / PASSO);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_fim),
                             (16 * 60 - PRIMEIRA_HORA) / PASSO);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->repeticao), 0);
  t->montando = FALSE;
  atualizar(t);
}

static void ao_mudou(GObject *o, GParamSpec *p, gpointer dados) {
  atualizar((TelaNova *)dados);
}
static void ao_data(gpointer dados) { atualizar((TelaNova *)dados); }

/* ---- Dias cancelados (so aparece ao editar uma reserva semanal) ---- */

static void atualizar_cancelados(TelaNova *t) {
  int i = t->editando_id > 0 ? dados_indice_reserva(t->editando_id) : -1;
  gboolean semanal = i >= 0 && g_reservas[i].isRecorrente;
  gtk_widget_set_visible(t->caixa_cancelados, semanal);

  // ao editar aula semanal, a repeticao nao muda: mostra como ela e
  gtk_widget_set_visible(t->repeticao, !semanal);
  gtk_widget_set_visible(t->info_repeticao, semanal);
  if (!semanal)
    return;

  const ReservaVista *r = &g_reservas[i];
  char ini[8], fim[8], ate[16];
  dados_hora_texto(r->ini, ini, sizeof ini);
  dados_hora_texto(r->fim, fim, sizeof fim);
  dados_data_texto(r->fimRecorrencia, ate, sizeof ate);
  char *repeticao = g_strdup_printf("Toda %s, das %s às %s, até %s",
                                    dia_da_semana(r->dia), ini, fim, ate);
  gtk_label_set_text(GTK_LABEL(t->info_repeticao), repeticao);
  g_free(repeticao);

  int dias[12];
  const char *motivos[12];
  int n = dados_cancelamentos(t->editando_id, dias, motivos, 12);
  GString *texto = g_string_new(NULL);
  if (n == 0)
    g_string_append(texto, "Nenhuma. Use para feriado, falta do professor ou "
                           "prova em outra sala.");
  for (int k = 0; k < n; k++) {
    g_string_append_printf(texto, "%s%02d/%02d", k ? ", " : "", dias[k] % 100,
                           (dias[k] / 100) % 100);
    if (motivos[k][0])
      g_string_append_printf(texto, " (%s)", motivos[k]);
  }
  gtk_label_set_text(GTK_LABEL(t->resumo_cancelados), texto->str);
  g_string_free(texto, TRUE);
}

static void ao_fechar_cancelar_dia(GtkWidget *janela, gpointer dados) {
  (void)janela;
  atualizar_cancelados((TelaNova *)dados);
}

static void ao_cancelar_dia(GtkButton *b, gpointer dados) {
  (void)b;
  TelaNova *t = (TelaNova *)dados;
  GtkWidget *janela = cancelar_dia_abrir(t->e, t->editando_id);
  if (janela)
    g_signal_connect(janela, "destroy", G_CALLBACK(ao_fechar_cancelar_dia), t);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) {
  TelaNova *t = (TelaNova *)dados;
  recarregar_salas(t);
  t->da_busca = t->pend_busca;
  t->pend_busca = FALSE;
  gtk_button_set_label(GTK_BUTTON(t->b_voltar), t->da_busca
                                                    ? "Voltar para a busca"
                                                    : "Voltar para Reservas");
  gtk_label_set_text(GTK_LABEL(t->titulo_pag),
                     t->editando_id > 0 ? "Editar reserva" : "Nova reserva");
  gtk_button_set_label(GTK_BUTTON(t->b_salvar_w), t->editando_id > 0
                                                      ? "Salvar alterações"
                                                      : "Salvar reserva");
  gtk_widget_set_sensitive(t->repeticao, t->editando_id == 0);
  atualizar_cancelados(t);
  atualizar(t);
}

static void ao_salvar(GtkButton *b, gpointer dados) {
  TelaNova *t = (TelaNova *)dados;
  ReservaVista r;
  char msg[240];
  ler_formulario(t, &r);
  acao_log("salvar_reserva id:%d atividade:%s tipo:%s responsavel:%s sala:%s "
           "dia:%d inicio:%s fim:%s semanal:%d",
           t->editando_id, r.atividade, r.tipo, r.responsavel, r.sala, r.dia,
           acao_hora(r.ini), acao_hora(r.fim), r.isRecorrente);
  r.idReserva = t->editando_id;
  if (dados_salvar_reserva(&r, msg, sizeof msg) < 0) {
    interface_mensagem(t->e, msg, TRUE);
    return;
  }
  interface_mensagem(t->e, msg, FALSE);
  limpar(t);
  interface_ir_para(t->e, "reservas");
}

static void ao_descartar(GtkButton *b, gpointer dados) {
  TelaNova *t = (TelaNova *)dados;
  limpar(t);
  interface_mensagem(t->e, "Reserva descartada. Nada foi salvo.", FALSE);
  interface_ir_para(t->e, "reservas");
}

static void ao_voltar(GtkButton *b, gpointer dados) {
  {
    TelaNova *t = (TelaNova *)dados;
    interface_ir_para(t->e, t->da_busca ? "busca" : "reservas");
  }
}

static GtkWidget *nova_linha_check(GtkWidget *caixa) {
  GtkWidget *l = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_box_append(GTK_BOX(caixa), l);
  return l;
}

/* ---- Sugestao de nomes no campo Responsavel ----
 * Ao digitar 3 letras ou mais, mostra ate 6 nomes que ja aparecem em outras
 * reservas (a maioria veio do PDF). g_str_match_string ignora maiusculas e
 * acentos e compara pelo inicio de cada palavra: "leni" acha "LENIN ...". */
#define MAX_SUGESTOES 6

static int comparar_nomes(gconstpointer a, gconstpointer b) {
  return g_utf8_collate(*(const char *const *)a, *(const char *const *)b);
}

static void escolher_sugestao(TelaNova *t, GtkListBoxRow *linha) {
  const char *nome = g_object_get_data(G_OBJECT(linha), "nome");
  gtk_popover_popdown(GTK_POPOVER(t->sugestoes));
  t->montando = TRUE; // evita abrir as sugestoes de novo
  gtk_editable_set_text(GTK_EDITABLE(t->responsavel), nome);
  gtk_editable_set_position(GTK_EDITABLE(t->responsavel), -1);
  t->montando = FALSE;
}

static void ao_clicar_sugestao(GtkListBox *lista, GtkListBoxRow *linha,
                               gpointer dados) {
  (void)lista;
  escolher_sugestao((TelaNova *)dados, linha);
}

static void ao_digitar_responsavel(GtkEditable *campo, gpointer dados) {
  TelaNova *t = (TelaNova *)dados;
  const char *texto = gtk_editable_get_text(campo);
  gtk_popover_popdown(GTK_POPOVER(t->sugestoes));
  if (t->montando || g_utf8_strlen(texto, -1) < 3)
    return;

  // junta os nomes que combinam, sem repetir
  GHashTable *vistos = g_hash_table_new(g_str_hash, g_str_equal);
  GPtrArray *nomes = g_ptr_array_new();
  for (int i = 0; i < g_n_reservas; i++) {
    const char *nome = g_reservas[i].responsavel;
    if (nome[0] == '\0' || g_hash_table_contains(vistos, nome))
      continue;
    if (g_str_match_string(texto, nome, TRUE)) {
      g_hash_table_add(vistos, (gpointer)nome);
      g_ptr_array_add(nomes, (gpointer)nome);
    }
  }
  g_ptr_array_sort(nomes, comparar_nomes);

  // limpa a lista antiga
  GtkWidget *filho;
  while ((filho = gtk_widget_get_first_child(t->lista_sugestoes)) != NULL)
    gtk_list_box_remove(GTK_LIST_BOX(t->lista_sugestoes), filho);

  guint total = nomes->len < MAX_SUGESTOES ? nomes->len : MAX_SUGESTOES;
  gboolean so_o_proprio =
      nomes->len == 1 && g_ascii_strcasecmp(nomes->pdata[0], texto) == 0;
  for (guint i = 0; i < total && !so_o_proprio; i++) {
    GtkWidget *rotulo = gtk_label_new(nomes->pdata[i]);
    gtk_label_set_xalign(GTK_LABEL(rotulo), 0);
    GtkWidget *linha = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(linha), rotulo);
    // a linha nao pega o foco, para o cursor continuar no campo
    gtk_widget_set_focusable(linha, FALSE);
    g_object_set_data_full(G_OBJECT(linha), "nome", g_strdup(nomes->pdata[i]),
                           g_free);
    gtk_list_box_append(GTK_LIST_BOX(t->lista_sugestoes), linha);
  }
  if (total > 0 && !so_o_proprio)
    gtk_popover_popup(GTK_POPOVER(t->sugestoes));

  g_ptr_array_free(nomes, TRUE);
  g_hash_table_destroy(vistos);
}

/* Enter no campo escolhe a primeira sugestao. */
static void ao_enter_responsavel(GtkEntry *campo, gpointer dados) {
  (void)campo;
  TelaNova *t = (TelaNova *)dados;
  if (!gtk_widget_get_visible(t->sugestoes))
    return;
  GtkListBoxRow *primeira =
      gtk_list_box_get_row_at_index(GTK_LIST_BOX(t->lista_sugestoes), 0);
  if (primeira)
    escolher_sugestao(t, primeira);
}

static void ao_sair_responsavel(GtkEventControllerFocus *foco, gpointer dados) {
  (void)foco;
  gtk_popover_popdown(GTK_POPOVER(((TelaNova *)dados)->sugestoes));
}

static void criar_sugestoes(TelaNova *t) {
  t->lista_sugestoes = gtk_list_box_new();
  gtk_widget_set_focusable(t->lista_sugestoes, FALSE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(t->lista_sugestoes),
                                            TRUE);
  g_signal_connect(t->lista_sugestoes, "row-activated",
                   G_CALLBACK(ao_clicar_sugestao), t);

  t->sugestoes = gtk_popover_new();
  gtk_popover_set_child(GTK_POPOVER(t->sugestoes), t->lista_sugestoes);
  gtk_popover_set_autohide(GTK_POPOVER(t->sugestoes), FALSE);
  gtk_popover_set_has_arrow(GTK_POPOVER(t->sugestoes), FALSE);
  gtk_popover_set_position(GTK_POPOVER(t->sugestoes), GTK_POS_BOTTOM);
  gtk_widget_set_halign(t->sugestoes, GTK_ALIGN_START);
  gtk_widget_set_parent(t->sugestoes, t->responsavel);

  g_signal_connect(t->responsavel, "changed",
                   G_CALLBACK(ao_digitar_responsavel), t);
  g_signal_connect(t->responsavel, "activate",
                   G_CALLBACK(ao_enter_responsavel), t);
  GtkEventController *foco = gtk_event_controller_focus_new();
  g_signal_connect(foco, "leave", G_CALLBACK(ao_sair_responsavel), t);
  gtk_widget_add_controller(t->responsavel, foco);
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
  t->titulo_pag = titulo;
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

  const char *vazio[] = {NULL};
  t->atividade = gtk_entry_new();
  t->responsavel = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(t->atividade), 49);
  gtk_entry_set_max_length(GTK_ENTRY(t->responsavel), 60);
  criar_sugestoes(t);
  t->tipo = gtk_drop_down_new_from_strings(TIPOS);
  t->sala = gtk_drop_down_new_from_strings(vazio);
  lista_ligar_busca(t->sala);
  t->data = seletor_data_criar(dados_data_hoje(), ao_data, t);
  t->h_ini = lista_horarios((14 * 60 - PRIMEIRA_HORA) / PASSO);
  t->h_fim = lista_horarios((16 * 60 - PRIMEIRA_HORA) / PASSO);
  t->repeticao = gtk_drop_down_new_from_strings(REPETICOES);

  GtkWidget *data = t->data;
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

  gtk_grid_attach(GTK_GRID(grade), rotulo("Atividade"), 0, 0, 2, 1);
  gtk_grid_attach(GTK_GRID(grade), t->atividade, 0, 1, 2, 1);
  gtk_grid_attach(GTK_GRID(grade), rotulo("Responsável"), 0, 2, 2, 1);
  gtk_grid_attach(GTK_GRID(grade), t->responsavel, 0, 3, 2, 1);
  gtk_grid_attach(GTK_GRID(grade), rotulo("Tipo"), 0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), rotulo("Sala"), 1, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), t->tipo, 0, 5, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), t->sala, 1, 5, 1, 1);
  t->rotulo_dia = rotulo("Dia");
  gtk_grid_attach(GTK_GRID(grade), t->rotulo_dia, 0, 6, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), rotulo("Horário"), 1, 6, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), data, 0, 7, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), horas, 1, 7, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), rotulo("Repetição"), 0, 8, 2, 1);
  t->info_repeticao = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(t->info_repeticao), 0);
  gtk_widget_add_css_class(t->info_repeticao, "texto-destaque");
  gtk_widget_set_visible(t->info_repeticao, FALSE);
  GtkWidget *repeticao = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(repeticao), t->repeticao);
  gtk_box_append(GTK_BOX(repeticao), t->info_repeticao);
  gtk_grid_attach(GTK_GRID(grade), repeticao, 0, 9, 2, 1);

  GtkWidget *botoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_margin_top(botoes, 14);
  GtkWidget *b_salvar = gtk_button_new_with_label("Salvar reserva");
  t->b_salvar_w = b_salvar;
  gtk_widget_add_css_class(b_salvar, "primario");
  g_signal_connect(b_salvar, "clicked", G_CALLBACK(ao_salvar), t);
  GtkWidget *b_desc = gtk_button_new_with_label("Descartar");
  gtk_widget_add_css_class(b_desc, "secundario");
  g_signal_connect(b_desc, "clicked", G_CALLBACK(ao_descartar), t);
  gtk_box_append(GTK_BOX(botoes), b_salvar);
  gtk_box_append(GTK_BOX(botoes), b_desc);

  // feriado, falta do professor: tira so uma semana, sem mexer nas outras
  t->caixa_cancelados = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_top(t->caixa_cancelados, 6);
  gtk_widget_add_css_class(t->caixa_cancelados, "caixa-cancelados");
  GtkWidget *linha_cancelados = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  t->resumo_cancelados = gtk_label_new("");
  gtk_widget_add_css_class(t->resumo_cancelados, "motivo");
  gtk_label_set_xalign(GTK_LABEL(t->resumo_cancelados), 0);
  gtk_label_set_wrap(GTK_LABEL(t->resumo_cancelados), TRUE);
  gtk_widget_set_hexpand(t->resumo_cancelados, TRUE);
  GtkWidget *b_cancelar_dia = gtk_button_new_with_label("Cancelar um dia");
  gtk_widget_add_css_class(b_cancelar_dia, "secundario");
  gtk_widget_set_tooltip_text(b_cancelar_dia,
                              "Feriado, falta do professor ou prova em outra "
                              "sala: tira só uma semana");
  g_signal_connect(b_cancelar_dia, "clicked", G_CALLBACK(ao_cancelar_dia), t);
  gtk_box_append(GTK_BOX(linha_cancelados), t->resumo_cancelados);
  gtk_box_append(GTK_BOX(linha_cancelados), b_cancelar_dia);
  gtk_box_append(GTK_BOX(t->caixa_cancelados), rotulo("Semanas sem aula"));
  gtk_box_append(GTK_BOX(t->caixa_cancelados), linha_cancelados);
  gtk_widget_set_visible(t->caixa_cancelados, FALSE);
  gtk_grid_attach(GTK_GRID(grade), t->caixa_cancelados, 0, 10, 2, 1);

  gtk_grid_attach(GTK_GRID(grade), botoes, 0, 11, 2, 1);
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

  t->l_proj = nova_linha_check(painel);
  t->l_hor = nova_linha_check(painel);
  t->l_dia = nova_linha_check(painel);

  t->faixa = gtk_drawing_area_new();
  gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(t->faixa), 290);
  gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(t->faixa), 48);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(t->faixa), desenhar_faixa, t,
                                 NULL);
  gtk_widget_set_margin_top(t->faixa, 10);
  gtk_box_append(GTK_BOX(painel), t->faixa);
  gtk_box_append(GTK_BOX(corpo), painel);

  /* o painel se atualiza a cada mudança no formulário */
  g_signal_connect(t->sala, "notify::selected", G_CALLBACK(ao_mudou), t);
  g_signal_connect(t->h_ini, "notify::selected", G_CALLBACK(ao_mudou), t);
  g_signal_connect(t->h_fim, "notify::selected", G_CALLBACK(ao_mudou), t);
  g_signal_connect(t->repeticao, "notify::selected", G_CALLBACK(ao_mudou), t);

  GtkWidget *rolagem = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rolagem), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), pagina);
  g_signal_connect(rolagem, "map", G_CALLBACK(ao_mostrar), t);

  recarregar_salas(t);
  atualizar(t);
  return rolagem;
}

/* Chamada pela tela "Buscar salas": deixa o formulário já preenchido */
void nova_reserva_preencher(const char *sala, int dia, int ini, int fim) {
  TelaNova *t = g_nova;
  if (!t)
    return;
  recarregar_salas(t);
  limpar(t);
  t->montando = TRUE;
  for (int i = 0; i < g_n_salas; i++)
    if (strcmp(g_salas[i].nome, sala) == 0)
      gtk_drop_down_set_selected(GTK_DROP_DOWN(t->sala), i);
  seletor_data_set(t->data, dia);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_ini),
                             (ini - PRIMEIRA_HORA) / PASSO);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_fim),
                             (fim - PRIMEIRA_HORA) / PASSO);
  t->montando = FALSE;
  t->pend_busca = TRUE;
  atualizar(t);
}

/* Chamada pela tela "Reservas": abre o formulário preenchido para editar */
void nova_reserva_editar(int id) {
  TelaNova *t = g_nova;
  int i = dados_indice_reserva(id);
  if (!t || i < 0)
    return;
  const ReservaVista *r = &g_reservas[i];

  recarregar_salas(t);
  limpar(t);
  t->montando = TRUE;
  gtk_editable_set_text(GTK_EDITABLE(t->atividade), r->atividade);
  gtk_editable_set_text(GTK_EDITABLE(t->responsavel), r->responsavel);

  for (guint k = 0; TIPOS[k] != NULL; k++)
    if (strcmp(TIPOS[k], r->tipo) == 0)
      gtk_drop_down_set_selected(GTK_DROP_DOWN(t->tipo), k);
  for (int k = 0; k < g_n_salas; k++)
    if (strcmp(g_salas[k].nome, r->sala) == 0)
      gtk_drop_down_set_selected(GTK_DROP_DOWN(t->sala), k);

  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->repeticao),
                             r->isRecorrente ? 1 : 0);
  seletor_data_set(t->data, r->dia);
  Aplicacao *app = aplicacao_padrao();
  const Reserva *reserva = aplicacao_buscar_reserva(app, id);
  gtk_widget_set_sensitive(t->data, reserva == NULL || !reserva->isRecorrente);

  int a = (r->ini - PRIMEIRA_HORA) / PASSO;
  int b = (r->fim - PRIMEIRA_HORA) / PASSO;
  if (a < 0)
    a = 0;
  if (a >= NUM_HORARIOS)
    a = NUM_HORARIOS - 1;
  if (b < 0)
    b = 0;
  if (b >= NUM_HORARIOS)
    b = NUM_HORARIOS - 1;
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_ini), a);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(t->h_fim), b);

  t->editando_id = id;
  t->montando = FALSE;
  atualizar_cancelados(t);
  atualizar(t);
}
