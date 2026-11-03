#ifndef AMBIENTE_H
#define AMBIENTE_H

#include "salas.h"

int buscarIndicePorId(int id);
int cadastrarSala(Sala nova);
void listarSalas();
int editarSala(int id, Sala dadosNovos);
int excluirSala(int id);

#endif