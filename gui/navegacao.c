#include "interface.h"
#include <string.h>

static const char *NOMES[5] = {"inicio", "visao", "busca", "reservas", "salas"};

void interface_ir_para(Estado *e, const char *tela) {
  if (!e || !e->stack || !tela)
    return;

  int idx = 3; /* telas fora do menu (ex.: "nova") destacam Reservas */
  for (int i = 0; i < 5; i++) {
    if (strcmp(NOMES[i], tela) == 0) {
      idx = i;
      break;
    }
  }

  const char *atual = gtk_stack_get_visible_child_name(GTK_STACK(e->stack));
  if (!atual || strcmp(atual, tela) != 0) {
    gtk_stack_set_visible_child_name(GTK_STACK(e->stack), tela);
  }

  if (e->botoes_menu[idx] &&
      !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e->botoes_menu[idx]))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->botoes_menu[idx]), TRUE);
  }
}
