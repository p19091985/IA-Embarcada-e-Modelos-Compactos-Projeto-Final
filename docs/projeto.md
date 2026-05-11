# Jogo da Velha com IA Embarcada — ESP32-S3

## Visao geral

Este projeto implementa um jogo da velha embarcado no ESP32-S3 com ESP-IDF. A experiencia do usuario segue o jogo legado `EmuladorDeSerHumanoNoJogoDaVelhaByPatrikLimaPereira-Alpha0`: o jogador usa o teclado matricial, o OLED exibe menu/tabuleiro/placar/mensagens e o LCD1602 mostra o unico algoritmo de IA usado pelo computador na linha 1.

A camada de IA embarcada foi mantida sem mudar o fluxo visual do jogo: o computador usa somente um modelo TFLite Micro INT8 para escolher jogadas, enquanto o HC-SR04 alimenta um classificador TFLite de presenca executado em segundo plano. O modo tecnico escondido na tecla `9` coleta CSV bruto para evoluir o dataset proprio.

No boot, o console mostra uma abertura no estilo TensorFlow Lite Micro Hello World com o fluxo de treino, hashes, datasets, metricas INT8 e metadados das AIs embarcadas. Todas as telas do OLED tambem sao espelhadas no console por blocos `[OLED:...]`, para que a interface possa ser acompanhada pelo monitor serial mesmo quando o display do Wokwi nao estiver em foco.

## Funcionalidades

- Menu principal no OLED igual ao legado: `A - Jogar`, `B - Placar`, `C - Sair`, `D - About`, `0 - Zerar`, `Escolha`.
- Entrada por teclado matricial 4x4 para menu e selecao de posicoes.
- Tabuleiro 3x3 no OLED com o formato ` 1 | 2 | 3 ` e separadores `---+---+---`.
- Modelo TFLite Micro INT8 como unico algoritmo de IA do computador.
- Relatorios JSON de treino em `ml/relatorios/` com matriz de confusao, contrato INT8, hashes e rastreabilidade do PDF.
- LCD1602 com linha 1 exibindo `TFLite` e linha 2 rolando `Autores : Janiel e Patrik`.
- Placar acumulado de vitorias do jogador, vitorias do computador e empates.
- LED dourado controlado por `*` e `#`, com apoio automatico do LDR enquanto nao houver comando manual.
- Buzzer para tecla, inicializacao e vitoria.
- HC-SR04 com classificador TFLite de presenca e coleta CSV bruta em modo tecnico.
- Console serial com abertura Hello World/TinyML e espelho completo das telas OLED.

## Controles

| Tecla | Funcao |
| --- | --- |
| `A` | Iniciar partida |
| `B` | Exibir placar |
| `C` | Encerrar programa |
| `D` | Exibir About com `Janiel e Patrik` |
| `0` | Zerar placar |
| `1` a `9` | Selecionar posicao no tabuleiro |
| `*` | Ligar LED dourado |
| `#` | Desligar LED dourado |

Modo tecnico: no menu principal, a tecla `9` ativa a coleta CSV bruta do HC-SR04 para diagnostico. Essa opcao nao aparece no menu do OLED para preservar a experiencia do Alpha0.

O About no OLED identifica os autores como `Janiel e Patrik`. No LCD1602, a linha 1 permanece dedicada ao algoritmo de IA atual e a linha 2 rola `Autores : Janiel e Patrik` para a esquerda, em efeito de marquee.

## Hardware

| Componente | Sinal | GPIO ESP32-S3 |
| --- | --- | --- |
| Teclado | R1, R2, R3, R4 | 2, 3, 4, 5 |
| Teclado | C1, C2, C3, C4 | 6, 7, 8, 9 |
| LED azul | A | 11 |
| LED verde | A | 12 |
| LEDs dourados | A | 13 |
| OLED SSD1306 | SDA, SCL | 14, 15 |
| LCD1602 I2C | SDA, SCL | 16, 17 |
| Buzzers | Sinal | 18 |
| HC-SR04 | TRIG, ECHO | 19, 20 |
| LDR | AO | 10 |

## Arquitetura do firmware

| Arquivo | Responsabilidade |
| --- | --- |
| `main/main.c` | Inicializacao, menu legado, fluxo das partidas e modo tecnico de coleta |
| `main/jogo_da_velha.*` | Regras, tabuleiro, deteccao de vitoria e empate, placar |
| `main/ia_jogo_da_velha.*` | Enum e nome do unico algoritmo exibido: TFLite |
| `main/ia_tflite.*` | Inferencia TFLite Micro INT8 e mascara de casas ocupadas |
| `main/hcsr04.*` | Driver do sensor ultrassonico |
| `main/presenca_tflite.*` | Classificador TFLite de presenca |
| `main/ldr.*` | Leitura ADC do sensor de luminosidade |
| `main/teclado_matricial.*` | Leitura do teclado 4x4 |
| `main/leds.*` | Controle dos LEDs |
| `main/buzzer.*` | Sons via LEDC |
| `main/ssd1306_i2c.*` | Driver do OLED SSD1306 |
| `main/lcd1602_i2c.*` | Driver do LCD1602 I2C |

## Pipeline de IA

O projeto usa duas pipelines de IA, cobrindo coleta de dados de sensor, treinamento, conversao e compressao e pipeline de inferencia no dispositivo:

- **Modelo do jogo da velha**: `ml/pipeline_tictactoe.py` segue o fluxo do notebook `codigo/tflite_hello_world_training.ipynb`: gera dados, treina uma MLP com politica multi-alvo de jogadas minimax otimas, converte para TFLite, quantiza para INT8 e exporta o header do firmware. O minimax aparece apenas no treino offline para rotular/avaliar as melhores jogadas.
- **Classificador de presenca**: `ml/pipeline_presenca.py` usa `distancia_cm` e `eco_us` do HC-SR04, treina o limiar/modelo com dataset proprio, converte/comprime para TFLite INT8 e exporta `main/presenca_model_data.h` para inferencia embarcada em segundo plano.

As duas pipelines geram `ml/relatorios/tictactoe_training_report.json` e `ml/relatorios/presenca_training_report.json`. Esses arquivos documentam dataset, split treino/teste, matriz de confusao, amostras representativas de quantizacao, contrato `full_integer_int8`, tamanho, SHA-256 dos artefatos e a ligacao direta com as quatro exigencias do PDF: sensor, treinamento, compressao e inferencia no dispositivo.

## Checklist de validacao no Wokwi

- [ ] Menu legado aparece no OLED.
- [ ] Teclado 4x4 navega pelos comandos do menu.
- [ ] Tecla `A` inicia partida.
- [ ] Teclas `1` a `9` realizam jogadas no tabuleiro.
- [ ] LCD1602 exibe `TFLite` na linha 1 e a linha 2 rola `Autores : Janiel e Patrik`.
- [ ] Serial registra a inferencia de presenca do HC-SR04.
- [ ] Tecla `*` liga o LED dourado.
- [ ] Tecla `#` desliga o LED dourado.
- [ ] Tecla `B` exibe o placar.
- [ ] Tecla `D` exibe o autor.
- [ ] Tecla `0` zera o placar.
- [ ] Tecla `9`, no modo tecnico, coleta CSV do HC-SR04 pelo serial.
