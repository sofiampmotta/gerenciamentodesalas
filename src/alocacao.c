#include "alocacao.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

Dia diaDaSemana(Data data) // converte uma data de calendario em dia semanal.
{
  struct tm calendario = {0};

  calendario.tm_mday = data.dia;    // (dia do mês)
  calendario.tm_mon = data.mes - 1; // janeiro é 0
  calendario.tm_year = data.ano - 1900;
  calendario.tm_hour =
      12; // horário pra auxiliar, ajuda em horário de verão, vai que né
  calendario.tm_isdst = -1; // vê se existe horário de verão, vai que

  mktime(&calendario);
  int diaConvertido =
      (calendario.tm_wday + 6) %
      7; // calculo pq a sofia disse que segunda==0 só que o 0 no mktime é
         // domingo, fazendo isso converte do mktime pro da sofia
  Dia resultado = (Dia)diaConvertido;
  return resultado;
}

bool horariosConflitam(
    Horario a,
    Horario b) // compara  se 2 horários acontecem no mesmo dia da semanada
{
  if (a.dia !=
      b.dia) // esse dia aqui é o dia da semana , tipo TERÇA, n um número
  {
    return false; // os horários nao entram em conflito
  }

  return intervalosConflitam(a, b);
}

bool reservaOcorreEm(Reserva reserva, Data data) {
  if (!reserva.isRecorrente)
    return datasIguais(reserva.data, data);

  int dataConsulta = data.ano * 10000 + data.mes * 100 + data.dia;
  int inicio =
      reserva.data.ano * 10000 + reserva.data.mes * 100 + reserva.data.dia;
  int fim = reserva.dataFim.ano * 10000 + reserva.dataFim.mes * 100 +
            reserva.dataFim.dia;
  return dataConsulta >= inicio && dataConsulta <= fim &&
         diaDaSemana(data) == reserva.horario.dia;
}

bool reservasConflitam(Reserva a, Reserva b) {
  if (a.idSala != b.idSala) {
    return false; // as reservas nao entram em conflito, nao tem o msm id de
                  // Sala
  }
  if (a.isRecorrente && b.isRecorrente) {
    int inicio = a.data.ano * 10000 + a.data.mes * 100 + a.data.dia;
    int fim = a.dataFim.ano * 10000 + a.dataFim.mes * 100 + a.dataFim.dia;
    int outroInicio = b.data.ano * 10000 + b.data.mes * 100 + b.data.dia;
    int outroFim = b.dataFim.ano * 10000 + b.dataFim.mes * 100 + b.dataFim.dia;
    return a.horario.dia == b.horario.dia && inicio <= outroFim &&
           outroInicio <= fim && horariosConflitam(a.horario, b.horario);
  }
  if (!a.isRecorrente && !b.isRecorrente) {
    return datasIguais(a.data, b.data) &&
           intervalosConflitam(a.horario, b.horario);
  }
  if (!a.isRecorrente && b.isRecorrente) // transforma a data de "a" em um dia
                                         // de semana e compara com o dia de "b"
  {
    return reservaOcorreEm(b, a.data) &&
           intervalosConflitam(a.horario, b.horario);
  }
  if (a.isRecorrente && !b.isRecorrente) {
    return reservaOcorreEm(a, b.data) &&
           intervalosConflitam(a.horario, b.horario);
  }
  return false;
}

bool datasIguais(Data a, Data b) {
  if (a.dia != b.dia) // aqui o dia é um número, por exemplo 10, não é TERÇA
  {
    return false; // os dias nao sao iguais
  }
  if (a.mes != b.mes) {
    return false; // os meses nao sao iguais
  }
  if (a.ano != b.ano) {
    return false; // os anos nao sao iguais
  }
  return true;
}
bool intervalosConflitam(
    Horario a,
    Horario b) // verifica se uma aula começa antes do fim da outra e vice-versa
{
  return a.comeco < b.fim && b.comeco < a.fim;
}

bool existeConflito( // basicamente cria um loop pra ir de duas em duas reservas
                     // e ir comparando elas entre si
    Reserva nova,    // a reserva que a leidjane quer cadastras
    const Reserva reservas[], // o vetor com as reservas já cadastradas
    size_t quantidade         // quantas reservas tem nesse vetor
) {
  for (size_t i = 0; i < quantidade; i++) {
    if (reservasConflitam(nova, reservas[i])) {
      return true; // qualquer comparação que retorne true, a existeConflito
                   // também retorna true e o loop para, ele n precisa mais
                   // procurar
    }
  }
  return false;
}
