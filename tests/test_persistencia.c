/*
 * test_persistencia.c - Testes do modulo de persistencia.
 * Compilar e rodar:  make test   (roda dentro da pasta build/)
 */

#include "persistencia.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define CRIAR_PASTA(p)   _mkdir(p)
#define REMOVER_PASTA(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define CRIAR_PASTA(p)   mkdir(p, 0700)
#define REMOVER_PASTA(p) rmdir(p)
#endif

/* Pasta com os dados reais da planilha 2026.2 (relativa a build/). */
#ifndef DADOS_DIR
#define DADOS_DIR "../dados_exemplo/"
#endif

static int total = 0, falhas = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        total++;                                                             \
        if (!(cond)) {                                                       \
            falhas++;                                                        \
            fprintf(stderr, "    FALHOU (linha %d): %s\n", __LINE__, #cond); \
        }                                                                    \
    } while (0)

#define ARQ_SAL "teste_salas.txt"
#define ARQ_RES "teste_reservas.txt"

/* ---------------------------------------------------------------- helpers */

static void limpar(const char *caminho)
{
    char p[PERS_CAMINHO_MAX];
    remove(caminho);
    snprintf(p, sizeof p, "%s.tmp", caminho); remove(p);
    snprintf(p, sizeof p, "%s.bak", caminho); remove(p);
}

static void escrever_texto(const char *caminho, const char *conteudo)
{
    FILE *f = fopen(caminho, "wb");
    if (f) { fputs(conteudo, f); fclose(f); }
}

static int existe(const char *caminho)
{
    FILE *f = fopen(caminho, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static Sala sala(int id, const char *nome)
{
    Sala s;
    memset(&s, 0, sizeof s);
    s.id = id;
    strcpy(s.nome, nome);
    s.capacidade = 40;
    s.isAcessivel = true;
    s.hasProjetor = true;
    s.hasTelevisao = false;
    s.computador = 0;
    return s;
}

/* Aula semanal, como as da planilha de horarios. */
static Reserva aula(int id_sala, Dia dia, int comeco, int fim)
{
    Reserva r;
    memset(&r, 0, sizeof r);
    r.idSala = id_sala;
    r.atividade = AULA;
    strcpy(r.descricao, "DEB CÁLCULO 1 (TC)");
    strcpy(r.responsavel, "LENIN ALEXANDRE DE ALMEIDA BEZERRA");
    r.isRecorrente = true;
    r.horario.dia = dia;
    r.horario.comeco = comeco;
    r.horario.fim = fim;
    return r;
}

/* Reserva unica, numa data (prova, evento...). */
static Reserva evento(int id_sala, int dia, int mes, int ano, Dia dsem, int comeco, int fim)
{
    Reserva r = aula(id_sala, dsem, comeco, fim);
    r.atividade = EVENTO;
    r.isRecorrente = false;
    r.data.dia = dia;
    r.data.mes = mes;
    r.data.ano = ano;
    strcpy(r.descricao, "Semana de Engenharia");
    strcpy(r.responsavel, "Coordenação");
    return r;
}

static int salas_iguais(const Sala *a, const Sala *b)
{
    return a->id == b->id && strcmp(a->nome, b->nome) == 0 &&
           a->capacidade == b->capacidade && a->isAcessivel == b->isAcessivel &&
           a->hasProjetor == b->hasProjetor && a->hasTelevisao == b->hasTelevisao &&
           a->computador == b->computador;
}

static int reservas_iguais(const Reserva *a, const Reserva *b)
{
    return a->idSala == b->idSala && a->atividade == b->atividade &&
           strcmp(a->descricao, b->descricao) == 0 &&
           strcmp(a->responsavel, b->responsavel) == 0 &&
           a->isRecorrente == b->isRecorrente &&
           a->data.dia == b->data.dia && a->data.mes == b->data.mes &&
           a->data.ano == b->data.ano &&
           a->horario.dia == b->horario.dia &&
           a->horario.comeco == b->horario.comeco && a->horario.fim == b->horario.fim;
}

#define H(h, m) ((h) * 60 + (m))

/* ------------------------------------------------------------------ testes */

static void test_salas_ida_e_volta(void)
{
    Sala v[3], *lidos = NULL;
    size_t n = 0, i;
    PersRelatorio rel;
    int ok = 1;

    v[0] = sala(1, "I-03");
    v[1] = sala(2, "LAB. MECT");                /* espaco e ponto no nome  */
    v[1].computador = 25;
    v[1].hasTelevisao = true;
    v[2] = sala(3, "Auditório");                /* acento (UTF-8)          */
    v[2].capacidade = 0;
    v[2].isAcessivel = false;

    limpar(ARQ_SAL);
    CHECK(pers_salvar_salas(ARQ_SAL, v, 3) == PERS_OK);
    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, &rel) == PERS_OK);
    CHECK(n == 3);
    CHECK(rel.registros_lidos == 3 && rel.linhas_invalidas == 0 && !rel.arquivo_ausente);
    for (i = 0; lidos && i < n && i < 3; i++) ok = ok && salas_iguais(&lidos[i], &v[i]);
    CHECK(lidos != NULL && ok);
    pers_liberar(lidos);
    limpar(ARQ_SAL);
}

static void test_reservas_ida_e_volta(void)
{
    Reserva v[4], *lidos = NULL;
    size_t n = 0, i;
    int ok = 1;

    v[0] = aula(1, SEG, H(7, 10), H(8, 50));
    v[1] = aula(2, SAB, H(8, 50), H(12, 10));
    strcpy(v[1].responsavel, "STELA FUCALE / ANTONIO CARLOS COSTA");
    v[2] = evento(1, 29, 2, 2028, TER, H(19, 0), H(22, 0));     /* ano bissexto */
    v[3] = aula(3, DOM, H(0, 0), H(23, 59));                    /* extremos     */
    v[3].atividade = MONITORIA;
    v[3].data.dia = 3; v[3].data.mes = 8; v[3].data.ano = 2026; /* recorrente c/ data de inicio */

    limpar(ARQ_RES);
    CHECK(pers_salvar_reservas(ARQ_RES, v, 4) == PERS_OK);
    CHECK(pers_carregar_reservas(ARQ_RES, &lidos, &n, NULL) == PERS_OK);
    CHECK(n == 4);
    for (i = 0; lidos && i < n && i < 4; i++) ok = ok && reservas_iguais(&lidos[i], &v[i]);
    CHECK(lidos != NULL && ok);
    pers_liberar(lidos);
    limpar(ARQ_RES);
}

/* Simula fechar e reabrir o programa varias vezes, com alteracoes no meio. */
static void test_preserva_entre_execucoes(void)
{
    Sala v[2], *lidos = NULL;
    size_t n = 0;

    limpar(ARQ_SAL);
    v[0] = sala(1, "I-03");
    CHECK(pers_salvar_salas(ARQ_SAL, v, 1) == PERS_OK);

    /* "reabre": carrega, adiciona uma, salva */
    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, NULL) == PERS_OK && n == 1);
    if (lidos) v[0] = lidos[0];
    v[1] = sala(2, "I-06");
    pers_liberar(lidos);
    CHECK(pers_salvar_salas(ARQ_SAL, v, 2) == PERS_OK);

    /* "reabre" de novo */
    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, NULL) == PERS_OK);
    CHECK(n == 2 && strcmp(lidos[1].nome, "I-06") == 0);
    pers_liberar(lidos);

    /* remove todas e confirma que a lista vazia tambem persiste */
    CHECK(pers_salvar_salas(ARQ_SAL, NULL, 0) == PERS_OK);
    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, NULL) == PERS_OK);
    CHECK(n == 0 && lidos == NULL);
    limpar(ARQ_SAL);
}

static void test_arquivo_ausente(void)
{
    Sala *s = (Sala *)1;          /* lixo: a funcao deve zerar */
    Reserva *r = (Reserva *)1;
    size_t n = 99;
    PersRelatorio rel;

    limpar(ARQ_SAL);
    limpar(ARQ_RES);
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, &rel) == PERS_OK);
    CHECK(s == NULL && n == 0 && rel.arquivo_ausente == 1);
    n = 99;
    CHECK(pers_carregar_reservas(ARQ_RES, &r, &n, &rel) == PERS_OK);
    CHECK(r == NULL && n == 0 && rel.arquivo_ausente == 1);
}

static void test_arquivo_vazio(void)
{
    Sala *s = NULL;
    size_t n = 99;
    PersRelatorio rel;

    escrever_texto(ARQ_SAL, "");
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, &rel) == PERS_OK);
    CHECK(s == NULL && n == 0 && rel.arquivo_ausente == 0);

    escrever_texto(ARQ_SAL, "\n\n\r\n");
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, &rel) == PERS_OK && n == 0);

    escrever_texto(ARQ_SAL, "#POLI-SALAS;SALAS;v1\n");       /* so cabecalho */
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, &rel) == PERS_OK && n == 0);
    limpar(ARQ_SAL);
}

static void test_cabecalho_invalido(void)
{
    Sala *s = NULL;
    Reserva *r = NULL;
    size_t n = 0;

    escrever_texto(ARQ_SAL, "1;I-03;40;1;1;0;0\n");           /* sem cabecalho */
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, NULL) == PERS_ERRO_FORMATO);
    CHECK(s == NULL && n == 0);

    escrever_texto(ARQ_SAL, "#POLI-SALAS;SALAS;v9\n");        /* versao errada */
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, NULL) == PERS_ERRO_FORMATO);

    escrever_texto(ARQ_RES, "#POLI-SALAS;SALAS;v1\n");        /* arquivo trocado */
    CHECK(pers_carregar_reservas(ARQ_RES, &r, &n, NULL) == PERS_ERRO_FORMATO);

    escrever_texto(ARQ_SAL, "\x01\x02\xff\xfe lixo binario\n");
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, NULL) == PERS_ERRO_FORMATO);
    limpar(ARQ_SAL);
    limpar(ARQ_RES);
}

static void test_linhas_invalidas_salas(void)
{
    Sala *s = NULL;
    size_t n = 0;
    PersRelatorio rel;
    static char longa[1300], conteudo[4096];

    memset(longa, 'x', sizeof longa - 2);
    longa[sizeof longa - 2] = '\n';
    longa[sizeof longa - 1] = '\0';

    snprintf(conteudo, sizeof conteudo,
        "#POLI-SALAS;SALAS;v1\n"
        "1;I-03;40;1;1;0;0\n"                    /* 2: ok                       */
        "2;I-06;40;1;1;0\n"                      /* 3: falta campo              */
        "abc;I-07;40;1;1;0;0\n"                  /* 4: id nao numerico          */
        "4;I-08;-5;1;1;0;0\n"                    /* 5: capacidade negativa      */
        "5;I-09;40;2;1;0;0\n"                    /* 6: booleano invalido        */
        "1;I-10;40;1;1;0;0\n"                    /* 7: id repetido              */
        "7;i-03;40;1;1;0;0\n"                    /* 8: nome repetido (caixa)    */
        "8;;40;1;1;0;0\n"                        /* 9: nome vazio               */
        "9;NOME-COM-MAIS-DE-19-BYTES;40;1;1;0;0\n" /* 10: nome grande demais    */
        "10;I-12;40;1;1;0;0;extra\n"             /* 11: campo a mais            */
        "%s"                                     /* 12: linha gigante           */
        "# comentario ignorado\n"
        "\n"
        "13;LIP-03;30;0;0;1;25\n",               /* 15: ok                      */
        longa);
    escrever_texto(ARQ_SAL, conteudo);

    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, &rel) == PERS_OK);
    CHECK(n == 2);
    CHECK(rel.linhas_invalidas == 10);
    CHECK(rel.primeira_linha_invalida == 3);
    CHECK(s != NULL && s[0].id == 1 && s[1].id == 13 && s[1].computador == 25 && s[1].hasTelevisao);
    pers_liberar(s);
    limpar(ARQ_SAL);
}

static void test_linhas_invalidas_reservas(void)
{
    Reserva *r = NULL;
    size_t n = 0;
    PersRelatorio rel;

    escrever_texto(ARQ_RES,
        "#POLI-SALAS;RESERVAS;v1\n"
        "1;AULA;1;;SEG;07:10;08:50;QUÍMICA (EM);Prof. A\n"          /* ok                     */
        "1;PALESTRA;1;;SEG;07:10;08:50;QUÍMICA (EM);Prof. A\n"      /* atividade inexistente  */
        "1;AULA;1;;seg;07:10;08:50;QUÍMICA (EM);Prof. A\n"          /* dia em minusculas      */
        "1;AULA;1;;XYZ;07:10;08:50;QUÍMICA (EM);Prof. A\n"          /* dia inexistente        */
        "1;AULA;sim;;SEG;07:10;08:50;QUÍMICA (EM);Prof. A\n"        /* recorrente nao e 0/1   */
        "1;EVENTO;0;;SEG;07:10;08:50;Palestra;Prof. A\n"            /* unica sem data         */
        "1;EVENTO;0;2026-02-30;SEG;07:10;08:50;Palestra;Prof. A\n"  /* 30 de fevereiro        */
        "1;EVENTO;0;2027-02-29;SEG;07:10;08:50;Palestra;Prof. A\n"  /* 2027 nao e bissexto    */
        "1;EVENTO;0;03/11/2026;SEG;07:10;08:50;Palestra;Prof. A\n"  /* formato de data        */
        "1;AULA;1;;TER;10:30;08:50;QUÍMICA (EM);Prof. A\n"          /* fim antes do inicio    */
        "1;AULA;1;;TER;10:30;10:30;QUÍMICA (EM);Prof. A\n"          /* duracao zero           */
        "1;AULA;1;;TER;24:00;25:00;QUÍMICA (EM);Prof. A\n"          /* hora inexistente       */
        "1;AULA;1;;TER;7:10;08:50;QUÍMICA (EM);Prof. A\n"           /* hora sem zero          */
        "0;AULA;1;;TER;07:10;08:50;QUÍMICA (EM);Prof. A\n"          /* sala invalida          */
        "1;AULA;1;;TER;07:10;08:50;;Prof. A\n"                      /* sem descricao          */
        "1;AULA;1;;TER;07:10;08:50;QUÍMICA (EM);\n"                 /* sem responsavel        */
        "1;AULA;1;;TER;07:10;08:50;UMA DESCRICAO QUE PASSA DO LIMITE DE 49 BYTES - TEXTO EXTRA;Prof\n" /* grande */
        "3;MONITORIA;1;;SAB;08:50;12:10;Monitoria de Cálculo;Laura\n" /* ok (sabado)          */
        "2;EVENTO;0;2026-12-01;TER;14:00;15:40;Defesa de TCC;Coord.\n"); /* ok (data)         */

    CHECK(pers_carregar_reservas(ARQ_RES, &r, &n, &rel) == PERS_OK);
    CHECK(n == 3);
    CHECK(rel.linhas_invalidas == 16);
    CHECK(r != NULL && r[0].idSala == 1 && r[1].idSala == 3 && r[2].idSala == 2);
    CHECK(r != NULL && r[1].horario.dia == SAB && r[1].atividade == MONITORIA);
    CHECK(r != NULL && !r[2].isRecorrente && r[2].data.dia == 1 && r[2].data.mes == 12 &&
          r[2].data.ano == 2026 && r[2].horario.comeco == H(14, 0));
    pers_liberar(r);
    limpar(ARQ_RES);
}

static void test_crlf_e_bom(void)
{
    Sala *s = NULL;
    size_t n = 0;

    /* Arquivo editado no Bloco de Notas do Windows: BOM + CRLF, sem \n final */
    escrever_texto(ARQ_SAL,
        "\xEF\xBB\xBF#POLI-SALAS;SALAS;v1\r\n"
        "1;I-03;40;1;1;0;0\r\n"
        "2;I-06;20;0;0;0;0");
    CHECK(pers_carregar_salas(ARQ_SAL, &s, &n, NULL) == PERS_OK);
    CHECK(n == 2 && s != NULL && strcmp(s[0].nome, "I-03") == 0 && s[1].capacidade == 20);
    pers_liberar(s);
    limpar(ARQ_SAL);
}

static void test_salvar_dado_invalido_nao_altera_arquivo(void)
{
    Sala v[2], *lidos = NULL;
    Reserva x[1];
    size_t n = 0;

    limpar(ARQ_SAL);
    limpar(ARQ_RES);
    v[0] = sala(1, "Original");
    CHECK(pers_salvar_salas(ARQ_SAL, v, 1) == PERS_OK);

    v[1] = sala(2, "Sala;5");                            /* separador no texto */
    CHECK(pers_salvar_salas(ARQ_SAL, v, 2) == PERS_ERRO_DADO_INVALIDO);

    v[1] = sala(2, "Sala\n5");                           /* quebra de linha */
    CHECK(pers_salvar_salas(ARQ_SAL, v, 2) == PERS_ERRO_DADO_INVALIDO);

    v[1] = sala(1, "Outra");                             /* id repetido */
    CHECK(pers_salvar_salas(ARQ_SAL, v, 2) == PERS_ERRO_DADO_INVALIDO);

    v[1] = sala(2, "ORIGINAL");                          /* nome repetido */
    CHECK(pers_salvar_salas(ARQ_SAL, v, 2) == PERS_ERRO_DADO_INVALIDO);

    v[1] = sala(2, "Outra");
    memset(v[1].nome, 'z', sizeof v[1].nome);            /* string sem '\0' */
    CHECK(pers_salvar_salas(ARQ_SAL, v, 2) == PERS_ERRO_DADO_INVALIDO);

    v[1] = sala(2, "Outra");
    v[1].computador = -1;                                /* negativo */
    CHECK(pers_salvar_salas(ARQ_SAL, v, 2) == PERS_ERRO_DADO_INVALIDO);

    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, NULL) == PERS_OK);
    CHECK(n == 1 && strcmp(lidos[0].nome, "Original") == 0);
    pers_liberar(lidos);

    x[0] = aula(1, SEG, H(10, 0), H(9, 0));              /* fim antes do inicio */
    CHECK(pers_salvar_reservas(ARQ_RES, x, 1) == PERS_ERRO_DADO_INVALIDO);
    x[0] = aula(1, (Dia)9, H(7, 10), H(8, 50));          /* dia fora do enum */
    CHECK(pers_salvar_reservas(ARQ_RES, x, 1) == PERS_ERRO_DADO_INVALIDO);
    x[0] = aula(1, SEG, H(7, 10), H(8, 50));
    x[0].atividade = (Atividade)42;                      /* atividade fora do enum */
    CHECK(pers_salvar_reservas(ARQ_RES, x, 1) == PERS_ERRO_DADO_INVALIDO);
    x[0] = evento(1, 0, 0, 0, SEG, H(7, 10), H(8, 50));  /* unica sem data */
    CHECK(pers_salvar_reservas(ARQ_RES, x, 1) == PERS_ERRO_DADO_INVALIDO);
    x[0] = aula(1, SEG, H(7, 10), 1440);                 /* passa de 23:59 */
    CHECK(pers_salvar_reservas(ARQ_RES, x, 1) == PERS_ERRO_DADO_INVALIDO);
    CHECK(!existe(ARQ_RES));
    limpar(ARQ_SAL);
}

static void test_gravacao_segura_backup(void)
{
    Sala v[1], *lidos = NULL;
    size_t n = 0;

    limpar(ARQ_SAL);
    v[0] = sala(1, "Versao 1");
    CHECK(pers_salvar_salas(ARQ_SAL, v, 1) == PERS_OK);
    strcpy(v[0].nome, "Versao 2");
    CHECK(pers_salvar_salas(ARQ_SAL, v, 1) == PERS_OK);

    CHECK(!existe(ARQ_SAL ".tmp"));                       /* nao sobra temporario */
    CHECK(pers_carregar_salas(ARQ_SAL ".bak", &lidos, &n, NULL) == PERS_OK);
    CHECK(n == 1 && strcmp(lidos[0].nome, "Versao 1") == 0);
    pers_liberar(lidos);

    /* Arquivo principal corrompido -> restaura do backup */
    escrever_texto(ARQ_SAL, "conteudo corrompido");
    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, NULL) == PERS_ERRO_FORMATO);
    CHECK(pers_restaurar_backup(ARQ_SAL) == PERS_OK);
    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, NULL) == PERS_OK);
    CHECK(n == 1 && strcmp(lidos[0].nome, "Versao 1") == 0);
    pers_liberar(lidos);

    limpar(ARQ_SAL);
    CHECK(pers_restaurar_backup(ARQ_SAL) == PERS_ERRO_SEM_BACKUP);
}

/* Se a gravacao falha (aqui: o .tmp nao pode ser criado), o original fica intacto. */
static void test_falha_na_escrita_preserva_original(void)
{
    Sala v[1], *lidos = NULL;
    size_t n = 0;

    limpar(ARQ_SAL);
    REMOVER_PASTA(ARQ_SAL ".tmp");             /* sobra de execucao anterior */
    v[0] = sala(1, "Intacto");
    CHECK(pers_salvar_salas(ARQ_SAL, v, 1) == PERS_OK);

    CHECK(CRIAR_PASTA(ARQ_SAL ".tmp") == 0);   /* bloqueia a criacao do temporario */
    strcpy(v[0].nome, "Nao deveria");
    CHECK(pers_salvar_salas(ARQ_SAL, v, 1) == PERS_ERRO_ABRIR);
    CHECK(REMOVER_PASTA(ARQ_SAL ".tmp") == 0);

    CHECK(pers_carregar_salas(ARQ_SAL, &lidos, &n, NULL) == PERS_OK);
    CHECK(n == 1 && strcmp(lidos[0].nome, "Intacto") == 0);
    pers_liberar(lidos);
    limpar(ARQ_SAL);
}

static void test_erros_de_parametro_e_caminho(void)
{
    Sala v[1], *s = NULL;
    size_t n = 0;
    char caminho_longo[PERS_CAMINHO_MAX + 10];

    v[0] = sala(1, "I-03");
    CHECK(pers_salvar_salas(NULL, v, 1) == PERS_ERRO_PARAMETRO);
    CHECK(pers_salvar_salas(ARQ_SAL, NULL, 1) == PERS_ERRO_PARAMETRO);
    CHECK(pers_carregar_salas(NULL, &s, &n, NULL) == PERS_ERRO_PARAMETRO);
    CHECK(pers_carregar_salas(ARQ_SAL, NULL, &n, NULL) == PERS_ERRO_PARAMETRO);
    CHECK(pers_carregar_salas(ARQ_SAL, &s, NULL, NULL) == PERS_ERRO_PARAMETRO);
    CHECK(pers_restaurar_backup(NULL) == PERS_ERRO_PARAMETRO);

    CHECK(pers_salvar_salas("pasta_que_nao_existe/salas.txt", v, 1) == PERS_ERRO_ABRIR);

    memset(caminho_longo, 'a', sizeof caminho_longo - 1);
    caminho_longo[sizeof caminho_longo - 1] = '\0';
    CHECK(pers_salvar_salas(caminho_longo, v, 1) == PERS_ERRO_PARAMETRO);

    CHECK(strlen(pers_mensagem(PERS_ERRO_ESCRITA)) > 0);
    CHECK(pers_mensagem((PersStatus)999) != NULL);
}

static void test_muitos_registros(void)
{
    enum { QTD = 5000 };
    Reserva *v = malloc(QTD * sizeof *v), *lidos = NULL;
    size_t n = 0, i;
    int ok = 1;

    CHECK(v != NULL);
    if (!v) return;
    for (i = 0; i < QTD; i++)
        v[i] = aula((int)(i % 42) + 1, (Dia)(i % 7), H(7, 10), H(8, 50));

    limpar(ARQ_RES);
    CHECK(pers_salvar_reservas(ARQ_RES, v, QTD) == PERS_OK);
    CHECK(pers_carregar_reservas(ARQ_RES, &lidos, &n, NULL) == PERS_OK);
    CHECK(n == QTD);
    for (i = 0; lidos && i < n; i++) ok = ok && reservas_iguais(&lidos[i], &v[i]);
    CHECK(ok);
    pers_liberar(lidos);
    free(v);
    limpar(ARQ_RES);
}

/* Carrega os dados reais da planilha 2026.2 e confere que nada se perde. */
static void test_dados_reais_2026_2(void)
{
    Sala *s = NULL, *s2 = NULL;
    Reserva *r = NULL, *r2 = NULL;
    size_t ns = 0, nr = 0, ns2 = 0, nr2 = 0, i, j;
    PersRelatorio rel;
    int refs_ok = 1, iguais = 1;

    CHECK(pers_carregar_salas(DADOS_DIR "salas.txt", &s, &ns, &rel) == PERS_OK);
    CHECK(!rel.arquivo_ausente && rel.linhas_invalidas == 0 && ns == 42);

    CHECK(pers_carregar_reservas(DADOS_DIR "reservas.txt", &r, &nr, &rel) == PERS_OK);
    CHECK(!rel.arquivo_ausente && rel.linhas_invalidas == 0 && nr == 794);

    /* toda reserva aponta para uma sala existente */
    for (i = 0; i < nr; i++) {
        int achou = 0;
        for (j = 0; j < ns && !achou; j++) achou = r[i].idSala == s[j].id;
        refs_ok = refs_ok && achou;
    }
    CHECK(refs_ok);

    /* salvar e carregar de novo devolve exatamente os mesmos dados */
    limpar(ARQ_SAL);
    limpar(ARQ_RES);
    CHECK(pers_salvar_salas(ARQ_SAL, s, ns) == PERS_OK);
    CHECK(pers_salvar_reservas(ARQ_RES, r, nr) == PERS_OK);
    CHECK(pers_carregar_salas(ARQ_SAL, &s2, &ns2, NULL) == PERS_OK && ns2 == ns);
    CHECK(pers_carregar_reservas(ARQ_RES, &r2, &nr2, NULL) == PERS_OK && nr2 == nr);
    for (i = 0; s2 && i < ns2 && i < ns; i++) iguais = iguais && salas_iguais(&s[i], &s2[i]);
    for (i = 0; r2 && i < nr2 && i < nr; i++) iguais = iguais && reservas_iguais(&r[i], &r2[i]);
    CHECK(iguais);

    pers_liberar(s);  pers_liberar(r);
    pers_liberar(s2); pers_liberar(r2);
    limpar(ARQ_SAL);
    limpar(ARQ_RES);
}

/* ------------------------------------------------------------------- main */

#define RODAR(t) do { printf("- %s\n", #t); t(); } while (0)

int main(void)
{
    printf("Testes do modulo de persistencia\n");
    RODAR(test_salas_ida_e_volta);
    RODAR(test_reservas_ida_e_volta);
    RODAR(test_preserva_entre_execucoes);
    RODAR(test_arquivo_ausente);
    RODAR(test_arquivo_vazio);
    RODAR(test_cabecalho_invalido);
    RODAR(test_linhas_invalidas_salas);
    RODAR(test_linhas_invalidas_reservas);
    RODAR(test_crlf_e_bom);
    RODAR(test_salvar_dado_invalido_nao_altera_arquivo);
    RODAR(test_gravacao_segura_backup);
    RODAR(test_falha_na_escrita_preserva_original);
    RODAR(test_erros_de_parametro_e_caminho);
    RODAR(test_muitos_registros);
    RODAR(test_dados_reais_2026_2);

    printf("\n%d verificacoes, %d falha(s)\n", total, falhas);
    return falhas == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
