#ifndef INTERFACE_H
#define INTERFACE_H
#include <gtk/gtk.h>
#include "dados.h"

typedef struct {
    GtkWidget *janela;
    GtkWidget *raiz;            /* alterna entre "login" e "app" */
    GtkWidget *stack;
    GtkWidget *barra_status;
    GtkWidget *rotulo_nome;
    GtkWidget *rotulo_setor;
    GtkWidget *botoes_menu[5];
} Estado;

int  interface_iniciar(int argc, char **argv);
void interface_mensagem(Estado *e, const char *texto, gboolean erro);
void interface_ir_para(Estado *e, const char *tela);
void estilo_aplicar(void);
void login_sair(GtkButton *b, gpointer dados);

GtkWidget *tela_login_criar(Estado *e);
GtkWidget *tela_inicio_criar(Estado *e);
GtkWidget *tela_visao_criar(Estado *e);
GtkWidget *tela_busca_criar(Estado *e);
GtkWidget *tela_reservas_criar(Estado *e);
GtkWidget *tela_nova_reserva_criar(Estado *e);
GtkWidget *tela_salas_criar(Estado *e);
void nova_reserva_preencher(const char *sala, int dia, int ini, int fim, int pessoas);
#endif
