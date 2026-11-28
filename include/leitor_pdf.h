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

// Resultado de uma importacao: o que entrou e o que ficou de fora.
typedef struct
{
  size_t importadas; // reservas novas salvas
  size_t duplicadas; // ja existiam iguais no sistema
  size_t conflitos;  // batiam com outra reserva na mesma sala e horario
  size_t semSala;    // sala nao cadastrada, dia ou horario invalido
} ResumoImportacao;

// Respostas de lerPDF e importarPDF
#define LEITOR_OK 1
#define LEITOR_ERRO 0        // arquivo em formato desconhecido ou sem turmas
#define LEITOR_SEM_PDF (-1)  // nao foi possivel ler o PDF (pdftotext ausente?)
#define LEITOR_ERRO_SALVAR (-2) // leu o PDF, mas nao conseguiu gravar as reservas

// Converte as turmas em reservas recorrentes. Horarios com sala nao
// cadastrada ou dia/horario invalido sao pulados e contados em 'ignorados'
// (pode ser NULL).
int converter_turmas_reservas(const Aplicacao *app,
                              const TurmaPDF *turmas,
                              size_t quantidadeTurmas,
                              Data dataInicio,
                              Data dataFim,
                              Reserva **resultado,
                              size_t *quantidadeReservas,
                              size_t *ignorados);
int lerPDF(const char *caminho,
           TurmaPDF **turmas,
           size_t *quantidade);

// Importa o relatorio Turma/Horario. As reservas que dao certo sao salvas;
// as com conflito ou sala desconhecida sao puladas e contadas no resumo.
int importarPDF(Aplicacao *app,
                const char *caminho,
                Data inicio,
                Data fim,
                ResumoImportacao *resumo);

#endif
