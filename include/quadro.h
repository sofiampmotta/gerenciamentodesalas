#ifndef QUADRO_H
#define QUADRO_H

#include "salas.h"
#include <stdbool.h>

// Criterios de filtro para obterReservasDoDia.
// Cada campo "filtrarPor..." controla se o criterio correspondente e aplicado.
// Preparado para extensao futura (curso, turma) sem quebrar quem ja chama a funcao.
typedef struct {
    bool filtrarPorAtividade;
    Atividade atividade;

    bool filtrarPorSala;
    int idSala;
} FiltroReservas;

// Retorna, em saida, as reservas do dia informado que passam pelos filtros,
// ordenadas cronologicamente pelo horario de inicio.
// capacidadeSaida e o tamanho maximo do array saida.
// Retorna a quantidade de reservas encontradas (pode ser 0).
int obterReservasDoDia(Dia dia, FiltroReservas filtro, Reserva saida[], int capacidadeSaida);

// Quais colunas incluir na exportacao. Permite customizar o que sai no arquivo,
// conforme o criterio de aceitacao "permitir escolher quais informacoes serao incluidas".
typedef struct {
    bool incluirHorario;
    bool incluirAtividade;
    bool incluirDescricao;
    bool incluirResponsavel;
    bool incluirSala;
} ColunasExportacao;

// Devolve uma configuracao padrao com as colunas que aparecem no quadro impresso
// (horario, atividade, descricao, responsavel, sala) -- todas incluidas.
ColunasExportacao colunasExportacaoPadrao(void);

// Exporta para CSV as reservas ja filtradas/ordenadas (normalmente vindas de obterReservasDoDia).
// Retorna 1 (sucesso) ou 0 (falha ao abrir o arquivo).
int exportarReservasCSV(Reserva lista[], int quantidade, ColunasExportacao colunas, const char *caminhoArquivo);

// Exporta as reservas (ja filtradas/ordenadas) para um PDF simples, pronto pra impressao,
// quebrando automaticamente em paginas A4 quando necessario.
int exportarReservasPDF(Reserva lista[], int quantidade, const char *tituloDia, const char *caminhoArquivo);

#endif