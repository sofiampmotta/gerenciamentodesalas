#ifndef INTERFACE_H
#define INTERFACE_H
#include <gtk/gtk.h>

typedef struct {
    GtkWidget *janela;
    GtkWidget *stack;
    GtkWidget *barra_status;
    GtkWidget *raiz;
    GtkWidget *rotulo_nome;
    GtkWidget *rotulo_setor;
} Estado;

int  interface_iniciar(int argc, char **argv);
void interface_mensagem(Estado *e, const char *texto, gboolean erro);
void estilo_aplicar(void);

GtkWidget *tela_inicio_criar(Estado *e);
GtkWidget *tela_visao_criar(Estado *e);
GtkWidget *tela_busca_criar(Estado *e);
GtkWidget *tela_reservas_criar(Estado *e);
GtkWidget *tela_salas_criar(Estado *e);
GtkWidget *tela_login_criar(Estado *e);
void login_sair(GtkButton *b, gpointer dados);
#endif
