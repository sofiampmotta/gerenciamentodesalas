#ifndef ALOCACAO_H
#define ALOCACAO_H

#include "salas.h"
#include <stddef.h>
#include <stdbool.h>

Dia diaDaSemana(Data data); //Dia é o retorno (seg, ter) diaDaSemana é o nome da func, Data data é dia,mês,ano (parametros)

bool horariosConflitam(Horario a, Horario b); //comparo 2 horários

bool reservasConflitam(Reserva a, Reserva b); //comparo 2 reservas

bool intervalosConflitam(Horario a, Horario b); //comparo se o tempo daquela reserva acaba tomando minimamente o de outra, nem que um minuto

bool datasIguais(Data a, Data b);

bool existeConflito(Reserva nova, const Reserva reservas[], size_t quantidade);
#endif

