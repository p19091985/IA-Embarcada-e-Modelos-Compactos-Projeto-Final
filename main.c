#include <stdbool.h>
#include <stdint.h>
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
#include "jogo_da_velha.h"
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
#define LCD_PRESENCA_ENDERECO 0x3F
#define LCD_ESTATISTICAS_ENDERECO 0x26
#define LCD_FREQ_HZ 100000

#define INTERVALO_MENU_MS 80
#define INTERVALO_TECLADO_MS 20
#define INTERVALO_COLETA_HCSR04_MS 100
#define INTERVALO_PRESENCA_AMOSTRA_MS 250
#define INTERVALO_PRESENCA_LOG_MS 1000
#define INTERVALO_LCD_AUTORES_MS 350
#define INTERVALO_LCD_PRESENCA_MS 500
#define INTERVALO_LCD_ESTATISTICAS_MS 500
#define TAREFA_PRESENCA_STACK_BYTES 6144
#define TAREFA_ESTATISTICAS_STACK_BYTES 3072
#define LCD_AUTORES_TEXTO "Autores: Patrik, Janiel e Joao"
#define COLETA_HCSR04_CABECALHO_CSV "timestamp_ms,distancia_cm,eco_us,label"
#define COLETA_HCSR04_LABEL_AUSENTE 0
#define COLETA_HCSR04_LABEL_PRESENTE 1

static const char *TAG = "ProjetoFinal_Patrik_Janiel_Joao";

typedef struct {
    bool leitura_valida;
    bool presente;
    uint16_t distancia_cm;
    uint32_t eco_us;
    int32_t score;
    uint32_t atualizado_ms;
} estado_presenca_t;

typedef struct {
    bool em_andamento;
    uint32_t inicio_ms;
    uint32_t ultima_duracao_ms;
    uint32_t duracao_total_ms;
    uint32_t partidas_finalizadas;
    uint8_t jogadas_atual;
    uint8_t ultimas_jogadas;
    char ultimo_resultado;
} estatisticas_jogo_t;

static ssd1306_t oled;
static lcd1602_t lcd;
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
static bool lcd_presenca_pronto;
static bool lcd_estatisticas_pronto;
static bool luz_dourada_manual;
static int ldr_histerese_estado;
static bool presenca_log_iniciado;
static bool ultima_presenca_logada;
static uint32_t ultima_amostra_presenca_ms;
static uint32_t ultimo_log_presenca_ms;
static bool lcd_status_iniciado;
static uint32_t ultimo_lcd_status_ms;
static uint32_t ultimo_lcd_presenca_ms;
static uint32_t ultimo_lcd_estatisticas_ms;
static uint32_t lcd_autores_passo;
static SemaphoreHandle_t mutex_lcd_i2c;
static estado_presenca_t estado_presenca;
static estatisticas_jogo_t estatisticas_jogo;

static void mostrar_hello_world_treinamento(void);
static void imprimir_acuracia_modelo(const char *rotulo, int valor_permyriad);
static void mostrar_manual_serial(void);
static void inicializar_lcds_auxiliares(void);
static void inicializar_sensores(void);
static void inicializar_modelo_ia(void);
static void iniciar_tarefas_background(void);
static void exibir_oled_linhas(const char *origem, const char *linhas[], int quantidade);
static void espelhar_oled_console(const char *origem, const char *linhas[], int quantidade);
static void mostrar_menu(void);
static void mostrar_menu_coleta_hcsr04(int label);
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
static void lcd_escrever_duas_linhas(lcd1602_t *lcd_alvo, const char *linha0, const char *linha1);
static bool aguardar_jogada_teclado(int *posicao);
static void atualizar_presenca_ambiente(void);
static void atualizar_luz_automatica(void);
static void coletar_dados_hcsr04(void) __attribute__((unused));
static void tarefa_presenca_hcsr04(void *parametro);
static void tarefa_lcd_estatisticas(void *parametro);
static void estatisticas_iniciar_partida(void);
static void estatisticas_registrar_jogada(void);
static void estatisticas_finalizar_partida(char resultado);
static void formatar_tempo_curto(uint32_t duracao_ms, char saida[6]);
static int32_t limitar_score_lcd(int32_t score);
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

    lcd1602_config_t config_lcd = {
        .porta_i2c = LCD_PORTA_I2C,
        .pino_sda = LCD_SDA,
        .pino_scl = LCD_SCL,
        .endereco = LCD_IA_ENDERECO,
        .frequencia_hz = LCD_FREQ_HZ,
    };

    ESP_ERROR_CHECK(ssd1306_iniciar(&oled, &config_oled));
    ESP_ERROR_CHECK(lcd1602_iniciar(&lcd, &config_lcd));
    mutex_lcd_i2c = xSemaphoreCreateMutex();
    inicializar_lcds_auxiliares();
    ESP_ERROR_CHECK(teclado_matricial_iniciar(&teclado));
    ESP_ERROR_CHECK(leds_iniciar(&leds));
    ESP_ERROR_CHECK(buzzer_iniciar(&buzzer));
    inicializar_sensores();
    inicializar_modelo_ia();

    jogo_iniciar(&jogo);
    lcd_status_iniciado = false;
    ultimo_lcd_status_ms = 0;
    lcd_autores_passo = 0;
    atualizar_lcd_status_ia(true);
    atualizar_lcd_presenca(true);
    atualizar_lcd_estatisticas(true);
    iniciar_tarefas_background();
    buzzer_som_inicial();
    mostrar_menu();

    while (true) {
        atualizar_luz_automatica();
        atualizar_lcd_status_ia(false);
        char tecla = teclado_matricial_ler(&teclado);

        if (tecla != 0) {
            ESP_LOGI(TAG, "Tecla detectada no menu: %c", tecla);
            buzzer_som_tecla();
        }

        switch (tecla) {
        case '*':
            luz_dourada_manual = true;
            leds_definir_dourado(&leds, true);
            break;
        case '#':
            luz_dourada_manual = true;
            leds_definir_dourado(&leds, false);
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
    ESP_LOGI(TAG, "HC-SR04 rodando classificador TFLite continuamente em background.");
    ESP_LOGI(TAG, "LCDs auxiliares: presenca em 0x%02X e estatisticas em 0x%02X.",
             LCD_PRESENCA_ENDERECO,
             LCD_ESTATISTICAS_ENDERECO);
}

static void inicializar_lcds_auxiliares(void)
{
    lcd_presenca_pronto = false;
    lcd_estatisticas_pronto = false;
    ultimo_lcd_presenca_ms = 0;
    ultimo_lcd_estatisticas_ms = 0;

    esp_err_t erro = lcd1602_iniciar_no_barramento(&lcd_presenca,
                                                   lcd.barramento,
                                                   LCD_PRESENCA_ENDERECO,
                                                   LCD_FREQ_HZ);
    if (erro == ESP_OK) {
        lcd_presenca_pronto = true;
        ESP_LOGI(TAG, "LCD de presenca pronto no endereco 0x%02X.", LCD_PRESENCA_ENDERECO);
    } else {
        ESP_LOGW(TAG,
                 "LCD de presenca ausente em 0x%02X (%s); jogo segue normalmente.",
                 LCD_PRESENCA_ENDERECO,
                 esp_err_to_name(erro));
    }

    erro = lcd1602_iniciar_no_barramento(&lcd_estatisticas,
                                         lcd.barramento,
                                         LCD_ESTATISTICAS_ENDERECO,
                                         LCD_FREQ_HZ);
    if (erro == ESP_OK) {
        lcd_estatisticas_pronto = true;
        ESP_LOGI(TAG, "LCD de estatisticas pronto no endereco 0x%02X.", LCD_ESTATISTICAS_ENDERECO);
    } else {
        ESP_LOGW(TAG,
                 "LCD de estatisticas ausente em 0x%02X (%s); jogo segue normalmente.",
                 LCD_ESTATISTICAS_ENDERECO,
                 esp_err_to_name(erro));
    }
}

static void inicializar_sensores(void)
{
    sensor_hcsr04_pronto = false;
    sensor_luz_pronto = false;
    estado_presenca = (estado_presenca_t){0};
    estatisticas_jogo = (estatisticas_jogo_t){
        .ultimo_resultado = '-',
    };
    presenca_log_iniciado = false;
    ultima_presenca_logada = false;
    ultima_amostra_presenca_ms = 0;
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

static void iniciar_tarefas_background(void)
{
    BaseType_t criada = xTaskCreate(tarefa_presenca_hcsr04,
                                    "presenca_hcsr04",
                                    TAREFA_PRESENCA_STACK_BYTES,
                                    NULL,
                                    4,
                                    NULL);
    if (criada != pdPASS) {
        ESP_LOGW(TAG, "Nao foi possivel iniciar a task de presenca; jogo segue sem bloquear.");
    }

    criada = xTaskCreate(tarefa_lcd_estatisticas,
                         "lcd_estatisticas",
                         TAREFA_ESTATISTICAS_STACK_BYTES,
                         NULL,
                         2,
                         NULL);
    if (criada != pdPASS) {
        ESP_LOGW(TAG, "Nao foi possivel iniciar a task de estatisticas; jogo segue sem bloquear.");
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
        "D - Creditos",
        "0 - Zerar  ",
        "Escolha ",
    };

    exibir_oled_linhas("MENU", linhas, 6);
}

static void mostrar_menu_coleta_hcsr04(int label)
{
    const char *descricao = label == COLETA_HCSR04_LABEL_PRESENTE ? "1 PRESENTE" : "0 AUSENTE";
    const char *linhas[] = {
        "COLETA HC-SR04",
        descricao,
        "0 AUSENTE",
        "1 PRESENTE",
        "D SAIR",
        "CSV SERIAL",
    };

    exibir_oled_linhas("COLETA_HCSR04", linhas, 6);
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
        " ",
        " Patrik, Janiel e Joao",
    };

    exibir_oled_linhas("CREDITOS", linhas, 5);
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
    uint32_t agora_ms = ler_millis();
    if (!forcar && lcd_status_iniciado && (agora_ms - ultimo_lcd_status_ms) < INTERVALO_LCD_AUTORES_MS) {
        return;
    }

    char autores[LCD1602_COLUNAS + 1];
    lcd1602_formatar_janela_scroll(LCD_AUTORES_TEXTO, lcd_autores_passo, autores);

    lcd_escrever_duas_linhas(&lcd, texto_lcd_algoritmo(NULL), autores);

    lcd_autores_passo++;
    ultimo_lcd_status_ms = agora_ms;
    lcd_status_iniciado = true;
}

static void atualizar_lcd_algoritmo(const ia_resultado_t *resultado)
{
    (void)resultado;
    atualizar_lcd_status_ia(true);
}

static const char *texto_lcd_algoritmo(const ia_resultado_t *resultado)
{
    (void)resultado;
    return "TFLite";
}

static void atualizar_lcd_presenca(bool forcar)
{
    if (!lcd_presenca_pronto) {
        return;
    }

    uint32_t agora_ms = ler_millis();
    if (!forcar && (agora_ms - ultimo_lcd_presenca_ms) < INTERVALO_LCD_PRESENCA_MS) {
        return;
    }

    char linha0[LCD1602_COLUNAS + 1];
    char linha1[LCD1602_COLUNAS + 1];

    if (!sensor_hcsr04_pronto) {
        snprintf(linha0, sizeof(linha0), "Presenca");
        snprintf(linha1, sizeof(linha1), "Sensor ausente");
    } else if (!estado_presenca.leitura_valida) {
        snprintf(linha0, sizeof(linha0), "Presenca HC-SR04");
        snprintf(linha1, sizeof(linha1), "Aguardando...");
    } else {
        int32_t score_lcd = limitar_score_lcd(estado_presenca.score);
        snprintf(linha0,
                 sizeof(linha0),
                 "Pres: %s",
                 estado_presenca.presente ? "PRESENTE" : "AUSENTE");
        snprintf(linha1,
                 sizeof(linha1),
                 "D:%03ucm S:%4ld",
                 (unsigned int)estado_presenca.distancia_cm,
                 (long)score_lcd);
    }

    lcd_escrever_duas_linhas(&lcd_presenca, linha0, linha1);
    ultimo_lcd_presenca_ms = agora_ms;
}

static void atualizar_lcd_estatisticas(bool forcar)
{
    if (!lcd_estatisticas_pronto) {
        return;
    }

    uint32_t agora_ms = ler_millis();
    if (!forcar && (agora_ms - ultimo_lcd_estatisticas_ms) < INTERVALO_LCD_ESTATISTICAS_MS) {
        return;
    }

    uint32_t duracao_ms = estatisticas_jogo.em_andamento
                              ? agora_ms - estatisticas_jogo.inicio_ms
                              : estatisticas_jogo.ultima_duracao_ms;
    uint32_t media_ms = estatisticas_jogo.partidas_finalizadas > 0
                            ? estatisticas_jogo.duracao_total_ms / estatisticas_jogo.partidas_finalizadas
                            : 0;
    uint32_t partidas_lcd = estatisticas_jogo.partidas_finalizadas > 99
                                ? 99
                                : estatisticas_jogo.partidas_finalizadas;
    unsigned int jogadas_atuais_lcd = estatisticas_jogo.jogadas_atual > 99
                                          ? 99
                                          : estatisticas_jogo.jogadas_atual;
    unsigned int ultimas_jogadas_lcd = estatisticas_jogo.ultimas_jogadas > 99
                                           ? 99
                                           : estatisticas_jogo.ultimas_jogadas;

    char tempo[6];
    char media[6];
    char linha0[LCD1602_COLUNAS + 1];
    char linha1[LCD1602_COLUNAS + 1];

    formatar_tempo_curto(duracao_ms, tempo);
    formatar_tempo_curto(media_ms, media);

    if (estatisticas_jogo.em_andamento) {
        snprintf(linha0, sizeof(linha0), "Jogo %s P%02lu", tempo, (unsigned long)partidas_lcd);
        snprintf(linha1, sizeof(linha1), "Jog:%02u Med %s", jogadas_atuais_lcd, media);
    } else {
        snprintf(linha0,
                 sizeof(linha0),
                 "Ult %s %c P%02lu",
                 tempo,
                 estatisticas_jogo.ultimo_resultado,
                 (unsigned long)partidas_lcd);
        snprintf(linha1, sizeof(linha1), "Jog:%02u Med %s", ultimas_jogadas_lcd, media);
    }

    lcd_escrever_duas_linhas(&lcd_estatisticas, linha0, linha1);
    ultimo_lcd_estatisticas_ms = agora_ms;
}

static void lcd_escrever_duas_linhas(lcd1602_t *lcd_alvo, const char *linha0, const char *linha1)
{
    bool bloqueado = false;
    if (mutex_lcd_i2c != NULL) {
        bloqueado = xSemaphoreTake(mutex_lcd_i2c, pdMS_TO_TICKS(200)) == pdTRUE;
        if (!bloqueado) {
            return;
        }
    }

    (void)lcd1602_escrever_linha(lcd_alvo, 0, linha0);
    (void)lcd1602_escrever_linha(lcd_alvo, 1, linha1);

    if (bloqueado) {
        xSemaphoreGive(mutex_lcd_i2c);
    }
}

static bool aguardar_jogada_teclado(int *posicao)
{
    if (posicao == NULL) {
        return false;
    }

    while (true) {
        atualizar_lcd_status_ia(false);
        char tecla = teclado_matricial_ler(&teclado);
        if (tecla == 0) {
            leds_atualizar(&leds);
            vTaskDelay(pdMS_TO_TICKS(INTERVALO_TECLADO_MS));
            continue;
        }

        ESP_LOGI(TAG, "Tecla detectada na partida: %c", tecla);
        buzzer_som_tecla();

        if (tecla == '*') {
            luz_dourada_manual = true;
            leds_definir_dourado(&leds, true);
            leds_atualizar(&leds);
            continue;
        }

        if (tecla == '#') {
            luz_dourada_manual = true;
            leds_definir_dourado(&leds, false);
            leds_atualizar(&leds);
            continue;
        }

        if (tecla >= '1' && tecla <= '9') {
            *posicao = tecla - '0';
            return true;
        }
    }
}

static void atualizar_presenca_ambiente(void)
{
    if (!sensor_hcsr04_pronto) {
        return;
    }

    uint32_t agora_ms = ler_millis();
    if ((agora_ms - ultima_amostra_presenca_ms) < INTERVALO_PRESENCA_AMOSTRA_MS) {
        return;
    }
    ultima_amostra_presenca_ms = agora_ms;

    uint16_t distancia_cm = 0;
    uint32_t eco_us = 0;
    esp_err_t erro = hcsr04_ler_distancia(&sensor_hcsr04, &distancia_cm, &eco_us);
    if (erro != ESP_OK) {
        return;
    }

    bool presente = presenca_tflite_classificar(&modelo_presenca, distancia_cm, eco_us);
    estado_presenca.leitura_valida = true;
    estado_presenca.presente = presente;
    estado_presenca.distancia_cm = distancia_cm;
    estado_presenca.eco_us = eco_us;
    estado_presenca.score = modelo_presenca.ultimo_score;
    estado_presenca.atualizado_ms = agora_ms;

    bool pode_logar = !presenca_log_iniciado || (agora_ms - ultimo_log_presenca_ms) >= INTERVALO_PRESENCA_LOG_MS;
    if (pode_logar && (!presenca_log_iniciado || presente != ultima_presenca_logada)) {
        ESP_LOGI(TAG,
                 "\033[1;36m[Sensor HC-SR04]\033[0m Detectei que voce esta: %s (dist: %u cm | IA_Score: %ld)",
                 presente ? "\033[1;32mPRESENTE\033[0m" : "\033[1;31mAUSENTE\033[0m",
                 (unsigned int)distancia_cm,
                 (long)modelo_presenca.ultimo_score);
        presenca_log_iniciado = true;
        ultima_presenca_logada = presente;
        ultimo_log_presenca_ms = agora_ms;
    }
}

static void atualizar_luz_automatica(void)
{
    if (!sensor_luz_pronto || luz_dourada_manual) {
        return;
    }

    int leitura = 0;
    if (ldr_ler_bruto(&sensor_luz, &leitura) != ESP_OK) {
        return;
    }

    int limiar_liga = LDR_LIMIAR_ESCURO - 200;
    int limiar_desliga = LDR_LIMIAR_ESCURO + 200;

    if (ldr_histerese_estado == 0 && leitura < limiar_liga) {
        ldr_histerese_estado = 1;
        leds_definir_dourado(&leds, true);
    } else if (ldr_histerese_estado == 1 && leitura > limiar_desliga) {
        ldr_histerese_estado = 0;
        leds_definir_dourado(&leds, false);
    }
}

static void coletar_dados_hcsr04(void)
{
    if (!sensor_hcsr04_pronto) {
        mostrar_mensagem("HC-SR04", "INDISPONIVEL", "TECLADO OK");
        vTaskDelay(pdMS_TO_TICKS(1400));
        return;
    }

    int label = COLETA_HCSR04_LABEL_AUSENTE;
    printf("\n\033[1;33m--- Iniciando Coleta de Dados do HC-SR04 ---\033[0m\n");
    printf("%s\n", COLETA_HCSR04_CABECALHO_CSV);
    ESP_LOGI(TAG, "Aperte 0 (Ausente), 1 (Presente) ou D (Sair do modo).");
    mostrar_menu_coleta_hcsr04(label);

    while (true) {
        atualizar_luz_automatica();
        atualizar_lcd_status_ia(false);

        char tecla = teclado_matricial_ler(&teclado);
        if (tecla != 0) {
            ESP_LOGI(TAG, "Tecla detectada na coleta: %c", tecla);
            buzzer_som_tecla();
        }

        if (tecla == '0') {
            label = COLETA_HCSR04_LABEL_AUSENTE;
            mostrar_menu_coleta_hcsr04(label);
        } else if (tecla == '1') {
            label = COLETA_HCSR04_LABEL_PRESENTE;
            mostrar_menu_coleta_hcsr04(label);
        } else if (tecla == 'D') {
            ESP_LOGI(TAG, "Coleta HC-SR04 encerrada.");
            return;
        }

        uint16_t distancia_cm = 0;
        uint32_t eco_us = 0;
        if (hcsr04_ler_distancia(&sensor_hcsr04, &distancia_cm, &eco_us) == ESP_OK) {
            printf("%lu,%u,%lu,%d\n",
                   (unsigned long)ler_millis(),
                   (unsigned int)distancia_cm,
                   (unsigned long)eco_us,
                   label);
        }

        vTaskDelay(pdMS_TO_TICKS(INTERVALO_COLETA_HCSR04_MS));
    }
}

static void tarefa_presenca_hcsr04(void *parametro)
{
    (void)parametro;

    while (true) {
        atualizar_presenca_ambiente();
        atualizar_lcd_presenca(false);
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_PRESENCA_AMOSTRA_MS));
    }
}

static void tarefa_lcd_estatisticas(void *parametro)
{
    (void)parametro;

    while (true) {
        atualizar_lcd_estatisticas(false);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void estatisticas_iniciar_partida(void)
{
    estatisticas_jogo.em_andamento = true;
    estatisticas_jogo.inicio_ms = ler_millis();
    estatisticas_jogo.jogadas_atual = 0;
    atualizar_lcd_estatisticas(true);
}

static void estatisticas_registrar_jogada(void)
{
    if (estatisticas_jogo.jogadas_atual < UINT8_MAX) {
        estatisticas_jogo.jogadas_atual++;
    }
    atualizar_lcd_estatisticas(true);
}

static void estatisticas_finalizar_partida(char resultado)
{
    uint32_t agora_ms = ler_millis();
    uint32_t duracao_ms = estatisticas_jogo.em_andamento
                               ? agora_ms - estatisticas_jogo.inicio_ms
                               : estatisticas_jogo.ultima_duracao_ms;

    estatisticas_jogo.em_andamento = false;
    estatisticas_jogo.ultima_duracao_ms = duracao_ms;
    estatisticas_jogo.duracao_total_ms += duracao_ms;
    estatisticas_jogo.partidas_finalizadas++;
    estatisticas_jogo.ultimas_jogadas = estatisticas_jogo.jogadas_atual;
    estatisticas_jogo.ultimo_resultado = resultado;
    atualizar_lcd_estatisticas(true);
}

static void formatar_tempo_curto(uint32_t duracao_ms, char saida[6])
{
    uint32_t segundos = duracao_ms / 1000U;
    uint32_t minutos = (segundos / 60U) % 100U;
    segundos %= 60U;

    snprintf(saida, 6, "%02lu:%02lu", (unsigned long)minutos, (unsigned long)segundos);
}

static int32_t limitar_score_lcd(int32_t score)
{
    if (score > 9999) {
        return 9999;
    }
    if (score < -999) {
        return -999;
    }
    return score;
}

static void jogar_partida(void)
{
    bool vez_do_jogador = true;

    estatisticas_iniciar_partida();
    jogo_resetar_tabuleiro(&jogo);
    mostrar_tabuleiro();

    while (true) {
        atualizar_lcd_status_ia(false);
        if (vez_do_jogador) {
            int posicao = 0;

            if (!aguardar_jogada_teclado(&posicao)) {
                return;
            }

            if (!jogo_aplicar_posicao(&jogo, posicao, 'O')) {
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

            if (!jogo_aplicar_jogada(&jogo, resultado.jogada, 'X')) {
                ESP_LOGE(TAG, "Modelo TFLite nao retornou uma jogada valida.");
                mostrar_mensagem("IA TFLite", "INDISPONIVEL", " ");
                estatisticas_finalizar_partida('-');
                vTaskDelay(pdMS_TO_TICKS(1400));
                return;
            }
            estatisticas_registrar_jogada();
        }

        mostrar_tabuleiro();

        if (jogo_verificar_vitoria(&jogo, 'O')) {
            estatisticas_finalizar_partida('O');
            vTaskDelay(pdMS_TO_TICKS(1000));
            jogo.vitorias_jogador++;
            mostrar_mensagem("     Voce", " ", "    Venceu");
            buzzer_som_vitoria();
            vTaskDelay(pdMS_TO_TICKS(1000));
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }

        if (jogo_verificar_vitoria(&jogo, 'X')) {
            estatisticas_finalizar_partida('X');
            vTaskDelay(pdMS_TO_TICKS(1000));
            jogo.vitorias_computador++;
            mostrar_mensagem("  Computador", "   Venceu", " ");
            vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }

        if (jogo_verificar_empate(&jogo)) {
            estatisticas_finalizar_partida('E');
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
    leds_definir_status_parado(&leds);
    atualizar_lcd_status_ia(true);
    mostrar_mensagem("Programa", "Finalizado", " ");

    while (true) {
        atualizar_lcd_status_ia(false);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static uint32_t ler_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}
