#ifndef SALAS_H 
#define SALAS_H

typedef enum dia{
  SEG = 0,
  TER,
  QUA,
  QUI,
  SEX,
  SAB,
  DOM,
} Dia;

typedef struct horario{
  Dia dia; 
  int comeco; // Horario em minutos 0 - 1439
  int fim;
} Horario;

typedef struct sala{
  int id;
  char nome[20];
} Sala;

typedef struct reserva{
  int idSala;
  char cadeira[50];
  char professor[50];
  Horario horario;
} Reserva;

#endif
