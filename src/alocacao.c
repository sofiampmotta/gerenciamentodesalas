#include <stdio.h>
#include <string.h>
#include "alocacao.h"
#include <time.h>



Dia diaDaSemana(Data data) //essa função converte uma data (datas só são pra pontuais), em um dia da semana (SEG,TER) pra comparar algo pontual com fixo na mesma lógica
{
 struct tm calendario = {0};
 
 calendario.tm_mday=data.dia;// (dia do mês)
 calendario.tm_mon=data.mes -1; //janeiro é 0
 calendario.tm_year=data.ano -1900;
 calendario.tm_hour=12; //horário pra auxiliar, ajuda em horário de verão, vai que né
 calendario.tm_isdst = -1; //vê se existe horário de verão, vai que

 mktime(&calendario);
  int diaConvertido= (calendario.tm_wday +6)% 7; //calculo pq a sofia disse que segunda==0 só que o 0 no mktime é domingo, fazendo isso converte do mktime pro da sofia
  Dia resultado= (Dia)diaConvertido;
  return resultado;

}

bool horariosConflitam(Horario a, Horario b) // compara  se 2 horários acontecem no mesmo dia da semanada
{
    if (a.dia != b.dia) // esse dia aqui é o dia da semana , tipo TERÇA, n um número
    {
        return false; // os horários nao entram em conflito
    }

    return intervalosConflitam(a, b);
}

bool reservasConflitam(Reserva a, Reserva b)
{
    if (a.idSala != b.idSala)
    {
        return false; // as reservas nao entram em conflito, nao tem o msm id de Sala
    }
    if (a.isRecorrente && b.isRecorrente)
    {
        return horariosConflitam(a.horario, b.horario);
    }
    if (!a.isRecorrente && !b.isRecorrente)
    {
        return datasIguais(a.data, b.data)&& intervalosConflitam(a.horario, b.horario);
    }
    if (!a.isRecorrente && b.isRecorrente) //transforma a data de "a" em um dia de semana e compara com o dia de "b"
    {
        return diaDaSemana(a.data)== b.horario.dia && intervalosConflitam(a.horario, b.horario);
    }
    if (a.isRecorrente && !b.isRecorrente){
        return diaDaSemana(b.data)== a.horario.dia && intervalosConflitam(a.horario, b.horario);

    }
    return false;
}

bool datasIguais(Data a, Data b)
{
    if (a.dia != b.dia)// aqui o dia é um número, por exemplo 10, não é TERÇA
    {
        return false; // os dias nao sao iguais
    }
    if (a.mes != b.mes)
    {
        return false; // os meses nao sao iguais
    }
    if (a.ano != b.ano){
        return false; // os anos nao sao iguais
    }
return true;
    
}
bool intervalosConflitam(Horario a, Horario b)// verifica se uma aula começa antes do fim da outra e vice-versa
{
    return a.comeco < b.fim && b.comeco < a.fim;
}

bool existeConflito( // basicamente cria um loop pra ir de duas em duas reservas e ir comparando elas entre si
    Reserva nova, // a reserva que a leidjane quer cadastras
    const Reserva reservas[], //o vetor com as reservas já cadastradas
    size_t quantidade //quantas reservas tem nesse vetor
)
{
    for (size_t i=0; i<quantidade; i++){
        if (reservasConflitam(nova, reservas[i])){
            return true; //qualquer comparação que retorne true, a existeConflito também retorna true e o loop para, ele n precisa mais procurar
        }
    }
    return false;
}


