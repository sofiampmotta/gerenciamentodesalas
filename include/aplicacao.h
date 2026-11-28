#ifndef APLICACAO_H
#define APLICACAO_H

#include <stddef.h>
#include "salas.h"

typedef struct {
    Sala *salas;
    size_t quantidadeSalas;
    Reserva *reservas;
    size_t quantidadeReservas;
    char caminhoSalas[512];
    char caminhoReservas[512];
    char caminhoHistorico[512];
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

Sala *aplicacao_buscar_sala(Aplicacao *app, int id);
// Procura uma sala pelo nome, sem diferenciar maiusculas, hifen, ponto e
// espaco ("A-03" = "A03"). ignorarId pula a propria sala ao editar (0 = nenhuma).
Sala *aplicacao_buscar_sala_por_nome(Aplicacao *app, const char *nome,
                                     int ignorarId);
Reserva *aplicacao_buscar_reserva(Aplicacao *app, int id);

Aplicacao *aplicacao_padrao(void);
void aplicacao_padrao_finalizar(void);

#endif
