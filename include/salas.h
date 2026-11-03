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

// Data de calendario, usada em reservas NAO recorrentes (eventos pontuais)
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
} Sala;

// Uma reserva/alocacao de um ambiente.
// Se isRecorrente == true: usar horario.dia (repete toda semana nesse dia), data e ignorada.
// Se isRecorrente == false: usar data (evento pontual), horario.dia e ignorado.
typedef struct reserva{
  int id;              // identificador unico da reserva (necessario p/ editar/excluir)
  int idSala;
  Atividade atividade;
  char descricao[50];
  char responsavel[80]; 
  bool isRecorrente;
  Data data;
  Horario horario;
} Reserva;

#endif
