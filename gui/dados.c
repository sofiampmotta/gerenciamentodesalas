#include "dados.h"
#include "../include/aplicacao.h"
#include "../include/alocacao.h"
#include <string.h>

SalaVista *g_salas;
int g_n_salas = 0;
ReservaVista *g_reservas;
int g_n_reservas = 0;
static int dados_iniciados = 0;

static Data data_de_codigo(int dia) {
  Data data = {dia % 100, (dia / 100) % 100, dia / 10000};
  return data;
}

static int codigo_de_data(Data data) {
  return data.ano * 10000 + data.mes * 100 + data.dia;
}

void dados_iniciar(void) {
  if (dados_iniciados)
    return;

  Aplicacao *app = aplicacao_padrao();
  if (app == NULL)
    return;

  dados_iniciados = 1;
  g_n_salas = (int)app->quantidadeSalas;
  g_salas = g_new0(SalaVista, g_n_salas);
  for (int i = 0; i < g_n_salas; i++) {
    const Sala *origem = &app->salas[i];
    SalaVista *destino = &g_salas[i];
    destino->id = origem->id;
    g_strlcpy(destino->nome, origem->nome, sizeof destino->nome);
    g_strlcpy(destino->local,
              origem->local[0] ? origem->local : "Local não informado",
              sizeof destino->local);
    destino->lugares = origem->capacidade;
    destino->projetor = origem->hasProjetor;
    destino->acessivel = origem->isAcessivel;
  }

  static const char *const tipos[] = {"Aula", "Monitoria", "Extensão",
                                      "Evento", "Outro"};
  g_n_reservas = (int)app->quantidadeReservas;
  g_reservas = g_new0(ReservaVista, g_n_reservas);
  for (int i = 0; i < g_n_reservas; i++) {
    const Reserva *origem = &app->reservas[i];
    ReservaVista *destino = &g_reservas[i];
    memset(destino, 0, sizeof *destino);
    destino->idReserva = origem->id;
    g_strlcpy(destino->atividade, origem->descricao,
              sizeof destino->atividade);
    int indiceTipo = (int)origem->atividade;
    if (indiceTipo < 0 || indiceTipo > 4)
      indiceTipo = 4;
    g_strlcpy(destino->tipo, tipos[indiceTipo], sizeof destino->tipo);
    g_strlcpy(destino->responsavel, origem->responsavel,
              sizeof destino->responsavel);

    const Sala *sala = aplicacao_buscar_sala(app, origem->idSala);
    if (sala != NULL)
      g_strlcpy(destino->sala, sala->nome, sizeof destino->sala);

    destino->isRecorrente = origem->isRecorrente;
    destino->dia = codigo_de_data(origem->data);
    destino->fimRecorrencia = codigo_de_data(origem->dataFim);
    destino->ini = origem->horario.comeco;
    destino->fim = origem->horario.fim;
  }
}
void dados_recarregar(void)
{
    g_free(g_salas);
    g_free(g_reservas);

    g_salas = NULL;
    g_reservas = NULL;

    g_n_salas = 0;
    g_n_reservas = 0;

    dados_iniciados = 0;

    dados_iniciar();
}

int dados_data_hoje(void) {
  GDateTime *agora = g_date_time_new_now_local();
  int data = g_date_time_get_year(agora) * 10000 +
             g_date_time_get_month(agora) * 100 +
             g_date_time_get_day_of_month(agora);
  g_date_time_unref(agora);
  return data;
}

int dados_indice_sala(const char *nome) {
  for (int i = 0; i < g_n_salas; i++)
    if (strcmp(g_salas[i].nome, nome) == 0)
      return i;
  return -1;
}

int dados_data_valida(int dia) {
  return g_date_valid_dmy(dia % 100, (dia / 100) % 100, dia / 10000);
}

void dados_data_texto(int dia, char *buf, size_t tam) {
  g_snprintf(buf, tam, "%02d/%02d/%04d", dia % 100, (dia / 100) % 100,
             dia / 10000);
}

void dados_hora_texto(int min, char *buf, size_t tam) {
  g_snprintf(buf, tam, "%02d:%02d", min / 60, min % 60);
}

int dados_reserva_ocorre_em(const ReservaVista *r, int dia)
{
    if (!r || !dados_data_valida(dia))
        return 0;

    Aplicacao *app = aplicacao_padrao();

    if (!app)
        return 0;

    Reserva *reserva = aplicacao_buscar_reserva(
        app, r->idReserva);

    if (!reserva)
        return 0;

    return reservaOcorreEm(*reserva, data_de_codigo(dia));
}

static Atividade atividade_backend(const char *tipo) {
  if (strcmp(tipo, "Aula") == 0)
    return AULA;
  if (strcmp(tipo, "Monitoria") == 0)
    return MONITORIA;
  if (strcmp(tipo, "Extensão") == 0)
    return EXTENSAO;
  if (strcmp(tipo, "Evento") == 0)
    return EVENTO;
  return OUTRO;
}

static int id_sala_backend(Aplicacao *app, const char *nome) {
  if (app == NULL)
    return 0;
  for (size_t i = 0; i < app->quantidadeSalas; i++)
    if (strcmp(app->salas[i].nome, nome) == 0)
      return app->salas[i].id;
  return 0;
}

static void reserva_gui_backend(const ReservaVista *origem, Aplicacao *app,
                                const Reserva *anterior, Reserva *reserva) {
  memset(reserva, 0, sizeof *reserva);
  if (anterior != NULL)
    *reserva = *anterior;

  reserva->idSala = id_sala_backend(app, origem->sala);
  reserva->atividade = atividade_backend(origem->tipo);
  g_strlcpy(reserva->descricao, origem->atividade,
            sizeof reserva->descricao);
  g_strlcpy(reserva->responsavel, origem->responsavel,
            sizeof reserva->responsavel);
  reserva->horario.comeco = origem->ini;
  reserva->horario.fim = origem->fim;

  if (anterior == NULL || !anterior->isRecorrente) {
    reserva->isRecorrente = origem->isRecorrente;
    reserva->data = data_de_codigo(origem->dia);
    reserva->horario.dia = diaDaSemana(reserva->data);
    if (!reserva->isRecorrente)
      memset(&reserva->dataFim, 0, sizeof reserva->dataFim);
  }
}

static int textos_reserva_cabem_no_backend(const ReservaVista *r, char *msg,
                                           size_t tam) {
  if (strlen(r->atividade) >= sizeof(((Reserva *)0)->descricao)) {
    g_strlcpy(msg, "O nome da atividade excede o limite permitido.", tam);
    return 0;
  }
  if (strlen(r->responsavel) >= sizeof(((Reserva *)0)->responsavel)) {
    g_strlcpy(msg, "O nome do responsável excede o limite permitido.", tam);
    return 0;
  }
  if (strlen(r->sala) >= sizeof(((Sala *)0)->nome)) {
    g_strlcpy(msg, "O nome da sala excede o limite permitido.", tam);
    return 0;
  }
  return 1;
}

int dados_conflito(const char *sala, int dia, int ini, int fim,
                   int ignorar_id) {
  Aplicacao *app = aplicacao_padrao();
  if (app == NULL)
    return -1;
  ReservaVista teste = {0};
  g_strlcpy(teste.sala, sala, sizeof teste.sala);
  teste.dia = dia;
  teste.ini = ini;
  teste.fim = fim;
  Reserva proposta;
  reserva_gui_backend(&teste, app, NULL, &proposta);
  if (proposta.idSala == 0)
    return -1;
  return aplicacao_buscar_conflito_reserva(app, &proposta, ignorar_id, NULL);
}

static void titulo_motivo(VerificacaoReserva *v, const char *t, const char *m) {
  g_strlcpy(v->titulo, t, sizeof v->titulo);
  g_strlcpy(v->motivo, m, sizeof v->motivo);
}

void dados_verificar_reserva(const ReservaVista *r, VerificacaoReserva *v) {
  memset(v, 0, sizeof *v);
  v->sala_idx = dados_indice_sala(r->sala);
  if (v->sala_idx < 0) {
    titulo_motivo(v, "Dados incompletos", "Escolha uma sala.");
    return;
  }

  v->projetor = g_salas[v->sala_idx].projetor;

  if (!dados_data_valida(r->dia)) {
    titulo_motivo(v, "Dados incompletos",
                  "Essa data não existe no calendário.");
    return;
  }
  if (r->fim <= r->ini) {
    titulo_motivo(v, "Dados incompletos",
                  "O horário final precisa ser depois do inicial.");
    return;
  }

  Aplicacao *app = aplicacao_padrao();
  if (app == NULL) {
    titulo_motivo(v, "Erro ao verificar",
                  "Não foi possível carregar as reservas.");
    return;
  }

  const Reserva *anterior =
      r->idReserva > 0 ? aplicacao_buscar_reserva(app, r->idReserva) : NULL;
  if (r->idReserva > 0 && anterior == NULL) {
    titulo_motivo(v, "Reserva não encontrada",
                  "Recarregue os dados antes de editar.");
    return;
  }

  Reserva proposta;
  reserva_gui_backend(r, app, anterior, &proposta);
  if (proposta.idSala <= 0) {
    titulo_motivo(v, "Sala não encontrada",
                  "Atualize a lista de salas e tente novamente.");
    return;
  }
  Reserva conflito;
  int status = aplicacao_buscar_conflito_reserva(
      app, &proposta, r->idReserva, &conflito);
  if (status < 0) {
    titulo_motivo(v, "Erro ao verificar",
                  "Não foi possível validar o horário.");
    return;
  }
  if (status > 0) {
    char txt[200];
    v->conflito = 1;
    g_strlcpy(v->atividade_conflito, conflito.descricao,
              sizeof v->atividade_conflito);
    if (r->isRecorrente)
      g_snprintf(txt, sizeof txt,
                 "A série semanal conflita com %s durante o período. Nada foi "
                 "alterado.",
                 conflito.descricao);
    else
      g_snprintf(txt, sizeof txt,
                 "A %s já está ocupada nesse horário por %s. Nada foi "
                 "alterado.",
                 r->sala, conflito.descricao);
    titulo_motivo(v, "Conflito de horário", txt);
    return;
  }

  titulo_motivo(v, "Sem conflitos", "");
  v->ok = 1;
}

int dados_salvar_reserva(const ReservaVista *r, char *msg, size_t tam) {
  if (r->idReserva > 0)
    return dados_atualizar_reserva(r, msg, tam) == 0 ? 1 : -1;
  if (r->atividade[0] == '\0') {
    g_strlcpy(msg, "Informe o nome da atividade.", tam);
    return -1;
  }
  if (r->responsavel[0] == '\0') {
    g_strlcpy(msg, "Informe o responsável pela reserva.", tam);
    return -1;
  }
  if (!textos_reserva_cabem_no_backend(r, msg, tam))
    return -1;

  VerificacaoReserva v;
  dados_verificar_reserva(r, &v);
  if (!v.ok) {
    g_strlcpy(msg, v.motivo[0] ? v.motivo : v.titulo, tam);
    return -1;
  }

  Aplicacao *app = aplicacao_padrao();
  if (app == NULL) {
    g_strlcpy(msg, "Não foi possível carregar os dados para salvar a reserva.",
              tam);
    return -1;
  }

  Reserva nova;
  reserva_gui_backend(r, app, NULL, &nova);
  if (!aplicacao_salvar_reservas(app, &nova, 1)) {
    g_strlcpy(msg, "Não foi possível persistir a reserva.", tam);
    return -1;
  }

  g_reservas = g_realloc_n(g_reservas, g_n_reservas + 1, sizeof *g_reservas);
  ReservaVista *n = &g_reservas[g_n_reservas++];
  *n = *r;
  n->idReserva = nova.id;
  n->isRecorrente = nova.isRecorrente;
  n->dia = codigo_de_data(nova.data);
  n->fimRecorrencia = codigo_de_data(nova.dataFim);
  if (n->isRecorrente)
    g_snprintf(msg, tam, "Reserva semanal salva na %s até 31/12/%04d.",
               r->sala, nova.dataFim.ano);
  else
    g_snprintf(msg, tam, "Reserva salva na %s.", r->sala);
  return 1;
}

int dados_indice_reserva(int id) {
  for (int i = 0; i < g_n_reservas; i++)
    if (g_reservas[i].idReserva == id)
      return i;
  return -1;
}

int dados_atualizar_reserva(const ReservaVista *r, char *msg, size_t tam) {
  int i = dados_indice_reserva(r->idReserva);
  if (i < 0) {
    g_strlcpy(msg, "Reserva não encontrada.", tam);
    return -1;
  }
  if (r->atividade[0] == '\0') {
    g_strlcpy(msg, "Informe o nome da atividade.", tam);
    return -1;
  }
  if (r->responsavel[0] == '\0') {
    g_strlcpy(msg, "Informe o responsável pela reserva.", tam);
    return -1;
  }
  if (!textos_reserva_cabem_no_backend(r, msg, tam))
    return -1;

  VerificacaoReserva v;
  dados_verificar_reserva(r, &v);
  if (!v.ok) {
    g_strlcpy(msg, v.motivo[0] ? v.motivo : v.titulo, tam);
    return -1;
  }

  Aplicacao *app = aplicacao_padrao();
  Reserva *anterior = aplicacao_buscar_reserva(app, r->idReserva);
  if (anterior == NULL) {
    g_strlcpy(msg, "Reserva não encontrada nos dados persistentes.", tam);
    return -1;
  }
  Reserva atualizada;
  reserva_gui_backend(r, app, anterior, &atualizada);
  if (!aplicacao_editar_reserva(app, r->idReserva, atualizada)) {
    g_strlcpy(msg, "Não foi possível persistir as alterações da reserva.",
              tam);
    return -1;
  }

  g_reservas[i] = *r;
  g_reservas[i].isRecorrente = atualizada.isRecorrente;
  g_reservas[i].dia = codigo_de_data(atualizada.data);
  g_reservas[i].fimRecorrencia = codigo_de_data(atualizada.dataFim);
  g_snprintf(msg, tam, "Reserva atualizada: %s, sala %s.", r->atividade,
             r->sala);
  return 0;
}
