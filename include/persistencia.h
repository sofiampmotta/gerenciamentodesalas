#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

// persistencia.h - guarda as salas e reservas em arquivos e le de volta.
//
// Quem usa este modulo e o aplicacao.c:
//   - pers_carregar_* uma vez, quando o programa abre;
//   - pers_salvar_*   depois de cada alteracao (incluir, editar, excluir).
//
// Formato dos arquivos: ver a Wiki do repositorio (pagina Persistencia).

#include <stddef.h>
#include "salas.h"

// Nomes padrao dos arquivos (na pasta onde o programa e executado)
#define PERS_ARQ_SALAS    "salas.csv"
#define PERS_ARQ_RESERVAS "reservas.csv"

// Tamanho maximo do caminho de um arquivo
#define PERS_CAMINHO_MAX 512

typedef enum {
    PERS_OK = 0,
    PERS_ERRO_PARAMETRO,     // ponteiro NULL, caminho longo demais etc.
    PERS_ERRO_ABRIR,         // nao conseguiu abrir ou criar o arquivo
    PERS_ERRO_LEITURA,       // erro durante a leitura
    PERS_ERRO_ESCRITA,       // erro durante a gravacao
    PERS_ERRO_FORMATO,       // cabecalho errado ou versao errada
    PERS_ERRO_MEMORIA,       // malloc/realloc falhou
    PERS_ERRO_DADO_INVALIDO, // registro invalido ao salvar (nada e gravado)
    PERS_ERRO_SEM_BACKUP     // pediu para restaurar, mas nao ha arquivo .bak
} PersStatus;

// Resumo de uma carga. Pode passar NULL se nao precisar.
typedef struct {
    int    arquivo_ausente;         // 1 se o arquivo nao existia
    size_t registros_lidos;         // quantos registros bons foram carregados
    size_t linhas_invalidas;        // quantas linhas foram puladas
    size_t primeira_linha_invalida; // numero da 1a linha pulada (0 = nenhuma)
} PersRelatorio;

// ---- Salas ----

// Grava as n salas em 'caminho', substituindo o conteudo anterior.
// Confere todas antes de gravar. Se alguma estiver errada (nome vazio,
// texto com ';' ou quebra de linha, id ou nome repetido...), retorna
// PERS_ERRO_DADO_INVALIDO e o arquivo nao e alterado.
// n pode ser 0 (e o vetor pode ser NULL nesse caso).
PersStatus pers_salvar_salas(const char *caminho, const Sala *salas, size_t n);

// Le as salas de 'caminho'. Se der certo, *salas aponta para um vetor
// criado com malloc (NULL se n == 0), que deve ser liberado com
// pers_liberar(). Se der erro, *salas = NULL e *n = 0.
//
// - Arquivo ausente ou vazio: PERS_OK com *n = 0.
// - Cabecalho errado: PERS_ERRO_FORMATO (nada e carregado).
// - Linha errada: so ela e pulada (e contada no relatorio).
PersStatus pers_carregar_salas(const char *caminho, Sala **salas, size_t *n,
                               PersRelatorio *rel);

// ---- Reservas ----
// Funcionam igual as de salas. O id de cada reserva tambem e gravado
// e nao pode repetir.

PersStatus pers_salvar_reservas(const char *caminho, const Reserva *reservas, size_t n);

PersStatus pers_carregar_reservas(const char *caminho, Reserva **reservas, size_t *n,
                                  PersRelatorio *rel);

// ---- Dias cancelados ----
// Uma linha por dia cancelado: id_reserva;data;motivo
// Nao pode repetir a mesma reserva no mesmo dia.

PersStatus pers_salvar_cancelamentos(const char *caminho, const Cancelamento *v,
                                     size_t n);

PersStatus pers_carregar_cancelamentos(const char *caminho, Cancelamento **v,
                                       size_t *n, PersRelatorio *rel);

// ---- Outras ----

// Libera o vetor devolvido pelas funcoes pers_carregar_*. Aceita NULL.
void pers_liberar(void *vetor);

// Volta o arquivo para a copia de seguranca 'caminho.bak'
// (criada a cada gravacao). Util se o arquivo estragar.
PersStatus pers_restaurar_backup(const char *caminho);

// Frase pronta para mostrar ao usuario. Nunca retorna NULL.
const char *pers_mensagem(PersStatus status);

#endif