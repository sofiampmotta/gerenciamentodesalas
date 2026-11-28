#include "pdf_quadro.h"

#include <cairo.h>
#include <cairo-pdf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ------------------------------------------------------------------ constantes */

static const double PI = 3.14159265358979323846;

static const char *NOME_DIA[7]  = { "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira",
                                    "Sexta-feira", "Sábado", "Domingo" };
static const char *CURTO_DIA[7] = { "Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado", "Domingo" };

/* pagina em retrato (quadro por dia) */
#define RET_L      595.0
#define RET_A      842.0
/* pagina em paisagem (horario do professor) */
#define PAI_L      842.0
#define PAI_A      595.0

#define MARGEM     40.0
#define H_FAIXA    24.0    /* altura do titulo de cada faixa de horario */
#define H_LINHA    36.0    /* altura de cada aula na lista */
#define GAP_FAIXA  12.0
#define W_SALA     86.0    /* largura da caixa com o nome da sala */
#define RODAPE     26.0

#define H_CELULA   58.0    /* altura de cada linha do horario do professor */
#define W_HORA     96.0    /* largura da coluna de horario */

/* ------------------------------------------------------------------ utilidades de desenho */

static void cinza(cairo_t *cr, double v) { cairo_set_source_rgb(cr, v, v, v); }

static void fonte(cairo_t *cr, double tam, int negrito) {
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           negrito ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, tam);
}

static double largura(cairo_t *cr, const char *t) {
    cairo_text_extents_t e;
    cairo_text_extents(cr, t, &e);
    return e.x_advance;
}

static void texto(cairo_t *cr, double x, double y, const char *t) {
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, t);
}

static void texto_direita(cairo_t *cr, double x_dir, double y, const char *t) {
    texto(cr, x_dir - largura(cr, t), y, t);
}

static void texto_centro(cairo_t *cr, double x_centro, double y, const char *t) {
    texto(cr, x_centro - largura(cr, t) / 2.0, y, t);
}

/* Escreve o texto; se passar de 'max' pontos, corta e poe reticencias (respeitando UTF-8). */
static void texto_cortado(cairo_t *cr, double x, double y, const char *t, double max) {
    char buf[320];
    size_t len = strlen(t);
    if (len > sizeof buf - 4) {
        len = sizeof buf - 4;
        while (len > 0 && ((unsigned char) t[len] & 0xC0) == 0x80) len--;
    }
    memcpy(buf, t, len);
    buf[len] = '\0';

    if (largura(cr, buf) > max) {
        while (len > 0) {
            len--;
            while (len > 0 && ((unsigned char) t[len] & 0xC0) == 0x80) len--;
            memcpy(buf, t, len);
            memcpy(buf + len, "\xE2\x80\xA6", 4);   /* reticencias + terminador */
            if (largura(cr, buf) <= max) break;
        }
    }
    texto(cr, x, y, buf);
}

/* Quebra o texto em ate 2 linhas (na ultima palavra que cabe). Retorna quantas linhas precisa. */
static int linhas_necessarias(cairo_t *cr, const char *t, double max) {
    return largura(cr, t) <= max ? 1 : 2;
}

static void texto_duas_linhas(cairo_t *cr, double x, double y, double entrelinha, const char *t, double max) {
    if (largura(cr, t) <= max) { texto(cr, x, y, t); return; }

    char buf[320];
    size_t len = strlen(t);
    if (len > sizeof buf - 1) len = sizeof buf - 1;
    memcpy(buf, t, len);
    buf[len] = '\0';

    /* maior prefixo que cabe, recuando ate um espaco */
    size_t corte = len;
    while (corte > 0) {
        buf[corte] = '\0';
        if (largura(cr, buf) <= max) break;
        corte--;
        while (corte > 0 && ((unsigned char) t[corte] & 0xC0) == 0x80) corte--;
    }
    size_t esp = corte;
    while (esp > 0 && t[esp] != ' ') esp--;
    if (esp > 0) corte = esp;

    memcpy(buf, t, corte);
    buf[corte] = '\0';
    texto(cr, x, y, buf);

    const char *resto = t + corte;
    while (*resto == ' ') resto++;
    texto_cortado(cr, x, y + entrelinha, resto, max);
}

static void retangulo_arredondado(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, PI / 2);
    cairo_arc(cr, x + r,     y + h - r, r, PI / 2, PI);
    cairo_arc(cr, x + r,     y + r,     r, PI, 3 * PI / 2);
    cairo_close_path(cr);
}

static void faixa_texto(int ini, int fim, char *buf, size_t cap) {
    snprintf(buf, cap, "%02d:%02d \xE2\x80\x93 %02d:%02d", ini / 60, ini % 60, fim / 60, fim % 60);
}

/* "Turma TM  ·  Sala B04" / junta so as partes que existem */
static void juntar(char *buf, size_t cap, const char *a, const char *b) {
    buf[0] = '\0';
    if (a[0] && b[0]) snprintf(buf, cap, "%s  \xC2\xB7  %s", a, b);
    else if (a[0])    snprintf(buf, cap, "%s", a);
    else if (b[0])    snprintf(buf, cap, "%s", b);
}

static int cmp_aula(const void *pa, const void *pb) {
    const AulaPDF *a = pa, *b = pb;
    if (a->ini != b->ini) return a->ini - b->ini;
    if (a->fim != b->fim) return a->fim - b->fim;
    int c = strcmp(a->turma, b->turma);
    if (c) return c;
    return strcmp(a->disciplina, b->disciplina);
}

static void rodape(cairo_t *cr, double larg, double alt, int pagina, int total, const char *por) {
    char data[32], esq[200], dir[40];
    time_t agora = time(NULL);
    struct tm *lt = localtime(&agora);
    if (lt) strftime(data, sizeof data, "%d/%m/%Y %H:%M", lt);
    else    snprintf(data, sizeof data, "--");

    if (por && por[0]) snprintf(esq, sizeof esq, "Gerado em %s por %s", data, por);
    else               snprintf(esq, sizeof esq, "Gerado em %s", data);
    if (total > 0) snprintf(dir, sizeof dir, "Página %d de %d", pagina, total);
    else           snprintf(dir, sizeof dir, "Página %d", pagina);

    cinza(cr, 0.55);
    fonte(cr, 8, 0);
    texto(cr, MARGEM, alt - MARGEM + 6, esq);
    texto_direita(cr, larg - MARGEM, alt - MARGEM + 6, dir);
}

/* ------------------------------------------------------------------ saida: arquivo UTF-8 + passada de contagem */

#ifdef _WIN32
static FILE *abrir_escrita(const char *caminho) {
    wchar_t w[1024];
    if (!MultiByteToWideChar(CP_UTF8, 0, caminho, -1, w, 1024)) return NULL;
    return _wfopen(w, L"wb");
}
#else
static FILE *abrir_escrita(const char *caminho) { return fopen(caminho, "wb"); }
#endif

static cairo_status_t escrever(void *f, const unsigned char *dados, unsigned int tam) {
    return fwrite(dados, 1, tam, (FILE *) f) == tam ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_WRITE_ERROR;
}

static cairo_status_t descartar(void *f, const unsigned char *dados, unsigned int tam) {
    (void) f; (void) dados; (void) tam;
    return CAIRO_STATUS_SUCCESS;
}

typedef int (*Renderizador)(cairo_t *cr, const void *args, int total_paginas);

/* Passada 1 (descartada) so para saber quantas paginas serao, para o "Pagina X de Y".
   Passada 2 grava o arquivo de verdade. */
static int executar(const char *arquivo, double larg, double alt, Renderizador r, const void *args) {
    cairo_surface_t *s = cairo_pdf_surface_create_for_stream(descartar, NULL, larg, alt);
    cairo_t *cr = cairo_create(s);
    int total = r(cr, args, 0);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    if (total <= 0) return PDF_SEM_AULAS;

    FILE *f = abrir_escrita(arquivo);
    if (!f) return PDF_ERRO_ARQUIVO;

    s = cairo_pdf_surface_create_for_stream(escrever, f, larg, alt);
    cr = cairo_create(s);
    r(cr, args, total);
    cairo_status_t st1 = cairo_status(cr);
    cairo_destroy(cr);
    cairo_surface_finish(s);
    cairo_status_t st2 = cairo_surface_status(s);
    cairo_surface_destroy(s);
    int fechou = fclose(f);

    return (st1 == CAIRO_STATUS_SUCCESS && st2 == CAIRO_STATUS_SUCCESS && fechou == 0)
           ? PDF_OK : PDF_ERRO_ARQUIVO;
}

/* ------------------------------------------------------------------ quadro por dia */

typedef struct {
    const AulaPDF *aulas;
    int n;
    const char *periodo, *por;
} ArgsDias;

typedef struct {
    cairo_t *cr;
    int pagina, total;
    double y;
    const ArgsDias *a;
} PagDia;

static void abrir_pagina_dia(PagDia *p, int dia, int continuacao) {
    cairo_t *cr = p->cr;
    p->pagina++;

    cinza(cr, 0.45);
    fonte(cr, 9, 1);
    texto(cr, MARGEM, MARGEM + 4, "QUADRO DE SALAS");

    char titulo[80];
    snprintf(titulo, sizeof titulo, "%s%s", NOME_DIA[dia], continuacao ? " (continuação)" : "");
    cinza(cr, 0.05);
    fonte(cr, 24, 1);
    texto(cr, MARGEM, MARGEM + 32, titulo);

    if (p->a->periodo && p->a->periodo[0]) {
        cinza(cr, 0.3);
        fonte(cr, 11, 0);
        texto_direita(cr, RET_L - MARGEM, MARGEM + 32, p->a->periodo);
    }

    cinza(cr, 0.05);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, MARGEM, MARGEM + 42);
    cairo_line_to(cr, RET_L - MARGEM, MARGEM + 42);
    cairo_stroke(cr);

    p->y = MARGEM + 56;
}

static void fechar_pagina(PagDia *p) {
    rodape(p->cr, RET_L, RET_A, p->pagina, p->total, p->a->por);
    cairo_show_page(p->cr);
}

static void desenhar_faixa(PagDia *p, const AulaPDF *a, int qtd) {
    cairo_t *cr = p->cr;
    char h[32], q[24];
    faixa_texto(a->ini, a->fim, h, sizeof h);
    snprintf(q, sizeof q, "%d %s", qtd, qtd == 1 ? "aula" : "aulas");

    cinza(cr, 0.86);
    cairo_rectangle(cr, MARGEM, p->y, RET_L - 2 * MARGEM, H_FAIXA);
    cairo_fill(cr);

    cinza(cr, 0.05);
    fonte(cr, 12, 1);
    texto(cr, MARGEM + 10, p->y + 16.5, h);

    cinza(cr, 0.4);
    fonte(cr, 9, 0);
    texto_direita(cr, RET_L - MARGEM - 10, p->y + 16, q);

    p->y += H_FAIXA;
}

static void desenhar_aula(PagDia *p, const AulaPDF *a, int zebra) {
    cairo_t *cr = p->cr;
    double x0 = MARGEM, x1 = RET_L - MARGEM, y = p->y;

    if (zebra) {
        cinza(cr, 0.965);
        cairo_rectangle(cr, x0, y, x1 - x0, H_LINHA);
        cairo_fill(cr);
    }

    /* sala: caixa no canto direito */
    double bx = x1 - W_SALA - 6, by = y + 6, bh = H_LINHA - 12;
    retangulo_arredondado(cr, bx, by, W_SALA, bh, 5);
    cinza(cr, 0.93);
    cairo_fill_preserve(cr);
    cinza(cr, 0.1);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    fonte(cr, 12, 1);
    char sala[40];
    snprintf(sala, sizeof sala, "%s", a->sala[0] ? a->sala : "-");
    while (largura(cr, sala) > W_SALA - 8 && strlen(sala) > 1) sala[strlen(sala) - 1] = '\0';
    texto_centro(cr, bx + W_SALA / 2, by + bh / 2 + 4.3, sala);

    /* disciplina + professor/turma */
    double max_txt = bx - 12 - (x0 + 10);
    cinza(cr, 0.05);
    fonte(cr, 11.5, 1);
    texto_cortado(cr, x0 + 10, y + 15, a->disciplina, max_txt);

    char l2[220], turma[48] = "";
    if (a->turma[0]) snprintf(turma, sizeof turma, "Turma %s", a->turma);
    juntar(l2, sizeof l2, a->professor, turma);
    cinza(cr, 0.35);
    fonte(cr, 9, 0);
    texto_cortado(cr, x0 + 10, y + 28, l2, max_txt);

    cinza(cr, 0.8);
    cairo_set_line_width(cr, 0.5);
    cairo_move_to(cr, x0, y + H_LINHA);
    cairo_line_to(cr, x1, y + H_LINHA);
    cairo_stroke(cr);

    p->y += H_LINHA;
}

static int renderizar_dias(cairo_t *cr, const void *args, int total) {
    const ArgsDias *a = args;
    PagDia p = { cr, 0, total, 0, a };
    const double limite = RET_A - MARGEM - RODAPE;

    AulaPDF *dia = malloc((size_t) (a->n > 0 ? a->n : 1) * sizeof *dia);
    if (!dia) return 0;

    for (int d = 0; d < 7; d++) {
        int m = 0;
        for (int i = 0; i < a->n; i++)
            if (a->aulas[i].dia == d) dia[m++] = a->aulas[i];
        if (m == 0) continue;
        qsort(dia, (size_t) m, sizeof *dia, cmp_aula);

        abrir_pagina_dia(&p, d, 0);
        int i = 0;
        while (i < m) {
            int j = i;
            while (j < m && dia[j].ini == dia[i].ini && dia[j].fim == dia[i].fim) j++;

            if (p.y + H_FAIXA + H_LINHA > limite) {
                fechar_pagina(&p);
                abrir_pagina_dia(&p, d, 1);
            }
            desenhar_faixa(&p, &dia[i], j - i);
            for (int k = i; k < j; k++) {
                if (p.y + H_LINHA > limite) {
                    fechar_pagina(&p);
                    abrir_pagina_dia(&p, d, 1);
                    desenhar_faixa(&p, &dia[i], j - i);
                }
                desenhar_aula(&p, &dia[k], (k - i) % 2);
            }
            p.y += GAP_FAIXA;
            i = j;
        }
        fechar_pagina(&p);
    }
    free(dia);
    return p.pagina;
}

int pdf_quadro_por_dia(const AulaPDF *aulas, int n, const char *periodo,
                       const char *gerado_por, const char *arquivo) {
    if (!aulas || n <= 0 || !arquivo) return PDF_SEM_AULAS;
    ArgsDias a = { aulas, n, periodo, gerado_por };
    return executar(arquivo, RET_L, RET_A, renderizar_dias, &a);
}

/* ------------------------------------------------------------------ horario do professor */

typedef struct {
    const AulaPDF *aulas;
    int n;
    const char *prof, *periodo, *por;
} ArgsProf;

static void cabecalho_prof(cairo_t *cr, const ArgsProf *a) {
    cinza(cr, 0.45);
    fonte(cr, 9, 1);
    texto(cr, MARGEM, MARGEM + 4, "HORÁRIO SEMANAL DO PROFESSOR");

    cinza(cr, 0.05);
    fonte(cr, 22, 1);
    double max = PAI_L - 2 * MARGEM - 200;
    texto_cortado(cr, MARGEM, MARGEM + 30, a->prof, max);

    if (a->periodo && a->periodo[0]) {
        cinza(cr, 0.3);
        fonte(cr, 11, 0);
        texto_direita(cr, PAI_L - MARGEM, MARGEM + 30, a->periodo);
    }

    cinza(cr, 0.05);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, MARGEM, MARGEM + 40);
    cairo_line_to(cr, PAI_L - MARGEM, MARGEM + 40);
    cairo_stroke(cr);
}

static void celula_prof(cairo_t *cr, double x, double y, double w, const AulaPDF *e[], int qtd) {
    int mostrar = qtd > 2 ? 2 : qtd;
    double eh = H_CELULA / mostrar;
    double tam1 = mostrar == 1 ? 9.5 : 8.0, tam2 = mostrar == 1 ? 8.5 : 7.0;

    cinza(cr, 0.93);
    cairo_rectangle(cr, x, y, w, H_CELULA);
    cairo_fill(cr);

    if (mostrar == 1) {
        char l2[120], sala[48] = "", turma[48] = "";
        if (e[0]->turma[0]) snprintf(turma, sizeof turma, "Turma %s", e[0]->turma);
        if (e[0]->sala[0])  snprintf(sala, sizeof sala, "Sala %s", e[0]->sala);
        juntar(l2, sizeof l2, turma, sala);

        double max = w - 10;
        fonte(cr, 9.5, 1);
        int ln = linhas_necessarias(cr, e[0]->disciplina, max);
        double topo = y + (H_CELULA - (ln * 11.5 + 12)) / 2.0;
        cinza(cr, 0.05);
        texto_duas_linhas(cr, x + 5, topo + 9, 11.5, e[0]->disciplina, max);
        cinza(cr, 0.3);
        fonte(cr, 8, 0);
        if (largura(cr, l2) > max)   /* nao coube: versao curta, sem as palavras "Turma" e "Sala" */
            juntar(l2, sizeof l2, e[0]->turma, e[0]->sala);
        texto_cortado(cr, x + 5, topo + ln * 11.5 + 9, l2, max);
        return;
    }

    for (int k = 0; k < mostrar; k++) {
        double ey = y + k * eh;
        char l2[120], sala[48] = "", turma[48] = "";
        if (e[k]->turma[0]) snprintf(turma, sizeof turma, "Turma %s", e[k]->turma);
        if (e[k]->sala[0])  snprintf(sala, sizeof sala, "Sala %s", e[k]->sala);
        juntar(l2, sizeof l2, turma, sala);

        cinza(cr, 0.05);
        fonte(cr, tam1, 1);
        texto_cortado(cr, x + 5, ey + eh / 2 - (mostrar == 1 ? 2 : 1), e[k]->disciplina, w - 10);
        cinza(cr, 0.3);
        fonte(cr, tam2, 0);
        texto_cortado(cr, x + 5, ey + eh / 2 + (mostrar == 1 ? 11 : 9), l2, w - 10);
    }
    if (qtd > 2) {
        char mais[24];
        snprintf(mais, sizeof mais, "+%d", qtd - 2);
        cinza(cr, 0.6);
        fonte(cr, 8, 1);
        texto_direita(cr, x + w - 3, y + 9, mais);
    }
}

static int renderizar_professor(cairo_t *cr, const void *args, int total) {
    const ArgsProf *a = args;

    AulaPDF *m = malloc((size_t) (a->n > 0 ? a->n : 1) * sizeof *m);
    if (!m) return 0;
    int qtd = 0, max_dia = 4;
    for (int i = 0; i < a->n; i++) {
        if (strcmp(a->aulas[i].professor, a->prof) != 0) continue;
        if (a->aulas[i].dia < 0 || a->aulas[i].dia > 6) continue;
        m[qtd++] = a->aulas[i];
        if (a->aulas[i].dia > max_dia) max_dia = a->aulas[i].dia;
    }
    if (qtd == 0) { free(m); return 0; }
    qsort(m, (size_t) qtd, sizeof *m, cmp_aula);

    /* faixas de horario distintas, em ordem */
    int (*faixas)[2] = malloc((size_t) qtd * sizeof *faixas);
    if (!faixas) { free(m); return 0; }
    int nf = 0;
    for (int i = 0; i < qtd; i++)
        if (nf == 0 || faixas[nf - 1][0] != m[i].ini || faixas[nf - 1][1] != m[i].fim) {
            faixas[nf][0] = m[i].ini;
            faixas[nf][1] = m[i].fim;
            nf++;
        }

    int ncol = max_dia + 1;
    double wcol = (PAI_L - 2 * MARGEM - W_HORA) / ncol;
    double topo_tab = MARGEM + 54, h_cab = 24;
    int por_pagina = (int) ((PAI_A - MARGEM - RODAPE - topo_tab - h_cab) / H_CELULA);
    if (por_pagina < 1) por_pagina = 1;

    int pagina = 0;
    for (int ini = 0; ini < nf; ini += por_pagina) {
        int fim = ini + por_pagina < nf ? ini + por_pagina : nf;
        pagina++;
        cabecalho_prof(cr, a);

        /* cabecalho da tabela */
        cinza(cr, 0.82);
        cairo_rectangle(cr, MARGEM, topo_tab, PAI_L - 2 * MARGEM, h_cab);
        cairo_fill(cr);
        cinza(cr, 0.05);
        fonte(cr, 10, 1);
        texto_centro(cr, MARGEM + W_HORA / 2, topo_tab + 16, "Horário");
        for (int c = 0; c < ncol; c++)
            texto_centro(cr, MARGEM + W_HORA + c * wcol + wcol / 2, topo_tab + 16, CURTO_DIA[c]);

        for (int f = ini; f < fim; f++) {
            double y = topo_tab + h_cab + (f - ini) * H_CELULA;
            char h[32];
            faixa_texto(faixas[f][0], faixas[f][1], h, sizeof h);
            cinza(cr, 0.05);
            fonte(cr, 10, 1);
            texto_centro(cr, MARGEM + W_HORA / 2, y + H_CELULA / 2 + 3.5, h);

            for (int c = 0; c < ncol; c++) {
                const AulaPDF *e[64];
                int ne = 0;
                for (int i = 0; i < qtd && ne < 64; i++)
                    if (m[i].dia == c && m[i].ini == faixas[f][0] && m[i].fim == faixas[f][1])
                        e[ne++] = &m[i];
                if (ne) celula_prof(cr, MARGEM + W_HORA + c * wcol, y, wcol, e, ne);
            }
        }

        /* grade */
        double yfim = topo_tab + h_cab + (fim - ini) * H_CELULA;
        cinza(cr, 0.55);
        cairo_set_line_width(cr, 0.6);
        for (int f = 0; f <= fim - ini; f++) {
            double y = topo_tab + h_cab + f * H_CELULA;
            cairo_move_to(cr, MARGEM, y);
            cairo_line_to(cr, PAI_L - MARGEM, y);
        }
        for (int c = 0; c <= ncol; c++) {
            double x = MARGEM + W_HORA + c * wcol;
            cairo_move_to(cr, x, topo_tab);
            cairo_line_to(cr, x, yfim);
        }
        cairo_move_to(cr, MARGEM, topo_tab);
        cairo_line_to(cr, MARGEM, yfim);
        cairo_move_to(cr, MARGEM, topo_tab);
        cairo_line_to(cr, PAI_L - MARGEM, topo_tab);
        cairo_stroke(cr);

        rodape(cr, PAI_L, PAI_A, pagina, total, a->por);
        cairo_show_page(cr);
    }
    free(faixas);
    free(m);
    return pagina;
}

int pdf_horario_professor(const AulaPDF *aulas, int n, const char *professor,
                          const char *periodo, const char *gerado_por, const char *arquivo) {
    if (!aulas || n <= 0 || !professor || !professor[0] || !arquivo) return PDF_SEM_AULAS;
    ArgsProf a = { aulas, n, professor, periodo, gerado_por };
    return executar(arquivo, PAI_L, PAI_A, renderizar_professor, &a);
}

/* ------------------------------------------------------------------ lista de professores */

static int cmp_nome(const void *a, const void *b) { return strcmp((const char *) a, (const char *) b); }

int pdf_listar_professores(const AulaPDF *aulas, int n, char nomes[][80], int max) {
    int qtd = 0;
    if (!aulas || !nomes) return 0;
    for (int i = 0; i < n && qtd < max; i++) {
        const char *p = aulas[i].professor;
        if (!p[0]) continue;
        int existe = 0;
        for (int k = 0; k < qtd; k++)
            if (strcmp(nomes[k], p) == 0) { existe = 1; break; }
        if (!existe) {
            strncpy(nomes[qtd], p, 79);
            nomes[qtd][79] = '\0';
            qtd++;
        }
    }
    qsort(nomes, (size_t) qtd, 80, cmp_nome);
    return qtd;
}