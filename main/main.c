#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "buzzer.h"
#include "hcsr04.h"
#include "ia_jogo_da_velha.h"
#include "ia_tflite.h"
#include "jogo_auditoria.h"
#include "jogo_da_velha.h"
#include "jogo_interface.h"
#include "ldr.h"
#include "lcd1602_i2c.h"
#include "leds.h"
#include "presenca_tflite.h"
#include "ssd1306_i2c.h"
#include "teclado_matricial.h"

#define OLED_PORTA_I2C I2C_NUM_0
#define OLED_SDA GPIO_NUM_14
#define OLED_SCL GPIO_NUM_15
#define OLED_ENDERECO 0x3C
#define OLED_FREQ_HZ 400000

#define LCD_PORTA_I2C I2C_NUM_1
#define LCD_SDA GPIO_NUM_16
#define LCD_SCL GPIO_NUM_17
#define LCD_IA_ENDERECO 0x27
#define LCD_PRESENCA_ENDERECO 0x26
#define LCD_ESTATISTICAS_ENDERECO 0x25
#define LCD_FREQ_HZ 100000

#define INTERVALO_MENU_MS 80
#define INTERVALO_PRESENCA_MS 1000
#define INTERVALO_LCD_AUTORES_MS 350
#define INTERVALO_LCD_PRESENCA_MS 500
#define INTERVALO_LCD_ESTATISTICAS_MS 500
#define TASK_PRESENCA_PILHA_BYTES 4096
#define LCD_AUTORES_TEXTO "Autores: Janiel, Joao e Patrik"

static const char *TAG = "ProjetoFinal_Patrik";
static const char *TAG_AUDIT = "JOGO_AUDIT";

typedef struct {
    bool sensor_pronto;
    bool leitura_valida;
    bool presente;
    uint16_t distancia_cm;
    uint32_t eco_us;
    int32_t score;
    uint32_t atualizado_ms;
} presenca_estado_t;

typedef struct {
    bool partida_em_andamento;
    bool partida_iniciada;
    uint32_t inicio_ms;
    uint32_t ultima_jogada_ms;
    uint32_t duracao_ms;
    uint32_t jogadas;
    uint32_t soma_intervalos_jogada_ms;
} jogo_estatisticas_t;

static ssd1306_t oled;
static lcd1602_t lcd_ia;
static lcd1602_t lcd_presenca;
static lcd1602_t lcd_estatisticas;
static teclado_matricial_t teclado;
static leds_t leds;
static buzzer_t buzzer;
static jogo_estado_t jogo;
static ia_tflite_t modelo_ia;
static presenca_tflite_t modelo_presenca;
static hcsr04_t sensor_hcsr04;
static ldr_t sensor_luz;
static bool sensor_hcsr04_pronto;
static bool sensor_luz_pronto;
static bool lcd_ia_pronto;
static bool lcd_presenca_pronto;
static bool lcd_estatisticas_pronto;
static bool luz_dourada_manual;
static int ldr_histerese_estado;
static bool presenca_log_iniciado;
static bool ultima_presenca_logada;
static uint32_t ultimo_log_presenca_ms;
static bool lcd_status_iniciado;
static uint32_t ultimo_lcd_status_ms;
static uint32_t lcd_autores_passo;
static bool lcd_presenca_status_iniciado;
static uint32_t ultimo_lcd_presenca_ms;
static bool lcd_estatisticas_status_iniciado;
static uint32_t ultimo_lcd_estatisticas_ms;
static presenca_estado_t estado_presenca;
static jogo_estatisticas_t estatisticas_jogo;
static SemaphoreHandle_t mutex_estado_presenca;
static TaskHandle_t task_presenca_handle;
static uint32_t auditoria_partida_id;
static uint32_t auditoria_jogada_id;

static void mostrar_hello_world_treinamento(void);
static void imprimir_acuracia_modelo(const char *rotulo, int valor_permyriad);
static void mostrar_manual_serial(void);
static void inicializar_lcds(void);
static bool inicializar_lcd1602(const char *nome, lcd1602_t *lcd_alvo, uint8_t endereco);
static void inicializar_sensores(void);
static void inicializar_modelo_ia(void);
static void exibir_oled_linhas(const char *origem, const char *linhas[], int quantidade);
static void espelhar_oled_console(const char *origem, const char *linhas[], int quantidade);
static void mostrar_menu(void);
static void mostrar_tabuleiro(void);
static void mostrar_placar(void);
static void mostrar_autor(void);
static void mostrar_placar_zerado(void);
static void mostrar_mensagem(const char *linha1, const char *linha2, const char *linha3);
static void atualizar_lcd_status_ia(bool forcar);
static void atualizar_lcd_algoritmo(const ia_resultado_t *resultado);
static void atualizar_lcd_presenca(bool forcar);
static void atualizar_lcd_estatisticas(bool forcar);
static const char *texto_lcd_algoritmo(const ia_resultado_t *resultado);
static bool aguardar_jogada_teclado(int *posicao);
static void iniciar_task_presenca(void);
static void task_presenca_ambiente(void *parametro);
static void atualizar_presenca_ambiente(void);
static void atualizar_estado_presenca(bool sensor_pronto,
                                      bool leitura_valida,
                                      bool presente,
                                      uint16_t distancia_cm,
                                      uint32_t eco_us,
                                      int32_t score,
                                      uint32_t atualizado_ms);
static presenca_estado_t copiar_estado_presenca(void);
static bool tecla_liberada_para_interacao(char tecla);
static void definir_luz_manual(bool ligada);
static void atualizar_luz_automatica(void);
static void estatisticas_iniciar_partida(void);
static void estatisticas_registrar_jogada(void);
static void estatisticas_finalizar_partida(void);
static void auditoria_jogo_inicio(void);
static void auditoria_jogo_jogada(const char *ator,
                                  char simbolo,
                                  int posicao,
                                  jogo_jogada_t jogada,
                                  bool aplicada,
                                  const ia_resultado_t *resultado_ia);
static void auditoria_jogo_fim(const char *resultado_final, char vencedor);
static void jogar_partida(void);
static void parar_programa(void);
static uint32_t ler_millis(void);

void app_main(void)
{
    mostrar_hello_world_treinamento();
    mostrar_manual_serial();

    ssd1306_config_t config_oled = {
        .porta_i2c = OLED_PORTA_I2C,
        .pino_sda = OLED_SDA,
        .pino_scl = OLED_SCL,
        .endereco = OLED_ENDERECO,
        .frequencia_hz = OLED_FREQ_HZ,
    };

    ESP_ERROR_CHECK(ssd1306_iniciar(&oled, &config_oled));
    inicializar_lcds();
    ESP_ERROR_CHECK(teclado_matricial_iniciar(&teclado));
    ESP_ERROR_CHECK(leds_iniciar(&leds));
    ESP_ERROR_CHECK(buzzer_iniciar(&buzzer));
    mutex_estado_presenca = xSemaphoreCreateMutex();
    if (mutex_estado_presenca == NULL) {
        ESP_LOGW(TAG, "Sem mutex de presenca; estado sera atualizado sem bloqueio dedicado.");
    }
    inicializar_sensores();
    inicializar_modelo_ia();

    jogo_iniciar(&jogo);
    estatisticas_jogo = (jogo_estatisticas_t) {0};
    lcd_status_iniciado = false;
    ultimo_lcd_status_ms = 0;
    lcd_autores_passo = 0;
    lcd_presenca_status_iniciado = false;
    ultimo_lcd_presenca_ms = 0;
    lcd_estatisticas_status_iniciado = false;
    ultimo_lcd_estatisticas_ms = 0;
    atualizar_lcd_status_ia(true);
    atualizar_lcd_presenca(true);
    atualizar_lcd_estatisticas(true);
    iniciar_task_presenca();
    buzzer_som_inicial();
    mostrar_menu();

    while (true) {
        atualizar_luz_automatica();
        atualizar_lcd_status_ia(false);
        atualizar_lcd_estatisticas(false);
        char tecla = teclado_matricial_ler(&teclado);

        if (!tecla_liberada_para_interacao(tecla)) {
            ESP_LOGW(TAG, "Tecla ignorada: jogador ausente.");
            vTaskDelay(pdMS_TO_TICKS(INTERVALO_MENU_MS));
            continue;
        }

        if (tecla != 0) {
            ESP_LOGI(TAG, "Tecla detectada no menu: %c", tecla);
            buzzer_som_tecla();
        }

        switch (tecla) {
        case '*':
            definir_luz_manual(true);
            break;
        case '#':
            definir_luz_manual(false);
            break;
        case 'A':
            jogar_partida();
            mostrar_menu();
            break;
        case 'B':
            mostrar_placar();
            break;
        case 'C':
            parar_programa();
            break;
        case 'D':
            mostrar_autor();
            break;
        case '0':
            jogo_zerar_placar(&jogo);
            mostrar_placar_zerado();
            break;
        default:
            break;
        }

        leds_atualizar(&leds);
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_MENU_MS));
    }
}

static void mostrar_hello_world_treinamento(void)
{
    printf("\n");
    printf("\033[1;36m============================================================\033[0m\n");
    printf("\033[1;33m    Projeto Final: Jogo da Velha com IA (ESP32-S3)\033[0m\n");
    printf("\033[1;36m============================================================\033[0m\n");
    printf("Fala pessoal! Aqui e o Patrik. Este e o resumo do sistema:\n\n");
    printf("Como a IA foi treinada:\n");
    printf("  1. Criei o dataset e treinei o modelo em Python (Keras)\n");
    printf("  2. Converti pra TFLite e apliquei quantizacao INT8\n");
    printf("  3. Transformei o modelo em um array C e embarquei aqui\n\n");
    
    printf("\033[1;32m[+] Modelos Embarcados Ativos:\033[0m\n");
    printf("  -> \033[1;37mJogo da Velha (TFLite INT8)\033[0m\n");
    printf("     Entrada: %d | Saida: %d | Dataset: %d linhas\n", 
           TICTACTOE_MODEL_INPUT_SIZE, TICTACTOE_MODEL_OUTPUT_SIZE, TICTACTOE_MODEL_DATASET_ROWS);
    printf("     Tamanho do Modelo: %u bytes | Arena: %d bytes\n", 
           (unsigned int)TICTACTOE_MODEL_TFLITE_LEN, TICTACTOE_MODEL_TENSOR_ARENA_BYTES);
    printf("     Hash SHA: %.12s\n", TICTACTOE_MODEL_INT8_SHA256);
    imprimir_acuracia_modelo("     Taxa de Jogada Otima", TICTACTOE_MODEL_OPTIMAL_MOVE_PERMYRIAD);
    printf("\n");
    
    printf("  -> \033[1;37mPresenca HC-SR04 (TFLite INT8)\033[0m\n");
    printf("     Entrada: %d | Saida: %d | Dataset: %d linhas\n", 
           PRESENCA_MODEL_INPUT_SIZE, PRESENCA_MODEL_OUTPUT_SIZE, PRESENCA_MODEL_DATASET_ROWS);
    printf("     Tamanho do Modelo: %u bytes | Arena: %d bytes\n", 
           (unsigned int)PRESENCA_MODEL_TFLITE_LEN, PRESENCA_MODEL_TENSOR_ARENA_BYTES);
    printf("     Hash SHA: %.12s\n", PRESENCA_MODEL_INT8_SHA256);
    imprimir_acuracia_modelo("     Acuracia de Teste", PRESENCA_MODEL_TEST_ACCURACY_PERMYRIAD);
    printf("\n");
    
    printf("\033[1;35m[i] Monitoramento do OLED ativado.\033[0m Tudo que aparecer na telinha \n");
    printf("sera espelhado aqui embaixo no console pra facilitar os testes.\n");
    printf("\033[1;36m============================================================\033[0m\n\n");
    fflush(stdout);
}

static void imprimir_acuracia_modelo(const char *rotulo, int valor_permyriad)
{
    if (valor_permyriad < 0) {
        printf("%s: indisponivel no header atual\n", rotulo);
        return;
    }

    printf("%s: %d.%02d%%\n", rotulo, valor_permyriad / 100, valor_permyriad % 100);
}

static void mostrar_manual_serial(void)
{
    ESP_LOGI(TAG, "Iniciando o Jogo da Velha...");
    ESP_LOGI(TAG, "Dica: O jogador usa 'O' e a Inteligencia Artificial usa 'X'.");
    ESP_LOGI(TAG, "Utilize as teclas de 1 a 9 no teclado matricial para jogar.");
    ESP_LOGI(TAG, "Teclas * e # acendem e apagam o LED dourado manualmente.");
    ESP_LOGI(TAG, "Atalhos: A=Jogar, B=Placar, C=Sair, D=Creditos, 0=Zerar placar.");
    ESP_LOGI(TAG, "HC-SR04 rodando classificador TFLite sempre ativo em task dedicada.");
}

static void inicializar_lcds(void)
{
    lcd_ia_pronto = inicializar_lcd1602("IA", &lcd_ia, LCD_IA_ENDERECO);
    lcd_presenca_pronto = inicializar_lcd1602("Presenca", &lcd_presenca, LCD_PRESENCA_ENDERECO);
    lcd_estatisticas_pronto = inicializar_lcd1602("Estatisticas", &lcd_estatisticas, LCD_ESTATISTICAS_ENDERECO);
}

static bool inicializar_lcd1602(const char *nome, lcd1602_t *lcd_alvo, uint8_t endereco)
{
    lcd1602_config_t config_lcd = {
        .porta_i2c = LCD_PORTA_I2C,
        .pino_sda = LCD_SDA,
        .pino_scl = LCD_SCL,
        .endereco = endereco,
        .frequencia_hz = LCD_FREQ_HZ,
    };

    esp_err_t erro = lcd1602_iniciar(lcd_alvo, &config_lcd);
    if (erro != ESP_OK) {
        ESP_LOGW(TAG, "LCD %s indisponivel em 0x%02X (%s).", nome, endereco, esp_err_to_name(erro));
        return false;
    }

    ESP_LOGI(TAG, "LCD %s pronto no I2C compartilhado: endereco=0x%02X SDA=%d SCL=%d", nome, endereco, LCD_SDA, LCD_SCL);
    return true;
}

static void inicializar_sensores(void)
{
    sensor_hcsr04_pronto = false;
    sensor_luz_pronto = false;
    presenca_log_iniciado = false;
    ultima_presenca_logada = false;
    ultimo_log_presenca_ms = 0;

    esp_err_t erro = hcsr04_iniciar(&sensor_hcsr04, HCSR04_TRIGGER_GPIO, HCSR04_ECHO_GPIO);
    if (erro != ESP_OK) {
        ESP_LOGW(TAG, "HC-SR04 indisponivel (%s); jogo segue pelo teclado.", esp_err_to_name(erro));
    } else {
        sensor_hcsr04_pronto = true;
        ESP_LOGI(TAG, "HC-SR04 pronto: TRIG=%d ECHO=%d", HCSR04_TRIGGER_GPIO, HCSR04_ECHO_GPIO);
    }

    erro = presenca_tflite_iniciar(&modelo_presenca);
    if (erro != ESP_OK) {
        ESP_LOGW(TAG, "Modelo de presenca indisponivel (%s); classificador compacto sera usado.", esp_err_to_name(erro));
    } else {
        ESP_LOGI(TAG,
                 "Modelo de presenca pronto (%s); arena sugerida: %d bytes",
                 modelo_presenca.runtime_tflite ? "TFLite" : "compacto",
                 modelo_presenca.arena_bytes);
    }

    erro = ldr_iniciar(&sensor_luz);
    if (erro != ESP_OK) {
        ESP_LOGW(TAG, "LDR indisponivel (%s); LED dourado segue por teclado.", esp_err_to_name(erro));
    } else {
        sensor_luz_pronto = true;
        ESP_LOGI(TAG, "LDR pronto: GPIO%d ADC1_CH9", LDR_GPIO_NUM);
    }

    atualizar_estado_presenca(sensor_hcsr04_pronto, false, false, 0, 0, 0, ler_millis());
}

static void inicializar_modelo_ia(void)
{
    esp_err_t erro_ia = ia_tflite_iniciar(&modelo_ia);
    if (erro_ia != ESP_OK) {
        ESP_LOGE(TAG, "Modelo TFLite do jogo indisponivel (%s); nenhuma outra IA sera executada.", esp_err_to_name(erro_ia));
    } else {
        ESP_LOGI(TAG, "Modelo do jogo pronto; arena sugerida: %d bytes", modelo_ia.arena_bytes);
    }
}

static void exibir_oled_linhas(const char *origem, const char *linhas[], int quantidade)
{
    espelhar_oled_console(origem, linhas, quantidade);
    ssd1306_mostrar_linhas(&oled, linhas, quantidade); /* ignora falha I2C */
}

static void espelhar_oled_console(const char *origem, const char *linhas[], int quantidade)
{
    printf("\n\033[1;34m=== Tela OLED: %s ===\033[0m\n", origem != NULL ? origem : "GERAL");
    for (int i = 0; i < quantidade; i++) {
        printf("  | %s\n", linhas[i] != NULL ? linhas[i] : "");
    }
    printf("\033[1;34m=======================\033[0m\n\n");
    fflush(stdout);
}

static void mostrar_menu(void)
{
    const char *linhas[] = {
        "A - Jogar       ",
        "B - Placar",
        "C - Sair        ",
        "D - About",
        "0 - Zerar  ",
        "Escolha ",
    };

    exibir_oled_linhas("MENU", linhas, 6);
}

static void mostrar_tabuleiro(void)
{
    char linha0[16];
    char linha1[16];
    char linha2[16];

    jogo_formatar_linha_tabuleiro_expandida(&jogo, 0, linha0, sizeof(linha0));
    jogo_formatar_linha_tabuleiro_expandida(&jogo, 1, linha1, sizeof(linha1));
    jogo_formatar_linha_tabuleiro_expandida(&jogo, 2, linha2, sizeof(linha2));

    const char *linhas[] = {
        " ",
        linha0,
        "---+---+---",
        linha1,
        "---+---+---",
        linha2,
        " ",
    };

    exibir_oled_linhas("TABULEIRO", linhas, 7);
}

static void mostrar_placar(void)
{
    char jogador[24];
    char computador[24];
    char empates[24];

    snprintf(jogador, sizeof(jogador), "Jogador: %u", jogo.vitorias_jogador);
    snprintf(computador, sizeof(computador), "Computador: %u", jogo.vitorias_computador);
    snprintf(empates, sizeof(empates), "Empates: %u", jogo.empates);

    const char *linhas[] = {
        " ",
        jogador,
        " ",
        computador,
        " ",
        empates,
    };

    exibir_oled_linhas("PLACAR", linhas, 6);
}

static void mostrar_autor(void)
{
    const char *linhas[] = {
        " ",
        "  Desenvolvido",
        "      por:",
        " Janiel, Joao",
        "   e Patrik",
        " ",
    };

    exibir_oled_linhas("AUTOR", linhas, 6);
}

static void mostrar_placar_zerado(void)
{
    const char *linhas[] = {
        " ",
        " ",
        "     Placar",
        " ",
        "     Zerado",
    };

    exibir_oled_linhas("PLACAR_ZERADO", linhas, 5);
}

static void mostrar_mensagem(const char *linha1, const char *linha2, const char *linha3)
{
    const char *linhas[] = {
        " ",
        " ",
        linha1,
        linha2,
        linha3,
    };

    exibir_oled_linhas("MENSAGEM", linhas, 5);
}

static void atualizar_lcd_status_ia(bool forcar)
{
    if (!lcd_ia_pronto) {
        return;
    }

    uint32_t agora_ms = ler_millis();
    if (!forcar && lcd_status_iniciado && (agora_ms - ultimo_lcd_status_ms) < INTERVALO_LCD_AUTORES_MS) {
        return;
    }

    char autores[LCD1602_COLUNAS + 1];
    lcd1602_formatar_janela_scroll(LCD_AUTORES_TEXTO, lcd_autores_passo, autores);

    lcd1602_escrever_linha(&lcd_ia, 0, texto_lcd_algoritmo(NULL)); /* ignora falha I2C */
    lcd1602_escrever_linha(&lcd_ia, 1, autores); /* ignora falha I2C */

    lcd_autores_passo++;
    ultimo_lcd_status_ms = agora_ms;
    lcd_status_iniciado = true;
}

static void atualizar_lcd_algoritmo(const ia_resultado_t *resultado)
{
    (void)resultado;
    atualizar_lcd_status_ia(true);
}

static void atualizar_lcd_presenca(bool forcar)
{
    if (!lcd_presenca_pronto) {
        return;
    }

    uint32_t agora_ms = ler_millis();
    if (!forcar && lcd_presenca_status_iniciado &&
        (agora_ms - ultimo_lcd_presenca_ms) < INTERVALO_LCD_PRESENCA_MS) {
        return;
    }

    presenca_estado_t estado = copiar_estado_presenca();
    char linha0[LCD1602_COLUNAS + 1];
    char linha1[LCD1602_COLUNAS + 1];

    if (!estado.sensor_pronto) {
        snprintf(linha0, sizeof(linha0), "PRESENCA");
        snprintf(linha1, sizeof(linha1), "HC-SR04 ERRO");
    } else if (!estado.leitura_valida) {
        snprintf(linha0, sizeof(linha0), "PRESENCA");
        snprintf(linha1, sizeof(linha1), "Aguardando...");
    } else {
        long score = (long)estado.score;
        if (score > 999) {
            score = 999;
        } else if (score < -999) {
            score = -999;
        }

        snprintf(linha0, sizeof(linha0), "PRES: %s", estado.presente ? "PRESENTE" : "AUSENTE");
        snprintf(linha1, sizeof(linha1), "D:%03ucm S:%+ld", (unsigned int)estado.distancia_cm, score);
    }

    lcd1602_escrever_linha(&lcd_presenca, 0, linha0); /* ignora falha I2C */
    lcd1602_escrever_linha(&lcd_presenca, 1, linha1); /* ignora falha I2C */
    ultimo_lcd_presenca_ms = agora_ms;
    lcd_presenca_status_iniciado = true;
}

static void atualizar_lcd_estatisticas(bool forcar)
{
    if (!lcd_estatisticas_pronto) {
        return;
    }

    uint32_t agora_ms = ler_millis();
    if (!forcar && lcd_estatisticas_status_iniciado &&
        (agora_ms - ultimo_lcd_estatisticas_ms) < INTERVALO_LCD_ESTATISTICAS_MS) {
        return;
    }

    uint32_t duracao_ms = estatisticas_jogo.partida_em_andamento
                               ? agora_ms - estatisticas_jogo.inicio_ms
                               : estatisticas_jogo.duracao_ms;
    char linha0[LCD1602_COLUNAS + 1];
    char linha1[LCD1602_COLUNAS + 1];

    jogo_interface_formatar_estatisticas_lcd(duracao_ms,
                                             estatisticas_jogo.jogadas,
                                             estatisticas_jogo.soma_intervalos_jogada_ms,
                                             linha0,
                                             linha1);

    lcd1602_escrever_linha(&lcd_estatisticas, 0, linha0); /* ignora falha I2C */
    lcd1602_escrever_linha(&lcd_estatisticas, 1, linha1); /* ignora falha I2C */
    ultimo_lcd_estatisticas_ms = agora_ms;
    lcd_estatisticas_status_iniciado = true;
}

static const char *texto_lcd_algoritmo(const ia_resultado_t *resultado)
{
    (void)resultado;
    return "TFLite";
}

static bool aguardar_jogada_teclado(int *posicao)
{
    if (posicao == NULL) {
        return false;
    }

    while (true) {
        atualizar_luz_automatica();
        atualizar_lcd_status_ia(false);
        atualizar_lcd_estatisticas(false);
        char tecla = teclado_matricial_ler(&teclado);
        if (tecla == 0) {
            leds_atualizar(&leds);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!tecla_liberada_para_interacao(tecla)) {
            ESP_LOGW(TAG, "Tecla ignorada na partida: jogador ausente.");
            leds_atualizar(&leds);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        ESP_LOGI(TAG, "Tecla detectada na partida: %c", tecla);
        buzzer_som_tecla();

        if (tecla == '*') {
            definir_luz_manual(true);
            continue;
        }

        if (tecla == '#') {
            definir_luz_manual(false);
            continue;
        }

        if (tecla >= '1' && tecla <= '9') {
            *posicao = tecla - '0';
            return true;
        }
    }
}

static void iniciar_task_presenca(void)
{
    if (task_presenca_handle != NULL) {
        return;
    }

    BaseType_t resultado = xTaskCreate(
        task_presenca_ambiente,
        "presenca_hcsr04",
        TASK_PRESENCA_PILHA_BYTES,
        NULL,
        tskIDLE_PRIORITY + 1,
        &task_presenca_handle);

    if (resultado != pdPASS) {
        task_presenca_handle = NULL;
        ESP_LOGE(TAG, "Falha ao criar task permanente de presenca.");
    } else {
        ESP_LOGI(TAG, "Task permanente de presenca iniciada; nao ha opcao de desabilitar pelo teclado.");
    }
}

static void task_presenca_ambiente(void *parametro)
{
    (void)parametro;

    atualizar_lcd_presenca(true);
    while (true) {
        atualizar_presenca_ambiente();
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_PRESENCA_MS));
    }
}

static void atualizar_presenca_ambiente(void)
{
    if (!sensor_hcsr04_pronto) {
        atualizar_estado_presenca(false, false, false, 0, 0, 0, ler_millis());
        atualizar_lcd_presenca(false);
        return;
    }

    uint32_t agora_ms = ler_millis();
    if (presenca_log_iniciado && (agora_ms - ultimo_log_presenca_ms) < INTERVALO_PRESENCA_MS) {
        return;
    }

    uint16_t distancia_cm = 0;
    uint32_t eco_us = 0;
    esp_err_t erro = hcsr04_ler_distancia(&sensor_hcsr04, &distancia_cm, &eco_us);
    presenca_tflite_resultado_t resultado_presenca =
        presenca_tflite_avaliar_leitura_hcsr04(&modelo_presenca, erro, distancia_cm, eco_us);
    if (erro != ESP_OK) {
        ultimo_log_presenca_ms = agora_ms;
        atualizar_estado_presenca(
            true,
            resultado_presenca.leitura_valida,
            resultado_presenca.presente,
            resultado_presenca.distancia_cm,
            resultado_presenca.eco_us,
            resultado_presenca.score,
            agora_ms);
        atualizar_lcd_presenca(false);

        if (!presenca_log_iniciado || ultima_presenca_logada) {
            ESP_LOGW(TAG,
                     "\033[1;36m[Sensor HC-SR04]\033[0m Sem eco valido (%s); tratando como \033[1;31mAUSENTE\033[0m.",
                     esp_err_to_name(erro));
            presenca_log_iniciado = true;
            ultima_presenca_logada = false;
        }
        return;
    }

    ultimo_log_presenca_ms = agora_ms;
    atualizar_estado_presenca(
        true,
        resultado_presenca.leitura_valida,
        resultado_presenca.presente,
        resultado_presenca.distancia_cm,
        resultado_presenca.eco_us,
        resultado_presenca.score,
        agora_ms);
    atualizar_lcd_presenca(false);

    if (!presenca_log_iniciado || resultado_presenca.presente != ultima_presenca_logada) {
        ESP_LOGI(TAG,
                 "\033[1;36m[Sensor HC-SR04]\033[0m Detectei que voce esta: %s (dist: %u cm | IA_Score: %ld)",
                 resultado_presenca.presente ? "\033[1;32mPRESENTE\033[0m" : "\033[1;31mAUSENTE\033[0m",
                 (unsigned int)resultado_presenca.distancia_cm,
                 (long)resultado_presenca.score);
        presenca_log_iniciado = true;
        ultima_presenca_logada = resultado_presenca.presente;
    }
}

static void atualizar_estado_presenca(bool sensor_pronto,
                                      bool leitura_valida,
                                      bool presente,
                                      uint16_t distancia_cm,
                                      uint32_t eco_us,
                                      int32_t score,
                                      uint32_t atualizado_ms)
{
    presenca_estado_t novo_estado = {
        .sensor_pronto = sensor_pronto,
        .leitura_valida = leitura_valida,
        .presente = presente,
        .distancia_cm = distancia_cm,
        .eco_us = eco_us,
        .score = score,
        .atualizado_ms = atualizado_ms,
    };

    if (mutex_estado_presenca != NULL &&
        xSemaphoreTake(mutex_estado_presenca, pdMS_TO_TICKS(20)) == pdTRUE) {
        estado_presenca = novo_estado;
        xSemaphoreGive(mutex_estado_presenca);
        return;
    }

    estado_presenca = novo_estado;
}

static presenca_estado_t copiar_estado_presenca(void)
{
    presenca_estado_t copia = estado_presenca;

    if (mutex_estado_presenca != NULL &&
        xSemaphoreTake(mutex_estado_presenca, pdMS_TO_TICKS(20)) == pdTRUE) {
        copia = estado_presenca;
        xSemaphoreGive(mutex_estado_presenca);
    }

    return copia;
}

static bool tecla_liberada_para_interacao(char tecla)
{
    presenca_estado_t estado = copiar_estado_presenca();

    return jogo_interacao_tecla_liberada_por_presenca(tecla,
                                                      estado.sensor_pronto,
                                                      estado.leitura_valida,
                                                      estado.presente);
}

static void definir_luz_manual(bool ligada)
{
    luz_dourada_manual = true;
    leds_definir_dourado(&leds, ligada);
}

static void atualizar_luz_automatica(void)
{
    if (!ldr_deve_atualizar_iluminacao_automatica(sensor_luz_pronto, luz_dourada_manual)) {
        return;
    }

    int leitura = 0;
    if (ldr_ler_bruto(&sensor_luz, &leitura) != ESP_OK) {
        return;
    }

    bool ligar_led = ldr_calcular_led_automatico(leitura, &ldr_histerese_estado);
    leds_definir_dourado(&leds, ligar_led);
}

static void estatisticas_iniciar_partida(void)
{
    uint32_t agora_ms = ler_millis();

    estatisticas_jogo.partida_em_andamento = true;
    estatisticas_jogo.partida_iniciada = true;
    estatisticas_jogo.inicio_ms = agora_ms;
    estatisticas_jogo.ultima_jogada_ms = agora_ms;
    estatisticas_jogo.duracao_ms = 0;
    estatisticas_jogo.jogadas = 0;
    estatisticas_jogo.soma_intervalos_jogada_ms = 0;
    atualizar_lcd_estatisticas(true);
}

static void estatisticas_registrar_jogada(void)
{
    if (!estatisticas_jogo.partida_em_andamento) {
        return;
    }

    uint32_t agora_ms = ler_millis();
    estatisticas_jogo.jogadas++;
    estatisticas_jogo.soma_intervalos_jogada_ms += agora_ms - estatisticas_jogo.ultima_jogada_ms;
    estatisticas_jogo.ultima_jogada_ms = agora_ms;
    estatisticas_jogo.duracao_ms = agora_ms - estatisticas_jogo.inicio_ms;
    atualizar_lcd_estatisticas(true);
}

static void estatisticas_finalizar_partida(void)
{
    if (!estatisticas_jogo.partida_em_andamento) {
        atualizar_lcd_estatisticas(true);
        return;
    }

    estatisticas_jogo.partida_em_andamento = false;
    estatisticas_jogo.duracao_ms = ler_millis() - estatisticas_jogo.inicio_ms;
    atualizar_lcd_estatisticas(true);
}

static void auditoria_jogo_inicio(void)
{
    char tabuleiro[JOGO_AUDITORIA_TABULEIRO_TAMANHO];
    jogo_auditoria_resultado_t analise = jogo_auditoria_analisar(&jogo);

    auditoria_partida_id++;
    auditoria_jogada_id = 0;
    jogo_auditoria_serializar_tabuleiro(&jogo, tabuleiro, sizeof(tabuleiro));

    ESP_LOGI(TAG_AUDIT,
             "partida=%" PRIu32 " evento=INICIO tabuleiro=%s ocupadas=%u placar_humano=%u placar_ia=%u empates=%u",
             auditoria_partida_id,
             tabuleiro,
             (unsigned int)analise.casas_ocupadas,
             (unsigned int)jogo.vitorias_jogador,
             (unsigned int)jogo.vitorias_computador,
             (unsigned int)jogo.empates);
}

static void auditoria_jogo_jogada(const char *ator,
                                  char simbolo,
                                  int posicao,
                                  jogo_jogada_t jogada,
                                  bool aplicada,
                                  const ia_resultado_t *resultado_ia)
{
    char tabuleiro[JOGO_AUDITORIA_TABULEIRO_TAMANHO];
    jogo_auditoria_resultado_t analise = jogo_auditoria_analisar(&jogo);
    int linha = (jogada.linha >= 0) ? jogada.linha + 1 : 0;
    int coluna = (jogada.coluna >= 0) ? jogada.coluna + 1 : 0;

    auditoria_jogada_id++;
    jogo_auditoria_serializar_tabuleiro(&jogo, tabuleiro, sizeof(tabuleiro));

    if (resultado_ia != NULL) {
        ESP_LOGI(TAG_AUDIT,
                 "partida=%" PRIu32 " evento=JOGADA seq=%" PRIu32 " ator=%s simbolo=%c posicao=%d linha=%d coluna=%d aplicada=%s algoritmo=%s score_int8=%d indice_modelo=%d tabuleiro=%s ocupadas=%u vitoria_O=%d linha_O=%s vitoria_X=%d linha_X=%s empate=%d",
                 auditoria_partida_id,
                 auditoria_jogada_id,
                 ator != NULL ? ator : "-",
                 simbolo,
                 posicao,
                 linha,
                 coluna,
                 aplicada ? "sim" : "nao",
                 resultado_ia->nome_curto != NULL ? resultado_ia->nome_curto : "-",
                 (int)modelo_ia.ultimo_score,
                 modelo_ia.ultimo_indice,
                 tabuleiro,
                 (unsigned int)analise.casas_ocupadas,
                 analise.vitoria_o ? 1 : 0,
                 analise.linha_o,
                 analise.vitoria_x ? 1 : 0,
                 analise.linha_x,
                 analise.empate ? 1 : 0);
        return;
    }

    ESP_LOGI(TAG_AUDIT,
             "partida=%" PRIu32 " evento=JOGADA seq=%" PRIu32 " ator=%s simbolo=%c posicao=%d linha=%d coluna=%d aplicada=%s tabuleiro=%s ocupadas=%u vitoria_O=%d linha_O=%s vitoria_X=%d linha_X=%s empate=%d",
             auditoria_partida_id,
             auditoria_jogada_id,
             ator != NULL ? ator : "-",
             simbolo,
             posicao,
             linha,
             coluna,
             aplicada ? "sim" : "nao",
             tabuleiro,
             (unsigned int)analise.casas_ocupadas,
             analise.vitoria_o ? 1 : 0,
             analise.linha_o,
             analise.vitoria_x ? 1 : 0,
             analise.linha_x,
             analise.empate ? 1 : 0);
}

static void auditoria_jogo_fim(const char *resultado_final, char vencedor)
{
    char tabuleiro[JOGO_AUDITORIA_TABULEIRO_TAMANHO];
    jogo_auditoria_resultado_t analise = jogo_auditoria_analisar(&jogo);

    jogo_auditoria_serializar_tabuleiro(&jogo, tabuleiro, sizeof(tabuleiro));

    ESP_LOGI(TAG_AUDIT,
             "partida=%" PRIu32 " evento=FIM resultado=%s vencedor=%c jogadas_validas=%" PRIu32 " tabuleiro=%s ocupadas=%u vitoria_O=%d linha_O=%s vitoria_X=%d linha_X=%s empate=%d duracao_ms=%" PRIu32,
             auditoria_partida_id,
             resultado_final != NULL ? resultado_final : "-",
             vencedor,
             estatisticas_jogo.jogadas,
             tabuleiro,
             (unsigned int)analise.casas_ocupadas,
             analise.vitoria_o ? 1 : 0,
             analise.linha_o,
             analise.vitoria_x ? 1 : 0,
             analise.linha_x,
             analise.empate ? 1 : 0,
             estatisticas_jogo.duracao_ms);
}

static void jogar_partida(void)
{
    bool vez_do_jogador = true;

    estatisticas_iniciar_partida();
    jogo_resetar_tabuleiro(&jogo);
    auditoria_jogo_inicio();
    mostrar_tabuleiro();

    while (true) {
        atualizar_lcd_status_ia(false);
        atualizar_lcd_estatisticas(false);
        if (vez_do_jogador) {
            int posicao = 0;

            if (!aguardar_jogada_teclado(&posicao)) {
                estatisticas_finalizar_partida();
                auditoria_jogo_fim("PARTIDA_ABORTADA", '-');
                return;
            }

            jogo_jogada_t jogada_humano = {
                .linha = (posicao - 1) / JOGO_TAMANHO,
                .coluna = (posicao - 1) % JOGO_TAMANHO,
            };
            bool jogada_humano_aplicada = jogo_aplicar_posicao(&jogo, posicao, 'O');
            auditoria_jogo_jogada("HUMANO", 'O', posicao, jogada_humano, jogada_humano_aplicada, NULL);
            if (!jogada_humano_aplicada) {
                continue;
            }
            estatisticas_registrar_jogada();
        } else {
            atualizar_lcd_status_ia(true);

            ia_resultado_t resultado = ia_tflite_escolher_jogada(&modelo_ia, &jogo);
            atualizar_lcd_algoritmo(&resultado);
            if (modelo_ia.ultimo_indice >= 0) {
                ESP_LOGI(TAG,
                         "Inferencia TFLite jogo: posicao=%d score_int8=%d modelo_sha=%.12s",
                         modelo_ia.ultimo_indice + 1,
                         (int)modelo_ia.ultimo_score,
                         TICTACTOE_MODEL_INT8_SHA256);
            }

            uint32_t pensamento = (ler_millis() * 7U + 13U) % 400U;
            vTaskDelay(pdMS_TO_TICKS(pensamento));

            int posicao_ia = jogo_auditoria_posicao_da_jogada(resultado.jogada);
            bool jogada_ia_aplicada = jogo_aplicar_jogada(&jogo, resultado.jogada, 'X');
            auditoria_jogo_jogada("IA", 'X', posicao_ia, resultado.jogada, jogada_ia_aplicada, &resultado);
            if (!jogada_ia_aplicada) {
                ESP_LOGE(TAG, "Modelo TFLite nao retornou uma jogada valida.");
                mostrar_mensagem("IA TFLite", "INDISPONIVEL", " ");
                estatisticas_finalizar_partida();
                auditoria_jogo_fim("ERRO_IA_JOGADA_INVALIDA", '-');
                vTaskDelay(pdMS_TO_TICKS(1400));
                return;
            }
            estatisticas_registrar_jogada();
        }

        mostrar_tabuleiro();

        if (jogo_verificar_vitoria(&jogo, 'O')) {
            estatisticas_finalizar_partida();
            auditoria_jogo_fim("VITORIA_HUMANO", 'O');
            vTaskDelay(pdMS_TO_TICKS(1000));
            jogo.vitorias_jogador++;
            mostrar_mensagem("     Voce", " ", "    Venceu");
            buzzer_som_vitoria();
            vTaskDelay(pdMS_TO_TICKS(1000));
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }

        if (jogo_verificar_vitoria(&jogo, 'X')) {
            estatisticas_finalizar_partida();
            auditoria_jogo_fim("VITORIA_IA", 'X');
            vTaskDelay(pdMS_TO_TICKS(1000));
            jogo.vitorias_computador++;
            mostrar_mensagem("  Computador", "   Venceu", " ");
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }

        if (jogo_verificar_empate(&jogo)) {
            estatisticas_finalizar_partida();
            auditoria_jogo_fim("EMPATE", '-');
            vTaskDelay(pdMS_TO_TICKS(1000));
            jogo.empates++;
            mostrar_mensagem("     Empate", " ", " ");
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }

        vez_do_jogador = !vez_do_jogador;
    }
}

static void parar_programa(void)
{
    atualizar_lcd_status_ia(true);
    mostrar_mensagem("Programa", "Finalizado", " ");

    while (true) {
        atualizar_lcd_status_ia(false);
        atualizar_lcd_estatisticas(false);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static uint32_t ler_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}
