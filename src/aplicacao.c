#include "../include/aplicacao.h"
#include "../include/persistencia.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void definirCaminho(char destino[512], const char *caminho,
                           const char *padrao) {
  snprintf(destino, 512, "%s", caminho ? caminho : padrao);
}

static Aplicacao aplicacaoPadrao;
static int aplicacaoPadraoIniciada = 0;

int aplicacao_iniciar(Aplicacao *app, const char *caminhoSalas,
                      const char *caminhoReservas) {
  if (app == NULL)
    return 0;

  memset(app, 0, sizeof *app);
  definirCaminho(app->caminhoSalas, caminhoSalas, PERS_ARQ_SALAS);
  definirCaminho(app->caminhoReservas, caminhoReservas, PERS_ARQ_RESERVAS);

  PersRelatorio relatorioSalas;
  PersStatus status = pers_carregar_salas(app->caminhoSalas, &app->salas,
                                          &app->quantidadeSalas,
                                          &relatorioSalas);
  if (status != PERS_OK) {
    aplicacao_finalizar(app);
    return 0;
  }

  if (caminhoSalas == NULL && relatorioSalas.arquivo_ausente) {
    Sala *salasExemplo = NULL;
    size_t quantidadeSalasExemplo = 0;
    status = pers_carregar_salas("dados_exemplo/salas.txt", &salasExemplo,
                                 &quantidadeSalasExemplo, NULL);
    if (status == PERS_OK) {
      app->salas = salasExemplo;
      app->quantidadeSalas = quantidadeSalasExemplo;
    }
  }

  PersRelatorio relatorioReservas;
  status = pers_carregar_reservas(app->caminhoReservas, &app->reservas,
                                  &app->quantidadeReservas,
                                  &relatorioReservas);
  if (status != PERS_OK) {
    aplicacao_finalizar(app);
    return 0;
  }

  if (caminhoReservas == NULL && relatorioReservas.arquivo_ausente) {
    Reserva *reservasExemplo = NULL;
    size_t quantidadeReservasExemplo = 0;
    status = pers_carregar_reservas("dados_exemplo/reservas.txt",
                                    &reservasExemplo,
                                    &quantidadeReservasExemplo, NULL);
    if (status == PERS_OK) {
      app->reservas = reservasExemplo;
      app->quantidadeReservas = quantidadeReservasExemplo;
    }
  }
  return 1;
}

Aplicacao *aplicacao_padrao(void) {
  if (!aplicacaoPadraoIniciada) {
    aplicacaoPadraoIniciada =
        aplicacao_iniciar(&aplicacaoPadrao, NULL, NULL);
  }
  return aplicacaoPadraoIniciada ? &aplicacaoPadrao : NULL;
}

void aplicacao_padrao_finalizar(void) {
  aplicacao_finalizar(&aplicacaoPadrao);
  aplicacaoPadraoIniciada = 0;
}

void aplicacao_finalizar(Aplicacao *app) {
  if (app == NULL)
    return;
  pers_liberar(app->salas);
  pers_liberar(app->reservas);
  memset(app, 0, sizeof *app);
}

Sala *aplicacao_buscar_sala(Aplicacao *app, int id) {
  if (app == NULL)
    return NULL;
  for (size_t i = 0; i < app->quantidadeSalas; i++) {
    if (app->salas[i].id == id)
      return &app->salas[i];
  }
  return NULL;
}

Reserva *aplicacao_buscar_reserva(Aplicacao *app, int id) {
  if (app == NULL)
    return NULL;
  for (size_t i = 0; i < app->quantidadeReservas; i++) {
    if (app->reservas[i].id == id)
      return &app->reservas[i];
  }
  return NULL;
}

int aplicacao_salvar_sala(Aplicacao *app, Sala sala) {
  if (app == NULL || sala.id <= 0 || sala.nome[0] == '\0' ||
      sala.capacidade < 0)
    return 0;
  if (aplicacao_buscar_sala(app, sala.id) != NULL)
    return 0;

  Sala *novas = realloc(app->salas, (app->quantidadeSalas + 1) * sizeof *novas);
  if (novas == NULL)
    return 0;
  app->salas = novas;
  app->salas[app->quantidadeSalas] = sala;

  if (pers_salvar_salas(app->caminhoSalas, app->salas,
                        app->quantidadeSalas + 1) != PERS_OK)
    return 0;

  app->quantidadeSalas++;
  return 1;
}

int aplicacao_editar_sala(Aplicacao *app, int id, Sala dadosNovos) {
  if (app == NULL || dadosNovos.capacidade < 0)
    return 0;
  Sala *sala = aplicacao_buscar_sala(app, id);
  if (sala == NULL)
    return 0;

  Sala antiga = *sala;
  dadosNovos.id = id;
  *sala = dadosNovos;
  if (pers_salvar_salas(app->caminhoSalas, app->salas, app->quantidadeSalas) !=
      PERS_OK) {
    *sala = antiga;
    return 0;
  }
  return 1;
}

int aplicacao_excluir_sala(Aplicacao *app, int id) {
  if (app == NULL)
    return 0;
  Sala *sala = aplicacao_buscar_sala(app, id);
  if (sala == NULL)
    return 0;

  size_t indice = (size_t)(sala - app->salas);
  Sala removida = *sala;
  Sala antiga = app->salas[app->quantidadeSalas - 1];
  app->salas[indice] = antiga;
  app->quantidadeSalas--;

  if (pers_salvar_salas(app->caminhoSalas, app->salas, app->quantidadeSalas) !=
      PERS_OK) {
    app->quantidadeSalas++;
    app->salas[indice] = removida;
    if (indice != app->quantidadeSalas - 1)
      app->salas[app->quantidadeSalas - 1] = antiga;
    return 0;
  }
  return 1;
}

int aplicacao_salvar_reserva(Aplicacao *app, Reserva reserva) {
  if (app == NULL)
    return 0;
  if (reserva.id <= 0) {
    int maior = 0;
    for (size_t i = 0; i < app->quantidadeReservas; i++) {
      if (app->reservas[i].id > maior)
        maior = app->reservas[i].id;
    }
    reserva.id = maior + 1;
  } else if (aplicacao_buscar_reserva(app, reserva.id) != NULL) {
    return 0;
  }

  Reserva *novas =
      realloc(app->reservas, (app->quantidadeReservas + 1) * sizeof *novas);
  if (novas == NULL)
    return 0;
  app->reservas = novas;
  app->reservas[app->quantidadeReservas] = reserva;

  if (pers_salvar_reservas(app->caminhoReservas, app->reservas,
                           app->quantidadeReservas + 1) != PERS_OK)
    return 0;

  app->quantidadeReservas++;
  return 1;
}

int aplicacao_editar_reserva(Aplicacao *app, int id, Reserva dadosNovos) {
  if (app == NULL)
    return 0;
  Reserva *reserva = aplicacao_buscar_reserva(app, id);
  if (reserva == NULL)
    return 0;

  Reserva antiga = *reserva;
  dadosNovos.id = id;
  *reserva = dadosNovos;
  if (pers_salvar_reservas(app->caminhoReservas, app->reservas,
                           app->quantidadeReservas) != PERS_OK) {
    *reserva = antiga;
    return 0;
  }
  return 1;
}

int aplicacao_excluir_reserva(Aplicacao *app, int id) {
  if (app == NULL)
    return 0;
  Reserva *reserva = aplicacao_buscar_reserva(app, id);
  if (reserva == NULL)
    return 0;

  size_t indice = (size_t)(reserva - app->reservas);
  Reserva removida = *reserva;
  Reserva antiga = app->reservas[app->quantidadeReservas - 1];
  app->reservas[indice] = antiga;
  app->quantidadeReservas--;

  if (pers_salvar_reservas(app->caminhoReservas, app->reservas,
                           app->quantidadeReservas) != PERS_OK) {
    app->quantidadeReservas++;
    app->reservas[indice] = removida;
    if (indice != app->quantidadeReservas - 1)
      app->reservas[app->quantidadeReservas - 1] = antiga;
    return 0;
  }
  return 1;
}
