#include <stdio.h>
// declarar funçao para conseguir carregar as salas do CSV
void carregarSalasDoCSV();

int main() {
    printf("=== INICIANDO TESTE DO SISTEMA DE SALAS ===\n\n");
    
    carregarSalasDoCSV(); // chama a função
    
    return 0;
}