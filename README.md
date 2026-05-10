# IA Embarcada e Modelos Compactos

Jogo da velha embarcado para ESP32-S3 com ESP-IDF.

O sistema combina jogador humano no teclado matricial 4x4, computador com IA, placar, OLED SSD1306, LCD1602 I2C, LEDs e buzzer no simulador Wokwi.

## O que faz

- Mostra menu e tabuleiro no OLED SSD1306.
- Usa o teclado 4x4 para controlar o menu e jogar.
- Mostra no LCD1602 o algoritmo de IA usado pelo computador.
- Controla LED dourado com `*` e `#`.
- Mantem placar de jogador, computador e empates.
- Usa buzzer para tecla, inicializacao e vitoria.

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

## Circuito

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

## Como compilar

```bash
. /home/patrik/.espressif/v6.0.1/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

Ou use o script:

```bash
./iniciar.sh
```

Comandos uteis do script:

```bash
./iniciar.sh build          # compila o firmware principal
./iniciar.sh validar        # compila firmware, roda pytest e compila testes Unity
./iniciar.sh simular        # valida, abre VS Code e mostra checklist do Wokwi
./iniciar.sh unity          # compila apenas o app Unity/ESP-IDF dos testes C
./iniciar.sh flash-testes   # grava o app Unity e abre o monitor serial
```

Para rodar a simulacao, use `./iniciar.sh simular` e execute `Wokwi: Start Simulator` no VS Code.

## Arquivos principais

- `main/main.c`: inicializacao, menu e fluxo do jogo.
- `main/jogo_da_velha.*`: regras e estado do tabuleiro.
- `main/ia_jogo_da_velha.*`: escolha de jogadas da IA.
- `main/teclado_matricial.*`: leitura do teclado 4x4.
- `main/ssd1306_i2c.*`: driver OLED SSD1306.
- `main/lcd1602_i2c.*`: driver LCD1602 I2C.
- `main/buzzer.*`: sons via LEDC.
- `main/leds.*`: controle dos LEDs.
- `diagram.json`: circuito Wokwi do sistema.
- `docs/projeto.md`: documentacao tecnica do projeto.
- `test/`: testes automatizados e teste Python do diagrama.

## Testes

```bash
./iniciar.sh validar
```

Os testes C em `test/` foram integrados a um app Unity/ESP-IDF separado:

```bash
./iniciar.sh unity
```

Para executar os testes Unity em placa real:

```bash
./iniciar.sh flash-testes
```

## Licenca

MIT - ver arquivo `LICENSE`.
