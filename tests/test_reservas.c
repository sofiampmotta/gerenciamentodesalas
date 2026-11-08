#include <stdio.h>
#include "../include/salas.h"
#include "../include/reservas.h"

Reserva reservas[1000];
int totalReservas = 0;

int main() {

    printf("=== TESTE: editar reserva sem conflito (so descricao) ===\n");
    Reserva r1 = {1, 1, AULA, "Calculo 1", "Prof. A", true, {0,0,0}, {SEG, 420, 480}};
    reservas[totalReservas++] = r1;

    Reserva r1editada = r1;
    snprintf(r1editada.descricao, sizeof(r1editada.descricao), "Calculo 1 - sala trocada de nome");
    int ok1 = editarReserva(1, r1editada);
    printf("Esperado: sucesso | Resultado: %s\n\n", ok1 ? "sucesso" : "falhou");

    printf("=== TESTE: editar reserva pra horario ocupado por outra ===\n");
    Reserva r2 = {2, 1, AULA, "Fisica 1", "Prof. B", true, {0,0,0}, {TER, 420, 480}};
    reservas[totalReservas++] = r2;

    Reserva r1conflito = r1editada;
    r1conflito.horario.dia = TER; // tenta mover a reserva 1 pro mesmo horario da reserva 2
    int ok2 = editarReserva(1, r1conflito);
    printf("Esperado: falhou | Resultado: %s\n\n", ok2 ? "sucesso" : "falhou");

    printf("=== TESTE: editar id inexistente ===\n");
    int ok3 = editarReserva(999, r1editada);
    printf("Esperado: falhou | Resultado: %s\n\n", ok3 ? "sucesso" : "falhou");

    printf("=== TESTE: excluir reserva existente ===\n");
    int ok4 = excluirReserva(2);
    printf("Esperado: sucesso | Resultado: %s\n\n", ok4 ? "sucesso" : "falhou");

    printf("=== TESTE: excluir id ja excluido ===\n");
    int ok5 = excluirReserva(2);
    printf("Esperado: falhou | Resultado: %s\n\n", ok5 ? "sucesso" : "falhou");

    printf("=== Fim dos testes ===\n");
    return 0;
}