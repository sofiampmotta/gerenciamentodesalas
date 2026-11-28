#include "pdf_quadro.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX 300
static AulaPDF aulas[MAX];
static int n = 0;

static void add(int dia, int ini, int fim, const char *disc, const char *prof, const char *turma, const char *sala) {
    AulaPDF *a = &aulas[n++];
    memset(a, 0, sizeof *a);
    a->dia = dia; a->ini = ini; a->fim = fim;
    snprintf(a->disciplina, sizeof a->disciplina, "%s", disc);
    snprintf(a->professor, sizeof a->professor, "%s", prof);
    snprintf(a->turma, sizeof a->turma, "%s", turma);
    snprintf(a->sala, sizeof a->sala, "%s", sala);
}

int main(void) {
    /* segunda: varias salas no mesmo horario (o caso que nao pode perder ninguem) */
    add(0, 530, 630, "Álgebra Linear",        "Cláudio Maciel", "TM", "B04");
    add(0, 530, 630, "Programação",           "Heloísa Paiva",  "TN", "B03");
    add(0, 530, 630, "Desenho Técnico",       "Marcos Lima",    "TA", "D05");
    add(0, 530, 630, "Cálculo I",             "Ana Souza",      "TB", "I08");
    add(0, 640, 750, "Física Geral",          "Ana Souza",      "TB", "A02");
    add(0, 640, 750, "Estruturas de Dados e Algoritmos Avançados", "Heloísa Paiva", "TN", "Auditório");
    add(0, 840, 960, "Eletrônica",            "Marcos Lima",    "TC", "C01");
    add(0, 1140, 1260, "Banco de Dados",      "Heloísa Paiva",  "TN", "B03");
    /* quarta */
    add(2, 530, 630, "Álgebra Linear",        "Cláudio Maciel", "TM", "B04");
    add(2, 840, 1020, "Seminário de Inovação", "Direção",       "",   "Auditório");
    /* sexta e sabado */
    add(4, 530, 630, "Cálculo I",             "Ana Souza",      "TB", "I08");
    add(5, 480, 720, "Reposição de Programação", "Heloísa Paiva", "TN", "B03");
    /* terça: dia cheio, para forçar mais de uma página */
    for (int i = 0; i < 26; i++) {
        char d[40]; snprintf(d, sizeof d, "Disciplina %d", i);
        add(1, 480, 580, d, "Prof. Teste", "T1", "S01");
    }

    int r = pdf_quadro_por_dia(aulas, n, "Semestre 2026.2", "Leidjane", "exemplo_quadro_dias.pdf");
    printf("quadro por dia: %d\n", r);
    assert(r == PDF_OK);

    r = pdf_horario_professor(aulas, n, "Cláudio Maciel", "Semestre 2026.2", "Leidjane", "exemplo_prof_claudio.pdf");
    printf("professor Claudio: %d\n", r);
    assert(r == PDF_OK);

    r = pdf_horario_professor(aulas, n, "Heloísa Paiva", "Semestre 2026.2", "Leidjane", "exemplo_prof_heloisa.pdf");
    printf("professor Heloisa: %d\n", r);
    assert(r == PDF_OK);

    assert(pdf_horario_professor(aulas, n, "Fulano Inexistente", NULL, NULL, "nao_deve_existir.pdf") == PDF_SEM_AULAS);
    assert(pdf_quadro_por_dia(aulas, 0, NULL, NULL, "nao_deve_existir.pdf") == PDF_SEM_AULAS);
    assert(pdf_quadro_por_dia(aulas, n, NULL, NULL, "/pasta/que/nao/existe/x.pdf") == PDF_ERRO_ARQUIVO);

    char nomes[20][80];
    int q = pdf_listar_professores(aulas, n, nomes, 20);
    printf("professores (%d):\n", q);
    for (int i = 0; i < q; i++) printf("  %s\n", nomes[i]);
    assert(q == 6);

    printf("Todos os testes passaram.\n");
    return 0;
}