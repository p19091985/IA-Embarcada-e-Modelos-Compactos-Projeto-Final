#include <stdbool.h>
#include <stdio.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "auto_scan.h"
#include "buzzer.h"
#include "coleta_mpu6050.h"
#include "gesto.h"
#include "gesto_model_data.h"
#include "gesto_tflite.h"
#include "ia_jogo_da_velha.h"
#include "ia_tflite.h"
#include "jogo_da_velha.h"
#include "lcd1602_i2c.h"
#include "leds.h"
#include "mpu6050.h"
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
#define LCD_ENDERECO 0x27
#define LCD_FREQ_HZ 100000

#define INTERVALO_MENU_MS 80

static const char *TAG = "jogo_da_velha";

static ssd1306_t oled;
static lcd1602_t lcd;
static teclado_matricial_t teclado;
static leds_t leds;
static buzzer_t buzzer;
static jogo_estado_t jogo;
static mpu6050_t mpu;
static bool mpu_pronto;
static gesto_tflite_t modelo_gesto;
static ia_tflite_t modelo_ia;

static void mostrar_manual_serial(void);
static void inicializar_mpu6050(void);
static void inicializar_modelos_tinyml(void);
static void mostrar_menu(void);
static void mostrar_menu_coleta(int label);
static void mostrar_tabuleiro(void);
static void mostrar_tabuleiro_com_cursor(int posicao_cursor);
static void mostrar_placar(void);
static void mostrar_autor(void);
static void mostrar_placar_zerado(void);
static void mostrar_mensagem(const char *linha1, const char *linha2, const char *linha3);
static void atualizar_lcd_algoritmo(const ia_resultado_t *resultado);
static char aguardar_tecla(void);
static bool aguardar_jogada_teclado(int *posicao);
static bool aguardar_jogada_jogador(int *posicao);
static void coletar_dados_mpu6050(void);
static void jogar_partida(void);
static void jogar_partida_com_gesto(void);
static bool processar_resultado(char ultimo_jogador);
static void parar_programa(void);
static uint32_t ler_millis(void);

void app_main(void)
{
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
        .endereco = LCD_ENDERECO,
        .frequencia_hz = LCD_FREQ_HZ,
    };

    ESP_ERROR_CHECK(ssd1306_iniciar(&oled, &config_oled));
    inicializar_mpu6050();
    ESP_ERROR_CHECK(lcd1602_iniciar(&lcd, &config_lcd));
    ESP_ERROR_CHECK(teclado_matricial_iniciar(&teclado));
    ESP_ERROR_CHECK(leds_iniciar(&leds));
    ESP_ERROR_CHECK(buzzer_iniciar(&buzzer));
    inicializar_modelos_tinyml();

    jogo_iniciar(&jogo);
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 0, "Jogo da Velha"));
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 1, modelo_ia.pronto ? "IA TFLite" : "IA minimax"));
    buzzer_som_inicial();
    mostrar_menu();

    while (true) {
        char tecla = teclado_matricial_ler(&teclado);

        if (tecla != 0) {
            ESP_LOGI(TAG, "Tecla detectada no menu: %c", tecla);
            buzzer_som_tecla();
        }

        switch (tecla) {
        case '*':
            leds_definir_dourado(&leds, true);
            break;
        case '#':
            leds_definir_dourado(&leds, false);
            break;
        case 'A':
            jogar_partida();
            mostrar_menu();
            break;
        case '8':
            jogar_partida_com_gesto();
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
        case '9':
            coletar_dados_mpu6050();
            mostrar_menu();
            break;
        default:
            break;
        }

        leds_atualizar(&leds);
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_MENU_MS));
    }
}

static void mostrar_manual_serial(void)
{
    ESP_LOGI(TAG, "=== Jogo da Velha ESP32-S3 ===");
    ESP_LOGI(TAG, "O jogador utiliza O e o computador utiliza X.");
    ESP_LOGI(TAG, "Teclas 1 a 9 selecionam a posicao desejada no tabuleiro.");
    ESP_LOGI(TAG, "A=jogar, B=placar, C=encerrar, D=autor, 0=zerar placar.");
    ESP_LOGI(TAG, "Tecla 8 inicia partida com gesto/auto-scan; tecla 9 inicia coleta CSV do MPU6050.");
    ESP_LOGI(TAG, "Na coleta, 0=repouso, 1=confirmar, D=sair.");
    ESP_LOGI(TAG, "* liga LED dourado e # desliga LED dourado.");
    ESP_LOGI(TAG, "OLED exibe menu e tabuleiro; LCD exibe o algoritmo de IA utilizado.");
}

static void inicializar_mpu6050(void)
{
    mpu_pronto = false;
    esp_err_t erro = mpu6050_iniciar(&mpu, oled.barramento);

    if (erro != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 indisponivel (%s); jogo segue pelo teclado.", esp_err_to_name(erro));
        return;
    }

    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    erro = mpu6050_ler_aceleracao(&mpu, &ax, &ay, &az);

    if (erro != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 iniciou, mas a leitura inicial falhou (%s).", esp_err_to_name(erro));
        return;
    }

    mpu_pronto = true;
    ESP_LOGI(TAG, "MPU6050 pronto: ax=%d ay=%d az=%d", ax, ay, az);
}

static void inicializar_modelos_tinyml(void)
{
    esp_err_t erro_gesto = gesto_tflite_iniciar(&modelo_gesto);
    if (erro_gesto != ESP_OK) {
        ESP_LOGW(TAG, "Modelo de gesto indisponivel (%s); heuristica sera usada.", esp_err_to_name(erro_gesto));
    } else {
        ESP_LOGI(TAG, "Modelo de gesto pronto; arena sugerida: %d bytes", GESTO_MODEL_TENSOR_ARENA_BYTES);
    }

    esp_err_t erro_ia = ia_tflite_iniciar(&modelo_ia);
    if (erro_ia != ESP_OK) {
        ESP_LOGW(TAG, "Modelo TFLite do jogo indisponivel (%s); minimax sera usado.", esp_err_to_name(erro_ia));
    } else {
        ESP_LOGI(TAG, "Modelo do jogo pronto; arena sugerida: %d bytes", modelo_ia.arena_bytes);
    }
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

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 6));
}

static void mostrar_menu_coleta(int label)
{
    const char *label_texto = label == COLETA_MPU6050_LABEL_CONFIRMAR ? "LABEL 1" : "LABEL 0";
    const char *descricao = label == COLETA_MPU6050_LABEL_CONFIRMAR ? "CONFIRMAR" : "REPOUSO";
    const char *linhas[] = {
        "COLETA MPU",
        label_texto,
        descricao,
        "0 REPOUSO",
        "1 CONFIRM",
        "D SAIR",
        "CSV SERIAL",
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 7));
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

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 7));
}

static void formatar_linha_com_cursor(int linha, int posicao_cursor, char *saida, size_t tamanho)
{
    char celulas[JOGO_TAMANHO][4];

    for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
        int posicao = linha * JOGO_TAMANHO + coluna + 1;
        char valor = jogo.casas[linha][coluna];

        if (valor == ' ') {
            valor = (char)('0' + posicao);
        }

        if (posicao == posicao_cursor && jogo.casas[linha][coluna] == ' ') {
            snprintf(celulas[coluna], sizeof(celulas[coluna]), "[%c]", valor);
        } else {
            snprintf(celulas[coluna], sizeof(celulas[coluna]), " %c ", valor);
        }
    }

    snprintf(saida, tamanho, "%s|%s|%s", celulas[0], celulas[1], celulas[2]);
}

static void mostrar_tabuleiro_com_cursor(int posicao_cursor)
{
    char linha0[16];
    char linha1[16];
    char linha2[16];

    formatar_linha_com_cursor(0, posicao_cursor, linha0, sizeof(linha0));
    formatar_linha_com_cursor(1, posicao_cursor, linha1, sizeof(linha1));
    formatar_linha_com_cursor(2, posicao_cursor, linha2, sizeof(linha2));

    const char *linhas[] = {
        "AUTO-SCAN",
        "GESTO=OK",
        linha0,
        linha1,
        linha2,
        "1-9 TECLADO",
        "* LUZ # OFF",
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 7));
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

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 6));
}

static void mostrar_autor(void)
{
    const char *linhas[] = {
        " ",
        " ",
        "     Autor",
        " ",
        "  Patrik Lima",
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 5));
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

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 5));
}

static void mostrar_mensagem(const char *linha1, const char *linha2, const char *linha3)
{
    const char *linhas[] = {
        " ",
        linha1,
        linha2,
        linha3,
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 4));
}

static void atualizar_lcd_algoritmo(const ia_resultado_t *resultado)
{
    char linha[17];

    snprintf(linha, sizeof(linha), "IA %s", resultado->nome_curto);
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 0, "Algoritmo IA"));
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 1, linha));
}

static char aguardar_tecla(void)
{
    while (true) {
        char tecla = teclado_matricial_ler(&teclado);

        if (tecla != 0) {
            ESP_LOGI(TAG, "Tecla detectada na partida: %c", tecla);
            buzzer_som_tecla();
            return tecla;
        }

        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

static bool aguardar_jogada_teclado(int *posicao)
{
    if (posicao == NULL) {
        return false;
    }

    while (true) {
        char tecla = aguardar_tecla();

        if (tecla == '*') {
            leds_definir_dourado(&leds, true);
            leds_atualizar(&leds);
            continue;
        }

        if (tecla == '#') {
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

static bool aguardar_jogada_jogador(int *posicao)
{
    if (posicao == NULL) {
        return false;
    }

    if (!mpu_pronto) {
        while (true) {
            char tecla = aguardar_tecla();

            if (tecla == '*') {
                leds_definir_dourado(&leds, true);
                leds_atualizar(&leds);
                continue;
            }

            if (tecla == '#') {
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

    auto_scan_t scan;
    gesto_detector_t detector;
    auto_scan_iniciar(&scan, &jogo, ler_millis());
    gesto_detector_iniciar(&detector);
    gesto_detector_definir_classificador(&detector, gesto_tflite_classificar, &modelo_gesto);

    if (auto_scan_posicao_atual(&scan) == 0) {
        return false;
    }

    mostrar_tabuleiro_com_cursor(auto_scan_posicao_atual(&scan));

    while (true) {
        uint32_t agora_ms = ler_millis();

        if (auto_scan_atualizar(&scan, &jogo, agora_ms)) {
            mostrar_tabuleiro_com_cursor(auto_scan_posicao_atual(&scan));
        }

        char tecla = teclado_matricial_ler(&teclado);
        if (tecla != 0) {
            ESP_LOGI(TAG, "Tecla detectada na partida: %c", tecla);
            buzzer_som_tecla();

            if (tecla == '*') {
                leds_definir_dourado(&leds, true);
                leds_atualizar(&leds);
            } else if (tecla == '#') {
                leds_definir_dourado(&leds, false);
                leds_atualizar(&leds);
            } else if (tecla >= '1' && tecla <= '9') {
                *posicao = tecla - '0';
                return true;
            }
        }

        int16_t ax = 0;
        int16_t ay = 0;
        int16_t az = 0;
        if (mpu6050_ler_aceleracao(&mpu, &ax, &ay, &az) == ESP_OK) {
            gesto_evento_t evento = gesto_detector_processar_amostra(&detector, agora_ms, ax, ay, az);
            if (evento == GESTO_EVENTO_CONFIRMAR) {
                int posicao_atual = auto_scan_posicao_atual(&scan);
                if (jogo_posicao_valida(posicao_atual)) {
                    ESP_LOGI(TAG, "Gesto confirmou a posicao %d", posicao_atual);
                    buzzer_som_tecla();
                    *posicao = posicao_atual;
                    return true;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(COLETA_MPU6050_PERIODO_MS));
    }
}

static void coletar_dados_mpu6050(void)
{
    if (!mpu_pronto) {
        mostrar_mensagem("MPU6050", "INDISPONIVEL", "TECLADO OK");
        vTaskDelay(pdMS_TO_TICKS(1400));
        return;
    }

    coleta_mpu6050_t coleta;
    TaskHandle_t tarefa_coleta = NULL;
    coleta_mpu6050_configurar(&coleta, &mpu);

    BaseType_t criada = xTaskCreate(mpu_data_collection_task,
                                    "mpu_data_collection_task",
                                    4096,
                                    &coleta,
                                    5,
                                    &tarefa_coleta);

    if (criada != pdPASS) {
        mostrar_mensagem("COLETA", "FALHOU", "SEM TASK");
        vTaskDelay(pdMS_TO_TICKS(1400));
        return;
    }

    ESP_LOGI(TAG, "Coleta CSV iniciada. Salve o serial a partir do cabecalho: %s", COLETA_MPU6050_CABECALHO_CSV);
    ESP_LOGI(TAG, "Use 0 para repouso, 1 para confirmar jogada e D para sair da coleta.");
    mostrar_menu_coleta(coleta.label);

    while (coleta.ativa) {
        char tecla = teclado_matricial_ler(&teclado);

        if (tecla != 0) {
            ESP_LOGI(TAG, "Tecla detectada na coleta: %c", tecla);
            buzzer_som_tecla();
        }

        switch (tecla) {
        case '0':
            ESP_ERROR_CHECK(coleta_mpu6050_definir_label(&coleta, COLETA_MPU6050_LABEL_REPOUSO));
            mostrar_menu_coleta(coleta.label);
            break;
        case '1':
            ESP_ERROR_CHECK(coleta_mpu6050_definir_label(&coleta, COLETA_MPU6050_LABEL_CONFIRMAR));
            mostrar_menu_coleta(coleta.label);
            break;
        case 'D':
            coleta_mpu6050_parar(&coleta);
            break;
        default:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(INTERVALO_MENU_MS));
    }

    vTaskDelay(pdMS_TO_TICKS(COLETA_MPU6050_PERIODO_MS * 2));
    ESP_LOGI(TAG, "Coleta CSV encerrada.");
}

static void jogar_partida(void)
{
    bool vez_do_jogador = true;

    jogo_resetar_tabuleiro(&jogo);
    mostrar_tabuleiro();

    while (true) {
        if (vez_do_jogador) {
            int posicao = 0;

            if (!aguardar_jogada_teclado(&posicao)) {
                return;
            }

            if (!jogo_aplicar_posicao(&jogo, posicao, 'O')) {
                continue;
            }

            mostrar_tabuleiro();
            if (processar_resultado('O')) {
                return;
            }
        } else {
            ia_resultado_t resultado = ia_escolher_jogada(&jogo);
            atualizar_lcd_algoritmo(&resultado);
            vTaskDelay(pdMS_TO_TICKS(350));
            jogo_aplicar_jogada(&jogo, resultado.jogada, 'X');
            mostrar_tabuleiro();

            if (processar_resultado('X')) {
                return;
            }
        }

        vez_do_jogador = !vez_do_jogador;
    }
}

static void jogar_partida_com_gesto(void)
{
    bool vez_do_jogador = true;

    jogo_resetar_tabuleiro(&jogo);
    mostrar_tabuleiro();

    while (true) {
        if (vez_do_jogador) {
            int posicao = 0;

            if (!aguardar_jogada_jogador(&posicao)) {
                return;
            }

            if (!jogo_aplicar_posicao(&jogo, posicao, 'O')) {
                ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 0, "Casa ocupada"));
                ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 1, "Tente outra"));
                continue;
            }

            mostrar_tabuleiro();
            if (processar_resultado('O')) {
                return;
            }
        } else {
            ia_resultado_t resultado = ia_tflite_escolher_jogada(&modelo_ia, &jogo);
            atualizar_lcd_algoritmo(&resultado);
            vTaskDelay(pdMS_TO_TICKS(350));
            jogo_aplicar_jogada(&jogo, resultado.jogada, 'X');
            mostrar_tabuleiro();

            if (processar_resultado('X')) {
                return;
            }
        }

        vez_do_jogador = !vez_do_jogador;
    }
}

static bool processar_resultado(char ultimo_jogador)
{
    if (jogo_verificar_vitoria(&jogo, ultimo_jogador)) {
        if (ultimo_jogador == 'O') {
            jogo.vitorias_jogador++;
            mostrar_mensagem("     Voce", " ", "    Venceu");
            buzzer_som_vitoria();
        } else {
            jogo.vitorias_computador++;
            mostrar_mensagem("  Computador", "   Venceu", " ");
        }

        vTaskDelay(pdMS_TO_TICKS(1600));
        return true;
    }

    if (jogo_verificar_empate(&jogo)) {
        jogo.empates++;
        mostrar_mensagem("     Empate", " ", " ");
        vTaskDelay(pdMS_TO_TICKS(1600));
        return true;
    }

    return false;
}

static void parar_programa(void)
{
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 0, "Programa"));
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 1, "Finalizado"));
    mostrar_mensagem("Programa", "Finalizado", " ");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static uint32_t ler_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}
