# Datasets

Esta pasta armazena os CSVs utilizados pelas pipelines de treinamento.

## Gestos (MPU6050)

Formato do CSV bruto produzido pelo firmware no modo de coleta (tecla `9`):

```text
timestamp_ms,ax,ay,az,label
```

Labels utilizados:

- `0`: repouso ou movimento normal.
- `1`: gesto de confirmacao de jogada.

O CSV bruto e transformado em janelas deslizantes pela pipeline `ml/pipeline_gestos.py`, gerando o arquivo `gestos_janelas.csv` com 48 features por janela (16 amostras x 3 eixos).

Procedimento de coleta:

1. Iniciar o firmware no simulador Wokwi.
2. Pressionar `9` no menu para ativar o modo de coleta.
3. Capturar a saida serial a partir do cabecalho CSV.
4. Tecla `0` define label repouso; tecla `1` define label confirmacao.
5. Tecla `D` encerra a coleta.

Pipeline completa:

```bash
python3 ml/pipeline_gestos.py --raw ml/datasets/gestos_raw.csv
```

Quando o CSV bruto nao esta disponivel, o parametro `--gerar-exemplo` produz dados por modelagem algoritmica para validacao da pipeline.

## Jogo da Velha

O dataset do modelo de selecao de jogadas e gerado programaticamente via minimax:

```bash
python3 ml/pipeline_tictactoe.py --sem-treino
```

Arquivo gerado: `dataset_tictactoe.csv`.

Formato:

```text
c0,c1,c2,c3,c4,c5,c6,c7,c8,best_move
```

Mapeamento:

- `1`: computador (`X` no firmware).
- `-1`: jogador (`O` no firmware).
- `0`: casa vazia.
- `best_move`: indice `0..8` da melhor casa para o computador.
