#ifndef DADOS_H
#define DADOS_H
#include <glib.h>

#define MAX_SALAS     64
#define MAX_RESERVAS  512
#define DATA_PADRAO   20261113   /* provisório: sexta, 13/11/2026 (dia do protótipo) */
#define FIM_SEMESTRE  20261218   /* provisório: confirmar com o DATP */

/* PROVISÓRIO: trocar pelas structs reais do grupo na integração */
typedef struct {
    char nome[64];
    char local[64];
    int  lugares;
    int  projetor;
    int  acessivel;
} SalaTeste;

typedef struct {
    int  idReserva;
    char atividade[96];
    char tipo[16];          /* Aula, Monitoria, Extensão, Evento */
    char responsavel[64];
    char descricao[128];
    char sala[64];
    int  dia;               /* AAAAMMDD */
    int  ini;               /* minutos desde 00:00 */
    int  fim;
} ReservaTeste;

typedef struct {
    int  ok;                /* 1 = pode salvar */
    int  sala_idx;          /* -1 se a sala não existe */
    int  conflito_idx;      /* índice da reserva em conflito, ou -1 */
    int  dia_conflito;
    int  cabe;
    int  lugares;
    int  faltam;
    int  projetor;
    char titulo[48];
    char motivo[200];
} VerificacaoReserva;

extern SalaTeste    g_salas[MAX_SALAS];
extern int          g_n_salas;
extern ReservaTeste g_reservas[MAX_RESERVAS];
extern int          g_n_reservas;

void dados_iniciar(void);
int  dados_indice_sala(const char *nome);
int  dados_data_valida(int dia);
void dados_data_texto(int dia, char *buf, size_t tam);
void dados_hora_texto(int min, char *buf, size_t tam);
int  dados_conflito(const char *sala, int dia, int ini, int fim, int ignorar_id);
void dados_verificar_reserva(const ReservaTeste *r, int pessoas, int repetir, VerificacaoReserva *v);
int  dados_salvar_reserva(const ReservaTeste *r, int pessoas, int repetir, char *msg, size_t tam);
#endif
