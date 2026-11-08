#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/quadro.h"

extern Reserva reservas[];
extern int totalReservas;

// funcao auxiliar usada pelo qsort, pra comparar duas reservas pelo horario de inicio
static int compararPorHorarioInicio(const void *a, const void *b) {
    const Reserva *ra = (const Reserva *)a;
    const Reserva *rb = (const Reserva *)b;
    return ra->horario.comeco - rb->horario.comeco;
}

// verifica se uma reserva individual passa pelos filtros escolhidos
static bool passaNosFiltros(Reserva r, FiltroReservas filtro) {

    if (filtro.filtrarPorAtividade && r.atividade != filtro.atividade) {
        return false;
    }

    if (filtro.filtrarPorSala && r.idSala != filtro.idSala) {
        return false;
    }

    return true;
}

int obterReservasDoDia(Dia dia, FiltroReservas filtro, Reserva saida[], int capacidadeSaida) {

    int quantidadeEncontrada = 0;

    for (int i = 0; i < totalReservas; i++) {

        if (reservas[i].horario.dia != dia) {
            continue;
        }

        if (!passaNosFiltros(reservas[i], filtro)) {
            continue;
        }

        if (quantidadeEncontrada >= capacidadeSaida) {
            break; // array de saida cheio, nao da pra adicionar mais
        }

        saida[quantidadeEncontrada] = reservas[i];
        quantidadeEncontrada++;
    }

    qsort(saida, quantidadeEncontrada, sizeof(Reserva), compararPorHorarioInicio);

    return quantidadeEncontrada;
}

ColunasExportacao colunasExportacaoPadrao(void) {
    ColunasExportacao c;
    c.incluirHorario = true;
    c.incluirAtividade = true;
    c.incluirDescricao = true;
    c.incluirResponsavel = true;
    c.incluirSala = true;
    return c;
}

// converte o enum Atividade pra texto legivel no CSV
static const char *nomeAtividade(Atividade a) {
    switch (a) {
        case AULA: return "AULA";
        case MONITORIA: return "MONITORIA";
        case EXTENSAO: return "EXTENSAO";
        case EVENTO: return "EVENTO";
        default: return "OUTRO";
    }
}

int exportarReservasCSV(Reserva lista[], int quantidade, ColunasExportacao colunas, const char *caminhoArquivo) {

    FILE *arquivo = fopen(caminhoArquivo, "w");

    if (arquivo == NULL) {
        return 0;
    }

    // cabecalho -- so escreve a coluna se ela estiver habilitada
    bool primeiraColuna = true;

    if (colunas.incluirHorario) {
        fprintf(arquivo, "%shorarioInicio,horarioFim", primeiraColuna ? "" : ",");
        primeiraColuna = false;
    }
    if (colunas.incluirAtividade) {
        fprintf(arquivo, "%satividade", primeiraColuna ? "" : ",");
        primeiraColuna = false;
    }
    if (colunas.incluirDescricao) {
        fprintf(arquivo, "%sdescricao", primeiraColuna ? "" : ",");
        primeiraColuna = false;
    }
    if (colunas.incluirResponsavel) {
        fprintf(arquivo, "%sresponsavel", primeiraColuna ? "" : ",");
        primeiraColuna = false;
    }
    if (colunas.incluirSala) {
        fprintf(arquivo, "%sidSala", primeiraColuna ? "" : ",");
    }
    fprintf(arquivo, "\n");

    // uma linha por reserva
    for (int i = 0; i < quantidade; i++) {
        primeiraColuna = true;

        if (colunas.incluirHorario) {
            fprintf(arquivo, "%s%d,%d", primeiraColuna ? "" : ",", lista[i].horario.comeco, lista[i].horario.fim);
            primeiraColuna = false;
        }
        if (colunas.incluirAtividade) {
            fprintf(arquivo, "%s%s", primeiraColuna ? "" : ",", nomeAtividade(lista[i].atividade));
            primeiraColuna = false;
        }
        if (colunas.incluirDescricao) {
            fprintf(arquivo, "%s%s", primeiraColuna ? "" : ",", lista[i].descricao);
            primeiraColuna = false;
        }
        if (colunas.incluirResponsavel) {
            fprintf(arquivo, "%s%s", primeiraColuna ? "" : ",", lista[i].responsavel);
            primeiraColuna = false;
        }
        if (colunas.incluirSala) {
            fprintf(arquivo, "%s%d", primeiraColuna ? "" : ",", lista[i].idSala);
        }
        fprintf(arquivo, "\n");
    }

    fclose(arquivo);
    return 1;
}

#define PDF_LINHAS_POR_PAGINA 45
#define PDF_MAX_LINHAS 2000
#define PDF_LARGURA_LINHA 120

// converte minutos (0-1439) pra formato HH:MM legivel
static void formatarHorario(int minutos, char *saida) {
    int h = minutos / 60;
    int m = minutos % 60;
    sprintf(saida, "%02d:%02d", h, m);
}

// o formato PDF exige escapar parenteses e barra invertida dentro de texto,
// senao o leitor de PDF confunde com a sintaxe do proprio formato
static void escaparTextoPDF(const char *entrada, char *saida, int tamanhoSaida) {
    int j = 0;
    for (int i = 0; entrada[i] != '\0' && j < tamanhoSaida - 2; i++) {
        char c = entrada[i];
        if (c == '(' || c == ')' || c == '\\') {
            saida[j++] = '\\';
        }
        saida[j++] = c;
    }
    saida[j] = '\0';
}

int exportarReservasPDF(Reserva lista[], int quantidade, const char *tituloDia, const char *caminhoArquivo) {

    // monta todas as linhas de texto primeiro (cabecalho + uma linha por reserva)
    char linhas[PDF_MAX_LINHAS][PDF_LARGURA_LINHA];
    int totalLinhas = 0;

    char tituloEscapado[PDF_LARGURA_LINHA];
    escaparTextoPDF(tituloDia, tituloEscapado, PDF_LARGURA_LINHA);
    snprintf(linhas[totalLinhas++], PDF_LARGURA_LINHA, "Quadro de horarios - %s", tituloEscapado);
    snprintf(linhas[totalLinhas++], PDF_LARGURA_LINHA, " ");

    for (int i = 0; i < quantidade && totalLinhas < PDF_MAX_LINHAS - 1; i++) {
        char inicio[6], fim[6];
        formatarHorario(lista[i].horario.comeco, inicio);
        formatarHorario(lista[i].horario.fim, fim);

        char descricaoEsc[PDF_LARGURA_LINHA];
        escaparTextoPDF(lista[i].descricao, descricaoEsc, PDF_LARGURA_LINHA);
        char responsavelEsc[PDF_LARGURA_LINHA];
        escaparTextoPDF(lista[i].responsavel, responsavelEsc, PDF_LARGURA_LINHA);

        snprintf(linhas[totalLinhas], PDF_LARGURA_LINHA, "%s-%s  %s  Sala %d  %s",
                 inicio, fim, descricaoEsc, lista[i].idSala, responsavelEsc);
        totalLinhas++;
    }

    int numPaginas = (totalLinhas + PDF_LINHAS_POR_PAGINA - 1) / PDF_LINHAS_POR_PAGINA;
    if (numPaginas < 1) numPaginas = 1;

    FILE *arquivo = fopen(caminhoArquivo, "wb");
    if (arquivo == NULL) {
        return 0;
    }

    int totalObjetos = 3 + 2 * numPaginas;
    long offsets[totalObjetos + 1]; // guarda em que byte do arquivo cada objeto comeca

    fprintf(arquivo, "%%PDF-1.4\n");

    // objeto 1: Catalog -- o "ponto de entrada" do documento
    offsets[1] = ftell(arquivo);
    fprintf(arquivo, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    // objeto 2: Pages -- lista todas as paginas (seus ids sao 4, 6, 8... nessa numeracao)
    offsets[2] = ftell(arquivo);
    fprintf(arquivo, "2 0 obj\n<< /Type /Pages /Kids [");
    for (int p = 0; p < numPaginas; p++) {
        fprintf(arquivo, "%d 0 R ", 4 + 2 * p);
    }
    fprintf(arquivo, "] /Count %d >>\nendobj\n", numPaginas);

    // objeto 3: Fonte usada no texto
    offsets[3] = ftell(arquivo);
    fprintf(arquivo, "3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n");

    int linhaAtual = 0;
    for (int p = 0; p < numPaginas; p++) {
        int idPagina = 4 + 2 * p;
        int idConteudo = 5 + 2 * p;

        // monta o conteudo de texto dessa pagina (comandos do formato PDF: Tj = escreve texto, T* = proxima linha)
        char conteudo[PDF_LINHAS_POR_PAGINA * (PDF_LARGURA_LINHA + 40) + 200];
        int pos = 0;
        pos += sprintf(conteudo + pos, "BT /F1 10 Tf 40 800 Td 12 TL\n");

        int linhasNestaPagina = 0;
        while (linhaAtual < totalLinhas && linhasNestaPagina < PDF_LINHAS_POR_PAGINA) {
            pos += sprintf(conteudo + pos, "(%s) Tj T*\n", linhas[linhaAtual]);
            linhaAtual++;
            linhasNestaPagina++;
        }
        pos += sprintf(conteudo + pos, "ET\n");

        offsets[idPagina] = ftell(arquivo);
        fprintf(arquivo,
            "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
            "/Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",
            idPagina, idConteudo);

        offsets[idConteudo] = ftell(arquivo);
        fprintf(arquivo, "%d 0 obj\n<< /Length %d >>\nstream\n%sendstream\nendobj\n",
                idConteudo, (int)strlen(conteudo), conteudo);
    }

    // tabela de referencias (xref) -- diz em que byte cada objeto comeca
    long posXref = ftell(arquivo);
    fprintf(arquivo, "xref\n0 %d\n", totalObjetos + 1);
    fprintf(arquivo, "0000000000 65535 f \n");
    for (int i = 1; i <= totalObjetos; i++) {
        fprintf(arquivo, "%010ld 00000 n \n", offsets[i]);
    }

    fprintf(arquivo, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
            totalObjetos + 1, posXref);

    fclose(arquivo);
    return 1;
}