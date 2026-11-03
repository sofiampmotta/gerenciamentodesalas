#include <stdio.h>
#include "../include/ambiente.h"

int main() {
    printf("=== TESTE: cadastro de sala valida ===\n");
    Sala s1 = {1, "TESTE-A", 30, true, false, false, 0};
    int r1 = cadastrarSala(s1);
    printf("Esperado: sucesso | Resultado: %s\n\n", r1 ? "sucesso" : "falhou");

    printf("=== TESTE: cadastro com id duplicado ===\n");
    int r2 = cadastrarSala(s1); // mesmo id de novo
    printf("Esperado: falhou | Resultado: %s\n\n", r2 ? "sucesso" : "falhou");

    printf("=== TESTE: cadastro com capacidade negativa ===\n");
    Sala s2 = {2, "TESTE-B", -5, false, false, false, 0};
    int r3 = cadastrarSala(s2);
    printf("Esperado: falhou | Resultado: %s\n\n", r3 ? "sucesso" : "falhou");

    printf("=== TESTE: editar sala existente ===\n");
    Sala editada = {1, "TESTE-A-EDITADA", 50, false, true, false, 3};
    int r4 = editarSala(1, editada);
    printf("Esperado: sucesso | Resultado: %s\n\n", r4 ? "sucesso" : "falhou");

    printf("=== TESTE: editar sala inexistente ===\n");
    int r5 = editarSala(999, editada);
    printf("Esperado: falhou | Resultado: %s\n\n", r5 ? "sucesso" : "falhou");

    printf("=== TESTE: excluir sala existente ===\n");
    int r6 = excluirSala(1);
    printf("Esperado: sucesso | Resultado: %s\n\n", r6 ? "sucesso" : "falhou");

    printf("=== TESTE: excluir sala ja excluida ===\n");
    int r7 = excluirSala(1);
    printf("Esperado: falhou | Resultado: %s\n\n", r7 ? "sucesso" : "falhou");

    printf("=== Fim dos testes ===\n");
    return 0;
}