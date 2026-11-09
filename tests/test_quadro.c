// tests/test_quadro.c
#include <stdio.h>
#include "../include/salas.h"
#include "../include/quadro.h"

Reserva reservas[1000];
int totalReservas = 0;

int main() {

    // monta um cenario falso: 3 reservas na segunda, 1 na terca
    Reserva r1 = {1, 10, AULA, "Calculo 1", "Prof. A", true, {0,0,0}, {SEG, 480, 540}};
    Reserva r2 = {2, 20, MONITORIA, "Monitoria Fisica", "Prof. B", true, {0,0,0}, {SEG, 420, 480}};
    Reserva r3 = {3, 10, AULA, "Quimica", "Prof. C", true, {0,0,0}, {SEG, 600, 660}};
    Reserva r4 = {4, 10, AULA, "Historia", "Prof. D", true, {0,0,0}, {TER, 480, 540}};

    reservas[totalReservas++] = r1;
    reservas[totalReservas++] = r2;
    reservas[totalReservas++] = r3;
    reservas[totalReservas++] = r4;

    Reserva saida[100];
    FiltroReservas semFiltro = {0}; // todos os campos false/0

    printf("=== TESTE: segunda-feira, sem filtro (espera 3 reservas, ordenadas) ===\n");
    int qtd1 = obterReservasDoDia(SEG, semFiltro, saida, 100);
    printf("Esperado: 3 | Resultado: %d\n", qtd1);
    for (int i = 0; i < qtd1; i++) {
        printf("  %d - inicio %d - %s\n", i, saida[i].horario.comeco, saida[i].descricao);
    }
    printf("Esperado ordem: Monitoria Fisica (420), Calculo 1 (480), Quimica (600)\n\n");

    printf("=== TESTE: segunda-feira, filtrando so AULA (espera 2) ===\n");
    FiltroReservas filtroAula = {0};
    filtroAula.filtrarPorAtividade = true;
    filtroAula.atividade = AULA;
    int qtd2 = obterReservasDoDia(SEG, filtroAula, saida, 100);
    printf("Esperado: 2 | Resultado: %d\n\n", qtd2);

    printf("=== TESTE: segunda-feira, filtrando sala 20 (espera 1) ===\n");
    FiltroReservas filtroSala = {0};
    filtroSala.filtrarPorSala = true;
    filtroSala.idSala = 20;
    int qtd3 = obterReservasDoDia(SEG, filtroSala, saida, 100);
    printf("Esperado: 1 | Resultado: %d\n\n", qtd3);

    printf("=== TESTE: terca-feira (espera 1) ===\n");
    int qtd4 = obterReservasDoDia(TER, semFiltro, saida, 100);
    printf("Esperado: 1 | Resultado: %d\n\n", qtd4);

    printf("=== Fim dos testes ===\n");

printf("=== TESTE: exportar CSV com todas as colunas ===\n");
Reserva saidaParaExport[100];
int qtdExport = obterReservasDoDia(SEG, semFiltro, saidaParaExport, 100);
ColunasExportacao colunasTudo = colunasExportacaoPadrao();
int okExport1 = exportarReservasCSV(saidaParaExport, qtdExport, colunasTudo, "teste_export_completo.csv");
printf("Esperado: sucesso | Resultado: %s\n\n", okExport1 ? "sucesso" : "falhou");

printf("=== TESTE: exportar CSV so com descricao e sala ===\n");
ColunasExportacao colunasParciais = {0};
colunasParciais.incluirDescricao = true;
colunasParciais.incluirSala = true;
int okExport2 = exportarReservasCSV(saidaParaExport, qtdExport, colunasParciais, "teste_export_parcial.csv");
printf("Esperado: sucesso | Resultado: %s\n\n", okExport2 ? "sucesso" : "falhou");

printf("=== TESTE: exportar PDF ===\n");
int okPDF = exportarReservasPDF(saidaParaExport, qtdExport, "Segunda-feira", "teste_quadro.pdf");
printf("Esperado: sucesso | Resultado: %s\n\n", okPDF ? "sucesso" : "falhou");

    return 0;
}