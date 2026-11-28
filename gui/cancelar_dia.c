#include "interface.h"
#include <stdlib.h>

/* Janela "Cancelar um dia": tira uma semana de uma reserva semanal
 * (feriado, falta do professor, prova em outra sala) sem mexer nas outras.
 * Tambem mostra os dias ja cancelados, com a opcao de desfazer. */

#define MAX_CANCELADOS 60

static const char *DIAS_SEMANA[] = {"domingo", "segunda", "terça", "quarta",
                                    "quinta",  "sexta",   "sábado"};

typedef struct {
  Estado *e;
  int idReserva;
  GtkWidget *janela, *data, *motivo, *aviso, *b_cancelar, *lista;
} CancelarDia;

static GDate *gdate_de_codigo(int dia) {
  return g_date_new_dmy(dia % 100, (dia / 100) % 100, dia / 10000);
}

static int codigo_de_gdate(const GDate *d) {
  return g_date_get_year(d) * 10000 + g_date_get_month(d) * 100 +
         g_date_get_day(d);
}

static const char *nome_dia_semana(int dia) {
  GDate *d = gdate_de_codigo(dia);
  int semana = g_date_get_weekday(d) % 7; // segunda = 1 ... domingo = 7 -> 0
  g_date_free(d);
  return DIAS_SEMANA[semana];
}

// Proxima semana em que a aula acontece, a partir de hoje
static int proxima_aula(const ReservaVista *r) {
  int hoje = dados_data_hoje();
  int inicio = r->dia > hoje ? r->dia : hoje;
  GDate *d = gdate_de_codigo(inicio);
  for (int i = 0; i < 7; i++) {
    int codigo = codigo_de_gdate(d);
    if (dados_reserva_ocorre_em(r, codigo)) {
      g_date_free(d);
      return codigo;
    }
    g_date_add_days(d, 1);
  }
  g_date_free(d);
  return r->dia;
}

static const ReservaVista *reserva_atual(CancelarDia *c) {
  int i = dados_indice_reserva(c->idReserva);
  return i >= 0 ? &g_reservas[i] : NULL;
}

// Explica o que vai acontecer no dia escolhido e libera ou trava o botao
static void atualizar_aviso(gpointer dados) {
  CancelarDia *c = (CancelarDia *)dados;
  const ReservaVista *r = reserva_atual(c);
  if (r == NULL)
    return;
  int dia = seletor_data_get(c->data);
  char data[16], ini[8], fim[8], *texto;
  dados_data_texto(dia, data, sizeof data);
  dados_hora_texto(r->ini, ini, sizeof ini);
  dados_hora_texto(r->fim, fim, sizeof fim);

  gboolean pode = FALSE;
  if (dados_dia_cancelado(c->idReserva, dia))
    texto = g_strdup_printf("O dia %s já está cancelado.", data);
  else if (!dados_reserva_ocorre_em(r, dia))
    texto = g_strdup_printf("Essa aula não acontece em %s (%s).", data,
                            nome_dia_semana(dia));
  else {
    texto = g_strdup_printf("Em %s, a sala %s fica livre das %s às %s. "
                            "As outras semanas continuam iguais.",
                            data, r->sala, ini, fim);
    pode = TRUE;
  }
  gtk_label_set_text(GTK_LABEL(c->aviso), texto);
  gtk_widget_set_sensitive(c->b_cancelar, pode);
  g_free(texto);
}

static void preencher_cancelados(CancelarDia *c);

static void ao_desfazer(GtkButton *b, gpointer dados) {
  CancelarDia *c = (CancelarDia *)dados;
  int dia = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "dia"));
  char msg[200];
  if (dados_desfazer_cancelamento(c->idReserva, dia, msg, sizeof msg) < 0) {
    gtk_label_set_text(GTK_LABEL(c->aviso), msg); // a janela fica na frente
    return;
  }
  interface_mensagem(c->e, msg, FALSE);
  preencher_cancelados(c);
  atualizar_aviso(c);
}

static int comparar_dias(const void *a, const void *b) {
  return *(const int *)a - *(const int *)b;
}

static void preencher_cancelados(CancelarDia *c) {
  GtkWidget *filho;
  while ((filho = gtk_widget_get_first_child(c->lista)) != NULL)
    gtk_box_remove(GTK_BOX(c->lista), filho);

  int dias[MAX_CANCELADOS];
  const char *motivos[MAX_CANCELADOS];
  int n = dados_cancelamentos(c->idReserva, dias, motivos, MAX_CANCELADOS);
  if (n == 0) {
    GtkWidget *vazio = gtk_label_new("Nenhum dia cancelado.");
    gtk_widget_add_css_class(vazio, "subtitulo");
    gtk_label_set_xalign(GTK_LABEL(vazio), 0);
    gtk_box_append(GTK_BOX(c->lista), vazio);
    return;
  }

  // ordena por data, levando o motivo junto
  int ordem[MAX_CANCELADOS][2];
  for (int i = 0; i < n; i++) {
    ordem[i][0] = dias[i];
    ordem[i][1] = i;
  }
  qsort(ordem, n, sizeof ordem[0], comparar_dias);

  for (int k = 0; k < n; k++) {
    int dia = ordem[k][0];
    const char *motivo = motivos[ordem[k][1]];
    char data[16];
    dados_data_texto(dia, data, sizeof data);
    char *texto = motivo[0] ? g_strdup_printf("%s  ·  %s", data, motivo)
                            : g_strdup(data);

    GtkWidget *linha = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *rotulo = gtk_label_new(texto);
    gtk_label_set_xalign(GTK_LABEL(rotulo), 0);
    gtk_widget_set_hexpand(rotulo, TRUE);
    GtkWidget *b = gtk_button_new_with_label("Desfazer");
    gtk_widget_add_css_class(b, "secundario");
    g_object_set_data(G_OBJECT(b), "dia", GINT_TO_POINTER(dia));
    g_signal_connect(b, "clicked", G_CALLBACK(ao_desfazer), c);
    gtk_box_append(GTK_BOX(linha), rotulo);
    gtk_box_append(GTK_BOX(linha), b);
    gtk_box_append(GTK_BOX(c->lista), linha);
    g_free(texto);
  }
}

static void ao_confirmar(GtkButton *b, gpointer dados) {
  (void)b;
  CancelarDia *c = (CancelarDia *)dados;
  char motivo[60], msg[200];
  g_strlcpy(motivo, gtk_editable_get_text(GTK_EDITABLE(c->motivo)),
            sizeof motivo);
  g_strstrip(motivo);
  g_strdelimit(motivo, ";", ','); // ';' separa os campos no arquivo

  if (dados_cancelar_dia(c->idReserva, seletor_data_get(c->data), motivo, msg,
                         sizeof msg) < 0) {
    gtk_label_set_text(GTK_LABEL(c->aviso), msg);
    return;
  }
  interface_mensagem(c->e, msg, FALSE);
  gtk_editable_set_text(GTK_EDITABLE(c->motivo), "");
  preencher_cancelados(c);
  atualizar_aviso(c);
}

static void ao_fechar(GtkButton *b, gpointer dados) {
  (void)b;
  gtk_window_destroy(GTK_WINDOW(((CancelarDia *)dados)->janela));
}

static GtkWidget *rotulo(const char *texto, const char *classe) {
  GtkWidget *l = gtk_label_new(texto);
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_label_set_wrap(GTK_LABEL(l), TRUE);
  if (classe)
    gtk_widget_add_css_class(l, classe);
  return l;
}

GtkWidget *cancelar_dia_abrir(Estado *e, int idReserva) {
  int i = dados_indice_reserva(idReserva);
  if (i < 0)
    return NULL;
  const ReservaVista *r = &g_reservas[i];
  if (!r->isRecorrente) {
    interface_mensagem(e, "Só dá para cancelar um dia de reservas semanais.",
                       TRUE);
    return NULL;
  }

  CancelarDia *c = g_new0(CancelarDia, 1);
  c->e = e;
  c->idReserva = idReserva;

  c->janela = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(c->janela), "Cancelar um dia");
  gtk_window_set_modal(GTK_WINDOW(c->janela), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(c->janela), GTK_WINDOW(e->janela));
  gtk_window_set_default_size(GTK_WINDOW(c->janela), 460, -1);
  gtk_window_set_resizable(GTK_WINDOW(c->janela), FALSE);
  g_object_set_data_full(G_OBJECT(c->janela), "dados", c, g_free);

  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top(caixa, 20);
  gtk_widget_set_margin_bottom(caixa, 20);
  gtk_widget_set_margin_start(caixa, 22);
  gtk_widget_set_margin_end(caixa, 22);

  // qual aula
  char ini[8], fim[8], ate[16];
  dados_hora_texto(r->ini, ini, sizeof ini);
  dados_hora_texto(r->fim, fim, sizeof fim);
  dados_data_texto(r->fimRecorrencia, ate, sizeof ate);
  char *sobre = g_strdup_printf("Sala %s  ·  toda %s, %s às %s  ·  até %s",
                                r->sala, nome_dia_semana(r->dia), ini, fim, ate);
  gtk_box_append(GTK_BOX(caixa), rotulo(r->atividade, "negrito"));
  gtk_box_append(GTK_BOX(caixa), rotulo(sobre, "subtitulo"));
  g_free(sobre);

  // dia e motivo
  GtkWidget *grade = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grade), 6);
  gtk_grid_set_column_spacing(GTK_GRID(grade), 14);
  gtk_widget_set_margin_top(grade, 12);
  c->data = seletor_data_criar(proxima_aula(r), atualizar_aviso, c);
  c->motivo = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(c->motivo), 59);
  gtk_entry_set_placeholder_text(GTK_ENTRY(c->motivo),
                                 "Ex.: feriado, falta do professor");
  gtk_widget_set_hexpand(c->motivo, TRUE);
  gtk_grid_attach(GTK_GRID(grade), rotulo("Dia", "subtitulo"), 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), rotulo("Motivo (opcional)", "subtitulo"), 1,
                  0, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), c->data, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grade), c->motivo, 1, 1, 1, 1);
  gtk_box_append(GTK_BOX(caixa), grade);

  c->aviso = rotulo("", "subtitulo");
  gtk_widget_set_margin_top(c->aviso, 4);
  gtk_box_append(GTK_BOX(caixa), c->aviso);

  GtkWidget *botoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_top(botoes, 8);
  c->b_cancelar = gtk_button_new_with_label("Cancelar este dia");
  gtk_widget_add_css_class(c->b_cancelar, "primario");
  g_signal_connect(c->b_cancelar, "clicked", G_CALLBACK(ao_confirmar), c);
  GtkWidget *b_fechar = gtk_button_new_with_label("Fechar");
  gtk_widget_add_css_class(b_fechar, "secundario");
  g_signal_connect(b_fechar, "clicked", G_CALLBACK(ao_fechar), c);
  gtk_box_append(GTK_BOX(botoes), c->b_cancelar);
  gtk_box_append(GTK_BOX(botoes), b_fechar);
  gtk_box_append(GTK_BOX(caixa), botoes);

  // dias ja cancelados
  GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_margin_top(sep, 12);
  gtk_box_append(GTK_BOX(caixa), sep);
  GtkWidget *titulo = rotulo("Dias já cancelados", "negrito");
  gtk_widget_set_margin_top(titulo, 6);
  gtk_box_append(GTK_BOX(caixa), titulo);
  c->lista = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_append(GTK_BOX(caixa), c->lista);

  gtk_window_set_child(GTK_WINDOW(c->janela), caixa);
  preencher_cancelados(c);
  atualizar_aviso(c);
  gtk_window_present(GTK_WINDOW(c->janela));
  return c->janela;
}
