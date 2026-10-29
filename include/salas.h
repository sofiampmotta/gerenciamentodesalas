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

typedef enum atividade{
  AULA,
  MONITORIA,
  EXTENSAO,
  EVENTO,
  OUTRO
} Atividade;

typedef struct data {
  int dia;
  int mes;
  int ano;
} Data;

typedef struct sala{
  int id;
  char nome[20];
  int capacidade;
  bool isAcessivel;
  bool hasProjetor;
  bool hasTelevisao;
  int computador;
} Sala;

typedef struct reserva{
  int idSala;
  Atividade atividade;
  char descricao[50];
  char responsavel[50];
  bool isRecorrente;
  Data data;
  Horario horario;
} Reserva;

#endif
