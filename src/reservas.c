// reservas.c
#include <stdio.h>
#include "../include/salas.h"
#include "../include/alocacao.h"
#include "../include/reservas.h"

#define MAX_RESERVAS 1000

// essas duas variaveis ja existem no io_turmas.c (sao globais)
extern Reserva reservas[];
extern int totalReservas;

int buscarIndiceReservaPorId(int id) {
    for (int i = 0; i < totalReservas; i++) {
        if (reservas[i].id == id) {
            return i;
        }
    }
    return -1;
}

int editarReserva(int id, Reserva dadosNovos) {

    int indice = buscarIndiceReservaPorId(id);

    if (indice == -1) {
        printf("Erro: reserva com id %d nao encontrada.\n", id);
        return 0;
    }

    // monta uma copia do array SEM a reserva que estamos editando,
    // pra existeConflito nao comparar a reserva editada contra ela mesma
    Reserva copiaSemAtual[MAX_RESERVAS];
    int totalCopia = 0;

    for (int i = 0; i < totalReservas; i++) {
        if (i != indice) {
            copiaSemAtual[totalCopia] = reservas[i];
            totalCopia++;
        }
    }

    dadosNovos.id = id; // mantem o id original,

    if (existeConflito(dadosNovos, copiaSemAtual, totalCopia)) {
        printf("Erro: edicao gera conflito de horario com outra reserva.\n");
        return 0; // dados originais nao sao alterados -- so retornamos
    }

    reservas[indice] = dadosNovos;
    return 1;
}

int excluirReserva(int id) {

    int indice = buscarIndiceReservaPorId(id);

    if (indice == -1) {
        printf("Erro: reserva com id %d nao encontrada.\n", id);
        return 0;
    }

    reservas[indice] = reservas[totalReservas - 1];
    totalReservas--;

    return 1;
}