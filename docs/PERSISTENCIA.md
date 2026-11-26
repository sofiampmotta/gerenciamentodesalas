# Módulo de persistência

Salva e carrega as **salas** (`Sala`) e as **reservas** (`Reserva`) definidas
em `include/salas.h`, usando arquivos CSV locais com campos separados por `;`.

| Arquivo | Conteúdo |
|---|---|
| `include/persistencia.h` | funções que os outros módulos chamam |
| `src/persistencia.c` | implementação |
| `tests/test_persistencia.c` | testes (`make test`) |
| `dados_exemplo/` | 42 salas e 794 aulas reais de 2026.2 |

## Como usar

No programa, quem chama o módulo é o `src/aplicacao.c`: ele carrega as listas
ao abrir e salva depois de cada inclusão, edição ou exclusão. O exemplo abaixo
mostra as chamadas diretamente:

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

## Formato CSV

- Dá para abrir e conferir em planilhas e editores de texto.
- Funciona igual em Windows e Linux. Com `fwrite` da struct, o arquivo depende
  do compilador (padding, tamanho de `bool` e de `int`).
- Uma linha corrompida é ignorada, e o resto do arquivo continua sendo lido.
- Se uma struct mudar, troque a versão no cabeçalho e mantenha leitura
  compatível com versões anteriores quando os dados puderem ser migrados.

## Formato

Regras gerais:
- UTF-8, uma linha por registro, campos separados por `;`.
- A primeira linha é um cabeçalho com tipo e versão.
- Linhas em branco e linhas começando com `#` são ignoradas.
- Textos não podem conter `;` nem quebra de linha; salvar recusa esses
  registros. **Os módulos de cadastro devem validar isso na entrada.**
- Tamanhos de texto contam **bytes**: letra acentuada ocupa 2.
- Arquivos do Windows (`\r\n`, BOM do Bloco de Notas) são aceitos.

### `salas.csv`

```
#POLI-SALAS;SALAS;v2
id;nome;capacidade;acessivel;projetor;televisao;computadores;local
1;A-03;45;1;1;0;0;Bloco A
36;LIP-03;30;0;1;1;25;Laboratório
```

| Campo | Regra |
|---|---|
| `id` | > 0, único |
| `nome` | obrigatório, único (sem diferenciar maiúsc./minúsc.), até 19 bytes |
| `capacidade`, `computadores` | inteiro ≥ 0 |
| `acessivel`, `projetor`, `televisao` | `0` ou `1` |
| `local` | opcional, até 127 bytes |

Arquivos `salas.csv` `v1` continuam sendo lidos; o campo `local` começa vazio.
Ao salvar, o arquivo é gravado no formato `v2`.

### `reservas.csv`

```
#POLI-SALAS;RESERVAS;v3
id;id_sala;atividade;recorrente;data_inicio;fim_recorrencia;dia;inicio;fim;descricao;responsavel
1;1;AULA;1;2026-11-16;2026-12-31;SEG;07:10;08:50;DEC METODOLOGIA DA PESQUISA (AM);MICAELLA RAISSA FALCÃO DE MOURA
795;12;EVENTO;0;2026-12-01;;TER;14:00;15:40;Defesa de TCC;Coordenação
```

| Campo | Regra |
|---|---|
| `id` | > 0, único (é por ele que se edita ou exclui uma reserva) |
| `id_sala` | > 0 (o `id` da sala) |
| `atividade` | `AULA`, `MONITORIA`, `EXTENSAO`, `EVENTO` ou `OUTRO` |
| `recorrente` | `1` = uma série semanal, `0` = uma vez só; a série ocupa um único registro e ID |
| `data_inicio` | `AAAA-MM-DD`; início da série ou data da reserva pontual |
| `fim_recorrencia` | Para séries, 31/12 do ano da data inicial; vazio para reserva pontual |
| `dia` | `SEG` `TER` `QUA` `QUI` `SEX` `SAB` `DOM`; para séries, deve corresponder à data inicial |
| `inicio_hora`, `fim_hora` | `HH:MM` (no código: minutos 0–1439); fim depois do início |
| `descricao` | obrigatória, até 49 bytes |
| `responsavel` | obrigatório, até 79 bytes |

## Situações tratadas

| Situação | Resultado |
|---|---|
| Arquivo não existe (1ª execução) | `PERS_OK`, 0 registros, `rel.arquivo_ausente = 1` |
| Arquivo vazio | `PERS_OK`, 0 registros |
| Cabeçalho ausente ou de outro tipo/versão | `PERS_ERRO_FORMATO`, nada carregado |
| `reservas.csv` antigo (`v1`, sem o `id`) | `PERS_ERRO_FORMATO` |
| Reservas `v2` | Lidas e migradas em memória; séries sem data inicial usam 13/11/2026 como âncora e avançam até o próximo dia semanal. Se havia data, ela é alinhada ao dia semanal. O próximo salvamento grava `v3`. |
| Linha inválida (campo faltando, data/hora impossível, id repetido, nome de sala repetido...) | linha ignorada e contada em `rel.linhas_invalidas` |
| Registro inválido ao salvar | `PERS_ERRO_DADO_INVALIDO`, arquivo **não** é alterado |
| Erro de disco | `PERS_ERRO_ABRIR`, `PERS_ERRO_LEITURA` ou `PERS_ERRO_ESCRITA` |

**Gravação segura:** cada `pers_salvar_*` escreve primeiro em `<arquivo>.tmp`,
força a ida ao disco, guarda a versão anterior em `<arquivo>.bak` e só então
troca o arquivo de uma vez (`rename` / `MoveFileEx`). Se o programa cair no
meio, o arquivo original continua inteiro. A versão anterior pode ser
recuperada com `pers_restaurar_backup()`.

**Migração de TXT:** ao iniciar, se `salas.csv` ou `reservas.csv` estiver
ausente, o serviço procura o arquivo TXT homônimo. Se ele for válido, grava
uma cópia CSV com o mesmo esquema e mantém o TXT intacto. Se houver linhas
inválidas, cancela a migração e informa o erro em vez de gravar uma cópia
parcial. Os dados de exemplo também são carregados dos arquivos `.csv`.

## Exportação pela interface

Na tela **Visão**, as opções de exportação usam o dia e os filtros atualmente
exibidos. O CSV usa `;` como separador, inclui o horário legível, atividade,
responsável, sala e indicador de recorrência, escapando aspas e campos com
separadores. O PDF é formatado em páginas A4 e registra também o dia e os
filtros aplicados. Ambos abrem uma janela para escolher o destino.

## Testes

```
make test
```

As verificações cobrem os critérios da issue #4, migração de recorrências,
leitura/gravação dos dados reais de `dados_exemplo/` e geração de CSV/PDF. O
alvo `test` precisa das dependências GTK 4, Cairo e Pango usadas pela GUI e
pelas exportações.

No Windows sem `make`, rode os testes de persistência a partir da pasta `build/`:
```
gcc -std=c99 -Wall -I../include ../src/persistencia.c ../src/alocacao.c ../tests/test_persistencia.c -o test_persistencia.exe
.\test_persistencia.exe
```

## Observações para a equipe

Converter a planilha real **TURMA - HORÁRIO 2026.2** para as nossas structs
mostrou alguns pontos para discutir:

1. **`descricao[50]` é pequeno.** "DEPTO DISCIPLINA (TURMA)" não coube em 12
   das 794 aulas: 10 ficaram sem o departamento, 1 sem a turma e 1 teve o
   nome cortado (TÉCNICAS DE INTERVENÇÕES EM EDIFÍCIOS HISTÓRICOS). Sugestão: aumentar para 100, ou criar campos
   `turma` e `departamento`.
2. ~~**`Reserva` não tem `id`.**~~ Resolvido: o `id` foi adicionado à struct
   e o arquivo de reservas passou para a versão `v2`, com o `id` no primeiro campo.
3. **Duas turmas na mesma sala e horário:** a planilha tem 14 casos, como
   MICROCONTROLADORES e MICROPROCESSADORES na J-08, terça 20:30–22:10, com o
   mesmo professor. A checagem de conflito precisa permitir isso ou tratar
   como exceção.
4. **Os nomes das salas não são padronizados:** `LipECOMP` e `LIPECOMP`;
   `A-03` numa planilha e `A03` na outra. Em `dados_exemplo/` usei
   maiúsculas com hífen.
5. **A planilha não informa capacidade nem recursos** (projetor etc.). Em
   `dados_exemplo/salas.csv`, esses campos estão com 0.
6. Há aulas no sábado e horários fora dos blocos padrão (ex.: 19:40–21:20).
   O formato já aceita os dois casos.
