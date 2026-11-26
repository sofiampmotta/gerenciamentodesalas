#include "../include/aplicacao.h"
#include "../include/alocacao.h"
#include "../include/persistencia.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void definirCaminho(char destino[512], const char *caminho,
                           const char *padrao) {
  snprintf(destino, 512, "%s", caminho ? caminho : padrao);
}

static Aplicacao aplicacaoPadrao;
static int aplicacaoPadraoIniciada = 0;

static void normalizar_recorrencia(Reserva *reserva) {
  if (reserva->isRecorrente && reserva->dataFim.dia == 0 &&
      reserva->dataFim.mes == 0 && reserva->dataFim.ano == 0)
    reserva->dataFim = (Data){31, 12, reserva->data.ano};
}

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

int aplicacao_buscar_conflito_reserva(const Aplicacao *app,
                                      const Reserva *proposta, int ignorarId,
                                      Reserva *conflitante) {
  if (app == NULL || proposta == NULL)
    return -1;

  Reserva normalizada = *proposta;
  normalizar_recorrencia(&normalizada);
  for (size_t i = 0; i < app->quantidadeReservas; i++) {
    const Reserva *existente = &app->reservas[i];
    if (existente->id == ignorarId)
      continue;
    if (reservasConflitam(normalizada, *existente)) {
      if (conflitante != NULL)
        *conflitante = *existente;
      return 1;
    }
  }
  return 0;
}

int aplicacao_salvar_sala(Aplicacao *app, Sala *dados) {
  if (app == NULL || dados == NULL || dados->id < 0 ||
      dados->nome[0] == '\0' || dados->capacidade < 0)
    return 0;

  if (app->quantidadeSalas >= (size_t)-1 / sizeof *app->salas)
    return 0;
  Sala sala = *dados;
  if (sala.id == 0) {
    int maiorId = 0;
    for (size_t i = 0; i < app->quantidadeSalas; i++)
      if (app->salas[i].id > maiorId)
        maiorId = app->salas[i].id;
    if (maiorId == INT_MAX)
      return 0;
    sala.id = maiorId + 1;
  } else if (aplicacao_buscar_sala(app, sala.id) != NULL) {
    return 0;
  }

  Sala *novas = malloc((app->quantidadeSalas + 1) * sizeof *novas);
  if (novas == NULL)
    return 0;
  if (app->quantidadeSalas > 0)
    memcpy(novas, app->salas, app->quantidadeSalas * sizeof *novas);
  novas[app->quantidadeSalas] = sala;

  if (pers_salvar_salas(app->caminhoSalas, novas,
                        app->quantidadeSalas + 1) != PERS_OK) {
    pers_liberar(novas);
    return 0;
  }

  pers_liberar(app->salas);
  app->salas = novas;
  app->quantidadeSalas++;
  *dados = sala;
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
  return aplicacao_salvar_reservas(app, &reserva, 1);
}

int aplicacao_salvar_reservas(Aplicacao *app, Reserva reservas[],
                              size_t quantidade) {
  if (app == NULL || reservas == NULL || quantidade == 0)
    return 0;

  size_t maximo = (size_t)-1 / sizeof *app->reservas;
  if (app->quantidadeReservas > maximo ||
      quantidade > maximo - app->quantidadeReservas)
    return 0;

  size_t total = app->quantidadeReservas + quantidade;
  Reserva *novas = malloc(total * sizeof *novas);
  if (novas == NULL)
    return 0;
  if (app->quantidadeReservas > 0)
    memcpy(novas, app->reservas,
           app->quantidadeReservas * sizeof *novas);

  int maior = 0;
  for (size_t i = 0; i < app->quantidadeReservas; i++)
    if (app->reservas[i].id > maior)
      maior = app->reservas[i].id;

  for (size_t i = 0; i < quantidade; i++) {
    Reserva nova = reservas[i];
    normalizar_recorrencia(&nova);
    if (nova.id <= 0) {
      if (maior == INT_MAX) {
        pers_liberar(novas);
        return 0;
      }
      nova.id = ++maior;
    } else {
      if (aplicacao_buscar_reserva(app, nova.id) != NULL) {
        pers_liberar(novas);
        return 0;
      }
      for (size_t anterior = 0; anterior < i; anterior++) {
        if (novas[app->quantidadeReservas + anterior].id == nova.id) {
          pers_liberar(novas);
          return 0;
        }
      }
      if (nova.id > maior)
        maior = nova.id;
    }
    novas[app->quantidadeReservas + i] = nova;
  }

 PersStatus status = pers_salvar_reservas(
    app->caminhoReservas, novas, total);

if (status != PERS_OK) {
    fprintf(stderr, "Erro na persistencia: %d\n", (int)status);
    pers_liberar(novas);
    return 0;
}

  for (size_t i = 0; i < quantidade; i++)
    reservas[i] = novas[app->quantidadeReservas + i];
  pers_liberar(app->reservas);
  app->reservas = novas;
  app->quantidadeReservas = total;
  return 1;
}

int aplicacao_editar_reserva(Aplicacao *app, int id, Reserva dadosNovos) {
  if (app == NULL)
    return 0;
  Reserva *reserva = aplicacao_buscar_reserva(app, id);
  if (reserva == NULL)
    return 0;

  Reserva antiga = *reserva;
  normalizar_recorrencia(&dadosNovos);
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
