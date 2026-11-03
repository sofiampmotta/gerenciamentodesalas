#include <stdio.h>
#include <string.h>
#include "../include/salas.h"
#include "../include/ambiente.h"

void carregarSalasDoCSV();
void carregarReservasDoCSV();

int main() {
    printf("=== INICIANDO SISTEMA DE SALAS ===\n\n");

    carregarSalasDoCSV();

    printf("\n");

    carregarReservasDoCSV();

    return 0;
}