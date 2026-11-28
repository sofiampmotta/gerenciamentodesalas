#include "../include/aplicacao.h"
#include "interface.h"
#include <stdio.h>
#include <string.h>

/* Tutorial do primeiro uso: um balao que aponta para cada item do menu,
 * abrindo a tela correspondente. Aparece sozinho uma vez para cada pessoa
 * (cada nome usado em "Quem está usando?"); depois so pelo F1 ou pelo link
 * "Ver o tutorial" na caixinha do nome. */

typedef struct {
  const char *tela;   // tela que abre junto (NULL = fica onde esta)
  int menu;           // item do menu que o balao aponta (-1 = nome do usuario)
  const char *titulo;
  const char *texto;
} Passo;

static const Passo PASSOS[] = {
    {"inicio", 0, "Início",
     "Aqui fica o resumo do dia: as aulas de hoje e as salas livres agora."},
    {"visao", 1, "Visão geral",
     "O quadro de horários de todas as salas. Comece por aqui: o botão "
     "<b>Importar PDF</b> traz as aulas do semestre de uma vez. Em "
     "<b>Exportar grade</b> você gera PDF, planilha ou agenda."},
    {"busca", 2, "Buscar salas",
     "Informe o dia, o horário, quantas pessoas e os recursos que precisa. "
     "O sistema mostra só as salas que servem."},
    {"reservas", 3, "Reservas",
     "Tudo o que está marcado. Busque com <b>Ctrl+F</b>, crie com "
     "<b>Ctrl+N</b>, edite ou exclua. Ao editar uma aula semanal, dá para "
     "cancelar só um dia, como num feriado."},
    {"salas", 4, "Salas",
     "Cadastre e edite as salas e os recursos de cada uma: lugares, "
     "projetor, televisão e computadores."},
    {"historico", 5, "Histórico",
     "Tudo o que foi criado, editado ou excluído, com o dia, a hora e quem "
     "fez."},
    {NULL, -1, "Quem está usando",
     "Clique no seu nome para dizer quem está usando o sistema. Cada "
     "alteração fica no <b>Histórico</b> com esse nome, então troque sempre "
     "que outra pessoa da equipe for usar.\n\nPara ver este tutorial de novo, "
     "aperte <b>F1</b> ou clique em <b>Ver o tutorial</b>, aqui mesmo."},
};
#define TOTAL_PASSOS ((int)(sizeof PASSOS / sizeof PASSOS[0]))

typedef struct {
  Estado *e;
  int atual;
  GtkWidget *balao, *contador, *titulo, *texto, *pontos[TOTAL_PASSOS];
  GtkWidget *b_voltar, *b_proximo;
} Tutorial;

static Tutorial *g_tutorial = NULL;

// Lista de quem ja viu o tutorial, um nome por linha (pasta de perfil.txt)
static void caminho_marca(char destino[512]) {
  Aplicacao *app = aplicacao_padrao();
  const char *perfil = app ? app->caminhoPerfil : "perfil.txt";
  const char *barra = strrchr(perfil, '/');
  const char *contrabarra = strrchr(perfil, '\\');
  if (contrabarra != NULL && (barra == NULL || contrabarra > barra))
    barra = contrabarra;
  int pasta = barra != NULL ? (int)(barra - perfil + 1) : 0;
  snprintf(destino, 512, "%.*s%s", pasta, perfil, "tutorial_visto.txt");
}

static const char *usuario_atual(void) {
  Aplicacao *app = aplicacao_padrao();
  return app != NULL ? app->usuario : "";
}

// Compara nomes sem diferenciar maiusculas ("leidjane" = "Leidjane")
static int mesmo_nome(const char *a, const char *b) {
  char *x = g_utf8_casefold(a, -1), *y = g_utf8_casefold(b, -1);
  g_strstrip(x);
  g_strstrip(y);
  int iguais = strcmp(x, y) == 0;
  g_free(x);
  g_free(y);
  return iguais;
}

static int ja_viu(const char *nome) {
  char caminho[512], linha[128];
  caminho_marca(caminho);
  FILE *f = fopen(caminho, "r");
  if (f == NULL)
    return 0;
  int achou = 0;
  while (!achou && fgets(linha, sizeof linha, f) != NULL) {
    linha[strcspn(linha, "\r\n")] = '\0';
    achou = mesmo_nome(linha, nome);
  }
  fclose(f);
  return achou;
}

static void marcar_como_visto(void) {
  const char *nome = usuario_atual();
  if (nome[0] == '\0' || ja_viu(nome))
    return;
  char caminho[512];
  caminho_marca(caminho);
  FILE *f = fopen(caminho, "a");
  if (f != NULL) {
    fprintf(f, "%s\n", nome);
    fclose(f);
  }
}

static void esconder_balao(Tutorial *t) {
  if (gtk_widget_get_parent(t->balao) != NULL) {
    gtk_popover_popdown(GTK_POPOVER(t->balao));
    gtk_widget_unparent(t->balao);
  }
}

static void mostrar_passo(Tutorial *t) {
  const Passo *p = &PASSOS[t->atual];
  if (p->tela != NULL)
    interface_ir_para(t->e, p->tela);

  char contador[16];
  g_snprintf(contador, sizeof contador, "%d DE %d", t->atual + 1,
             TOTAL_PASSOS);
  gtk_label_set_text(GTK_LABEL(t->contador), contador);
  gtk_label_set_text(GTK_LABEL(t->titulo), p->titulo);
  gtk_label_set_markup(GTK_LABEL(t->texto), p->texto);
  for (int i = 0; i < TOTAL_PASSOS; i++) {
    if (i == t->atual)
      gtk_widget_add_css_class(t->pontos[i], "tutorial-ponto-atual");
    else
      gtk_widget_remove_css_class(t->pontos[i], "tutorial-ponto-atual");
  }
  gtk_widget_set_sensitive(t->b_voltar, t->atual > 0);
  gtk_button_set_label(GTK_BUTTON(t->b_proximo),
                       t->atual == TOTAL_PASSOS - 1 ? "Concluir" : "Próximo");

  // o balao aponta para o item do menu (ou para o nome do usuario)
  GtkWidget *alvo =
      p->menu >= 0 ? t->e->botoes_menu[p->menu] : t->e->botao_perfil;
  esconder_balao(t);
  gtk_widget_set_parent(t->balao, alvo);
  gtk_popover_popup(GTK_POPOVER(t->balao));
}

static void terminar(Tutorial *t) {
  esconder_balao(t);
  marcar_como_visto();
}

static void ao_proximo(GtkButton *b, gpointer dados) {
  (void)b;
  Tutorial *t = (Tutorial *)dados;
  if (t->atual == TOTAL_PASSOS - 1) {
    terminar(t);
    return;
  }
  t->atual++;
  mostrar_passo(t);
}

static void ao_voltar(GtkButton *b, gpointer dados) {
  (void)b;
  Tutorial *t = (Tutorial *)dados;
  if (t->atual > 0) {
    t->atual--;
    mostrar_passo(t);
  }
}

static void ao_pular(GtkButton *b, gpointer dados) {
  (void)b;
  terminar((Tutorial *)dados);
}

static Tutorial *criar_tutorial(Estado *e) {
  Tutorial *t = g_new0(Tutorial, 1);
  t->e = e;

  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_top(caixa, 10);
  gtk_widget_set_margin_bottom(caixa, 10);
  gtk_widget_set_margin_start(caixa, 12);
  gtk_widget_set_margin_end(caixa, 12);
  gtk_widget_set_size_request(caixa, 320, -1);

  t->contador = gtk_label_new("");
  gtk_widget_add_css_class(t->contador, "tutorial-contador");
  gtk_label_set_xalign(GTK_LABEL(t->contador), 0);
  t->titulo = gtk_label_new("");
  gtk_widget_add_css_class(t->titulo, "tutorial-titulo");
  gtk_label_set_xalign(GTK_LABEL(t->titulo), 0);
  t->texto = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(t->texto), 0);
  gtk_label_set_wrap(GTK_LABEL(t->texto), TRUE);
  gtk_label_set_max_width_chars(GTK_LABEL(t->texto), 40);
  gtk_box_append(GTK_BOX(caixa), t->contador);
  gtk_box_append(GTK_BOX(caixa), t->titulo);
  gtk_box_append(GTK_BOX(caixa), t->texto);

  GtkWidget *pontos = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_margin_top(pontos, 6);
  for (int i = 0; i < TOTAL_PASSOS; i++) {
    t->pontos[i] = gtk_label_new("●");
    gtk_widget_add_css_class(t->pontos[i], "tutorial-ponto");
    gtk_box_append(GTK_BOX(pontos), t->pontos[i]);
  }
  gtk_box_append(GTK_BOX(caixa), pontos);

  GtkWidget *rodape = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_top(rodape, 6);
  GtkWidget *b_pular = gtk_button_new_with_label("Pular tutorial");
  gtk_widget_add_css_class(b_pular, "tutorial-pular");
  gtk_widget_set_hexpand(b_pular, TRUE);
  gtk_widget_set_halign(b_pular, GTK_ALIGN_START);
  g_signal_connect(b_pular, "clicked", G_CALLBACK(ao_pular), t);
  t->b_voltar = gtk_button_new_with_label("Voltar");
  gtk_widget_add_css_class(t->b_voltar, "secundario");
  g_signal_connect(t->b_voltar, "clicked", G_CALLBACK(ao_voltar), t);
  t->b_proximo = gtk_button_new_with_label("Próximo");
  gtk_widget_add_css_class(t->b_proximo, "primario");
  g_signal_connect(t->b_proximo, "clicked", G_CALLBACK(ao_proximo), t);
  gtk_box_append(GTK_BOX(rodape), b_pular);
  gtk_box_append(GTK_BOX(rodape), t->b_voltar);
  gtk_box_append(GTK_BOX(rodape), t->b_proximo);
  gtk_box_append(GTK_BOX(caixa), rodape);

  t->balao = gtk_popover_new();
  gtk_widget_add_css_class(t->balao, "balao"); // cores em visual.c
  g_object_ref_sink(t->balao); // continua vivo ao trocar de item do menu
  gtk_popover_set_child(GTK_POPOVER(t->balao), caixa);
  gtk_popover_set_position(GTK_POPOVER(t->balao), GTK_POS_RIGHT);
  gtk_popover_set_autohide(GTK_POPOVER(t->balao), FALSE);
  return t;
}

void tutorial_iniciar(Estado *e) {
  if (g_tutorial == NULL)
    g_tutorial = criar_tutorial(e);
  g_tutorial->atual = 0;
  mostrar_passo(g_tutorial);
}

static gboolean iniciar_depois(gpointer dados) {
  tutorial_iniciar((Estado *)dados);
  return G_SOURCE_REMOVE;
}

void tutorial_se_primeira_vez(Estado *e) {
  if (ja_viu(usuario_atual()))
    return;
  // espera a janela aparecer para o balao saber onde apontar
  g_timeout_add(400, iniciar_depois, e);
}
