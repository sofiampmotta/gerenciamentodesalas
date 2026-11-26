#include "../include/aplicacao.h"
#include "../include/salas.h"
#include <stdio.h>
#include <string.h>

static Aplicacao *obterAplicacao(void) { return aplicacao_padrao(); }

// retorna o indice da sala com esse id, ou -1 se nao achar
int buscarIndicePorId(int id) {
  Aplicacao *app = obterAplicacao();
  if (app == NULL)
    return -1;
  for (size_t i = 0; i < app->quantidadeSalas; i++) {
    if (app->salas[i].id == id)
      return (int)i;
  }
  return -1;
}

// retorna 1 (sucesso) ou 0 (falha, com motivo no printf)
int cadastrarSala(Sala nova) {
  Aplicacao *app = obterAplicacao();
  if (app == NULL)
    return 0;
  if (!aplicacao_salvar_sala(app, &nova)) {
    printf("Erro: nao foi possivel cadastrar a sala.\n");
    return 0;
  }
  return 1;
}

void listarSalas() {
  Aplicacao *app = obterAplicacao();
  if (app == NULL)
    return;
  if (app->quantidadeSalas == 0) {
    printf("Nenhuma sala cadastrada.\n");
    return;
  }

  printf("--- Lista de salas (%zu no total) ---\n", app->quantidadeSalas);

  for (size_t i = 0; i < app->quantidadeSalas; i++) {
    printf("ID: %d | Nome: %s | Capacidade: %d | Acessivel: %s | Projetor: %s "
           "| TV: %s | Computadores: %d\n",
           app->salas[i].id, app->salas[i].nome, app->salas[i].capacidade,
           app->salas[i].isAcessivel ? "sim" : "nao",
           app->salas[i].hasProjetor ? "sim" : "nao",
           app->salas[i].hasTelevisao ? "sim" : "nao",
           app->salas[i].computador);
  }
}

// retorna 1 (sucesso) ou 0 (nao achou o id)
int editarSala(int id, Sala dadosNovos) {
  Aplicacao *app = obterAplicacao();
  if (app == NULL)
    return 0;
  if (!aplicacao_editar_sala(app, id, dadosNovos)) {
    printf("Erro: nao foi possivel editar a sala.\n");
    return 0;
  }
  return 1;
}

// retorna 1 (sucesso) ou 0 (nao achou o id)
int excluirSala(int id) {
  Aplicacao *app = obterAplicacao();
  if (app == NULL)
    return 0;
  if (!aplicacao_excluir_sala(app, id)) {
    printf("Erro: nao foi possivel excluir a sala.\n");
    return 0;
  }
  return 1;
}