#ifndef AGENDA_H
#define AGENDA_H

#include <stddef.h>
#include "salas.h"

// Exporta as reservas para um arquivo de agenda (.ics), que abre no Google
// Agenda, no Outlook e no calendario do celular.
// - Reserva recorrente vira um evento semanal ate a data de fim.
// - O nome da sala vai no campo "local" do evento.
// - Dias cancelados saem da repeticao (o calendario nao mostra a aula).
// salas/quantidadeSalas servem so para achar o nome de cada sala.
// cancelamentos pode ser NULL se nao houver nenhum.
// Retorna 1 se deu certo e 0 se nao conseguiu gravar o arquivo.
int exportarAgendaICS(const char *caminho, const Reserva reservas[],
                      size_t quantidade, const Sala salas[],
                      size_t quantidadeSalas, const Cancelamento cancelamentos[],
                      size_t quantidadeCancelamentos);

#endif
