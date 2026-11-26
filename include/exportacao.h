#ifndef EXPORTACAO_H
#define EXPORTACAO_H

#include <stddef.h>

typedef struct {
  char atividade[96];
  char tipo[16];
  char responsavel[64];
  char sala[64];
  int inicio;
  int fim;
  int recorrente;
} ReservaExportacao;

int exportar_csv(const char *caminho, int dia, const char *filtro_atividade,
                 const char *filtro_sala, const ReservaExportacao *reservas,
                 size_t quantidade);
int exportar_pdf(const char *caminho, int dia, const char *filtro_atividade,
                 const char *filtro_sala, const ReservaExportacao *reservas,
                 size_t quantidade);

#endif
