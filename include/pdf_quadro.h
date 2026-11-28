#ifndef PDF_QUADRO_H
#define PDF_QUADRO_H

/*
 * pdf_quadro - gera os PDFs de impressao do quadro de aulas.
 *
 * Nao depende de GTK nem das structs do projeto (Reserva, Sala...).
 * Quem chama converte os seus dados para AulaPDF e entrega o vetor.
 */

typedef struct {
    int  dia;              /* 0=SEG 1=TER 2=QUA 3=QUI 4=SEX 5=SAB 6=DOM */
    int  ini;              /* inicio em minutos desde 00:00 (8h50 = 530) */
    int  fim;              /* fim em minutos (10h30 = 630) */
    char disciplina[96];   /* ex.: "Algebra Linear" */
    char professor[80];    /* ex.: "Claudio Maciel" */
    char turma[32];        /* ex.: "TM" (pode ficar vazio) */
    char sala[32];         /* ex.: "B04" */
} AulaPDF;

/* Codigos de retorno das funcoes que geram PDF */
#define PDF_OK            1   /* PDF gerado */
#define PDF_SEM_AULAS     0   /* nao ha nada para imprimir (nenhum PDF criado) */
#define PDF_ERRO_ARQUIVO -1   /* nao foi possivel gravar (arquivo aberto em outro programa? pasta sem permissao?) */

/*
 * Quadro por dia da semana: uma pagina por dia (e mais paginas se o dia nao couber),
 * com as aulas agrupadas por faixa de horario e a sala no canto direito de cada linha.
 *   periodo   - texto do cabecalho, ex.: "Semestre 2026.2" (pode ser NULL)
 *   gerado_por - nome do usuario, aparece no rodape (pode ser NULL)
 *   arquivo   - caminho do PDF, em UTF-8
 */
int pdf_quadro_por_dia(const AulaPDF *aulas, int n, const char *periodo,
                       const char *gerado_por, const char *arquivo);

/*
 * Horario semanal de UM professor, no estilo horario de escola:
 * dias nas colunas, faixas de horario nas linhas, e em cada celula a disciplina, a turma e a sala.
 * O nome tem que ser igual ao que esta em AulaPDF.professor.
 */
int pdf_horario_professor(const AulaPDF *aulas, int n, const char *professor,
                          const char *periodo, const char *gerado_por, const char *arquivo);

/*
 * Preenche 'nomes' com os professores que aparecem em 'aulas', sem repetir e em ordem alfabetica.
 * Serve para montar a lista de escolha na tela. Retorna quantos nomes foram gravados (ate 'max').
 */
int pdf_listar_professores(const AulaPDF *aulas, int n, char nomes[][80], int max);

#endif