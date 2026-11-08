#include <stdio.h>
#include "../include/alocacao.h"

void testarConflito(const char *nome, Reserva a, Reserva b, bool esperado)
{
    bool resultado = reservasConflitam(a, b);

    if (resultado == esperado)
    {
        printf("[PASSOU] %s\n", nome);
    }
    else
    {
        printf("[FALHOU] %s | Esperado: %d | Obtido: %d\n",
               nome, esperado, resultado);
    }
}
  int main(void)
{
    Reserva a = {0};
    Reserva b = {0};

    a.idSala = 3;
    a.isRecorrente = true;
    a.horario.dia = TER;
    a.horario.comeco = 14 * 60;
    a.horario.fim = 16 * 60;

    b.idSala = 3;
    b.isRecorrente = false;
    b.horario.dia = TER;
    b.data.dia = 10;
    b.data.mes = 11;
    b.data.ano = 2026;
    b.horario.comeco = 15 * 60;
    b.horario.fim = 18 * 60;

    testarConflito("Recorrente + pontual: conflito", a, b, true);

b.data.dia = 11;

testarConflito("Recorrente + pontual: dias diferentes", a, b, false);

b.isRecorrente=true;
testarConflito("Recorrente + recorrente: dias iguais", a, b, true);

a.isRecorrente=false;
a.data.dia = 10;
a.data.mes = 11;
a.data.ano = 2026;
testarConflito("Recorrente + pontual: dias iguais", a, b, true);

a.idSala= 4;
testarConflito("Salas diferentes: salas diferentes", a, b, false);

a.idSala = 3;
a.isRecorrente = true;

b.isRecorrente = false;

b.data.dia = 10;
b.data.mes = 11;
b.data.ano = 2026;
b.horario.comeco = 16 * 60;
testarConflito("Recorrente + pontual: horarios consecutivos", a, b, false);

a.isRecorrente = false;
b.isRecorrente = false;

a.data.dia = 10;
a.data.mes = 11;
a.data.ano = 2026;

b.data.dia = 10;
b.data.mes = 11;
b.data.ano = 2026;

b.horario.comeco = 15 * 60;
b.horario.fim = 18 * 60;

testarConflito("Pontual + pontual: mesma data e conflito", a, b, true);

a.isRecorrente = false;
b.isRecorrente = false;

a.data.dia = 10;
a.data.mes = 11;
a.data.ano = 2026;

b.data.dia = 11;
b.data.mes = 11;
b.data.ano = 2026;

testarConflito("Pontuais: datas diferentes", a, b, false);


Reserva existentes[3] = {0};

existentes[0].idSala = 1;
existentes[0].isRecorrente = true;
existentes[0].horario.dia = SEG;
existentes[0].horario.comeco = 8 * 60;
existentes[0].horario.fim = 10 * 60;

existentes[1].idSala = 3;
existentes[1].isRecorrente = true;
existentes[1].horario.dia = TER;
existentes[1].horario.comeco = 14 * 60;
existentes[1].horario.fim = 16 * 60;

existentes[2].idSala = 3;
existentes[2].isRecorrente = true;
existentes[2].horario.dia = QUA;
existentes[2].horario.comeco = 10 * 60;
existentes[2].horario.fim = 12 * 60;

Reserva nova = {0};

nova.idSala = 3;
nova.isRecorrente = true;
nova.horario.dia = TER;
nova.horario.comeco = 15 * 60;
nova.horario.fim = 17 * 60;

if (existeConflito(nova, existentes, 3))
{
    printf("[PASSOU] Conflito encontrado no vetor\n");
}
else
{
    printf("[FALHOU] Conflito nao encontrado\n");
}

nova.horario.dia = QUI;

if (!existeConflito(nova, existentes, 3))
{
    printf("[PASSOU] Nenhum conflito encontrado\n");
}
else
{
    printf("[FALHOU] Conflito encontrado indevidamente\n");
}
if (!existeConflito(nova, existentes, 0))
{
    printf("[PASSOU] Vetor vazio\n");
}
else
{
    printf("[FALHOU] Vetor vazio\n");
}


    return 0;
}

