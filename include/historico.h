#ifndef HISTORICO_H
#define HISTORICO_H

#include <stddef.h>

// historico.h - registro de quem alterou o que no sistema.
//
// Cada alteracao vira uma linha no fim de historico.csv:
//   2026-11-28;14:02;Leidjane;Editou reserva;CALCULO 1 - TC: sala I-03 -> A-02
// O arquivo so recebe linhas novas, nunca e reescrito.

typedef struct {
  char data[11]; // 2026-11-28
  char hora[6];  // 14:02
  char quem[64];
  char acao[32];
  char detalhe[200];
} RegistroHistorico;

// Retorna 1 se gravou e 0 se deu erro.
int historico_registrar(const char *caminho, const char *quem,
                        const char *acao, const char *detalhe);

// Le o arquivo inteiro na ordem em que foi gravado (mais antigo primeiro).
// Arquivo que ainda nao existe conta como historico vazio.
// Retorna 1 se leu e 0 se deu erro. Liberar com historico_liberar.
int historico_carregar(const char *caminho, RegistroHistorico **registros,
                       size_t *quantidade);
void historico_liberar(RegistroHistorico *registros);

#endif
