# Módulo de persistência

Salva e carrega as **salas** (`Sala`) e as **reservas** (`Reserva`) definidas
em `include/salas.h`, usando arquivos de texto locais. Resolve a issue #4.

| Arquivo | Conteúdo |
|---|---|
| `include/persistencia.h` | funções que os outros módulos chamam |
| `src/persistencia.c` | implementação |
| `tests/test_persistencia.c` | testes (`make test`) |
| `dados_exemplo/` | 42 salas e 794 aulas reais de 2026.2 no formato abaixo |

## Como usar

```c
#include "persistencia.h"

/* Na inicialização */
Sala *salas = NULL;
size_t qtd_salas = 0;
PersRelatorio rel;

PersStatus st = pers_carregar_salas(PERS_ARQ_SALAS, &salas, &qtd_salas, &rel);
if (st != PERS_OK) {
    printf("%s\n", pers_mensagem(st));            /* ou exibir na interface */
} else if (rel.linhas_invalidas > 0) {
    printf("%zu registro(s) ignorado(s) (1o na linha %zu)\n",
           rel.linhas_invalidas, rel.primeira_linha_invalida);
}

/* Depois de cada inclusão/edição/remoção */
st = pers_salvar_salas(PERS_ARQ_SALAS, salas, qtd_salas);
if (st != PERS_OK) printf("%s\n", pers_mensagem(st));

/* Ao encerrar */
pers_liberar(salas);
```

Para reservas é igual, com `pers_carregar_reservas`, `pers_salvar_reservas` e
`PERS_ARQ_RESERVAS`. O vetor devolvido é alocado com `malloc`: quem recebe pode
ficar com ele (e usar `realloc` para crescer) ou copiar para outra estrutura.

## Por que texto (e não binário)

- Dá para abrir e conferir no Bloco de Notas.
- Funciona igual em Windows e Linux. Com `fwrite` da struct, o arquivo depende
  do compilador (padding, tamanho de `bool` e de `int`).
- Uma linha corrompida é ignorada, e o resto do arquivo continua sendo lido.
- Se uma struct mudar, basta trocar a versão no cabeçalho (`v1` → `v2`).

## Formato

Regras gerais:
- UTF-8, uma linha por registro, campos separados por `;`.
- A primeira linha é um cabeçalho com tipo e versão.
- Linhas em branco e linhas começando com `#` são ignoradas.
- Textos não podem conter `;` nem quebra de linha; salvar recusa esses
  registros. **Os módulos de cadastro devem validar isso na entrada.**
- Tamanhos de texto contam **bytes**: letra acentuada ocupa 2.
- Arquivos do Windows (`\r\n`, BOM do Bloco de Notas) são aceitos.

### `salas.txt`

```
#POLI-SALAS;SALAS;v1
id;nome;capacidade;acessivel;projetor;televisao;computadores
1;A-03;45;1;1;0;0
36;LIP-03;30;0;1;1;25
```

| Campo | Regra |
|---|---|
| `id` | > 0, único |
| `nome` | obrigatório, único (sem diferenciar maiúsc./minúsc.), até 19 bytes |
| `capacidade`, `computadores` | inteiro ≥ 0 |
| `acessivel`, `projetor`, `televisao` | `0` ou `1` |

### `reservas.txt`

```
#POLI-SALAS;RESERVAS;v1
id_sala;atividade;recorrente;data;dia;inicio;fim;descricao;responsavel
1;AULA;1;;SEG;07:10;08:50;DEC METODOLOGIA DA PESQUISA (AM);MICAELLA RAISSA FALCÃO DE MOURA
12;EVENTO;0;2026-12-01;TER;14:00;15:40;Defesa de TCC;Coordenação
```

| Campo | Regra |
|---|---|
| `id_sala` | > 0 (o `id` da sala) |
| `atividade` | `AULA`, `MONITORIA`, `EXTENSAO`, `EVENTO` ou `OUTRO` |
| `recorrente` | `1` = toda semana, `0` = uma vez só |
| `data` | `AAAA-MM-DD`. Obrigatória quando `recorrente = 0`; pode ficar vazia quando `1` |
| `dia` | `SEG` `TER` `QUA` `QUI` `SEX` `SAB` `DOM` |
| `inicio`, `fim` | `HH:MM` (no código: minutos 0–1439); `fim` depois de `inicio` |
| `descricao`, `responsavel` | obrigatórios, até 49 bytes |

## Situações tratadas

| Situação | Resultado |
|---|---|
| Arquivo não existe (1ª execução) | `PERS_OK`, 0 registros, `rel.arquivo_ausente = 1` |
| Arquivo vazio | `PERS_OK`, 0 registros |
| Cabeçalho ausente ou de outro tipo/versão | `PERS_ERRO_FORMATO`, nada carregado |
| Linha inválida (campo faltando, data/hora impossível, id ou nome de sala repetido...) | linha ignorada e contada em `rel.linhas_invalidas` |
| Registro inválido ao salvar | `PERS_ERRO_DADO_INVALIDO`, arquivo **não** é alterado |
| Erro de disco | `PERS_ERRO_ABRIR`, `PERS_ERRO_LEITURA` ou `PERS_ERRO_ESCRITA` |

**Gravação segura:** cada `pers_salvar_*` escreve primeiro em `<arquivo>.tmp`,
força a ida ao disco, guarda a versão anterior em `<arquivo>.bak` e só então
troca o arquivo de uma vez (`rename` / `MoveFileEx`). Se o programa cair no
meio, o arquivo original continua inteiro. A versão anterior pode ser
recuperada com `pers_restaurar_backup()`.

## Testes

```
make test
```

Rodam 99 verificações cobrindo todos os critérios da issue #4. Entre elas,
carregam os dados reais de `dados_exemplo/` e conferem que salvar e carregar
de novo devolve exatamente os mesmos dados. O alvo `test` não precisa do GTK.

No Windows sem `make`, rode a partir da pasta `build/`:
```
gcc -std=c99 -Wall -I../include ../src/persistencia.c ../tests/test_persistencia.c -o test_persistencia.exe
.\test_persistencia.exe
```

## Observações para a equipe

Converter a planilha real **TURMA - HORÁRIO 2026.2** para as nossas structs
mostrou alguns pontos para discutir:

1. **`descricao[50]` é pequeno.** "DEPTO DISCIPLINA (TURMA)" não coube em 12
   das 794 aulas: 10 ficaram sem o departamento, 1 sem a turma e 1 teve o
   nome cortado (TÉCNICAS DE INTERVENÇÕES EM EDIFÍCIOS HISTÓRICOS). Sugestão: aumentar para 100, ou criar campos
   `turma` e `departamento`.
2. **`Reserva` não tem `id`.** Fica difícil editar ou apagar uma reserva
   específica (a interface vai precisar disso). Sugestão: adicionar `int id`.
3. **Duas turmas na mesma sala e horário:** a planilha tem 14 casos, como
   MICROCONTROLADORES e MICROPROCESSADORES na J-08, terça 20:30–22:10, com o
   mesmo professor. A checagem de conflito precisa permitir isso ou tratar
   como exceção.
4. **Os nomes das salas não são padronizados:** `LipECOMP` e `LIPECOMP`;
   `A-03` numa planilha e `A03` na outra. Em `dados_exemplo/` usei
   maiúsculas com hífen.
5. **A planilha não informa capacidade nem recursos** (projetor etc.). Em
   `dados_exemplo/salas.txt`, esses campos estão com 0.
6. Há aulas no sábado e horários fora dos blocos padrão (ex.: 19:40–21:20).
   O formato já aceita os dois casos.
