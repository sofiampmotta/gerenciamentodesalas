#include "../include/aplicacao.h"
#include "interface.h"
#ifdef _WIN32
#include <gdk/win32/gdkwin32.h>
#include <windows.h>
#endif

static const char *NOMES[6] = {"inicio",   "visao", "busca",
                               "reservas", "salas", "historico"};
static const char *TITULOS[6] = {"Início",   "Visão geral", "Buscar salas",
                                 "Reservas", "Salas",       "Histórico"};

static guint g_timer_status = 0;

static gboolean limpar_mensagem_status(gpointer dados) {
  GtkWidget *barra = GTK_WIDGET(dados);
  if (GTK_IS_LABEL(barra)) {
    gtk_label_set_text(GTK_LABEL(barra), "");
    gtk_widget_remove_css_class(barra, "error");
    gtk_widget_remove_css_class(barra, "success");
    gtk_widget_set_visible(barra, FALSE);
  }
  g_timer_status = 0;
  return G_SOURCE_REMOVE;
}

void interface_mensagem(Estado *e, const char *texto, gboolean erro) {
  if (g_timer_status > 0) {
    g_source_remove(g_timer_status);
    g_timer_status = 0;
  }

  gtk_label_set_text(GTK_LABEL(e->barra_status), texto);
  gtk_widget_remove_css_class(e->barra_status, "error");
  gtk_widget_remove_css_class(e->barra_status, "success");
  gtk_widget_add_css_class(e->barra_status, erro ? "error" : "success");
  gtk_widget_set_visible(e->barra_status, TRUE);

  g_timer_status =
      g_timeout_add_seconds(3, limpar_mensagem_status, e->barra_status);
}

/* Janela "tem certeza?" usada antes de excluir. */
typedef struct {
  void (*ao_confirmar)(gpointer);
  gpointer dados;
} Confirmacao;

#if GTK_CHECK_VERSION(4, 10, 0)
static void ao_responder_confirmacao(GObject *origem, GAsyncResult *res,
                                     gpointer dados) {
  Confirmacao *c = (Confirmacao *)dados;
  GError *erro = NULL;
  int escolha =
      gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(origem), res, &erro);
  if (erro)
    g_error_free(erro);
  else if (escolha == 1)
    c->ao_confirmar(c->dados);
  g_free(c);
}
#else
static void ao_responder_confirmacao(GtkDialog *dialog, int resposta,
                                     gpointer dados) {
  Confirmacao *c = (Confirmacao *)dados;
  gtk_window_destroy(GTK_WINDOW(dialog));
  if (resposta == GTK_RESPONSE_ACCEPT)
    c->ao_confirmar(c->dados);
  g_free(c);
}
#endif

void interface_confirmar(Estado *e, const char *titulo, const char *detalhe,
                         const char *botao, void (*ao_confirmar)(gpointer),
                         gpointer dados) {
  Confirmacao *c = g_new(Confirmacao, 1);
  c->ao_confirmar = ao_confirmar;
  c->dados = dados;
  GtkWindow *janela = e && e->janela ? GTK_WINDOW(e->janela) : NULL;

#if GTK_CHECK_VERSION(4, 10, 0)
  const char *botoes[] = {"Cancelar", botao, NULL};
  GtkAlertDialog *alerta = gtk_alert_dialog_new("%s", titulo);
  gtk_alert_dialog_set_detail(alerta, detalhe);
  gtk_alert_dialog_set_buttons(alerta, botoes);
  gtk_alert_dialog_set_cancel_button(alerta, 0);
  gtk_alert_dialog_set_default_button(alerta, 0);
  gtk_alert_dialog_choose(alerta, janela, NULL, ao_responder_confirmacao, c);
  g_object_unref(alerta);
#else
  GtkWidget *dialog = gtk_message_dialog_new(
      janela, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s", titulo);
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s",
                                           detalhe);
  gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancelar", GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button(GTK_DIALOG(dialog), botao, GTK_RESPONSE_ACCEPT);
  g_signal_connect(dialog, "response", G_CALLBACK(ao_responder_confirmacao), c);
  gtk_widget_show(dialog);
#endif
}

/* ---- Caixinha "Quem está usando?" ---- */
typedef struct {
  Estado *e;
  GtkWidget *popover, *nome, *setor;
} Perfil;

static void ao_abrir_perfil(GtkWidget *w, gpointer dados) {
  (void)w;
  Perfil *p = (Perfil *)dados;
  Aplicacao *app = aplicacao_padrao();
  if (app == NULL)
    return;
  gtk_editable_set_text(GTK_EDITABLE(p->nome), app->usuario);
  gtk_editable_set_text(GTK_EDITABLE(p->setor), app->setor);
}

static void ao_salvar_perfil(GtkWidget *w, gpointer dados) {
  (void)w;
  Perfil *p = (Perfil *)dados;
  char nome[64], setor[32];
  g_strlcpy(nome, gtk_editable_get_text(GTK_EDITABLE(p->nome)), sizeof nome);
  g_strlcpy(setor, gtk_editable_get_text(GTK_EDITABLE(p->setor)), sizeof setor);
  g_strstrip(nome);
  g_strstrip(setor);

  if (nome[0] == '\0') {
    interface_mensagem(p->e, "Digite o nome de quem está usando.", TRUE);
    return;
  }
  if (!aplicacao_definir_usuario(aplicacao_padrao(), nome, setor)) {
    interface_mensagem(p->e, "Não foi possível salvar o nome.", TRUE);
    return;
  }
  gtk_label_set_text(GTK_LABEL(p->e->rotulo_nome), nome);
  gtk_label_set_text(GTK_LABEL(p->e->rotulo_setor), setor);
  gtk_popover_popdown(GTK_POPOVER(p->popover));

  char *msg = g_strdup_printf("As alterações agora ficam registradas como %s.",
                              nome);
  interface_mensagem(p->e, msg, FALSE);
  g_free(msg);
}

static void ao_cancelar_perfil(GtkButton *b, gpointer dados) {
  (void)b;
  gtk_popover_popdown(GTK_POPOVER(((Perfil *)dados)->popover));
}

static GtkWidget *campo_perfil(GtkWidget *caixa, const char *rotulo,
                               int max) {
  GtkWidget *l = gtk_label_new(rotulo);
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_widget_add_css_class(l, "subtitulo");
  GtkWidget *campo = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(campo), max);
  gtk_box_append(GTK_BOX(caixa), l);
  gtk_box_append(GTK_BOX(caixa), campo);
  return campo;
}

static GtkWidget *criar_perfil(Estado *e) {
  Perfil *p = g_new0(Perfil, 1);
  p->e = e;

  GtkWidget *caixa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_top(caixa, 10);
  gtk_widget_set_margin_bottom(caixa, 10);
  gtk_widget_set_margin_start(caixa, 10);
  gtk_widget_set_margin_end(caixa, 10);
  gtk_widget_set_size_request(caixa, 280, -1);

  GtkWidget *titulo = gtk_label_new("Quem está usando?");
  gtk_widget_add_css_class(titulo, "negrito");
  gtk_label_set_xalign(GTK_LABEL(titulo), 0);
  GtkWidget *dica = gtk_label_new("Esse nome aparece no histórico de alterações.");
  gtk_widget_add_css_class(dica, "subtitulo");
  gtk_label_set_xalign(GTK_LABEL(dica), 0);
  gtk_label_set_wrap(GTK_LABEL(dica), TRUE);
  gtk_box_append(GTK_BOX(caixa), titulo);
  gtk_box_append(GTK_BOX(caixa), dica);

  p->nome = campo_perfil(caixa, "Nome", 60);
  p->setor = campo_perfil(caixa, "Setor", 30);
  g_signal_connect(p->nome, "activate", G_CALLBACK(ao_salvar_perfil), p);
  g_signal_connect(p->setor, "activate", G_CALLBACK(ao_salvar_perfil), p);

  GtkWidget *botoes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_top(botoes, 8);
  GtkWidget *b_salvar = gtk_button_new_with_label("Salvar");
  gtk_widget_add_css_class(b_salvar, "primario");
  g_signal_connect(b_salvar, "clicked", G_CALLBACK(ao_salvar_perfil), p);
  GtkWidget *b_cancelar = gtk_button_new_with_label("Cancelar");
  gtk_widget_add_css_class(b_cancelar, "secundario");
  g_signal_connect(b_cancelar, "clicked", G_CALLBACK(ao_cancelar_perfil), p);
  gtk_box_append(GTK_BOX(botoes), b_salvar);
  gtk_box_append(GTK_BOX(botoes), b_cancelar);
  gtk_box_append(GTK_BOX(caixa), botoes);

  p->popover = gtk_popover_new();
  gtk_popover_set_child(GTK_POPOVER(p->popover), caixa);
  g_signal_connect(p->popover, "show", G_CALLBACK(ao_abrir_perfil), p);
  return p->popover;
}

static void ao_menu(GtkToggleButton *b, gpointer dados) {
  Estado *e = (Estado *)dados;
  if (gtk_toggle_button_get_active(b)) {
    const char *nome = g_object_get_data(G_OBJECT(b), "tela");
    const char *atual = gtk_stack_get_visible_child_name(GTK_STACK(e->stack));
    if (nome && (!atual || strcmp(atual, nome) != 0)) {
      gtk_stack_set_visible_child_name(GTK_STACK(e->stack), nome);
    }
  }
}

static GtkWidget *criar_menu(Estado *e) {
  GtkWidget *menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(menu, "menu-lateral");
  gtk_widget_set_hexpand(menu, FALSE);
  gtk_widget_set_size_request(menu, 330, -1);

  GtkWidget *faixa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(faixa, "faixa-vermelha");
  gtk_widget_set_halign(faixa, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(menu), faixa);

  gtk_box_append(GTK_BOX(menu), logo_poli_criar());

  GtkWidget *tag_sistema = gtk_label_new("gerenciamento de salas");
  gtk_widget_add_css_class(tag_sistema, "tag-sistema");
  gtk_label_set_xalign(GTK_LABEL(tag_sistema), 0);
  gtk_widget_set_halign(tag_sistema, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(menu), tag_sistema);

  GtkWidget *primeiro = NULL;
  for (int i = 0; i < 6; i++) {
    GtkWidget *b = gtk_toggle_button_new_with_label(TITULOS[i]);
    gtk_widget_add_css_class(b, "menu");
    gtk_widget_set_hexpand(b, TRUE);
    gtk_widget_set_halign(gtk_button_get_child(GTK_BUTTON(b)), GTK_ALIGN_START);
    g_object_set_data(G_OBJECT(b), "tela", (gpointer)NOMES[i]);
    if (primeiro)
      gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(b),
                                  GTK_TOGGLE_BUTTON(primeiro));
    else
      primeiro = b;
    g_signal_connect(b, "toggled", G_CALLBACK(ao_menu), e);
    e->botoes_menu[i] = b;
    gtk_box_append(GTK_BOX(menu), b);
  }
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(primeiro), TRUE);

  GtkWidget *espaco = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_vexpand(espaco, TRUE);
  gtk_box_append(GTK_BOX(menu), espaco);

  GtkWidget *rodape = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_bottom(rodape, 18);

  GtkWidget *chave_tema = tema_criar_chave();
  gtk_box_append(GTK_BOX(rodape), chave_tema);

  // quem esta usando; clicando, abre a caixinha para trocar o nome
  Aplicacao *app = aplicacao_padrao();
  GtkWidget *caixa_usuario = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

  e->rotulo_nome = gtk_label_new(app ? app->usuario : "Leidjane");
  gtk_widget_add_css_class(e->rotulo_nome, "usuario-nome");
  gtk_label_set_xalign(GTK_LABEL(e->rotulo_nome), 0);

  e->rotulo_setor = gtk_label_new(app ? app->setor : "DATP");
  gtk_widget_add_css_class(e->rotulo_setor, "usuario-info");
  gtk_label_set_xalign(GTK_LABEL(e->rotulo_setor), 0);

  GtkWidget *u3 = gtk_label_new("Clique para trocar");
  gtk_widget_add_css_class(u3, "usuario-info");
  gtk_label_set_xalign(GTK_LABEL(u3), 0);

  gtk_box_append(GTK_BOX(caixa_usuario), e->rotulo_nome);
  gtk_box_append(GTK_BOX(caixa_usuario), e->rotulo_setor);
  gtk_box_append(GTK_BOX(caixa_usuario), u3);

  GtkWidget *b_perfil = gtk_menu_button_new();
  gtk_widget_add_css_class(b_perfil, "perfil");
  gtk_widget_set_margin_start(b_perfil, 8);
  gtk_widget_set_margin_end(b_perfil, 8);
  gtk_menu_button_set_child(GTK_MENU_BUTTON(b_perfil), caixa_usuario);
  gtk_menu_button_set_direction(GTK_MENU_BUTTON(b_perfil), GTK_ARROW_RIGHT);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(b_perfil), criar_perfil(e));
  gtk_box_append(GTK_BOX(rodape), b_perfil);

  gtk_box_append(GTK_BOX(menu), rodape);

  return menu;
}

static gboolean ao_atalho_minimizar(GtkWidget *widget, GVariant *args,
                                    gpointer dados) {
  (void)widget;
  (void)args;
  GtkWindow *win = GTK_WINDOW(dados);
  g_print("Acionando gtk_window_minimize...\n");
  gtk_window_minimize(win);
  return TRUE;
}

static gboolean ao_atalho_nova_reserva(GtkWidget *widget, GVariant *args,
                                       gpointer dados) {
  (void)widget;
  (void)args;
  interface_ir_para((Estado *)dados, "nova");
  return TRUE;
}

static gboolean ao_atalho_buscar(GtkWidget *widget, GVariant *args,
                                 gpointer dados) {
  (void)widget;
  (void)args;
  interface_ir_para((Estado *)dados, "reservas");
  tela_reservas_focar_busca();
  return TRUE;
}

static void adicionar_atalho(GtkEventController *controle, const char *teclas,
                             GtkShortcutFunc funcao, gpointer dados) {
  GtkShortcut *atalho =
      gtk_shortcut_new(gtk_shortcut_trigger_parse_string(teclas),
                       gtk_callback_action_new(funcao, dados, NULL));
  gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(controle),
                                       atalho);
}

static void ao_notificar_minimizado(GObject *obj, GParamSpec *pspec,
                                    gpointer dados) {
  (void)pspec;
  (void)dados;
  GtkWindow *win = GTK_WINDOW(obj);
  GtkNative *native = gtk_widget_get_native(GTK_WIDGET(win));
  if (!native)
    return;
  GdkSurface *surface = gtk_native_get_surface(native);
  if (!surface || !GDK_IS_TOPLEVEL(surface))
    return;

  if (gdk_toplevel_get_state(GDK_TOPLEVEL(surface)) &
      GDK_TOPLEVEL_STATE_MINIMIZED) {
#ifdef _WIN32
    if (GDK_IS_WIN32_SURFACE(surface)) {
      HWND hwnd =
          (HWND)gdk_win32_surface_get_handle(GDK_WIN32_SURFACE(surface));
      if (hwnd) {
        ShowWindow(hwnd, SW_MINIMIZE);
      }
    }
#endif
  }
}

static void ativar(GtkApplication *app, gpointer dados) {
  Estado *e = g_new0(Estado, 1);
  dados_iniciar();
  estilo_aplicar();

  e->janela = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(e->janela), "POLI - Gestão de Ambientes");
  gtk_window_set_resizable(GTK_WINDOW(e->janela), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(e->janela), 1024, 650);

  e->stack = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(e->stack),
                                GTK_STACK_TRANSITION_TYPE_NONE);
  gtk_stack_set_transition_duration(GTK_STACK(e->stack), 0);
  gtk_stack_set_hhomogeneous(GTK_STACK(e->stack), FALSE);
  gtk_stack_set_vhomogeneous(GTK_STACK(e->stack), FALSE);
  gtk_widget_set_hexpand(e->stack, TRUE);
  gtk_widget_set_vexpand(e->stack, TRUE);
  gtk_stack_add_named(GTK_STACK(e->stack), tela_inicio_criar(e), "inicio");
  gtk_stack_add_named(GTK_STACK(e->stack), tela_visao_criar(e), "visao");
  gtk_stack_add_named(GTK_STACK(e->stack), tela_busca_criar(e), "busca");
  gtk_stack_add_named(GTK_STACK(e->stack), tela_reservas_criar(e), "reservas");
  gtk_stack_add_named(GTK_STACK(e->stack), tela_salas_criar(e), "salas");
  gtk_stack_add_named(GTK_STACK(e->stack), tela_nova_reserva_criar(e), "nova");
  gtk_stack_add_named(GTK_STACK(e->stack), tela_historico_criar(e), "historico");

  e->barra_status = gtk_label_new("");
  gtk_widget_add_css_class(e->barra_status, "barra-status");
  gtk_widget_set_visible(e->barra_status, FALSE);
  gtk_label_set_xalign(GTK_LABEL(e->barra_status), 0);

  GtkWidget *rol_stack = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rol_stack),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(rol_stack), 1);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(rol_stack), 1);
  gtk_widget_set_hexpand(rol_stack, TRUE);
  gtk_widget_set_vexpand(rol_stack, TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rol_stack), e->stack);

  GtkWidget *direita = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(direita, TRUE);
  gtk_widget_set_vexpand(direita, TRUE);
  gtk_box_append(GTK_BOX(direita), rol_stack);
  gtk_box_append(GTK_BOX(direita), e->barra_status);

  GtkWidget *app_raiz = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append(GTK_BOX(app_raiz), criar_menu(e));
  gtk_box_append(GTK_BOX(app_raiz), direita);

  e->raiz = app_raiz;
  gtk_window_set_child(GTK_WINDOW(e->janela), e->raiz);
  // atalhos de teclado: Ctrl+M minimiza, Ctrl+N abre nova reserva e
  // Ctrl+F vai para a busca de reservas
  GtkEventController *atalhos = gtk_shortcut_controller_new();
  adicionar_atalho(atalhos, "<Control>m", ao_atalho_minimizar, e->janela);
  adicionar_atalho(atalhos, "<Control>n", ao_atalho_nova_reserva, e);
  adicionar_atalho(atalhos, "<Control>f", ao_atalho_buscar, e);
  gtk_widget_add_controller(e->janela, atalhos);
  gtk_window_present(GTK_WINDOW(e->janela));
  g_signal_connect(e->janela, "notify::minimized",
                   G_CALLBACK(ao_notificar_minimizado), NULL);
  interface_mensagem(e, "POLI - Gestão de Ambientes", FALSE);
}

int interface_iniciar(int argc, char **argv) {
  GtkApplication *app =
      gtk_application_new("br.upe.datp", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(ativar), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
