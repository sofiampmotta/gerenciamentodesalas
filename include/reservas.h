#ifndef RESERVAS_H
#define RESERVAS_H

#include "salas.h"

// localiza o indice de uma reserva pelo id, ou -1 se nao existir
int buscarIndiceReservaPorId(int id);

// edita uma reserva existente, bloqueando se gerar conflito com outra reserva
// retorna 1 (sucesso) ou 0 (falha -- id nao existe ou geraria conflito)
int editarReserva(int id, Reserva dadosNovos);

// exclui uma reserva pelo id
// retorna 1 (sucesso) ou 0 (id nao encontrado)
int excluirReserva(int id);

#endif