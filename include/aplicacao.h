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
} Aplicacao;

int aplicacao_iniciar(Aplicacao *app,
                      const char *caminhoSalas,
                      const char *caminhoReservas);
void aplicacao_finalizar(Aplicacao *app);

int aplicacao_salvar_sala(Aplicacao *app, Sala sala);
int aplicacao_editar_sala(Aplicacao *app, int id, Sala dadosNovos);
int aplicacao_excluir_sala(Aplicacao *app, int id);

int aplicacao_salvar_reserva(Aplicacao *app, Reserva reserva);
int aplicacao_editar_reserva(Aplicacao *app, int id, Reserva dadosNovos);
int aplicacao_excluir_reserva(Aplicacao *app, int id);

Sala *aplicacao_buscar_sala(Aplicacao *app, int id);
Reserva *aplicacao_buscar_reserva(Aplicacao *app, int id);

Aplicacao *aplicacao_padrao(void);
void aplicacao_padrao_finalizar(void);

#endif
