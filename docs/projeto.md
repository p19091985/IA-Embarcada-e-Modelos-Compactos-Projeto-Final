# Jogo da Velha ESP32-S3

## Visao geral

Este projeto implementa um jogo da velha embarcado para ESP32-S3 usando ESP-IDF. A interface combina teclado matricial 4x4, OLED SSD1306, LCD1602 I2C, LEDs e buzzer no simulador Wokwi.

O jogador usa o teclado para navegar pelo menu e escolher as posicoes do tabuleiro. O computador responde com algoritmos de IA, e o LCD mostra qual estrategia foi usada na jogada.

## Funcionalidades

- Menu principal no OLED.
- Tabuleiro 3x3 no OLED.
- Entrada por teclado matricial 4x4.
- Computador com IA para escolher jogadas.
- Placar de vitorias do jogador, vitorias do computador e empates.
- LCD1602 mostrando o algoritmo de IA usado.
- LED dourado controlado por teclado.
- Buzzer para eventos de tecla, inicializacao e vitoria.

## Controles

| Tecla | Acao |
| --- | --- |
| `A` | Iniciar partida |
| `B` | Mostrar placar |
| `C` | Finalizar programa |
| `D` | Mostrar autor |
| `0` | Zerar placar |
| `1` a `9` | Jogar em uma casa do tabuleiro |
| `*` | Ligar LED dourado |
| `#` | Desligar LED dourado |

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

Barramentos I2C:

| Barramento | Uso | Pinos | Frequencia |
| --- | --- | --- | --- |
| `I2C_NUM_0` | OLED SSD1306 | SDA GPIO14, SCL GPIO15 | 400 kHz |
| `I2C_NUM_1` | LCD1602 | SDA GPIO16, SCL GPIO17 | 100 kHz |

## Arquitetura

Todo o firmware da aplicacao fica em `main`.

| Arquivo | Responsabilidade |
| --- | --- |
| `main/main.c` | Inicializacao, menu e fluxo da partida |
| `main/jogo_da_velha.*` | Regras, tabuleiro, vitoria, empate e placar |
| `main/ia_jogo_da_velha.*` | Algoritmos e escolha da jogada do computador |
| `main/teclado_matricial.*` | Leitura do teclado 4x4 |
| `main/leds.*` | Controle dos LEDs |
| `main/buzzer.*` | Sons via LEDC |
| `main/ssd1306_i2c.*` | Driver do OLED SSD1306 |
| `main/lcd1602_i2c.*` | Driver do LCD1602 I2C |

## Build e simulacao

Use o script principal:

```bash
./iniciar.sh
```

Comandos diretos:

```bash
./iniciar.sh build
./iniciar.sh validar
./iniciar.sh simular
```

Para simular, execute `./iniciar.sh simular` e, no VS Code, rode o comando:

```text
Wokwi: Start Simulator
```

## Testes

O projeto possui testes de host para o diagrama e um app Unity/ESP-IDF para os testes C.

```bash
./iniciar.sh validar
./iniciar.sh unity
```

Para executar o app Unity em uma placa real:

```bash
./iniciar.sh flash-testes
```

## Checklist de validacao no Wokwi

- [ ] Menu aparece no OLED.
- [ ] Teclado 4x4 navega pelos comandos do menu.
- [ ] Tecla `A` inicia partida.
- [ ] Teclas `1` a `9` realizam jogadas no tabuleiro.
- [ ] LCD1602 mostra o algoritmo de IA usado na jogada do computador.
- [ ] Tecla `*` liga o LED dourado.
- [ ] Tecla `#` desliga o LED dourado.
- [ ] Buzzer toca nos eventos principais.
- [ ] Partida completa com vitoria do jogador.
- [ ] Partida completa com vitoria do computador.
- [ ] Partida completa com empate.
