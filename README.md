# IA Embarcada e Modelos Compactos — Jogo da Velha no ESP32-S3

Sistema embarcado de jogo da velha desenvolvido para o microcontrolador ESP32-S3 com ESP-IDF. A arquitetura integra dois modelos TinyML quantizados em INT8, reconhecimento de gestos via acelerometro MPU6050, interface por teclado matricial 4x4, display OLED SSD1306, LCD1602 I2C, LEDs indicativos e buzzer sonoro, operando tanto no simulador Wokwi quanto em hardware real.

## Funcionalidades

O sistema oferece dois modos de interacao para o jogador humano:

- **Teclado direto**: a tecla `A` inicia uma partida onde as posicoes sao selecionadas pelas teclas `1` a `9`. O tabuleiro e desenhado no OLED no formato ` 1 | 2 | 3 ` com separadores `---+---+---`.
- **Gesto com auto-scan**: a tecla `8` inicia uma partida onde o cursor percorre automaticamente as casas livres no OLED e o jogador confirma a posicao desejada por um gesto detectado pelo MPU6050, mantendo o teclado como fallback paralelo.

O computador utiliza um modelo TFLite Micro INT8 treinado com dataset gerado por minimax para selecionar a melhor jogada. Caso a inferencia falhe, o sistema recorre ao minimax deterministico como fallback, exibindo no LCD1602 qual algoritmo foi empregado. O reconhecimento de gestos tambem utiliza um modelo INT8 treinado com dados de aceleracao, recorrendo a heuristica por limiares quando necessario.

A tecla `9` ativa o modo de coleta de dados do MPU6050 a 50 Hz, produzindo CSV via serial com o formato `timestamp_ms,ax,ay,az,label` para alimentar a pipeline de treinamento do modelo de gestos.

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

## Circuito

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

O OLED SSD1306 e o MPU6050 compartilham o barramento I2C_NUM_0 (GPIO14/SDA, GPIO15/SCL) a 400 kHz. O LCD1602 opera em barramento dedicado I2C_NUM_1 (GPIO16/SDA, GPIO17/SCL) a 100 kHz, evitando conflitos de inicializacao.

## Preparacao do ambiente

O unico pre-requisito externo e o [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) e o Python 3 com o modulo `venv`. Na primeira execucao, o script `iniciar.sh` cria automaticamente um ambiente virtual em `.venv/`, instala as dependencias Python do `requirements.txt` e localiza a instalacao do ESP-IDF.

```bash
./iniciar.sh setup          # verifica todas as dependencias e exibe o status
```

Caso o ESP-IDF esteja instalado em local diferente do padrao, defina a variavel antes de executar:

```bash
export IDF_PATH=/caminho/para/esp-idf
./iniciar.sh validar
```

## Compilacao e simulacao

```bash
./iniciar.sh                # abre o menu interativo
./iniciar.sh build          # compila o firmware principal
./iniciar.sh validar        # compila firmware, executa pytest e compila testes Unity
./iniciar.sh simular        # valida, abre VS Code e exibe checklist do Wokwi
./iniciar.sh unity          # compila o app Unity/ESP-IDF dos testes C
./iniciar.sh flash-testes   # grava o app Unity na placa e abre monitor serial
```

Para simular, execute `./iniciar.sh simular` e rode o comando `Wokwi: Start Simulator` no VS Code.

Para compilacao manual:

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
```

## Arquivos principais

| Arquivo | Responsabilidade |
| --- | --- |
| `main/main.c` | Inicializacao, menu e fluxo das partidas |
| `main/jogo_da_velha.*` | Regras, tabuleiro, deteccao de vitoria e empate, placar |
| `main/ia_jogo_da_velha.*` | Minimax e selecao de algoritmo para a jogada do computador |
| `main/ia_tflite.*` | Inferencia TFLite Micro INT8, mascara de casas ocupadas e fallback minimax |
| `main/mpu6050.*` | Driver I2C do acelerometro MPU6050 |
| `main/serie_temporal.*` | Buffer circular e extracao de features das leituras do MPU6050 |
| `main/gesto.*` | Heuristica de deteccao de gesto com limiar e debounce |
| `main/gesto_tflite.*` | Inferencia TFLite Micro INT8 para classificacao do gesto |
| `main/coleta_mpu6050.*` | Task FreeRTOS de coleta CSV a 50 Hz |
| `main/auto_scan.*` | Cursor automatico que percorre casas livres no OLED |
| `ml/pipeline_gestos.py` | Geracao de janelas, treinamento, quantizacao INT8 e exportacao do modelo de gestos |
| `ml/pipeline_tictactoe.py` | Dataset por minimax, treinamento, quantizacao INT8 e exportacao do modelo do jogo |
| `main/teclado_matricial.*` | Leitura do teclado 4x4 |
| `main/ssd1306_i2c.*` | Driver do OLED SSD1306 |
| `main/lcd1602_i2c.*` | Driver do LCD1602 I2C |
| `main/buzzer.*` | Sons via LEDC |
| `main/leds.*` | Controle dos LEDs |
| `diagram.json` | Circuito Wokwi completo |
| `docs/projeto.md` | Documentacao tecnica do projeto |
| `test/` | Testes automatizados (C Unity e Python pytest) |

## Acelerometro MPU6050

O MPU6050 e o componente central da pipeline de IA embarcada neste projeto. Ele conecta o mundo fisico ao sistema de inferencia, fornecendo os dados de aceleracao que alimentam tanto a coleta de datasets quanto a deteccao de gestos em tempo real. O sensor participa de cinco camadas do firmware:

```text
MPU6050 (I2C) → Driver → Coleta CSV → Serie Temporal → Gesto/TFLite → Acao no Jogo
```

### Hardware e barramento

O MPU6050 opera no endereco I2C `0x68` e compartilha o barramento `I2C_NUM_0` com o OLED SSD1306 nos pinos GPIO14 (SDA) e GPIO15 (SCL) a 400 kHz. O driver inicializa o sensor escrevendo `0x00` no registrador `PWR_MGMT_1` (`0x6B`) para acorda-lo do modo sleep. A faixa padrao de medicao e ±2g, onde o valor bruto `16384` corresponde a aproximadamente 1g.

Se o sensor nao responder durante a inicializacao, o firmware exibe um aviso no OLED e prossegue normalmente — todos os modos por teclado continuam funcionais.

### API do driver

```c
/* Inicializa o sensor no barramento I2C ja configurado pelo OLED */
esp_err_t mpu6050_iniciar(mpu6050_t *mpu, i2c_master_bus_handle_t barramento);

/* Le os 3 eixos de aceleracao (valores brutos int16_t) */
esp_err_t mpu6050_ler_aceleracao(mpu6050_t *mpu, int16_t *ax, int16_t *ay, int16_t *az);

/* Utilitarios de conversao (expostos para teste) */
int16_t mpu6050_unir_bytes(uint8_t byte_alto, uint8_t byte_baixo);
esp_err_t mpu6050_converter_bytes_aceleracao(const uint8_t bytes[6], int16_t *ax, int16_t *ay, int16_t *az);
```

### Coleta de dados (tecla `9`)

O modo de coleta produz um dataset CSV via serial a 50 Hz (20 ms entre leituras), utilizando uma task FreeRTOS com `vTaskDelayUntil` para garantir periodicidade estavel.

**Procedimento de coleta:**

1. Pressionar `9` no menu principal.
2. O OLED exibe `COLETA MPU` e o serial imprime o cabecalho `timestamp_ms,ax,ay,az,label`.
3. Pressionar `0` para definir label repouso ou `1` para definir label confirmacao.
4. Movimentar o sensor de acordo com o gesto desejado durante a coleta.
5. Pressionar `D` para encerrar e voltar ao menu.
6. Copiar a saida serial e salvar como `ml/datasets/gestos_raw.csv`.

**Formato CSV produzido:**

```text
timestamp_ms,ax,ay,az,label
0,16,-20,16384,0
20,25,-15,16390,0
40,3500,-1800,16200,1
```

### Serie temporal e features

As leituras do MPU6050 alimentam um buffer circular de 16 amostras (`serie_temporal_t`), que calcula automaticamente:

- **Amplitude** por eixo (max - min dentro da janela).
- **Maior amplitude** entre os tres eixos.
- **Energia delta** (soma das variacoes absolutas entre amostras consecutivas).
- **Media absoluta do delta** (energia normalizada pelo numero de transicoes).

Essas features sao utilizadas tanto pela heuristica de gesto quanto pelo pre-processamento INT8 que alimenta o modelo TFLite.

### Deteccao de gesto (tecla `8`)

Na partida com auto-scan, o sistema processa cada leitura do MPU6050 pela seguinte cadeia:

1. `mpu6050_ler_aceleracao()` obtem ax, ay, az brutos.
2. `serie_temporal_adicionar()` insere no buffer circular.
3. Quando o buffer esta cheio (16 amostras), o classificador e invocado.
4. Se o modelo TFLite estiver pronto, `gesto_tflite_classificar()` executa a inferencia INT8.
5. Se a inferencia nao estiver disponivel, `gesto_heuristica_detectar()` verifica se a maior amplitude e a media delta ultrapassam os limiares configurados (3500 e 700).
6. Um debounce de 700 ms impede disparos multiplos do mesmo gesto.
7. O evento `GESTO_EVENTO_CONFIRMAR` confirma a casa destacada pelo cursor automatico.

### Pipeline de treinamento

O CSV coletado e processado pela pipeline `ml/pipeline_gestos.py`:

```bash
python3 ml/pipeline_gestos.py --raw ml/datasets/gestos_raw.csv
```

O script transforma o CSV bruto em janelas deslizantes de 16 amostras × 3 eixos = 48 features, treina uma rede densa binaria com Keras, converte para TFLite float, aplica quantizacao INT8 com dataset representativo e exporta o array C em `main/gesto_model_data.h` para inclusao direta no firmware.

Quando o CSV bruto nao esta disponivel (ambiente de simulacao), o parametro `--gerar-exemplo` produz dados por modelagem algoritmica com ruido gaussiano e variacao de amplitude para validacao da pipeline.

### Testes do MPU6050

O projeto possui testes abrangentes cobrindo todas as camadas do MPU6050:

| Teste | O que valida |
| --- | --- |
| `test_mpu6050_constantes_do_driver` | Endereco I2C, registradores e frequencia |
| `test_mpu6050_converte_bytes_assinados` | 13 cenarios de conversao big-endian incluindo zero, ±1, ±32768, gravidade e fronteira do bit de sinal |
| `test_mpu6050_converte_vetor_de_aceleracao` | 6 cenarios de vetor: misto, zeros, extremos, 0xFF, repouso tipico e ordenacao big-endian |
| `test_mpu6050_rejeita_argumentos_invalidos` | NULL em cada ponteiro individualmente e struct sem dispositivo |
| `test_coleta_mpu6050_cabecalho_e_taxa` | Cabecalho CSV, frequencia 50 Hz e consistencia periodo/frequencia |
| `test_coleta_mpu6050_labels_aceitos` | Labels 0 e 1 aceitos; -1, 2, 99, -100 rejeitados |
| `test_coleta_mpu6050_formata_linha_csv_com_negativos` | 4 cenarios CSV: misto, todos negativos, extremos int16, zeros |
| `test_coleta_mpu6050_rejeita_linha_csv_invalida` | NULL, tamanho zero, label invalido e buffer overflow |
| `test_coleta_mpu6050_configura_estado_inicial` | Ciclo completo: configurar, trocar labels, rejeitar invalido, parar, NULL safety |
| `test_coleta_mpu6050_pipeline_bytes_ate_csv` | Fluxo ponta a ponta: bytes brutos → int16_t → linha CSV formatada |
| `test_serie_temporal_*` | Buffer circular, features e pre-processamento INT8 |
| `test_gesto_*` | Heuristica, debounce e deteccao |
| `test_gesto_tflite_*` | Inferencia TFLite, classificacao e fallback |

## Testes

```bash
./iniciar.sh validar
```

Os testes C em `test/` estao integrados a um app Unity/ESP-IDF compilado separadamente. Os testes Python validam a estrutura do diagrama Wokwi, as pipelines de ML e a conformidade dos datasets gerados.

O MPU6050 e o componente com maior cobertura de testes no projeto, validando desde a conversao de bytes individuais ate o fluxo completo de bytes brutos do sensor passando por conversao, formatacao CSV e deteccao de gesto.

## Licenca

MIT — ver arquivo `LICENSE`.
