#include "../include/aplicacao.h"
#include "../include/alocacao.h"
#include "../include/historico.h"
#include "../include/persistencia.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void definirCaminho(char destino[512], const char *caminho,
                           const char *padrao) {
  snprintf(destino, 512, "%s", caminho ? caminho : padrao);
}

// Monta o caminho de outro arquivo na mesma pasta de salas.csv
static void caminho_ao_lado(char destino[512], const char *caminhoSalas,
                            const char *nomeArquivo) {
  const char *barra = strrchr(caminhoSalas, '/');
  const char *contrabarra = strrchr(caminhoSalas, '\\');
  if (contrabarra != NULL && (barra == NULL || contrabarra > barra))
    barra = contrabarra;
  int pasta = barra != NULL ? (int)(barra - caminhoSalas + 1) : 0;
  snprintf(destino, 512, "%.*s%s", pasta, caminhoSalas, nomeArquivo);
}

static void tirar_quebra(char *texto) { texto[strcspn(texto, "\r\n")] = '\0'; }

// perfil.txt: primeira linha e o nome, segunda e o setor
static void carregar_perfil(Aplicacao *app) {
  snprintf(app->usuario, sizeof app->usuario, "Leidjane");
  snprintf(app->setor, sizeof app->setor, "DATP");

  FILE *arquivo = fopen(app->caminhoPerfil, "r");
  if (arquivo == NULL)
    return;
  char nome[64] = "", setor[32] = "";
  if (fgets(nome, sizeof nome, arquivo) != NULL) {
    tirar_quebra(nome);
    if (nome[0] != '\0')
      snprintf(app->usuario, sizeof app->usuario, "%s", nome);
  }
  if (fgets(setor, sizeof setor, arquivo) != NULL) {
    tirar_quebra(setor);
    snprintf(app->setor, sizeof app->setor, "%s", setor);
  }
  fclose(arquivo);
}

int aplicacao_definir_usuario(Aplicacao *app, const char *nome,
                              const char *setor) {
  if (app == NULL || nome == NULL || nome[0] == '\0')
    return 0;
  FILE *arquivo = fopen(app->caminhoPerfil, "w");
  if (arquivo == NULL)
    return 0;
  fprintf(arquivo, "%s\n%s\n", nome, setor != NULL ? setor : "");
  if (fclose(arquivo) != 0)
    return 0;
  snprintf(app->usuario, sizeof app->usuario, "%s", nome);
  snprintf(app->setor, sizeof app->setor, "%s", setor != NULL ? setor : "");
  tirar_quebra(app->usuario);
  tirar_quebra(app->setor);
  return 1;
}

/* ---- Textos do historico ---- */

static const char *DIAS[] = {"seg", "ter", "qua", "qui", "sex", "sáb", "dom"};

static void registrar(Aplicacao *app, const char *acao, const char *detalhe) {
  // se nada mudou na edicao, sobra "NOME: " no fim; tira o ": "
  char texto[200];
  snprintf(texto, sizeof texto, "%s", detalhe);
  size_t n = strlen(texto);
  while (n > 0 && (texto[n - 1] == ' ' || texto[n - 1] == ':'))
    texto[--n] = '\0';
  if (!historico_registrar(app->caminhoHistorico, app->usuario, acao, texto))
    fprintf(stderr, "Aviso: nao foi possivel gravar o historico em %s\n",
            app->caminhoHistorico);
}

// Junta mais um pedaco ao texto, separando com "; "
static void acrescentar(char *texto, size_t tamanho, const char *formato, ...) {
  size_t usado = strlen(texto);
  if (usado > 0 && texto[usado - 1] != ' ' && usado + 2 < tamanho) {
    strcat(texto, "; ");
    usado += 2;
  }
  va_list args;
  va_start(args, formato);
  vsnprintf(texto + usado, tamanho - usado, formato, args);
  va_end(args);
}

static const char *nome_sala(Aplicacao *app, int idSala) {
  Sala *sala = aplicacao_buscar_sala(app, idSala);
  return sala != NULL ? sala->nome : "?";
}

static void texto_horario(const Horario *h, char *texto, size_t tamanho) {
  int dia = h->dia >= SEG && h->dia <= DOM ? (int)h->dia : 0;
  snprintf(texto, tamanho, "%s %02d:%02d-%02d:%02d", DIAS[dia], h->comeco / 60,
           h->comeco % 60, h->fim / 60, h->fim % 60);
}

// "CALCULO 1 - TC, sala I-03, seg 07:30-09:10"
static void descrever_reserva(Aplicacao *app, const Reserva *r, char *texto,
                              size_t tamanho) {
  char horario[32];
  texto_horario(&r->horario, horario, sizeof horario);
  snprintf(texto, tamanho, "%s, sala %s, %s", r->descricao,
           nome_sala(app, r->idSala), horario);
}

static const char *sim_nao(bool valor) { return valor ? "sim" : "não"; }

static int caminho_legado_txt(const char *caminhoCsv, char caminhoTxt[512]) {
  size_t tamanho = strlen(caminhoCsv);
  if (tamanho < 4 || strcmp(caminhoCsv + tamanho - 4, ".csv") != 0 ||
      tamanho - 4 + sizeof ".txt" > 512)
    return 0;

  memcpy(caminhoTxt, caminhoCsv, tamanho - 4);
  memcpy(caminhoTxt + tamanho - 4, ".txt", sizeof ".txt");
  return 1;
}

static PersStatus carregar_salas_csv_ou_migrar_txt(const char *caminhoCsv,
                                                   Sala **salas,
                                                   size_t *quantidade,
                                                   PersRelatorio *relatorio) {
  PersStatus status =
      pers_carregar_salas(caminhoCsv, salas, quantidade, relatorio);
  if (status != PERS_OK || !relatorio->arquivo_ausente)
    return status;

  char caminhoTxt[512];
  if (!caminho_legado_txt(caminhoCsv, caminhoTxt))
    return PERS_OK;

  PersRelatorio legado;
  status = pers_carregar_salas(caminhoTxt, salas, quantidade, &legado);
  if (status != PERS_OK || legado.arquivo_ausente)
    return status;
  if (legado.linhas_invalidas > 0) {
    fprintf(stderr, "Migração cancelada: %s contém %zu linha(s) inválida(s).\n",
            caminhoTxt, legado.linhas_invalidas);
    return PERS_ERRO_DADO_INVALIDO;
  }

  status = pers_salvar_salas(caminhoCsv, *salas, *quantidade);
  if (status == PERS_OK)
    *relatorio = legado;
  return status;
}

static PersStatus
carregar_reservas_csv_ou_migrar_txt(const char *caminhoCsv, Reserva **reservas,
                                    size_t *quantidade,
                                    PersRelatorio *relatorio) {
  PersStatus status =
      pers_carregar_reservas(caminhoCsv, reservas, quantidade, relatorio);
  if (status != PERS_OK || !relatorio->arquivo_ausente)
    return status;

  char caminhoTxt[512];
  if (!caminho_legado_txt(caminhoCsv, caminhoTxt))
    return PERS_OK;

  PersRelatorio legado;
  status = pers_carregar_reservas(caminhoTxt, reservas, quantidade, &legado);
  if (status != PERS_OK || legado.arquivo_ausente)
    return status;
  if (legado.linhas_invalidas > 0) {
    fprintf(stderr, "Migração cancelada: %s contém %zu linha(s) inválida(s).\n",
            caminhoTxt, legado.linhas_invalidas);
    return PERS_ERRO_DADO_INVALIDO;
  }

  status = pers_salvar_reservas(caminhoCsv, *reservas, *quantidade);
  if (status == PERS_OK)
    *relatorio = legado;
  return status;
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
  caminho_ao_lado(app->caminhoHistorico, app->caminhoSalas, "historico.csv");
  caminho_ao_lado(app->caminhoPerfil, app->caminhoSalas, "perfil.txt");
  caminho_ao_lado(app->caminhoCancelamentos, app->caminhoSalas,
                  "cancelamentos.csv");
  carregar_perfil(app);

  PersRelatorio relatorioSalas;
  PersStatus status = carregar_salas_csv_ou_migrar_txt(
      app->caminhoSalas, &app->salas, &app->quantidadeSalas, &relatorioSalas);
  if (status != PERS_OK) {
    fprintf(stderr, "Erro ao carregar %s: %s\n", app->caminhoSalas,
            pers_mensagem(status));
    aplicacao_finalizar(app);
    return 0;
  }

  if (caminhoSalas == NULL && relatorioSalas.arquivo_ausente) {
    Sala *salasExemplo = NULL;
    size_t quantidadeSalasExemplo = 0;
    status = pers_carregar_salas("dados_exemplo/salas.csv", &salasExemplo,
                                 &quantidadeSalasExemplo, NULL);
    if (status == PERS_OK) {
      app->salas = salasExemplo;
      app->quantidadeSalas = quantidadeSalasExemplo;
    }
  }

  PersRelatorio relatorioReservas;
  status = carregar_reservas_csv_ou_migrar_txt(
      app->caminhoReservas, &app->reservas, &app->quantidadeReservas,
      &relatorioReservas);
  if (status != PERS_OK) {
    fprintf(stderr, "Erro ao carregar %s: %s\n", app->caminhoReservas,
            pers_mensagem(status));
    aplicacao_finalizar(app);
    return 0;
  }

  // se esse arquivo estiver com problema, o sistema abre sem os cancelamentos
  status = pers_carregar_cancelamentos(app->caminhoCancelamentos,
                                       &app->cancelamentos,
                                       &app->quantidadeCancelamentos, NULL);
  if (status != PERS_OK)
    fprintf(stderr, "Aviso: %s: %s\n", app->caminhoCancelamentos,
            pers_mensagem(status));

  if (caminhoReservas == NULL && relatorioReservas.arquivo_ausente) {
    Reserva *reservasExemplo = NULL;
    size_t quantidadeReservasExemplo = 0;
    status =
        pers_carregar_reservas("dados_exemplo/reservas.csv", &reservasExemplo,
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
    aplicacaoPadraoIniciada = aplicacao_iniciar(&aplicacaoPadrao, NULL, NULL);
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
  pers_liberar(app->cancelamentos);
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
    // reserva de um dia so, num dia em que a aula semanal foi cancelada
    if (!normalizada.isRecorrente && existente->isRecorrente &&
        aplicacao_dia_cancelado(app, existente->id, normalizada.data))
      continue;
    if (reservasConflitam(normalizada, *existente)) {
      if (conflitante != NULL)
        *conflitante = *existente;
      return 1;
    }
  }
  return 0;
}

// Compara nomes de sala ignorando maiusculas, hifen, ponto e espaco.
// Assim "A-03", "A03", "a 03" e "A.03" sao a mesma sala.
static int nomes_sala_iguais(const char *a, const char *b) {
  while (*a || *b) {
    while (*a == '-' || *a == '.' || *a == ' ')
      a++;
    while (*b == '-' || *b == '.' || *b == ' ')
      b++;
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
      return 0;
    if (*a == '\0')
      return 1;
    a++;
    b++;
  }
  return 1;
}

Sala *aplicacao_buscar_sala_por_nome(Aplicacao *app, const char *nome,
                                     int ignorarId) {
  if (app == NULL || nome == NULL)
    return NULL;
  for (size_t i = 0; i < app->quantidadeSalas; i++) {
    if (app->salas[i].id != ignorarId &&
        nomes_sala_iguais(app->salas[i].nome, nome))
      return &app->salas[i];
  }
  return NULL;
}

int aplicacao_salvar_sala(Aplicacao *app, Sala *dados) {
  if (app == NULL || dados == NULL || dados->id < 0 || dados->nome[0] == '\0' ||
      dados->capacidade < 0)
    return 0;
  if (aplicacao_buscar_sala_por_nome(app, dados->nome, 0) != NULL)
    return 0; // ja existe uma sala com esse nome

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

  if (pers_salvar_salas(app->caminhoSalas, novas, app->quantidadeSalas + 1) !=
      PERS_OK) {
    pers_liberar(novas);
    return 0;
  }

  pers_liberar(app->salas);
  app->salas = novas;
  app->quantidadeSalas++;
  *dados = sala;

  char detalhe[200];
  snprintf(detalhe, sizeof detalhe, "%s, %d lugares", sala.nome,
           sala.capacidade);
  registrar(app, "Criou sala", detalhe);
  return 1;
}

int aplicacao_editar_sala(Aplicacao *app, int id, Sala dadosNovos) {
  if (app == NULL || dadosNovos.capacidade < 0)
    return 0;
  Sala *sala = aplicacao_buscar_sala(app, id);
  if (sala == NULL)
    return 0;
  if (aplicacao_buscar_sala_por_nome(app, dadosNovos.nome, id) != NULL)
    return 0; // outra sala ja usa esse nome

  Sala antiga = *sala;
  dadosNovos.id = id;
  *sala = dadosNovos;
  if (pers_salvar_salas(app->caminhoSalas, app->salas, app->quantidadeSalas) !=
      PERS_OK) {
    *sala = antiga;
    return 0;
  }

  // anota so o que mudou: "I-03: lugares 30 → 40; projetor não → sim"
  char detalhe[200];
  snprintf(detalhe, sizeof detalhe, "%s: ", dadosNovos.nome);
  if (strcmp(antiga.nome, dadosNovos.nome) != 0)
    acrescentar(detalhe, sizeof detalhe, "nome %s → %s", antiga.nome,
                dadosNovos.nome);
  if (antiga.capacidade != dadosNovos.capacidade)
    acrescentar(detalhe, sizeof detalhe, "lugares %d → %d", antiga.capacidade,
                dadosNovos.capacidade);
  if (strcmp(antiga.local, dadosNovos.local) != 0)
    acrescentar(detalhe, sizeof detalhe, "local %s", dadosNovos.local);
  if (antiga.hasProjetor != dadosNovos.hasProjetor)
    acrescentar(detalhe, sizeof detalhe, "projetor %s → %s",
                sim_nao(antiga.hasProjetor), sim_nao(dadosNovos.hasProjetor));
  if (antiga.isAcessivel != dadosNovos.isAcessivel)
    acrescentar(detalhe, sizeof detalhe, "acessível %s → %s",
                sim_nao(antiga.isAcessivel), sim_nao(dadosNovos.isAcessivel));
  if (antiga.hasTelevisao != dadosNovos.hasTelevisao)
    acrescentar(detalhe, sizeof detalhe, "televisão %s → %s",
                sim_nao(antiga.hasTelevisao), sim_nao(dadosNovos.hasTelevisao));
  if (antiga.computador != dadosNovos.computador)
    acrescentar(detalhe, sizeof detalhe, "computadores %d → %d",
                antiga.computador, dadosNovos.computador);
  registrar(app, "Editou sala", detalhe);
  return 1;
}

size_t aplicacao_contar_reservas_da_sala(const Aplicacao *app, int idSala) {
  size_t total = 0;
  if (app == NULL)
    return 0;
  for (size_t i = 0; i < app->quantidadeReservas; i++)
    if (app->reservas[i].idSala == idSala)
      total++;
  return total;
}

int aplicacao_excluir_sala(Aplicacao *app, int id) {
  if (app == NULL)
    return 0;
  Sala *sala = aplicacao_buscar_sala(app, id);
  if (sala == NULL)
    return 0;
  // nao apaga sala que ainda tem reservas, senao elas ficam sem sala
  if (aplicacao_contar_reservas_da_sala(app, id) > 0)
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
  registrar(app, "Excluiu sala", removida.nome);
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
    memcpy(novas, app->reservas, app->quantidadeReservas * sizeof *novas);

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

  PersStatus status = pers_salvar_reservas(app->caminhoReservas, novas, total);

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

  // varias de uma vez so acontece na importacao do PDF
  char detalhe[200];
  if (quantidade == 1) {
    descrever_reserva(app, &reservas[0], detalhe, sizeof detalhe);
    registrar(app, "Criou reserva", detalhe);
  } else {
    snprintf(detalhe, sizeof detalhe, "%lu reservas adicionadas",
             (unsigned long)quantidade);
    registrar(app, "Importou reservas", detalhe);
  }
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

  // anota so o que mudou: "CALCULO 1 - TC: sala I-03 → A-02"
  char detalhe[200];
  snprintf(detalhe, sizeof detalhe, "%s: ", dadosNovos.descricao);
  if (strcmp(antiga.descricao, dadosNovos.descricao) != 0)
    acrescentar(detalhe, sizeof detalhe, "antes \"%s\"", antiga.descricao);
  if (antiga.idSala != dadosNovos.idSala) {
    char salaAntiga[20];
    snprintf(salaAntiga, sizeof salaAntiga, "%s", nome_sala(app, antiga.idSala));
    acrescentar(detalhe, sizeof detalhe, "sala %s → %s", salaAntiga,
                nome_sala(app, dadosNovos.idSala));
  }
  if (antiga.horario.dia != dadosNovos.horario.dia ||
      antiga.horario.comeco != dadosNovos.horario.comeco ||
      antiga.horario.fim != dadosNovos.horario.fim) {
    char de[32], para[32];
    texto_horario(&antiga.horario, de, sizeof de);
    texto_horario(&dadosNovos.horario, para, sizeof para);
    acrescentar(detalhe, sizeof detalhe, "horário %s → %s", de, para);
  }
  if (strcmp(antiga.responsavel, dadosNovos.responsavel) != 0)
    acrescentar(detalhe, sizeof detalhe, "responsável %s",
                dadosNovos.responsavel);
  if (antiga.data.dia != dadosNovos.data.dia ||
      antiga.data.mes != dadosNovos.data.mes ||
      antiga.data.ano != dadosNovos.data.ano ||
      antiga.dataFim.dia != dadosNovos.dataFim.dia ||
      antiga.dataFim.mes != dadosNovos.dataFim.mes ||
      antiga.dataFim.ano != dadosNovos.dataFim.ano)
    acrescentar(detalhe, sizeof detalhe, "período alterado");
  registrar(app, "Editou reserva", detalhe);
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
  char detalhe[200];
  descrever_reserva(app, &removida, detalhe, sizeof detalhe);
  registrar(app, "Excluiu reserva", detalhe);

  // os dias cancelados dessa reserva nao servem mais
  size_t mantidos = 0;
  for (size_t i = 0; i < app->quantidadeCancelamentos; i++)
    if (app->cancelamentos[i].idReserva != removida.id)
      app->cancelamentos[mantidos++] = app->cancelamentos[i];
  if (mantidos != app->quantidadeCancelamentos) {
    app->quantidadeCancelamentos = mantidos;
    pers_salvar_cancelamentos(app->caminhoCancelamentos, app->cancelamentos,
                              mantidos);
  }
  return 1;
}

/* ---- Cancelar um dia de uma reserva semanal ---- */

static int mesma_data(Data a, Data b) {
  return a.dia == b.dia && a.mes == b.mes && a.ano == b.ano;
}

static int indice_cancelamento(const Aplicacao *app, int idReserva, Data data) {
  for (size_t i = 0; i < app->quantidadeCancelamentos; i++)
    if (app->cancelamentos[i].idReserva == idReserva &&
        mesma_data(app->cancelamentos[i].data, data))
      return (int)i;
  return -1;
}

int aplicacao_dia_cancelado(const Aplicacao *app, int idReserva, Data data) {
  return app != NULL && indice_cancelamento(app, idReserva, data) >= 0;
}

int aplicacao_cancelar_dia(Aplicacao *app, int idReserva, Data data,
                           const char *motivo) {
  if (app == NULL)
    return 0;
  Reserva *reserva = aplicacao_buscar_reserva(app, idReserva);
  if (reserva == NULL || !reserva->isRecorrente ||
      !reservaOcorreEm(*reserva, data) ||
      aplicacao_dia_cancelado(app, idReserva, data))
    return 0;

  Cancelamento novo;
  memset(&novo, 0, sizeof novo);
  novo.idReserva = idReserva;
  novo.data = data;
  snprintf(novo.motivo, sizeof novo.motivo, "%s", motivo != NULL ? motivo : "");

  Cancelamento *maior = realloc(app->cancelamentos,
                                (app->quantidadeCancelamentos + 1) * sizeof *maior);
  if (maior == NULL)
    return 0;
  app->cancelamentos = maior;
  app->cancelamentos[app->quantidadeCancelamentos] = novo;

  if (pers_salvar_cancelamentos(app->caminhoCancelamentos, app->cancelamentos,
                                app->quantidadeCancelamentos + 1) != PERS_OK)
    return 0; // nao conta o novo, a lista fica como estava
  app->quantidadeCancelamentos++;

  char detalhe[200];
  snprintf(detalhe, sizeof detalhe, "%s, sala %s, %02d/%02d/%04d%s%s",
           reserva->descricao, nome_sala(app, reserva->idSala), data.dia,
           data.mes, data.ano, novo.motivo[0] ? " - " : "", novo.motivo);
  registrar(app, "Cancelou um dia", detalhe);
  return 1;
}

int aplicacao_desfazer_cancelamento(Aplicacao *app, int idReserva, Data data) {
  if (app == NULL)
    return 0;
  int i = indice_cancelamento(app, idReserva, data);
  if (i < 0)
    return 0;

  // se a sala ja foi usada por outra reserva nesse dia, nao da para voltar
  Reserva *reserva = aplicacao_buscar_reserva(app, idReserva);
  if (reserva != NULL) {
    Reserva naqueleDia = *reserva;
    naqueleDia.isRecorrente = false;
    naqueleDia.data = data;
    naqueleDia.dataFim = (Data){0, 0, 0};
    if (aplicacao_buscar_conflito_reserva(app, &naqueleDia, idReserva, NULL) ==
        1)
      return 0;
  }

  Cancelamento removido = app->cancelamentos[i];
  size_t n = app->quantidadeCancelamentos;
  memmove(&app->cancelamentos[i], &app->cancelamentos[i + 1],
          (n - (size_t)i - 1) * sizeof *app->cancelamentos);
  if (pers_salvar_cancelamentos(app->caminhoCancelamentos, app->cancelamentos,
                                n - 1) != PERS_OK) {
    // volta como estava
    memmove(&app->cancelamentos[i + 1], &app->cancelamentos[i],
            (n - (size_t)i - 1) * sizeof *app->cancelamentos);
    app->cancelamentos[i] = removido;
    return 0;
  }
  app->quantidadeCancelamentos--;

  char detalhe[200];
  snprintf(detalhe, sizeof detalhe, "%s, %02d/%02d/%04d volta a acontecer",
           reserva != NULL ? reserva->descricao : "Reserva", data.dia, data.mes,
           data.ano);
  registrar(app, "Desfez cancelamento", detalhe);
  return 1;
}
