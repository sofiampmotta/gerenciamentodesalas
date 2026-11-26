#ifndef DADOS_H
#define DADOS_H
#include <glib.h>

#define DATA_PADRAO 20261113

/* Adaptadores de apresentação para os campos consumidos pelas telas GTK. */
typedef struct {
    int id;
    char nome[64];
    char local[128];
    int  lugares;
    int  projetor;
    int  acessivel;
} SalaVista;

typedef struct {
    int  idReserva;
    char atividade[96];
    char tipo[16];          /* Aula, Monitoria, Extensão, Evento, Outro */
    char responsavel[64];
    char sala[64];
    int  isRecorrente;
    int  dia;               /* início ou data pontual: AAAAMMDD */
    int  fimRecorrencia;    /* AAAAMMDD; recorrências vão até 31/12 */
    int  ini;               /* minutos desde 00:00 */
    int  fim;
} ReservaVista;

typedef struct {
    int  ok;
    int  sala_idx;
    int  conflito;
    int  projetor;
    char atividade_conflito[50];
    char titulo[48];
    char motivo[200];
} VerificacaoReserva;

extern SalaVista   *g_salas;
extern int          g_n_salas;
extern ReservaVista *g_reservas;
extern int          g_n_reservas;

void dados_iniciar(void);
int  dados_data_hoje(void);
int  dados_indice_sala(const char *nome);
int  dados_data_valida(int dia);
void dados_data_texto(int dia, char *buf, size_t tam);
void dados_hora_texto(int min, char *buf, size_t tam);
int  dados_reserva_ocorre_em(const ReservaVista *r, int dia);
int  dados_conflito(const char *sala, int dia, int ini, int fim, int ignorar_id);
void dados_verificar_reserva(const ReservaVista *r, VerificacaoReserva *v);
int  dados_salvar_reserva(const ReservaVista *r, char *msg, size_t tam);
int  dados_indice_reserva(int id);
int  dados_atualizar_reserva(const ReservaVista *r, char *msg, size_t tam);
#endif
