#include "../include/aplicacao.h"
#include "interface.h"

/* Janela "Pendências da importação": aulas do PDF que não entraram
 * (conflito, sala não cadastrada ou sem sala no PDF). Cada uma pode ser
 * resolvida, escolhendo uma sala livre, ou descartada. */

static const char *DIAS[] = {"seg", "ter", "qua", "qui", "sex", "sáb", "dom"};

typedef struct {
  Estado *e;
  GtkWidget *janela, *lista, *resumo;
} Pendencias;

typedef struct {
  Pendencias *p;
  int id;
  GtkWidget *janela, *salas;
  int idsSalas[256];
} Resolver;

static void preencher(Pendencias *p);

static const char *texto_motivo(MotivoPendencia motivo) {
  switch (motivo) {
  case PEND_CONFLITO:
    return "Conflito";
  case PEND_SEM_SALA:
    return "Sala não cadastrada";
  default:
    return "Sem sala no PDF";
  }
}

static const Pendencia *buscar(int id) {
  Aplicacao *app = aplicacao_padrao();
  for (size_t i = 0; app && i < app->quantidadePendencias; i++)
    if (app->pendencias[i].id == id)
      return &app->pendencias[i];
  return NULL;
}

static GtkWidget *rotulo(const char *texto, const char *classe) {
  GtkWidget *l = gtk_label_new(texto);
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_label_set_wrap(GTK_LABEL(l), TRUE);
  if (classe)
    gtk_widget_add_css_class(l, classe);
  return l;
}

/* ---- Janelinha "Resolver": escolher uma sala livre ---- */

static void ao_salvar_resolver(GtkButton *b, gpointer dados) {
  (void)b;
  Resolver *r = (Resolver *)dados;
  guint escolhida = gtk_drop_down_get_selected(GTK_DROP_DOWN(r->salas));
  if (escolhida == GTK_INVALID_LIST_POSITION)
    return;

  if (!aplicacao_resolver_pendencia(aplicacao_padrao(), r->id,
                                    r->idsSalas[escolhida])) {
    interface_mensagem(r->p->e, "Não foi possível salvar nessa sala.", TRUE);
    return;
  }
  dados_recarregar(); // a reserva nova aparece nas outras telas
  interface_mensagem(r->p->e, "Pendência resolvida: a aula foi reservada.",
                     FALSE);
  preencher(r->p);
  gtk_window_destroy(GTK_WINDOW(r->janela));
}

static void ao_fechar_resolver(GtkButton *b, gpointer dados) {
  (void)b;
  gtk_window_destroy(GTK_WINDOW(((Resolver *)dados)->janela));
}

static void abrir_resolver(Pendencias *p, int id) {
  const Pendencia *pend = buscar(id);
  Aplicacao *app = aplicacao_padrao();
  if (pend == NULL || app == NULL)
    return;

  Resolver *r = g_new0(Resolver, 1);
  r->p = p;
  r->id = id;

  // so as salas livres nesse horario, em todas as semanas da aula
  GtkStringList *nomes = gtk_string_list_new(NULL);
  int livres = 0;
  for (size_t i = 0; i < app->quantidadeSalas && livres < 256; i++) {
    Reserva teste = pend->reserva;
    teste.idSala = app->salas[i].id;
    if (aplicacao_buscar_conflito_reserva(app, &teste, 0, NULL) != 0)
      continue;
    char *nome = app->salas[i].capacidade > 0
                     ? g_strdup_printf("%s  ·  %d lugares", app->salas[i].nome,
                                       app->salas[i].capacidade)
                     : g_strdup(app->salas[i].nome);
    gtk_string_list_append(nomes, nome);
    g_free(nome);
    r->idsSalas[livres++] = app->salas[i].id;
  }

  r->janela = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(r->janela), "Resolver pendência");
  gtk_window_set_modal(GTK_WINDOW(r->janela), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(r->janela), GTK_WINDOW(p->janela));
  gtk_window_set_default_size(GTK_WINDOW(r->janela), 420, -1);
  g_object_set_data_full(G_OBJECT(r->janela), "dados", r, g_free);

  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top(caixa, 20);
  gtk_widget_set_margin_bottom(caixa, 20);
  gtk_widget_set_margin_start(caixa, 22);
  gtk_widget_set_margin_end(caixa, 22);

  const Reserva *aula = &pend->reserva;
  char *quando = g_strdup_printf(
      "Toda %s, %02d:%02d às %02d:%02d, de %02d/%02d/%04d até %02d/%02d/%04d",
      DIAS[aula->horario.dia], aula->horario.comeco / 60,
      aula->horario.comeco % 60, aula->horario.fim / 60, aula->horario.fim % 60,
      aula->data.dia, aula->data.mes, aula->data.ano, aula->dataFim.dia,
      aula->dataFim.mes, aula->dataFim.ano);
  if (!aula->isRecorrente) {
    g_free(quando);
    quando = g_strdup_printf("%02d/%02d/%04d, %02d:%02d às %02d:%02d",
                             aula->data.dia, aula->data.mes, aula->data.ano,
                             aula->horario.comeco / 60, aula->horario.comeco % 60,
                             aula->horario.fim / 60, aula->horario.fim % 60);
  }
  gtk_box_append(GTK_BOX(caixa), rotulo(aula->descricao, "negrito"));
  gtk_box_append(GTK_BOX(caixa), rotulo(quando, "subtitulo"));
  g_free(quando);

  GtkWidget *botoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_top(botoes, 10);
  GtkWidget *b_fechar = gtk_button_new_with_label("Cancelar");
  gtk_widget_add_css_class(b_fechar, "secundario");
  g_signal_connect(b_fechar, "clicked", G_CALLBACK(ao_fechar_resolver), r);

  if (livres == 0) {
    gtk_box_append(GTK_BOX(caixa),
                   rotulo("Nenhuma sala cadastrada está livre nesse horário "
                          "em todas as semanas.",
                          NULL));
    g_object_unref(nomes);
  } else {
    GtkWidget *titulo = rotulo("Sala livre", "subtitulo");
    gtk_widget_set_margin_top(titulo, 8);
    gtk_box_append(GTK_BOX(caixa), titulo);
    r->salas = gtk_drop_down_new(G_LIST_MODEL(nomes), NULL);
    lista_ligar_busca(r->salas);
    gtk_box_append(GTK_BOX(caixa), r->salas);

    GtkWidget *b_salvar = gtk_button_new_with_label("Reservar nessa sala");
    gtk_widget_add_css_class(b_salvar, "primario");
    g_signal_connect(b_salvar, "clicked", G_CALLBACK(ao_salvar_resolver), r);
    gtk_box_append(GTK_BOX(botoes), b_salvar);
  }
  gtk_box_append(GTK_BOX(botoes), b_fechar);
  gtk_box_append(GTK_BOX(caixa), botoes);

  gtk_window_set_child(GTK_WINDOW(r->janela), caixa);
  gtk_window_present(GTK_WINDOW(r->janela));
}

/* ---- Lista de pendências ---- */

static void ao_resolver(GtkButton *b, gpointer dados) {
  abrir_resolver((Pendencias *)dados,
                 GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "id")));
}

static void ao_descartar(GtkButton *b, gpointer dados) {
  Pendencias *p = (Pendencias *)dados;
  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "id"));
  if (!aplicacao_descartar_pendencia(aplicacao_padrao(), id)) {
    interface_mensagem(p->e, "Não foi possível descartar a pendência.", TRUE);
    return;
  }
  interface_mensagem(p->e, "Pendência descartada.", FALSE);
  preencher(p);
}

static GtkWidget *linha_pendencia(Pendencias *p, const Pendencia *pend) {
  const Reserva *aula = &pend->reserva;
  GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_top(linha, 8);
  gtk_widget_set_margin_bottom(linha, 8);

  GtkWidget *col_aula = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_hexpand(col_aula, TRUE);
  gtk_box_append(GTK_BOX(col_aula), rotulo(aula->descricao, "negrito"));
  gtk_box_append(GTK_BOX(col_aula), rotulo(aula->responsavel, "subtitulo"));

  char horario[32];
  g_snprintf(horario, sizeof horario, "%s %02d:%02d-%02d:%02d",
             DIAS[aula->horario.dia], aula->horario.comeco / 60,
             aula->horario.comeco % 60, aula->horario.fim / 60,
             aula->horario.fim % 60);
  GtkWidget *col_horario = rotulo(horario, NULL);
  gtk_widget_set_size_request(col_horario, 140, -1);

  // motivo e o detalhe: "No PDF: I04" ou "I-03 já tem CÁLCULO 1 - TC"
  GtkWidget *col_motivo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_size_request(col_motivo, 230, -1);
  GtkWidget *motivo = rotulo(texto_motivo(pend->motivo), "negrito");
  gtk_widget_add_css_class(motivo, pend->motivo == PEND_CONFLITO
                                       ? "pendencia-conflito"
                                       : "pendencia-sala");
  gtk_box_append(GTK_BOX(col_motivo), motivo);
  char *detalhe = pend->motivo == PEND_CONFLITO
                      ? g_strdup(pend->detalhe)
                  : pend->motivo == PEND_SEM_SALA
                      ? g_strdup_printf("No PDF: %s", pend->salaPDF)
                      : g_strdup("O PDF não informa a sala");
  gtk_box_append(GTK_BOX(col_motivo), rotulo(detalhe, "subtitulo"));
  g_free(detalhe);

  GtkWidget *b_resolver = gtk_button_new_with_label("Resolver");
  gtk_widget_add_css_class(b_resolver, "primario");
  gtk_widget_set_valign(b_resolver, GTK_ALIGN_CENTER);
  g_object_set_data(G_OBJECT(b_resolver), "id", GINT_TO_POINTER(pend->id));
  g_signal_connect(b_resolver, "clicked", G_CALLBACK(ao_resolver), p);

  GtkWidget *b_descartar = gtk_button_new_with_label("Descartar");
  gtk_widget_add_css_class(b_descartar, "secundario");
  gtk_widget_set_valign(b_descartar, GTK_ALIGN_CENTER);
  g_object_set_data(G_OBJECT(b_descartar), "id", GINT_TO_POINTER(pend->id));
  g_signal_connect(b_descartar, "clicked", G_CALLBACK(ao_descartar), p);

  gtk_box_append(GTK_BOX(linha), col_aula);
  gtk_box_append(GTK_BOX(linha), col_horario);
  gtk_box_append(GTK_BOX(linha), col_motivo);
  gtk_box_append(GTK_BOX(linha), b_resolver);
  gtk_box_append(GTK_BOX(linha), b_descartar);
  return linha;
}

static void preencher(Pendencias *p) {
  GtkWidget *filho;
  while ((filho = gtk_widget_get_first_child(p->lista)) != NULL)
    gtk_box_remove(GTK_BOX(p->lista), filho);

  Aplicacao *app = aplicacao_padrao();
  size_t n = app ? app->quantidadePendencias : 0;
  for (size_t i = 0; i < n; i++) {
    if (i > 0)
      gtk_box_append(GTK_BOX(p->lista),
                     gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(p->lista),
                   linha_pendencia(p, &app->pendencias[i]));
  }

  char *texto = n == 0 ? g_strdup("Nenhuma pendência. Tudo do PDF já foi "
                                  "resolvido ou descartado.")
                       : g_strdup_printf("%lu %s.", (unsigned long)n,
                                         n == 1 ? "pendência" : "pendências");
  gtk_label_set_text(GTK_LABEL(p->resumo), texto);
  g_free(texto);
}

static void ao_fechar(GtkButton *b, gpointer dados) {
  (void)b;
  gtk_window_destroy(GTK_WINDOW(((Pendencias *)dados)->janela));
}

GtkWidget *pendencias_abrir(Estado *e) {
  Pendencias *p = g_new0(Pendencias, 1);
  p->e = e;

  p->janela = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(p->janela), "Pendências da importação");
  gtk_window_set_modal(GTK_WINDOW(p->janela), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(p->janela), GTK_WINDOW(e->janela));
  gtk_window_set_default_size(GTK_WINDOW(p->janela), 860, 560);
  g_object_set_data_full(G_OBJECT(p->janela), "dados", p, g_free);

  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_margin_top(caixa, 18);
  gtk_widget_set_margin_bottom(caixa, 18);
  gtk_widget_set_margin_start(caixa, 22);
  gtk_widget_set_margin_end(caixa, 22);

  gtk_box_append(GTK_BOX(caixa),
                 rotulo("Aulas do PDF que não entraram. Elas ficam guardadas "
                        "aqui até você resolver: escolha uma sala livre ou "
                        "descarte.",
                        "subtitulo"));

  p->lista = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *rolagem = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), p->lista);
  gtk_widget_set_vexpand(rolagem, TRUE);
  gtk_box_append(GTK_BOX(caixa), rolagem);

  GtkWidget *rodape = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  p->resumo = rotulo("", "subtitulo");
  gtk_widget_set_hexpand(p->resumo, TRUE);
  GtkWidget *b_fechar = gtk_button_new_with_label("Fechar");
  gtk_widget_add_css_class(b_fechar, "secundario");
  g_signal_connect(b_fechar, "clicked", G_CALLBACK(ao_fechar), p);
  gtk_box_append(GTK_BOX(rodape), p->resumo);
  gtk_box_append(GTK_BOX(rodape), b_fechar);
  gtk_box_append(GTK_BOX(caixa), rodape);

  gtk_window_set_child(GTK_WINDOW(p->janela), caixa);
  preencher(p);
  gtk_window_present(GTK_WINDOW(p->janela));
  return p->janela;
}
