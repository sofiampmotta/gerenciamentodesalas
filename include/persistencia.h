#ifndef PERSISTENCIA_H
#define PERSISTENCIA_H

/*
 * persistencia.h - Armazenamento e recuperacao dos dados em arquivos locais.
 *
 * Usa as estruturas Sala e Reserva definidas em salas.h. O modulo nao
 * depende da interface grafica: recebe e devolve vetores. Quem guarda os
 * dados em memoria (modulos de salas e de reservas) chama:
 *
 *   - pers_carregar_* uma vez na inicializacao;
 *   - pers_salvar_*   depois de cada alteracao (inclusao, edicao, remocao).
 *
 * Formato dos arquivos: ver docs/PERSISTENCIA.md.
 */

#include <stddef.h>
#include "salas.h"

/* Caminhos padrao (relativos a pasta onde o programa e executado). */
#define PERS_ARQ_SALAS    "salas.txt"
#define PERS_ARQ_RESERVAS "reservas.txt"

/* Tamanho maximo de um caminho de arquivo aceito pelo modulo. */
#define PERS_CAMINHO_MAX 512

typedef enum {
    PERS_OK = 0,
    PERS_ERRO_PARAMETRO,     /* ponteiro NULL, caminho longo demais etc.     */
    PERS_ERRO_ABRIR,         /* nao foi possivel abrir/criar o arquivo       */
    PERS_ERRO_LEITURA,       /* falha de E/S durante a leitura               */
    PERS_ERRO_ESCRITA,       /* falha de E/S durante a gravacao              */
    PERS_ERRO_FORMATO,       /* cabecalho ausente/invalido ou versao errada  */
    PERS_ERRO_MEMORIA,       /* malloc/realloc falhou                        */
    PERS_ERRO_DADO_INVALIDO, /* registro invalido ao salvar (nada e gravado) */
    PERS_ERRO_SEM_BACKUP     /* restauracao pedida, mas nao ha arquivo .bak  */
} PersStatus;

/* Informacoes sobre uma carga. Pode ser passado NULL se nao interessar. */
typedef struct {
    int    arquivo_ausente;         /* 1 se o arquivo nao existia (1a execucao)  */
    size_t registros_lidos;         /* registros validos carregados              */
    size_t linhas_invalidas;        /* linhas ignoradas por estarem invalidas    */
    size_t primeira_linha_invalida; /* numero da 1a linha invalida (0 = nenhuma) */
} PersRelatorio;

/* -------------------------------------------------------------------- Salas */

/*
 * Grava as n salas do vetor em 'caminho', substituindo o conteudo anterior.
 * Todos os registros sao validados ANTES de gravar; se algum for invalido
 * (nome vazio, texto com ';' ou quebra de linha, id ou nome repetido...)
 * retorna PERS_ERRO_DADO_INVALIDO e o arquivo existente nao e alterado.
 * n pode ser 0 (o vetor pode ser NULL nesse caso).
 */
PersStatus pers_salvar_salas(const char *caminho, const Sala *salas, size_t n);

/*
 * Le as salas de 'caminho'. Em caso de sucesso, *salas aponta para um vetor
 * alocado com malloc (NULL se n == 0) que deve ser liberado com
 * pers_liberar(). Em caso de erro, *salas = NULL e *n = 0.
 *
 * - Arquivo ausente ou vazio: PERS_OK com *n = 0 (rel->arquivo_ausente diz qual).
 * - Cabecalho invalido: PERS_ERRO_FORMATO (nada e carregado).
 * - Linhas invalidas: ignoradas e contadas em rel->linhas_invalidas;
 *   as validas sao carregadas normalmente.
 */
PersStatus pers_carregar_salas(const char *caminho, Sala **salas, size_t *n,
                               PersRelatorio *rel);

/* ----------------------------------------------------------------- Reservas */

PersStatus pers_salvar_reservas(const char *caminho, const Reserva *reservas, size_t n);

PersStatus pers_carregar_reservas(const char *caminho, Reserva **reservas, size_t *n,
                                  PersRelatorio *rel);

/* -------------------------------------------------------------- Utilitarios */

/* Libera vetores devolvidos pelas funcoes pers_carregar_*. Aceita NULL. */
void pers_liberar(void *vetor);

/*
 * Substitui 'caminho' pela copia de seguranca 'caminho.bak' (criada
 * automaticamente a cada gravacao). Util quando a carga retorna
 * PERS_ERRO_FORMATO, por exemplo.
 */
PersStatus pers_restaurar_backup(const char *caminho);

/* Mensagem legivel para exibir ao usuario. Nunca retorna NULL. */
const char *pers_mensagem(PersStatus status);

#endif /* PERSISTENCIA_H */
