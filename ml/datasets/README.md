# Datasets

Esta pasta armazena os CSVs utilizados pelas pipelines de treinamento.

As pipelines completas gravam relatorios em `ml/relatorios/` com auditoria dos datasets, split treino/teste, matriz de confusao, amostras representativas usadas na quantizacao e ligacao com os requisitos do PDF do projeto final.

## Presenca (HC-SR04)

O dataset do classificador de presenca usa leituras do sensor ultrassonico:

```bash
python3 ml/pipeline_presenca.py --sem-treino
```

Arquivo gerado: `presenca_hcsr04.csv`.

Formato:

```text
distancia_cm,eco_us,label
```

Mapeamento:

- `0`: jogador ausente/distante.
- `1`: jogador presente/proximo.

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
- `best_move`: indice `0..8` de uma melhor casa para o computador. Durante o treino, a pipeline tambem cria uma politica multi-alvo com todas as jogadas minimax otimas para nao penalizar empates entre jogadas igualmente corretas.
