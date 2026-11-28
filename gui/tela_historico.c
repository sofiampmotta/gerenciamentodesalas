#include "../include/aplicacao.h"
#include "../include/historico.h"
#include "interface.h"
#include <string.h>

/* Tela "Histórico": lista o que foi criado, editado, excluído ou importado,
 * do mais recente para o mais antigo. Os dados vêm de historico.csv. */

#define MAX_LINHAS 500 // as mais recentes; o arquivo guarda tudo

typedef struct {
  Estado *e;
  GtkWidget *lista, *busca, *total;
  char resumo[96]; // texto do total quando nao ha busca
} TelaHistorico;

static void estilo_historico(void) {
  static gboolean carregado = FALSE;
  if (carregado)
    return;
  carregado = TRUE;

  GtkCssProvider *css = gtk_css_provider_new();
  // as cores das etiquetas ficam em visual.c e visual_escuro.c
  gtk_css_provider_load_from_string(
      css, ".dia-historico { padding: 14px 16px 6px 16px; }");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);
}

static GtkWidget *celula(const char *texto, int largura) {
  GtkWidget *l = gtk_label_new(texto);
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
  gtk_widget_set_size_request(l, largura, -1);
  return l;
}

static GtkWidget *etiqueta_acao(const char *acao) {
  GtkWidget *l = gtk_label_new(acao);
  gtk_widget_add_css_class(l, "acao");
  if (g_str_has_prefix(acao, "Criou"))
    gtk_widget_add_css_class(l, "acao-criou");
  else if (g_str_has_prefix(acao, "Editou") || g_str_has_prefix(acao, "Desfez"))
    gtk_widget_add_css_class(l, "acao-editou");
  else if (g_str_has_prefix(acao, "Excluiu") ||
           g_str_has_prefix(acao, "Cancelou") ||
           g_str_has_prefix(acao, "Descartou"))
    gtk_widget_add_css_class(l, "acao-excluiu");
  else
    gtk_widget_add_css_class(l, "acao-importou");

  // a etiqueta fica do tamanho do texto, dentro de uma coluna fixa
  GtkWidget *coluna = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_size_request(coluna, 150, -1);
  gtk_widget_set_valign(l, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(coluna), l);
  return coluna;
}

// "Hoje", "Ontem" ou "21/11/2026"
static void nome_do_dia(const char *data, char *texto, size_t tamanho) {
  GDateTime *agora = g_date_time_new_now_local();
  GDateTime *ontem = g_date_time_add_days(agora, -1);
  char *hoje_txt = g_date_time_format(agora, "%Y-%m-%d");
  char *ontem_txt = g_date_time_format(ontem, "%Y-%m-%d");

  if (strcmp(data, hoje_txt) == 0)
    g_strlcpy(texto, "Hoje", tamanho);
  else if (strcmp(data, ontem_txt) == 0)
    g_strlcpy(texto, "Ontem", tamanho);
  else if (strlen(data) == 10)
    g_snprintf(texto, tamanho, "%.2s/%.2s/%.4s", data + 8, data + 5, data);
  else
    g_strlcpy(texto, data, tamanho);

  g_free(hoje_txt);
  g_free(ontem_txt);
  g_date_time_unref(ontem);
  g_date_time_unref(agora);
}

static void adicionar_dia(TelaHistorico *t, const char *data) {
  char texto[32];
  nome_do_dia(data, texto, sizeof texto);
  GtkWidget *rotulo = gtk_label_new(texto);
  gtk_label_set_xalign(GTK_LABEL(rotulo), 0);
  GtkWidget *linha = gtk_list_box_row_new();
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(linha), rotulo);
  gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(linha), FALSE);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(linha), FALSE);
  gtk_widget_add_css_class(linha, "dia-historico");
  g_object_set_data(G_OBJECT(linha), "dia", GINT_TO_POINTER(1));
  gtk_list_box_append(GTK_LIST_BOX(t->lista), linha);
}

static void adicionar_registro(TelaHistorico *t, const RegistroHistorico *r) {
  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_append(GTK_BOX(caixa), celula(r->hora, 110));
  gtk_box_append(GTK_BOX(caixa), celula(r->quem, 150));
  gtk_box_append(GTK_BOX(caixa), etiqueta_acao(r->acao));
  GtkWidget *detalhe = celula(r->detalhe, 300);
  gtk_widget_set_hexpand(detalhe, TRUE);
  gtk_box_append(GTK_BOX(caixa), detalhe);

  GtkWidget *linha = gtk_list_box_row_new();
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(linha), caixa);
  gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(linha), FALSE);

  // texto usado pela busca
  char *chave = g_strdup_printf("%s %s %s", r->quem, r->acao, r->detalhe);
  g_object_set_data_full(G_OBJECT(linha), "busca", chave, g_free);
  gtk_list_box_append(GTK_LIST_BOX(t->lista), linha);
}

static void ao_buscar(GtkEditable *campo, gpointer dados) {
  TelaHistorico *t = (TelaHistorico *)dados;
  const char *termo = gtk_editable_get_text(campo);
  gboolean buscando = termo[0] != '\0';
  int achados = 0;

  for (GtkWidget *linha = gtk_widget_get_first_child(t->lista); linha != NULL;
       linha = gtk_widget_get_next_sibling(linha)) {
    if (g_object_get_data(G_OBJECT(linha), "dia")) {
      // durante a busca os titulos de dia somem
      gtk_widget_set_visible(linha, !buscando);
      continue;
    }
    const char *chave = g_object_get_data(G_OBJECT(linha), "busca");
    gboolean casou = !buscando || (chave && g_str_match_string(termo, chave, TRUE));
    gtk_widget_set_visible(linha, casou);
    if (casou && buscando)
      achados++;
  }

  if (buscando) {
    char *txt = g_strdup_printf("%d alterações encontradas", achados);
    gtk_label_set_text(GTK_LABEL(t->total), txt);
    g_free(txt);
  } else {
    gtk_label_set_text(GTK_LABEL(t->total), t->resumo);
  }
}

static void recarregar(TelaHistorico *t) {
  GtkWidget *filho;
  while ((filho = gtk_widget_get_first_child(t->lista)) != NULL)
    gtk_list_box_remove(GTK_LIST_BOX(t->lista), filho);

  Aplicacao *app = aplicacao_padrao();
  RegistroHistorico *registros = NULL;
  size_t n = 0;
  if (app == NULL ||
      !historico_carregar(app->caminhoHistorico, &registros, &n)) {
    g_strlcpy(t->resumo, "Não foi possível ler o histórico.", sizeof t->resumo);
    gtk_label_set_text(GTK_LABEL(t->total), t->resumo);
    return;
  }

  if (n == 0) {
    g_strlcpy(t->resumo, "Nenhuma alteração registrada ainda.",
              sizeof t->resumo);
    gtk_label_set_text(GTK_LABEL(t->total), t->resumo);
    return;
  }

  // o arquivo esta do mais antigo para o mais novo; mostra ao contrario
  size_t primeiro = n > MAX_LINHAS ? n - MAX_LINHAS : 0;
  const char *dia_atual = "";
  for (size_t i = n; i-- > primeiro;) {
    if (strcmp(registros[i].data, dia_atual) != 0) {
      adicionar_dia(t, registros[i].data);
      dia_atual = registros[i].data;
    }
    adicionar_registro(t, &registros[i]);
  }

  if (n > MAX_LINHAS)
    g_snprintf(t->resumo, sizeof t->resumo,
               "Mostrando as %d alterações mais recentes (de %lu).", MAX_LINHAS,
               (unsigned long)n);
  else
    g_snprintf(t->resumo, sizeof t->resumo, "%lu alterações registradas.",
               (unsigned long)n);
  historico_liberar(registros);

  ao_buscar(GTK_EDITABLE(t->busca), t);
}

static void ao_mostrar(GtkWidget *w, gpointer dados) {
  (void)w;
  recarregar((TelaHistorico *)dados);
}

GtkWidget *tela_historico_criar(Estado *e) {
  TelaHistorico *t = g_new0(TelaHistorico, 1);
  t->e = e;
  estilo_historico();

  GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_margin_top(pagina, 28);
  gtk_widget_set_margin_bottom(pagina, 24);
  gtk_widget_set_margin_start(pagina, 32);
  gtk_widget_set_margin_end(pagina, 32);

  GtkWidget *titulo = gtk_label_new("Histórico");
  gtk_widget_add_css_class(titulo, "titulo");
  gtk_label_set_xalign(GTK_LABEL(titulo), 0);
  t->total = gtk_label_new("");
  gtk_widget_add_css_class(t->total, "subtitulo");
  gtk_label_set_xalign(GTK_LABEL(t->total), 0);
  gtk_box_append(GTK_BOX(pagina), titulo);
  gtk_box_append(GTK_BOX(pagina), t->total);

  t->busca = gtk_search_entry_new();
  g_object_set(t->busca, "placeholder-text", "Buscar por sala, aula ou pessoa",
               NULL);
  g_signal_connect(t->busca, "changed", G_CALLBACK(ao_buscar), t);
  gtk_box_append(GTK_BOX(pagina), t->busca);

  GtkWidget *cab = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_append(GTK_BOX(cab), celula("Quando", 110));
  gtk_box_append(GTK_BOX(cab), celula("Quem", 150));
  gtk_box_append(GTK_BOX(cab), celula("Ação", 150));
  gtk_box_append(GTK_BOX(cab), celula("Detalhe", 300));
  gtk_widget_add_css_class(cab, "cab-reservas");
  gtk_box_append(GTK_BOX(pagina), cab);

  t->lista = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(t->lista), GTK_SELECTION_NONE);
  gtk_widget_add_css_class(t->lista, "tab-reservas");
  GtkWidget *rolagem = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rolagem), t->lista);
  gtk_widget_set_vexpand(rolagem, TRUE);
  gtk_widget_add_css_class(rolagem, "moldura-reservas");
  gtk_box_append(GTK_BOX(pagina), rolagem);

  // le o arquivo de novo toda vez que a tela abre
  g_signal_connect(pagina, "map", G_CALLBACK(ao_mostrar), t);
  return pagina;
}
