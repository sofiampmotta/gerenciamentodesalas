#include "../include/exportacao.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int verificacoes;
static int falhas;

#define CHECK(condicao)                                                        \
  do {                                                                         \
    verificacoes++;                                                            \
    if (!(condicao)) {                                                         \
      falhas++;                                                                \
      fprintf(stderr, "Falhou na linha %d: %s\n", __LINE__, #condicao);        \
    }                                                                          \
  } while (0)

static int ler_arquivo(const char *caminho, char *conteudo, size_t capacidade) {
  FILE *arquivo = fopen(caminho, "rb");
  if (!arquivo || capacidade == 0)
    return 0;
  size_t lidos = fread(conteudo, 1, capacidade - 1, arquivo);
  int ok = feof(arquivo) && !ferror(arquivo);
  fclose(arquivo);
  conteudo[lidos] = '\0';
  return ok;
}

int main(void) {
  ReservaExportacao reservas[] = {{
      .atividade = "Química; \"Orgânica\"",
      .tipo = "Aula",
      .responsavel = "Prof.ª Ana",
      .sala = "A-03",
      .inicio = 430,
      .fim = 530,
      .recorrente = 1,
  }};
  char csv[2048];
  char pdf[8];

  remove("exportacao_teste.csv");
  remove("exportacao_teste.pdf");
  CHECK(exportar_csv("exportacao_teste.csv", 20261126, "Aula", "A-03", reservas,
                     1));
  CHECK(ler_arquivo("exportacao_teste.csv", csv, sizeof csv));
  CHECK(strstr(csv, "Data exibida;Filtro de atividade;Filtro de sala") == csv);
  CHECK(strstr(csv, "26/11/2026;Aula;A-03;07:10;08:50") != NULL);
  CHECK(strstr(csv, "\"Química; \"\"Orgânica\"\"\"") != NULL);
  CHECK(strstr(csv, "07:10;08:50") != NULL);
  CHECK(strstr(csv, "2026") != NULL);

  CHECK(exportar_pdf("exportacao_teste.pdf", 20261126, "Aula", "A-03", reservas,
                     1));
  FILE *arquivo = fopen("exportacao_teste.pdf", "rb");
  CHECK(arquivo != NULL);
  if (arquivo) {
    size_t lidos = fread(pdf, 1, 7, arquivo);
    pdf[lidos] = '\0';
    CHECK(strcmp(pdf, "%PDF-1.") == 0);
    char cauda[65] = {0};
    CHECK(fseek(arquivo, 0, SEEK_END) == 0);
    long tamanho = ftell(arquivo);
    CHECK(tamanho >= 5);
    if (tamanho >= 5) {
      long inicio = tamanho > 64 ? tamanho - 64 : 0;
      CHECK(fseek(arquivo, inicio, SEEK_SET) == 0);
      size_t bytes = fread(cauda, 1, (size_t)(tamanho - inicio), arquivo);
      cauda[bytes] = '\0';
      CHECK(strstr(cauda, "%%EOF") != NULL);
    }
    fclose(arquivo);
  }

  CHECK(
      !exportar_csv("exportacao_invalida.csv", 20260230, NULL, NULL, NULL, 0));
  remove("exportacao_teste.csv");
  remove("exportacao_teste.pdf");
  remove("exportacao_invalida.csv");

  printf("%d verificações, %d falha(s)\n", verificacoes, falhas);
  return falhas == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
