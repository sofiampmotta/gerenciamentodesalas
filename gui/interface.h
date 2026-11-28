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
/* Pergunta "tem certeza?"; ao_confirmar só roda se a pessoa clicar em botao. */
void interface_confirmar(Estado *e, const char *titulo, const char *detalhe,
                         const char *botao, void (*ao_confirmar)(gpointer),
                         gpointer dados);
void estilo_aplicar(void);
void login_sair(GtkButton *b, gpointer dados);

GtkWidget *tela_inicio_criar(Estado *e);
GtkWidget *tela_visao_criar(Estado *e);
GtkWidget *tela_busca_criar(Estado *e);
GtkWidget *tela_reservas_criar(Estado *e);
GtkWidget *tela_nova_reserva_criar(Estado *e);
GtkWidget *tela_salas_criar(Estado *e);
void nova_reserva_preencher(const char *sala, int dia, int ini, int fim);
GtkWidget *tema_criar_chave(void);
GtkWidget *seletor_data_criar(int data, void (*ao_mudar)(gpointer), gpointer dados);
int seletor_data_get(GtkWidget *w);
void seletor_data_set(GtkWidget *w, int data);
GtkWidget *logo_poli_criar(void);
void visual_aplicar(void);
void nova_reserva_editar(int id);
void tela_reservas_focar_busca(void);
void acao_log(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
const char *acao_hora(int min);
int  dados_dia_hoje(void);
int  dados_dia_base_inicial(void);
#endif
