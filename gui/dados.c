#include "dados.h"
#include "aplicacao.h"
#include <string.h>

SalaTeste g_salas[MAX_SALAS];
int g_n_salas = 0;
ReservaTeste g_reservas[MAX_RESERVAS];
int g_n_reservas = 0;
static int proximo_id = 1;
void dados_iniciar(void) {
  if (g_n_salas > 0 || g_n_reservas > 0)
    return;

  Aplicacao *app = aplicacao_padrao();
  if (app == NULL)
    return;

  size_t nSalas = app->quantidadeSalas;
  if (nSalas > MAX_SALAS)
    nSalas = MAX_SALAS;
  for (size_t i = 0; i < nSalas; i++) {
    const Sala *origem = &app->salas[i];
    SalaTeste *destino = &g_salas[g_n_salas++];
    g_strlcpy(destino->nome, origem->nome, sizeof destino->nome);
    g_strlcpy(destino->local, "Local não informado", sizeof destino->local);
    destino->lugares = origem->capacidade;
    destino->projetor = origem->hasProjetor;
    destino->acessivel = origem->isAcessivel;
  }

  static const char *const tipos[] = {"Aula", "Monitoria", "Extensão",
                                      "Evento", "Outro"};
  size_t nReservas = app->quantidadeReservas;
  if (nReservas > MAX_RESERVAS)
    nReservas = MAX_RESERVAS;
  for (size_t i = 0; i < nReservas; i++) {
    const Reserva *origem = &app->reservas[i];
    ReservaTeste *destino = &g_reservas[g_n_reservas++];
    memset(destino, 0, sizeof *destino);
    destino->idReserva = (int)i + 1;
    g_strlcpy(destino->atividade, origem->descricao,
              sizeof destino->atividade);
    int indiceTipo = (int)origem->atividade;
    if (indiceTipo < 0 || indiceTipo > 4)
      indiceTipo = 4;
    g_strlcpy(destino->tipo, tipos[indiceTipo], sizeof destino->tipo);
    g_strlcpy(destino->responsavel, origem->responsavel,
              sizeof destino->responsavel);
    g_strlcpy(destino->descricao, origem->descricao,
              sizeof destino->descricao);

    const Sala *sala = aplicacao_buscar_sala(app, origem->idSala);
    if (sala != NULL)
      g_strlcpy(destino->sala, sala->nome, sizeof destino->sala);

    destino->dia = origem->isRecorrente
                       ? DATA_PADRAO
                       : origem->data.ano * 10000 + origem->data.mes * 100 +
                             origem->data.dia;
    destino->ini = origem->horario.comeco;
    destino->fim = origem->horario.fim;
  }
  proximo_id = g_n_reservas + 1;
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

static int somar_dias(int dia, int n) {
  GDate *d = g_date_new_dmy(dia % 100, (dia / 100) % 100, dia / 10000);
  g_date_add_days(d, n);
  int r = g_date_get_year(d) * 10000 + g_date_get_month(d) * 100 +
          g_date_get_day(d);
  g_date_free(d);
  return r;
}

int dados_conflito(const char *sala, int dia, int ini, int fim,
                   int ignorar_id) {
  for (int i = 0; i < g_n_reservas; i++) {
    const ReservaTeste *o = &g_reservas[i];
    if (o->idReserva == ignorar_id)
      continue;
    if (o->dia != dia || strcmp(o->sala, sala) != 0)
      continue;
    if (ini < o->fim && o->ini < fim)
      return i; /* horários se sobrepõem */
  }
  return -1;
}

static void titulo_motivo(VerificacaoReserva *v, const char *t, const char *m) {
  g_strlcpy(v->titulo, t, sizeof v->titulo);
  g_strlcpy(v->motivo, m, sizeof v->motivo);
}

void dados_verificar_reserva(const ReservaTeste *r, int pessoas, int repetir,
                             VerificacaoReserva *v) {
  memset(v, 0, sizeof *v);
  v->conflito_idx = -1;
  v->sala_idx = dados_indice_sala(r->sala);
  if (v->sala_idx < 0) {
    titulo_motivo(v, "Dados incompletos", "Escolha uma sala.");
    return;
  }

  const SalaTeste *s = &g_salas[v->sala_idx];
  v->lugares = s->lugares;
  v->projetor = s->projetor;
  v->cabe = (pessoas <= s->lugares);
  v->faltam = v->cabe ? 0 : pessoas - s->lugares;

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

  int dia = r->dia;
  for (;;) {
    int c = dados_conflito(r->sala, dia, r->ini, r->fim, r->idReserva);
    if (c >= 0) {
      v->conflito_idx = c;
      v->dia_conflito = dia;
      break;
    }
    if (!repetir)
      break;
    dia = somar_dias(dia, 7);
    if (dia > FIM_SEMESTRE)
      break;
  }

  char txt[200];
  if (v->conflito_idx >= 0) {
    const ReservaTeste *o = &g_reservas[v->conflito_idx];
    int a = r->ini > o->ini ? r->ini : o->ini;
    int b = r->fim < o->fim ? r->fim : o->fim;
    char ha[8], hb[8], data[16];
    dados_hora_texto(a, ha, sizeof ha);
    dados_hora_texto(b, hb, sizeof hb);
    dados_data_texto(v->dia_conflito, data, sizeof data);
    if (v->dia_conflito == r->dia)
      g_snprintf(txt, sizeof txt,
                 "A %s está ocupada das %s às %s. %s já usa a sala nesse "
                 "período. Nada foi alterado.",
                 r->sala, ha, hb, o->atividade);
    else
      g_snprintf(
          txt, sizeof txt,
          "Em %s a %s está ocupada das %s às %s por %s. Nada foi alterado.",
          data, r->sala, ha, hb, o->atividade);
    titulo_motivo(v, "Conflito de horário", txt);
    return;
  }
  if (!v->cabe) {
    g_snprintf(txt, sizeof txt,
               "A %s tem %d lugares e a atividade prevê %d pessoas.", r->sala,
               s->lugares, pessoas);
    titulo_motivo(v, "Sala pequena demais", txt);
    return;
  }
  titulo_motivo(v, "Sem conflitos", "");
  v->ok = 1;
}

int dados_salvar_reserva(const ReservaTeste *r, int pessoas, int repetir,
                         char *msg, size_t tam) {
  if (r->idReserva > 0)
    return dados_atualizar_reserva(r, pessoas, msg, tam) == 0 ? 1 : -1;
  if (r->atividade[0] == '\0') {
    g_strlcpy(msg, "Informe o nome da atividade.", tam);
    return -1;
  }
  if (r->responsavel[0] == '\0') {
    g_strlcpy(msg, "Informe o responsável pela reserva.", tam);
    return -1;
  }

  VerificacaoReserva v;
  dados_verificar_reserva(r, pessoas, repetir, &v);
  if (!v.ok) {
    g_strlcpy(msg, v.motivo[0] ? v.motivo : v.titulo, tam);
    return -1;
  }

  int ocorrencias = 1;
  if (repetir)
    for (int d = somar_dias(r->dia, 7); d <= FIM_SEMESTRE; d = somar_dias(d, 7))
      ocorrencias++;
  if (g_n_reservas + ocorrencias > MAX_RESERVAS) {
    g_strlcpy(msg, "Limite de reservas atingido.", tam);
    return -1;
  }

  int dia = r->dia;
  for (int k = 0; k < ocorrencias; k++) {
    ReservaTeste *n = &g_reservas[g_n_reservas++];
    *n = *r;
    n->idReserva = proximo_id++;
    n->pessoas = pessoas;
    n->dia = dia;
    dia = somar_dias(dia, 7);
  }
  if (ocorrencias == 1)
    g_snprintf(msg, tam, "Reserva salva na %s.", r->sala);
  else
    g_snprintf(msg, tam,
               "%d reservas salvas na %s (toda semana até o fim do semestre).",
               ocorrencias, r->sala);
  return ocorrencias;
}

int dados_indice_reserva(int id) {
  for (int i = 0; i < g_n_reservas; i++)
    if (g_reservas[i].idReserva == id)
      return i;
  return -1;
}

int dados_atualizar_reserva(const ReservaTeste *r, int pessoas, char *msg,
                            size_t tam) {
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

  VerificacaoReserva v;
  dados_verificar_reserva(r, pessoas, 0, &v);
  if (!v.ok) {
    g_strlcpy(msg, v.motivo[0] ? v.motivo : v.titulo, tam);
    return -1;
  }

  g_reservas[i] = *r;
  g_reservas[i].pessoas = pessoas;
  g_snprintf(msg, tam, "Reserva atualizada: %s, sala %s.", r->atividade,
             r->sala);
  return 0;
}
