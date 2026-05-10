#include <stdbool.h>
#include <stdio.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "buzzer.h"
#include "ia_jogo_da_velha.h"
#include "jogo_da_velha.h"
#include "lcd1602_i2c.h"
#include "leds.h"
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

static void mostrar_manual_serial(void);
static void mostrar_menu(void);
static void mostrar_tabuleiro(void);
static void mostrar_placar(void);
static void mostrar_autor(void);
static void mostrar_mensagem(const char *linha1, const char *linha2, const char *linha3);
static void atualizar_lcd_algoritmo(const ia_resultado_t *resultado);
static char aguardar_tecla(void);
static void jogar_partida(void);
static bool processar_resultado(char ultimo_jogador);
static void parar_programa(void);

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
    ESP_ERROR_CHECK(lcd1602_iniciar(&lcd, &config_lcd));
    ESP_ERROR_CHECK(teclado_matricial_iniciar(&teclado));
    ESP_ERROR_CHECK(leds_iniciar(&leds));
    ESP_ERROR_CHECK(buzzer_iniciar(&buzzer));

    jogo_iniciar(&jogo);
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 0, "Jogo da Velha"));
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 1, "IA pronta"));
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
            mostrar_mensagem("PLACAR", "ZERADO", " ");
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
    ESP_LOGI(TAG, "=== Manual do Jogo da Velha ESP32-S3 ===");
    ESP_LOGI(TAG, "Voce joga com O e o computador joga com X.");
    ESP_LOGI(TAG, "Teclas 1 a 9 escolhem a posicao no tabuleiro.");
    ESP_LOGI(TAG, "A joga, B mostra placar, C finaliza, D mostra autor, 0 zera placar.");
    ESP_LOGI(TAG, "* liga LED dourado e # desliga LED dourado.");
    ESP_LOGI(TAG, "OLED mostra menu/tabuleiro; LCD mostra o algoritmo de IA.");
}

static void mostrar_menu(void)
{
    const char *linhas[] = {
        "A-JOGAR",
        "B-PLACAR",
        "C-SAIR",
        "D-AUTOR",
        "0-ZERAR",
        "* LUZ  # OFF",
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 6));
}

static void mostrar_tabuleiro(void)
{
    char linha0[16];
    char linha1[16];
    char linha2[16];

    jogo_formatar_linha_tabuleiro(&jogo, 0, linha0, sizeof(linha0));
    jogo_formatar_linha_tabuleiro(&jogo, 1, linha1, sizeof(linha1));
    jogo_formatar_linha_tabuleiro(&jogo, 2, linha2, sizeof(linha2));

    const char *linhas[] = {
        "JOGO DA VELHA",
        " ",
        linha0,
        "-+-+-",
        linha1,
        "-+-+-",
        linha2,
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 7));
}

static void mostrar_placar(void)
{
    char jogador[17];
    char computador[17];
    char empates[17];

    snprintf(jogador, sizeof(jogador), "JOGADOR %u", jogo.vitorias_jogador);
    snprintf(computador, sizeof(computador), "COMPUTADOR %u", jogo.vitorias_computador);
    snprintf(empates, sizeof(empates), "EMPATES %u", jogo.empates);

    const char *linhas[] = {
        "PLACAR",
        " ",
        jogador,
        computador,
        empates,
        "A-JOGAR",
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 6));
}

static void mostrar_autor(void)
{
    const char *linhas[] = {
        "AUTOR",
        " ",
        "PATRIK LIMA",
        " ",
        "ESP32-S3",
        "ESP-IDF",
    };

    ESP_ERROR_CHECK(ssd1306_mostrar_linhas(&oled, linhas, 6));
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

static void jogar_partida(void)
{
    bool vez_do_jogador = true;

    jogo_resetar_tabuleiro(&jogo);
    mostrar_tabuleiro();

    while (true) {
        if (vez_do_jogador) {
            char tecla = aguardar_tecla();

            if (tecla == '*') {
                leds_definir_dourado(&leds, true);
                continue;
            }

            if (tecla == '#') {
                leds_definir_dourado(&leds, false);
                continue;
            }

            if (tecla < '1' || tecla > '9') {
                continue;
            }

            if (!jogo_aplicar_posicao(&jogo, tecla - '0', 'O')) {
                ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 0, "Casa ocupada"));
                ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 1, "Tente outra"));
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

static bool processar_resultado(char ultimo_jogador)
{
    if (jogo_verificar_vitoria(&jogo, ultimo_jogador)) {
        if (ultimo_jogador == 'O') {
            jogo.vitorias_jogador++;
            mostrar_mensagem("VOCE", "VENCEU", " ");
            buzzer_som_vitoria();
        } else {
            jogo.vitorias_computador++;
            mostrar_mensagem("COMPUTADOR", "VENCEU", " ");
        }

        vTaskDelay(pdMS_TO_TICKS(1600));
        return true;
    }

    if (jogo_verificar_empate(&jogo)) {
        jogo.empates++;
        mostrar_mensagem("EMPATE", " ", " ");
        vTaskDelay(pdMS_TO_TICKS(1600));
        return true;
    }

    return false;
}

static void parar_programa(void)
{
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 0, "Programa"));
    ESP_ERROR_CHECK(lcd1602_escrever_linha(&lcd, 1, "Finalizado"));
    mostrar_mensagem("PROGRAMA", "FINALIZADO", " ");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
