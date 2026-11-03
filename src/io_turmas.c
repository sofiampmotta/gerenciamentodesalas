#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/salas.h"

#define MAX_RESERVAS 1000

Reserva reservas[MAX_RESERVAS];
int totalReservas = 0;

void carregarReservasDoCSV() {

    FILE *arquivo = fopen("data/reservas.csv", "r");

    if (arquivo == NULL) {
        printf("Erro: Nao foi possivel abrir o arquivo de reservas.\n");
        return;
    }

    char linha[256];
    int linhasLidas = 0;

    fgets(linha, sizeof(linha), arquivo); // pula o cabecalho (id,idSala,atividade,...)

    printf("--- Lendo reservas do arquivo com sucesso. ---\n");

    while (fgets(linha, sizeof(linha), arquivo) != NULL) {

        if (totalReservas >= MAX_RESERVAS) {
            printf("Aviso: limite de reservas atingido.\n");
            break;
        }

        int idLinha;
        char atividadeStr[20];
        char descricaoStr[50];
        char responsavelStr[80];
        char recorrenteStr[6];
        char diaStr[4];
        int comecoMin, fimMin;

        Reserva r;

        int itensLidos = sscanf(linha, "%d,%d,%19[^,],%49[^,],%79[^,],%5[^,],%3[^,],%d,%d",
                    &idLinha, &r.idSala, atividadeStr, descricaoStr,
                    responsavelStr, recorrenteStr, diaStr, &comecoMin, &fimMin);

        if (itensLidos == 9) {
            if (strcmp(atividadeStr, "AULA") == 0) r.atividade = AULA;
            else if (strcmp(atividadeStr, "MONITORIA") == 0) r.atividade = MONITORIA;
            else if (strcmp(atividadeStr, "EXTENSAO") == 0) r.atividade = EXTENSAO;
            else if (strcmp(atividadeStr, "EVENTO") == 0) r.atividade = EVENTO;
            else r.atividade = OUTRO;

            if (strcmp(diaStr, "SEG") == 0) r.horario.dia = SEG;
            else if (strcmp(diaStr, "TER") == 0) r.horario.dia = TER;
            else if (strcmp(diaStr, "QUA") == 0) r.horario.dia = QUA;
            else if (strcmp(diaStr, "QUI") == 0) r.horario.dia = QUI;
            else if (strcmp(diaStr, "SEX") == 0) r.horario.dia = SEX;
            else if (strcmp(diaStr, "SAB") == 0) r.horario.dia = SAB;
            else r.horario.dia = DOM;

            r.id = idLinha;
            strcpy(r.descricao, descricaoStr);
            strcpy(r.responsavel, responsavelStr);
            r.isRecorrente = (strcmp(recorrenteStr, "true") == 0);
            r.horario.comeco = comecoMin;
            r.horario.fim = fimMin;

            reservas[totalReservas] = r;
            totalReservas++;
            linhasLidas++;
        }
    }

    printf("Total de reservas carregadas: %d\n", linhasLidas);

    fclose(arquivo);
}