#ifndef APLICACAO_H
#define APLICACAO_H

#include <stddef.h>
#include "salas.h"

typedef struct {
    Sala *salas;
    size_t quantidadeSalas;
    Reserva *reservas;
    size_t quantidadeReservas;
    Cancelamento *cancelamentos; /* dias em que uma aula semanal nao acontece */
    size_t quantidadeCancelamentos;
    Pendencia *pendencias; /* aulas do PDF que nao entraram */
    size_t quantidadePendencias;
    char caminhoSalas[512];
    char caminhoReservas[512];
    char caminhoHistorico[512];
    char caminhoCancelamentos[512];
    char caminhoPendencias[512];
    char caminhoPerfil[512];
    char usuario[64]; /* quem esta usando, vai para o historico */
    char setor[32];
} Aplicacao;

int aplicacao_iniciar(Aplicacao *app,
                      const char *caminhoSalas,
                      const char *caminhoReservas);
void aplicacao_finalizar(Aplicacao *app);

/* Troca quem esta usando o sistema e grava em perfil.txt. */
int aplicacao_definir_usuario(Aplicacao *app, const char *nome,
                              const char *setor);

/* Aceita id 0 para sala nova e preenche o id gerado em caso de sucesso. */
int aplicacao_salvar_sala(Aplicacao *app, Sala *sala);
int aplicacao_editar_sala(Aplicacao *app, int id, Sala dadosNovos);
/* Recusa (retorna 0) se a sala ainda tiver reservas. */
int aplicacao_excluir_sala(Aplicacao *app, int id);
size_t aplicacao_contar_reservas_da_sala(const Aplicacao *app, int idSala);

int aplicacao_salvar_reserva(Aplicacao *app, Reserva reserva);
/* A persistencia é aplicada atomicamente, e os ids são atribidos posteriormente em caso de sucesso */
int aplicacao_salvar_reservas(Aplicacao *app, Reserva reservas[],
                              size_t quantidade);
int aplicacao_editar_reserva(Aplicacao *app, int id, Reserva dadosNovos);
int aplicacao_excluir_reserva(Aplicacao *app, int id);
/* Retorna 1 se houver conflito, 0 se estiver livre e -1 para argumentos
 * invalidos. */
int aplicacao_buscar_conflito_reserva(const Aplicacao *app,
                                      const Reserva *proposta, int ignorarId,
                                      Reserva *conflitante);

/* Cancela um dia de uma reserva semanal; as outras semanas continuam.
 * Recusa se a reserva nao for semanal, se ela nao acontece nesse dia ou se o
 * dia ja estiver cancelado. */
int aplicacao_cancelar_dia(Aplicacao *app, int idReserva, Data data,
                           const char *motivo);
int aplicacao_desfazer_cancelamento(Aplicacao *app, int idReserva, Data data);
int aplicacao_dia_cancelado(const Aplicacao *app, int idReserva, Data data);

/* Pendencias da importacao do PDF. Adicionar ignora as que ja estao na
 * lista (mesma aula, dia e horario) e devolve quantas entraram, ou -1 se
 * nao conseguiu gravar. Resolver cria a reserva na sala escolhida e tira a
 * pendencia da lista. */
int aplicacao_adicionar_pendencias(Aplicacao *app, Pendencia novas[],
                                   size_t quantidade);
int aplicacao_resolver_pendencia(Aplicacao *app, int id, int idSala);
int aplicacao_descartar_pendencia(Aplicacao *app, int id);

Sala *aplicacao_buscar_sala(Aplicacao *app, int id);
// Procura uma sala pelo nome, sem diferenciar maiusculas, hifen, ponto e
// espaco ("A-03" = "A03"). ignorarId pula a propria sala ao editar (0 = nenhuma).
Sala *aplicacao_buscar_sala_por_nome(Aplicacao *app, const char *nome,
                                     int ignorarId);
Reserva *aplicacao_buscar_reserva(Aplicacao *app, int id);

Aplicacao *aplicacao_padrao(void);
void aplicacao_padrao_finalizar(void);

#endif
