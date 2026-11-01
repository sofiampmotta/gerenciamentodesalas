#include <stdio.h>
#include <stdlib.h>
#include "../include/salas.h" 

void carregarSalasDoCSV() {
    
    FILE *arquivo = fopen("data/salas.csv", "r"); // Abre ponteiro de arquivo para leitura do arquivo salas.csv
    
    if (arquivo == NULL) {
        printf("Erro: Nao foi possivel abrir o arquivo de salas.\n"); // Verifica se abriu
        return;
    }

    char linha[256];
    int totalSalasLidas = 0;

    printf("--- Lendo salas do arquivo com sucesso. ---\n");

    while (fgets(linha, sizeof(linha), arquivo) != NULL) { // Lê cada linha do arquivo
        Sala s;
        
        int itensLidos = sscanf(linha, "%d,%19[^,],%d", &s.id, s.nome, &s.capacidade); // Armazena os dados na struct

        if (itensLidos >= 3) {
            printf("ID: %d | Sala: %s | Capacidade: %d pessoas\n", s.id, s.nome, s.capacidade); // Imprime os dados lidos
            totalSalasLidas++;
        }
    }

    printf("\nTotal de salas lidas com sucesso: %d\n", totalSalasLidas);
    fclose(arquivo);
}
