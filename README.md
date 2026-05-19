# Projeto Final: Jogo da Velha com IA Embarcada no ESP32-S3

**Autores:** Janiel - Joao Sanmartin - Patrik

---

## 1. Introdução

Este projeto implementa um jogo da velha interativo no ESP32-S3 onde o computador joga usando exclusivamente uma **rede neural TFLite Micro INT8 embarcada** — sem Minimax em tempo real, sem servidor remoto. A IA vive dentro do chip.

Além do jogo, um segundo modelo TFLite classifica a presença do jogador via sensor HC-SR04 em background. O hardware inclui teclado matricial 4×4, displays OLED e LCD1602, LEDs de status, sensor LDR e buzzer. O projeto roda tanto em bancada física quanto no simulador Wokwi (VS Code).

---

## 2. Funcionalidades

| Funcionalidade | Descrição |
| --- | --- |
| **Jogo da velha** | Jogador usa teclado 4×4; computador responde via rede neural |
| **OLED SSD1306** | Exibe tabuleiro, menus, placar e créditos |
| **LCD1602** | Mostra o algoritmo de IA ativo em tempo real |
| **Classificador de presença** | HC-SR04 + TFLite Micro rodando em segundo plano |
| **Controle de iluminação** | LDR acende LED dourado automaticamente; teclas `*`/`#` controlam manualmente |
| **Buzzer PWM** | Feedback sonoro em teclas, inicialização e vitória |
| **Console serial** | Espelha o OLED e exibe métricas dos modelos ao ligar |

---

## 3. Controles do Teclado

| Tecla | Função |
| --- | --- |
| `A` | Nova partida |
| `1` a `9` | Escolhe posição no tabuleiro |
| `B` | Placar atual |
| `0` | Zera o placar |
| `D` | Créditos / autores |
| `C` | Encerrar |
| `*` | LED dourado ligado |
| `#` | LED dourado desligado |
| `9` | (Técnico) Coleta CSV bruta do HC-SR04 no console |

---

## 4. Hardware

### Circuito completo (bancada física)

| Componente | Pino no ESP32-S3 |
| --- | --- |
| Teclado — linhas | R1=2, R2=3, R3=4, R4=5 |
| Teclado — colunas | C1=6, C2=7, C3=8, C4=9 |
| LDR (analógico) | ADC GPIO 10 |
| LED Azul | GPIO 11 |
| LED Verde | GPIO 12 |
| LED Dourado | GPIO 13 |
| OLED SSD1306 128×64 | I2C · SDA=14, SCL=15 (400 kHz) |
| LCD1602 I2C | I2C · SDA=16, SCL=17 (100 kHz) |
| Buzzer (LEDC PWM) | GPIO 18 |
| HC-SR04 Ultrassom | TRIG=19, ECHO=20 |

### Simulação Wokwi (diagrama simplificado)

O arquivo `diagram.json` mantém apenas os **6 componentes principais** para foco na lógica de IA e facilidade de simulação:

| Componente | Papel na simulação |
| --- | --- |
| ESP32-S3 DevKitC-1 | microcontrolador central |
| Teclado matricial 4×4 | entrada do jogador |
| OLED SSD1306 | tabuleiro e menus |
| LCD1602 I2C | exibe "TFLite" |
| HC-SR04 | modelo de presença em background |
| Buzzer | feedback sonoro |

LEDs e LDR estão no firmware e funcionam em hardware real; o diagrama Wokwi os omite para manter o circuito legível.

---

## 5. Como Compilar e Simular

### Requisitos

- **ESP-IDF v5.x ou v6.x** (testado com v6.0.1 via EIM)
- **Python 3.8+**
- **VS Code + extensão Wokwi** (para simulação)

### Linux

```bash
./iniciar.sh setup     # verifica dependências
./iniciar.sh build     # compila o firmware
./iniciar.sh simular   # compila + abre VS Code + checklist Wokwi
```

### Windows

```cmd
iniciar.bat setup
iniciar.bat build
```

> **Instalação via EIM (Windows):** O script detecta automaticamente o ESP-IDF instalado pelo [Espressif IDE Manager](https://github.com/espressif/idf-im-cli) em `C:\esp\<versão>\esp-idf`. Caso use um caminho personalizado, defina `IDF_PATH` antes de executar.

### Compilar manualmente (PowerShell com EIM)

```powershell
. "C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1"
idf.py build
```

Após a compilação, `build/flasher_args.json` e `build/*.elf` são gerados — o Wokwi precisa desses arquivos para iniciar a simulação.

---

## 6. IA Embarcada (TinyML)

Todo o treinamento é feito em Python com Keras na pasta `ml/`. Os modelos são convertidos para TFLite e quantizados em **INT8 full integer**, depois exportados como arrays C que o firmware inclui diretamente.

### Modelo do Jogo da Velha

| Etapa | Detalhe |
| --- | --- |
| Geração do dataset | Minimax percorre todos os estados válidos → 2.097 posições |
| Arquitetura | Input(9) → Dense(32, ReLU) → Dense(32, ReLU) → Dense(9, logits) |
| Treinamento | 100 épocas, split 80/20 (1.678 treino / 419 teste) |
| Acurácia float | 73,27% · jogada ótima: **96,18%** |
| Acurácia INT8 | 73,27% · jogada ótima: **96,42%** |
| Tamanho float → INT8 | 8,4 KB → **5,7 KB** (−32%, sem perda de acurácia) |

> A métrica de **jogada ótima** é a mais relevante: o modelo escolhe uma jogada tão boa quanto o Minimax perfeito em 96,42% dos casos.

### Modelo de Presença (HC-SR04)

| Etapa | Detalhe |
| --- | --- |
| Features | `distancia_cm`, `eco_us` — limiar 120 cm |
| Dataset | 1.775 amostras (1.195 ausente / 580 presente) |
| Arquitetura | Input(2) → Dense(2, logits) |
| Treinamento | 11 épocas (early stopping), split 80/20 |
| Acurácia float | **98,87%** |
| Acurácia INT8 | **98,03%** (−0,84% pelo custo da quantização) |
| Tamanho INT8 | **1,3 KB** |

Relatórios completos com métricas, SHA-256 e tamanhos são gerados automaticamente em `ml/relatorios/` a cada treinamento.

---

## 7. Estrutura do Repositório

```
├── main/               # Firmware ESP-IDF (C/C++)
│   ├── main.c          # Loop principal
│   ├── ia_tflite.cc    # Inferência TFLite Micro — jogo
│   ├── presenca_tflite.cc  # Inferência TFLite Micro — presença
│   ├── jogo_da_velha.c # Lógica do jogo
│   └── ...             # Drivers: teclado, OLED, LCD, buzzer, LDR, HC-SR04
├── ml/
│   ├── datasets/       # CSVs de treino
│   ├── models/         # .tflite float e INT8
│   ├── relatorios/     # JSON com métricas e hashes
│   ├── pipeline_tictactoe.py
│   └── pipeline_presenca.py
├── test/               # Testes Unity (C) e pytest
├── diagram.json        # Circuito Wokwi (componentes principais)
├── wokwi.toml          # Configuração do simulador
├── iniciar.bat / .sh   # Scripts de build e simulação
└── apresentacao.html   # Slides da apresentação
```

---

*Redes Neurais em microcontroladores: prático, rápido e offline.*
