#ifndef SALAS_H
#define SALAS_H

#include <stdbool.h>

typedef enum diaSemana{
  SEG = 0,
  TER,
  QUA,
  QUI,
  SEX,
  SAB,
  DOM
} Dia;

typedef struct horario{
  Dia dia;
  int comeco; // Horario em minutos 0 - 1439
  int fim;
} Horario;

// Tipo de uso de um ambiente (aula fixa, monitoria, extensao, evento pontual, etc)
typedef enum atividade{
  AULA,
  MONITORIA,
  EXTENSAO,
  EVENTO,
  OUTRO
} Atividade;

// Data de calendario: inicio de uma serie recorrente ou evento pontual.
typedef struct data {
  int dia;
  int mes;
  int ano;
} Data;

// Um ambiente fisico da POLI (sala de aula, laboratorio, auditorio)
typedef struct sala{
  int id;
  char nome[20];
  int capacidade;
  bool isAcessivel;
  bool hasProjetor;
  bool hasTelevisao;
  int computador;
  char local[128];
} Sala;

// Uma reserva/alocacao de um ambiente.
// Recorrente: data e dataFim delimitam a serie; horario.dia define o dia semanal.
// Pontual: data define a ocorrencia e dataFim fica vazia.
typedef struct reserva{
  int id;              // identificador unico da reserva (necessario p/ editar/excluir)
  int idSala;
  Atividade atividade;
  char descricao[50];
  char responsavel[80]; 
  bool isRecorrente;
  Data data;
  Horario horario;
  Data dataFim;
} Reserva;

// Um dia em que uma reserva recorrente nao acontece (feriado, falta do
// professor, prova em outra sala...). As outras semanas continuam normais.
typedef struct cancelamento{
  int idReserva;
  Data data;
  char motivo[60];
} Cancelamento;

// Aula do PDF que nao entrou no sistema e espera alguem resolver.
typedef enum {
  PEND_CONFLITO,   // a sala ja tinha outra aula nesse horario
  PEND_SEM_SALA,   // a sala do PDF nao esta cadastrada
  PEND_SALA_VAZIA, // o proprio PDF nao informa a sala
  QTD_MOTIVOS_PENDENCIA
} MotivoPendencia;

typedef struct pendencia{
  int id;
  MotivoPendencia motivo;
  Reserva reserva;  // a aula como veio do PDF (idSala 0 se nao tem sala)
  char salaPDF[64]; // nome da sala escrito no PDF
  char detalhe[100]; // ex.: "I-03 ja tem CALCULO 1 - TC"
} Pendencia;

#endif
