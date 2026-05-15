# Projeto Final: Jogo da Velha com IA Embarcada no ESP32-S3

**Autor:** Patrik, João Vitor e Janiel

---

## 1. Introdução

Este projeto apresenta o desenvolvimento de um sistema embarcado completo para o jogo da velha, utilizando o microcontrolador ESP32-S3 com o framework ESP-IDF. A ideia principal que eu quis trazer para cá foi criar um ambiente interativo onde o usuário joga contra o computador, mas com um diferencial: o "cérebro" do computador é um modelo de Inteligência Artificial (TFLite Micro INT8) embarcado diretamente na placa.

Além do jogo em si, integrei um sensor ultrassônico (HC-SR04) para rodar um classificador de presença em segundo plano, um teclado matricial 4x4 para as jogadas, um sensor de luminosidade (LDR) para controle de iluminação ambiente, displays OLED e LCD para interface visual, e componentes de feedback (LEDs e buzzer). O projeto foi desenhado para rodar tanto na bancada física quanto no simulador Wokwi.

## 2. Visão Geral das Funcionalidades

O funcionamento do sistema é focado na interação direta:

*   **Controle Principal:** Pelo teclado matricial, apertando a tecla `A` o jogo começa. O tabuleiro aparece no display OLED no formato tradicional ` 1 | 2 | 3 `, facilitando a escolha da casa desejada.
*   **A Inteligência do Jogo:** O computador decide suas jogadas usando exclusivamente um modelo TFLite Micro treinado previamente. Não existe nenhum "plano B" ou algoritmo tradicional rodando por trás, é IA pura. O display LCD1602 mostra em tempo real qual modelo está rodando na primeira linha.
*   **Sensores Auxiliares:** 
    *   **HC-SR04:** Fica medindo a distância do jogador e alimenta um classificador de presença, sem atrapalhar a partida.
    *   **LDR (Luminosidade):** Percebe quando o ambiente escurece e acende LEDs automaticamente. Claro, eu mantive a opção manual no teclado (teclas `*` e `#`) caso o jogador queira controlar.
*   **Monitoramento:** Logo ao ligar, o sistema exibe no console um resumo completo (estilo "Hello World" do TensorFlow) com as métricas e configuração dos modelos embarcados. Achei interessante também espelhar tudo que vai pro OLED direto no console serial, assim fica bem mais fácil de depurar.

## 3. Guia de Controles do Teclado

Para facilitar a vida na hora de jogar, organizei os comandos da seguinte forma:

| Tecla | Função no Sistema |
| --- | --- |
| `A` | Começa uma nova partida |
| `B` | Mostra o placar atualizado |
| `C` | Sai/Encerra o programa |
| `D` | Tela de créditos / autores |
| `0` | Zera a pontuação do placar |
| `1` a `9` | Escolhe a posição no tabuleiro durante o jogo |
| `*` | Acende o LED de iluminação manual |
| `#` | Apaga o LED de iluminação manual |
| `9` | (Modo Técnico) Ativa a leitura crua do HC-SR04 para coleta de dados no console |

## 4. Estrutura de Hardware (Circuito)

Caso queira montar na prática ou conferir as conexões, essa é a pinagem que defini no ESP32-S3:

| Componente | Conexão / Pino no ESP32-S3 |
| --- | --- |
| Teclado (Linhas) | R1=2, R2=3, R3=4, R4=5 |
| Teclado (Colunas)| C1=6, C2=7, C3=8, C4=9 |
| LED Azul | GPIO 11 |
| LED Verde | GPIO 12 |
| LEDs Dourados | GPIO 13 |
| OLED SSD1306 | I2C (SDA=14, SCL=15) - 400kHz |
| LCD1602 I2C | I2C Dedicado (SDA=16, SCL=17) - 100kHz |
| Buzzer | GPIO 18 |
| HC-SR04 (Ultrassom)| TRIG=19, ECHO=20 |
| LDR (Luz) | Analógico (AO) = 10 |

## 5. Como Preparar o Ambiente e Compilar

A única coisa que você precisa ter instalada no seu computador é o ESP-IDF e o Python 3. Eu criei um script bem prático (`iniciar.sh` no Linux ou `iniciar.bat` no Windows) que já faz todo o trabalho chato de criar ambiente virtual e instalar dependências.

**No Linux:**
```bash
./iniciar.sh setup          # Checa as dependências
./iniciar.sh                # Abre o painel de controle interativo (colorido e organizado!)
./iniciar.sh build          # Compila o código C do firmware
./iniciar.sh simular        # Prepara para simular no Wokwi integrado ao VS Code
```

**No Windows:**
```cmd
iniciar.bat setup
iniciar.bat
```

Se o seu ESP-IDF não estiver no local padrão, não esqueça de exportar a variável de ambiente (ex: `export IDF_PATH=/caminho/do/seu/esp-idf`).

## 6. Um Pouco Sobre a IA Embarcada (TinyML)

Todo o treinamento dos modelos foi feito usando Keras em Python (dentro da pasta `ml/`). 

1. **Jogo da Velha:** Treinei o modelo para mapear todas as jogadas ótimas de um algoritmo minimax offline. Depois, converti esse modelo para o formato TFLite quantizado em INT8, que é extremamente leve e cabe na memória do ESP32-S3.
2. **Presença:** Fiz a mesma coisa para o HC-SR04. O script `ml/pipeline_presenca.py` processa um dataset próprio de distâncias, treina e quantiza para que o microcontrolador saiba se tem alguém na frente dele usando Inteligência Artificial e não um simples `if/else`.

Sempre que a compilação do modelo roda, um relatório JSON é gerado em `ml/relatorios/` registrando as métricas, tamanho, e hash SHA-256 para auditoria do projeto.

---
Espero que este projeto sirva de referência para entender como podemos aplicar Redes Neurais em microcontroladores de forma prática e divertida!
