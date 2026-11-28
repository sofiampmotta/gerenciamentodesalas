#include "../include/historico.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CABECALHO "#POLI-SALAS;HISTORICO;v1"

// Copia o texto trocando ';' e quebras de linha, que estragariam o arquivo.
static void copiar_campo(char *destino, size_t tamanho, const char *origem) {
  size_t i = 0;
  if (origem == NULL)
    origem = "";
  for (; origem[i] != '\0' && i + 1 < tamanho; i++) {
    char c = origem[i];
    if (c == ';')
      c = ',';
    else if (c == '\n' || c == '\r')
      c = ' ';
    destino[i] = c;
  }
  destino[i] = '\0';
}

int historico_registrar(const char *caminho, const char *quem,
                        const char *acao, const char *detalhe) {
  if (caminho == NULL || acao == NULL)
    return 0;

  FILE *arquivo = fopen(caminho, "a");
  if (arquivo == NULL)
    return 0;

  // arquivo novo: escreve o cabecalho antes da primeira linha
  fseek(arquivo, 0, SEEK_END);
  if (ftell(arquivo) == 0)
    fprintf(arquivo, "%s\n", CABECALHO);

  RegistroHistorico r;
  time_t agora = time(NULL);
  struct tm *t = localtime(&agora);
  strftime(r.data, sizeof r.data, "%Y-%m-%d", t);
  strftime(r.hora, sizeof r.hora, "%H:%M", t);
  copiar_campo(r.quem, sizeof r.quem,
               quem != NULL && quem[0] != '\0' ? quem : "Não informado");
  copiar_campo(r.acao, sizeof r.acao, acao);
  copiar_campo(r.detalhe, sizeof r.detalhe, detalhe);

  fprintf(arquivo, "%s;%s;%s;%s;%s\n", r.data, r.hora, r.quem, r.acao,
          r.detalhe);
  int ok = !ferror(arquivo);
  if (fclose(arquivo) != 0)
    ok = 0;
  return ok;
}

// Separa a linha no proximo ';'. Devolve o comeco do campo.
static char *proximo_campo(char **cursor) {
  char *inicio = *cursor;
  char *fim = strchr(inicio, ';');
  if (fim != NULL) {
    *fim = '\0';
    *cursor = fim + 1;
  } else {
    *cursor = inicio + strlen(inicio);
  }
  return inicio;
}

int historico_carregar(const char *caminho, RegistroHistorico **registros,
                       size_t *quantidade) {
  if (caminho == NULL || registros == NULL || quantidade == NULL)
    return 0;
  *registros = NULL;
  *quantidade = 0;

  FILE *arquivo = fopen(caminho, "r");
  if (arquivo == NULL)
    return 1; // ainda nao tem historico

  RegistroHistorico *lista = NULL;
  size_t usados = 0, capacidade = 0;
  char linha[512];

  while (fgets(linha, sizeof linha, arquivo) != NULL) {
    linha[strcspn(linha, "\r\n")] = '\0';
    if (linha[0] == '\0' || linha[0] == '#')
      continue;

    if (usados == capacidade) {
      size_t nova = capacidade == 0 ? 64 : capacidade * 2;
      RegistroHistorico *maior = realloc(lista, nova * sizeof *maior);
      if (maior == NULL) {
        free(lista);
        fclose(arquivo);
        return 0;
      }
      lista = maior;
      capacidade = nova;
    }

    char *cursor = linha;
    RegistroHistorico *r = &lista[usados];
    copiar_campo(r->data, sizeof r->data, proximo_campo(&cursor));
    copiar_campo(r->hora, sizeof r->hora, proximo_campo(&cursor));
    copiar_campo(r->quem, sizeof r->quem, proximo_campo(&cursor));
    copiar_campo(r->acao, sizeof r->acao, proximo_campo(&cursor));
    // o detalhe e o resto da linha
    copiar_campo(r->detalhe, sizeof r->detalhe, cursor);
    usados++;
  }

  fclose(arquivo);
  *registros = lista;
  *quantidade = usados;
  return 1;
}

void historico_liberar(RegistroHistorico *registros) { free(registros); }
