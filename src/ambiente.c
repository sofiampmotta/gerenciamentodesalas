#include <stdio.h>
#include <string.h>
#include "../include/salas.h"

#define MAX_SALAS 200

static Sala salas[MAX_SALAS];
static int totalSalas = 0;

// retorna o indice da sala com esse id, ou -1 se nao achar
int buscarIndicePorId(int id) {
    for (int i = 0; i < totalSalas; i++) {
        if (salas[i].id == id) {
            return i;
        }
    }
    return -1;
}

// retorna 1 (sucesso) ou 0 (falha, com motivo no printf)
int cadastrarSala(Sala nova) {

    if (totalSalas >= MAX_SALAS) {
        printf("Erro: limite de salas atingido.\n");
        return 0;
    }

    if (buscarIndicePorId(nova.id) != -1) {
        printf("Erro: ja existe uma sala com id %d.\n", nova.id);
        return 0;
    }

    if (nova.capacidade < 0) {
        printf("Erro: capacidade nao pode ser negativa.\n");
        return 0;
    }

    salas[totalSalas] = nova;
    totalSalas++;
    return 1;
}

void listarSalas() {

    if (totalSalas == 0) {
        printf("Nenhuma sala cadastrada.\n");
        return;
    }

    printf("--- Lista de salas (%d no total) ---\n", totalSalas);

    for (int i = 0; i < totalSalas; i++) {
        printf("ID: %d | Nome: %s | Capacidade: %d | Acessivel: %s | Projetor: %s | TV: %s | Computadores: %d\n",
            salas[i].id,
            salas[i].nome,
            salas[i].capacidade,
            salas[i].isAcessivel ? "sim" : "nao",
            salas[i].hasProjetor ? "sim" : "nao",
            salas[i].hasTelevisao ? "sim" : "nao",
            salas[i].computador);
    }
}

// retorna 1 (sucesso) ou 0 (nao achou o id)
int editarSala(int id, Sala dadosNovos) {

    int indice = buscarIndicePorId(id);

    if (indice == -1) {
        printf("Erro: sala com id %d nao encontrada.\n", id);
        return 0;
    }

    if (dadosNovos.capacidade < 0) {
        printf("Erro: capacidade nao pode ser negativa.\n");
        return 0;
    }

    dadosNovos.id = id; // mantem o id original
    salas[indice] = dadosNovos;

    return 1;
}

// retorna 1 (sucesso) ou 0 (nao achou o id)
int excluirSala(int id) {

    int indice = buscarIndicePorId(id);

    if (indice == -1) {
        printf("Erro: sala com id %d nao encontrada.\n", id);
        return 0;
    }

    salas[indice] = salas[totalSalas - 1];
    totalSalas--;

    return 1;
}