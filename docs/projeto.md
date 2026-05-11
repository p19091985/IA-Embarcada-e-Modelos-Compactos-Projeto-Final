# Jogo da Velha com IA Embarcada — ESP32-S3

## Visao geral

Este projeto implementa um sistema de jogo da velha embarcado no microcontrolador ESP32-S3 utilizando o framework ESP-IDF. A interface combina teclado matricial 4x4, display OLED SSD1306, LCD1602 I2C, LEDs indicativos e buzzer sonoro. O acelerometro MPU6050 possibilita tanto a coleta de dados para treinamento de modelos quanto a interacao por gestos durante a partida.

O jogador seleciona posicoes pelo teclado ou por gesto, enquanto o computador responde utilizando inferencia TFLite Micro em modelos INT8 quantizados, com fallback automatico para minimax deterministico quando a inferencia nao esta disponivel.

## Funcionalidades

- Menu principal e tabuleiro 3x3 exibidos no OLED SSD1306.
- Entrada por teclado matricial 4x4 para controle do menu e selecao de posicoes.
- Modo de gesto com auto-scan via MPU6050, mantendo o teclado como fallback paralelo.
- Modelos TinyML INT8 para selecao da jogada do computador e classificacao de gestos.
- Fallback automatico para minimax (jogada) e heuristica por limiares (gesto) em caso de falha.
- LCD1602 exibindo o algoritmo de IA utilizado em cada jogada do computador.
- Placar acumulado de vitorias do jogador, vitorias do computador e empates.
- LED dourado controlado por teclado.
- Buzzer para eventos de tecla, inicializacao e vitoria.
- Coleta de dados do MPU6050 a 50 Hz em formato CSV via serial.

## Controles

| Tecla | Funcao |
| --- | --- |
| `A` | Iniciar partida por teclado |
| `B` | Exibir placar |
| `C` | Encerrar programa |
| `D` | Exibir autor |
| `0` | Zerar placar |
| `1` a `9` | Selecionar posicao no tabuleiro |
| `8` | Iniciar partida com gesto e auto-scan |
| `9` | Ativar coleta CSV do MPU6050 |
| `*` | Ligar LED dourado |
| `#` | Desligar LED dourado |

Na partida por teclado, o OLED desenha o tabuleiro com o formato ` 1 | 2 | 3 ` e separadores `---+---+---`, e o jogador indica a posicao desejada pelas teclas `1` a `9`.

## Hardware

| Componente | Sinal | GPIO ESP32-S3 |
| --- | --- | --- |
| Teclado | R1, R2, R3, R4 | 2, 3, 4, 5 |
| Teclado | C1, C2, C3, C4 | 6, 7, 8, 9 |
| LED azul | A | 11 |
| LED verde | A | 12 |
| LEDs dourados | A | 13 |
| OLED SSD1306 | SDA, SCL | 14, 15 |
| MPU6050 | SDA, SCL | 14, 15 |
| LCD1602 I2C | SDA, SCL | 16, 17 |
| Buzzers | Sinal | 18 |

### Barramentos I2C

| Barramento | Uso | Pinos | Frequencia |
| --- | --- | --- | --- |
| `I2C_NUM_0` | OLED SSD1306 e MPU6050 | SDA GPIO14, SCL GPIO15 | 400 kHz |
| `I2C_NUM_1` | LCD1602 | SDA GPIO16, SCL GPIO17 | 100 kHz |

## Acelerometro MPU6050

O acelerometro MPU6050 compartilha o barramento I2C_NUM_0 com o OLED SSD1306, utilizando o endereco `0x68`. O driver implementado em `main/mpu6050.c` efetua a inicializacao do sensor atraves da escrita no registrador `PWR_MGMT_1` e realiza leituras dos 6 bytes de aceleracao a partir do registrador `ACCEL_XOUT_H` (`0x3B`), convertendo-os para `int16_t` nos tres eixos (ax, ay, az).

### Fluxo de utilizacao

1. **Inicializacao**: a funcao `mpu6050_iniciar()` registra o dispositivo no barramento e acorda o sensor. Se houver falha, o firmware exibe aviso no OLED e prossegue normalmente pelo teclado.
2. **Leitura**: `mpu6050_ler_aceleracao()` retorna os valores brutos dos tres eixos, onde ±16384 equivale a aproximadamente 1g na faixa padrao de ±2g.
3. **Coleta CSV**: a tecla `9` inicia a task `mpu_data_collection_task` a 50 Hz (20 ms entre leituras), imprimindo no serial o cabecalho `timestamp_ms,ax,ay,az,label`. As teclas `0` e `1` definem o label durante a coleta e `D` encerra.
4. **Gesto**: a tecla `8` ativa a partida com auto-scan, onde a funcao `gesto_tflite_classificar()` analisa janelas de 16 amostras da serie temporal e gera eventos de confirmacao quando detecta o padrao treinado.

Caso o sensor nao responda, o sistema permanece funcional em todos os modos que nao dependem do acelerometro.

## Arquitetura do firmware

Todo o firmware da aplicacao reside no diretorio `main/`.

| Arquivo | Responsabilidade |
| --- | --- |
| `main/main.c` | Inicializacao, menu e fluxo das partidas |
| `main/jogo_da_velha.*` | Regras, tabuleiro, deteccao de vitoria e empate, placar |
| `main/ia_jogo_da_velha.*` | Minimax e selecao do algoritmo de jogada do computador |
| `main/ia_tflite.*` | Inferencia TFLite Micro INT8, mascara de casas ocupadas e fallback minimax |
| `main/mpu6050.*` | Driver I2C do acelerometro MPU6050 |
| `main/serie_temporal.*` | Buffer circular e extracao de features das leituras do sensor |
| `main/gesto.*` | Heuristica de deteccao de gesto com limiar e debounce |
| `main/gesto_tflite.*` | Inferencia TFLite Micro INT8 para classificacao do gesto |
| `main/coleta_mpu6050.*` | Task FreeRTOS de coleta CSV a 50 Hz |
| `main/auto_scan.*` | Cursor automatico que percorre casas livres no OLED |
| `ml/pipeline_gestos.py` | Geracao de janelas, treinamento, quantizacao INT8 e exportacao do modelo de gestos |
| `ml/pipeline_tictactoe.py` | Dataset por minimax, treinamento, quantizacao INT8 e exportacao do modelo do jogo |
| `main/teclado_matricial.*` | Leitura do teclado 4x4 |
| `main/leds.*` | Controle dos LEDs |
| `main/buzzer.*` | Sons via LEDC |
| `main/ssd1306_i2c.*` | Driver do OLED SSD1306 |
| `main/lcd1602_i2c.*` | Driver do LCD1602 I2C |

## Pipeline de IA

O projeto implementa duas pipelines de TinyML:

**Modelo de gestos (MPU6050)**: o CSV bruto coletado pelo firmware e transformado em janelas deslizantes de 16 amostras com 3 eixos (48 features), rotuladas como repouso (0) ou confirmacao (1). Uma rede densa binaria e treinada com Keras, convertida para TFLite float e quantizada para INT8 com dataset representativo. O modelo final e exportado como array C em `main/gesto_model_data.h` e executado pelo interpretador TFLite Micro no ESP32-S3.

**Modelo do jogo da velha**: um dataset e gerado programaticamente atraves do algoritmo minimax, enumerando todos os estados legais do tabuleiro em que e a vez do computador e registrando a melhor jogada como label. Uma MLP com duas camadas de 32 neuronios e treinada para aproximar essa funcao de decisao, passando pelo mesmo fluxo de conversao e quantizacao INT8. Na inferencia embarcada, casas ocupadas sao mascaradas na saida para garantir jogadas validas.

## Compilacao e simulacao

```bash
./iniciar.sh
./iniciar.sh build
./iniciar.sh validar
./iniciar.sh simular
```

Para simular, execute `./iniciar.sh simular` e, no VS Code, rode o comando `Wokwi: Start Simulator`.

## Testes

O projeto possui testes de host e um app Unity/ESP-IDF para validacao em hardware.

```bash
./iniciar.sh validar       # pytest + compilacao firmware + compilacao Unity
./iniciar.sh unity         # apenas compilacao do app Unity
./iniciar.sh flash-testes  # grava o app Unity na placa e abre monitor serial
```

## Checklist de validacao no Wokwi

- [ ] Menu aparece no OLED.
- [ ] Teclado 4x4 navega pelos comandos do menu.
- [ ] Tecla `A` inicia partida por teclado.
- [ ] Teclas `1` a `9` realizam jogadas no tabuleiro.
- [ ] LCD1602 exibe o algoritmo de IA utilizado na jogada do computador.
- [ ] Tecla `*` liga o LED dourado.
- [ ] Tecla `#` desliga o LED dourado.
- [ ] Buzzer toca nos eventos principais.
- [ ] Partida completa com vitoria do jogador.
- [ ] Partida completa com vitoria do computador.
- [ ] Partida completa com empate.
