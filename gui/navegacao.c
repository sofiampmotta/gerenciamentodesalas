#include "interface.h"
#include <string.h>

static const char *NOMES[5] = { "inicio", "visao", "busca", "reservas", "salas" };

void interface_ir_para(Estado *e, const char *tela) {
    int idx = 3;   /* telas fora do menu (ex.: "nova") destacam Reservas */
    for (int i = 0; i < 5; i++)
        if (strcmp(NOMES[i], tela) == 0) idx = i;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->botoes_menu[idx]), TRUE);
    gtk_stack_set_visible_child_name(GTK_STACK(e->stack), tela);
}
