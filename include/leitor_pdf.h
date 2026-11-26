#ifndef LEITOR_PDF_H
#define LEITOR_PDF_H
#define MAX_STR 256
#define MAX_SLOTS 16
#define MAX_DIAS 14
#include <stddef.h>
#include "aplicacao.h"
typedef struct
{
  char dia[16];
  char sala[64];
  char intervalos[MAX_SLOTS][16];
  int num_intervalos;
} HorarioPDF;

typedef struct
{
  char turma[32];
  int vagas;
  char codigo[32];
  char componente[MAX_STR];
  char professor[MAX_STR];
  int carga_horaria;
  HorarioPDF horarios[MAX_DIAS];
  int num_horarios;
} TurmaPDF;
int converter_turmas_reservas(const Aplicacao *app,
                              const TurmaPDF *turmas,
                              size_t quantidadeTurmas,
                              Data dataInicio,
                              Data dataFim,
                              Reserva **resultado,
                              size_t *quantidadeReservas);
int lerPDF(const char *caminho,
           TurmaPDF **turmas,
           size_t *quantidade);
int importarPDF(Aplicacao *app,
                const char *caminho,
                Data inicio,
                Data fim,
                size_t *importadas,
                size_t *duplicadas);

#endif
