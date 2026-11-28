// agenda.c - exporta as reservas no formato iCalendar (.ics).
//
// O formato e texto puro: cada evento fica entre BEGIN:VEVENT e END:VEVENT.
// Regras do formato que este arquivo segue:
//   - as linhas terminam com \r\n;
//   - linhas com mais de 75 bytes continuam na linha de baixo, que comeca
//     com um espaco;
//   - nos textos, \ ; , e quebra de linha precisam de uma \ antes.

#include "../include/agenda.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define LIMITE_LINHA 75

// Escreve uma linha, quebrando em pedacos de no maximo 75 bytes.
// Nao quebra no meio de uma letra acentuada (UTF-8).
static void escrever_linha(FILE *f, const char *linha) {
  size_t tamanho = strlen(linha);
  size_t inicio = 0;
  size_t limite = LIMITE_LINHA;

  while (tamanho - inicio > limite) {
    size_t fim = inicio + limite;
    // volta ate o comeco de uma letra (bytes 10xxxxxx continuam a anterior)
    while (fim > inicio && ((unsigned char)linha[fim] & 0xC0) == 0x80)
      fim--;
    fwrite(linha + inicio, 1, fim - inicio, f);
    fputs("\r\n ", f);
    inicio = fim;
    limite = LIMITE_LINHA - 1; // o espaco do comeco tambem conta
  }
  fputs(linha + inicio, f);
  fputs("\r\n", f);
}

// Copia o texto colocando \ antes dos caracteres especiais do formato.
static void escapar_texto(char *destino, size_t tamanho, const char *origem) {
  size_t j = 0;
  for (size_t i = 0; origem[i] != '\0' && j + 2 < tamanho; i++) {
    char c = origem[i];
    if (c == '\\' || c == ';' || c == ',') {
      destino[j++] = '\\';
      destino[j++] = c;
    } else if (c == '\n') {
      destino[j++] = '\\';
      destino[j++] = 'n';
    } else if (c != '\r') {
      destino[j++] = c;
    }
  }
  destino[j] = '\0';
}

static const char *nome_da_sala(const Sala salas[], size_t quantidadeSalas,
                                int idSala) {
  for (size_t i = 0; i < quantidadeSalas; i++) {
    if (salas[i].id == idSala)
      return salas[i].nome;
  }
  return "Sala não encontrada";
}

int exportarAgendaICS(const char *caminho, const Reserva reservas[],
                      size_t quantidade, const Sala salas[],
                      size_t quantidadeSalas) {
  if (caminho == NULL || (reservas == NULL && quantidade > 0))
    return 0;

  FILE *f = fopen(caminho, "wb");
  if (f == NULL)
    return 0;

  // Momento da exportacao, em UTC (campo obrigatorio DTSTAMP)
  char agora[32];
  time_t t = time(NULL);
  struct tm *utc = gmtime(&t);
  if (utc != NULL)
    strftime(agora, sizeof agora, "%Y%m%dT%H%M%SZ", utc);
  else
    strcpy(agora, "20260101T000000Z");

  escrever_linha(f, "BEGIN:VCALENDAR");
  escrever_linha(f, "VERSION:2.0");
  escrever_linha(f, "PRODID:-//POLI-UPE//Gerenciamento de Salas//PT");
  escrever_linha(f, "CALSCALE:GREGORIAN");
  escrever_linha(f, "X-WR-CALNAME:Salas POLI-UPE");

  char linha[512];
  char texto[256];

  for (size_t i = 0; i < quantidade; i++) {
    const Reserva *r = &reservas[i];
    const Data *d = &r->data;

    escrever_linha(f, "BEGIN:VEVENT");

    snprintf(linha, sizeof linha, "UID:reserva-%d@salas.poli.upe", r->id);
    escrever_linha(f, linha);

    snprintf(linha, sizeof linha, "DTSTAMP:%s", agora);
    escrever_linha(f, linha);

    // Horario local, sem fuso: o calendario usa o fuso de quem abrir
    snprintf(linha, sizeof linha, "DTSTART:%04d%02d%02dT%02d%02d00", d->ano,
             d->mes, d->dia, r->horario.comeco / 60, r->horario.comeco % 60);
    escrever_linha(f, linha);

    snprintf(linha, sizeof linha, "DTEND:%04d%02d%02dT%02d%02d00", d->ano,
             d->mes, d->dia, r->horario.fim / 60, r->horario.fim % 60);
    escrever_linha(f, linha);

    // Aula recorrente: repete toda semana ate o ultimo dia do periodo
    if (r->isRecorrente) {
      snprintf(linha, sizeof linha,
               "RRULE:FREQ=WEEKLY;UNTIL=%04d%02d%02dT235959", r->dataFim.ano,
               r->dataFim.mes, r->dataFim.dia);
      escrever_linha(f, linha);
    }

    escapar_texto(texto, sizeof texto, r->descricao);
    snprintf(linha, sizeof linha, "SUMMARY:%s", texto);
    escrever_linha(f, linha);

    escapar_texto(texto, sizeof texto,
                  nome_da_sala(salas, quantidadeSalas, r->idSala));
    snprintf(linha, sizeof linha, "LOCATION:%s", texto);
    escrever_linha(f, linha);

    escapar_texto(texto, sizeof texto, r->responsavel);
    snprintf(linha, sizeof linha, "DESCRIPTION:Responsável: %s", texto);
    escrever_linha(f, linha);

    escrever_linha(f, "END:VEVENT");
  }

  escrever_linha(f, "END:VCALENDAR");

  int ok = !ferror(f);
  if (fclose(f) != 0)
    ok = 0;
  return ok;
}
