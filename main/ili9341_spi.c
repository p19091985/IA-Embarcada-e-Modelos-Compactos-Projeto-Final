#include "ili9341_spi.h"

#include <ctype.h>
#include <string.h>

#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fonte_8x8.h"

#define ILI9341_PRETO 0x0000
#define ILI9341_BRANCO 0xFFFF
#define ILI9341_CIANO 0x07FF
#define ILI9341_VERDE 0x07E0
#define ILI9341_AZUL_ESCURO 0x0014

#define ILI9341_ESCALA_TEXTO 2
#define ILI9341_ALTURA_FONTE (8 * ILI9341_ESCALA_TEXTO)
#define ILI9341_LARGURA_FONTE (8 * ILI9341_ESCALA_TEXTO)
#define ILI9341_MARGEM_X 12
#define ILI9341_MARGEM_Y 16
#define ILI9341_ESPACO_LINHA 8

static const char *TAG = "ili9341";

static esp_err_t ili9341_comando(ili9341_t *display, uint8_t comando);
static esp_err_t ili9341_dados(ili9341_t *display, const void *dados, int tamanho);
static esp_err_t ili9341_configurar_janela(ili9341_t *display, int x0, int y0, int x1, int y1);
static esp_err_t ili9341_preencher_retangulo(ili9341_t *display, int x, int y, int largura, int altura, uint16_t cor);
static void ili9341_desenhar_texto(ili9341_t *display, int x, int y, const char *texto, uint16_t cor);
static int indice_fonte(char caractere);

esp_err_t ili9341_iniciar(ili9341_t *display, const ili9341_config_t *configuracao)
{
    display->pino_dc = configuracao->pino_dc;

    gpio_config_t config_dc = {
        .pin_bit_mask = 1ULL << configuracao->pino_dc,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config_dc), TAG, "falha ao configurar D/C");

    spi_bus_config_t config_barramento = {
        .mosi_io_num = configuracao->pino_mosi,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = configuracao->pino_sck,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = ILI9341_LARGURA * 40 * sizeof(uint16_t),
    };

    esp_err_t resultado = spi_bus_initialize(configuracao->host, &config_barramento, SPI_DMA_CH_AUTO);
    if (resultado != ESP_OK && resultado != ESP_ERR_INVALID_STATE) {
        return resultado;
    }

    spi_device_interface_config_t config_dispositivo = {
        .clock_speed_hz = configuracao->frequencia_hz,
        .mode = 0,
        .spics_io_num = configuracao->pino_cs,
        .queue_size = 1,
    };

    ESP_RETURN_ON_ERROR(spi_bus_add_device(configuracao->host, &config_dispositivo, &display->dispositivo), TAG, "falha ao adicionar ILI9341");

    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x01), TAG, "falha reset");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x28), TAG, "falha display off");

    const uint8_t pixel_format[] = {0x55};
    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x3A), TAG, "falha pixel format cmd");
    ESP_RETURN_ON_ERROR(ili9341_dados(display, pixel_format, sizeof(pixel_format)), TAG, "falha pixel format");

    const uint8_t madctl[] = {0x28};
    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x36), TAG, "falha madctl cmd");
    ESP_RETURN_ON_ERROR(ili9341_dados(display, madctl, sizeof(madctl)), TAG, "falha madctl");

    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x11), TAG, "falha sleep out");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x29), TAG, "falha display on");

    return ili9341_limpar(display, ILI9341_PRETO);
}

esp_err_t ili9341_limpar(ili9341_t *display, uint16_t cor)
{
    return ili9341_preencher_retangulo(display, 0, 0, ILI9341_LARGURA, ILI9341_ALTURA, cor);
}

esp_err_t ili9341_mostrar_linhas(ili9341_t *display, const char *linhas[], int quantidade)
{
    ESP_RETURN_ON_ERROR(ili9341_limpar(display, ILI9341_PRETO), TAG, "falha limpar tela");
    ESP_RETURN_ON_ERROR(ili9341_preencher_retangulo(display, 0, 0, ILI9341_LARGURA, 42, ILI9341_AZUL_ESCURO), TAG, "falha topo");

    for (int i = 0; i < quantidade && i < 8; i++) {
        uint16_t cor = i == 0 ? ILI9341_CIANO : ILI9341_BRANCO;
        ili9341_desenhar_texto(
            display,
            ILI9341_MARGEM_X,
            ILI9341_MARGEM_Y + (i * (ILI9341_ALTURA_FONTE + ILI9341_ESPACO_LINHA)),
            linhas[i],
            cor
        );
    }

    return ESP_OK;
}

static esp_err_t ili9341_comando(ili9341_t *display, uint8_t comando)
{
    gpio_set_level(display->pino_dc, 0);
    spi_transaction_t transacao = {
        .length = 8,
        .tx_buffer = &comando,
    };

    return spi_device_polling_transmit(display->dispositivo, &transacao);
}

static esp_err_t ili9341_dados(ili9341_t *display, const void *dados, int tamanho)
{
    if (tamanho <= 0) {
        return ESP_OK;
    }

    gpio_set_level(display->pino_dc, 1);
    spi_transaction_t transacao = {
        .length = tamanho * 8,
        .tx_buffer = dados,
    };

    return spi_device_polling_transmit(display->dispositivo, &transacao);
}

static esp_err_t ili9341_configurar_janela(ili9341_t *display, int x0, int y0, int x1, int y1)
{
    uint8_t coluna[] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    uint8_t pagina[] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0,
        (uint8_t)(y1 >> 8), (uint8_t)y1,
    };

    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x2A), TAG, "falha coluna");
    ESP_RETURN_ON_ERROR(ili9341_dados(display, coluna, sizeof(coluna)), TAG, "falha dados coluna");
    ESP_RETURN_ON_ERROR(ili9341_comando(display, 0x2B), TAG, "falha pagina");
    ESP_RETURN_ON_ERROR(ili9341_dados(display, pagina, sizeof(pagina)), TAG, "falha dados pagina");
    return ili9341_comando(display, 0x2C);
}

static esp_err_t ili9341_preencher_retangulo(ili9341_t *display, int x, int y, int largura, int altura, uint16_t cor)
{
    if (largura <= 0 || altura <= 0) {
        return ESP_OK;
    }

    if (x < 0 || y < 0 || x + largura > ILI9341_LARGURA || y + altura > ILI9341_ALTURA) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(ili9341_configurar_janela(display, x, y, x + largura - 1, y + altura - 1), TAG, "falha janela");

    uint8_t bloco[128];
    for (int i = 0; i < (int)sizeof(bloco); i += 2) {
        bloco[i] = (uint8_t)(cor >> 8);
        bloco[i + 1] = (uint8_t)cor;
    }

    int bytes_restantes = largura * altura * sizeof(uint16_t);
    while (bytes_restantes > 0) {
        int envio = bytes_restantes > (int)sizeof(bloco) ? (int)sizeof(bloco) : bytes_restantes;
        ESP_RETURN_ON_ERROR(ili9341_dados(display, bloco, envio), TAG, "falha pixels");
        bytes_restantes -= envio;
    }

    return ESP_OK;
}

static void ili9341_desenhar_texto(ili9341_t *display, int x, int y, const char *texto, uint16_t cor)
{
    while (*texto != '\0' && x <= ILI9341_LARGURA - ILI9341_LARGURA_FONTE) {
        int indice = indice_fonte(*texto) * 8;

        for (int coluna = 0; coluna < 8; coluna++) {
            uint8_t bits = fonte_8x8[indice + coluna];
            for (int linha = 0; linha < 8; linha++) {
                if (bits & (1 << linha)) {
                    ili9341_preencher_retangulo(
                        display,
                        x + (coluna * ILI9341_ESCALA_TEXTO),
                        y + (linha * ILI9341_ESCALA_TEXTO),
                        ILI9341_ESCALA_TEXTO,
                        ILI9341_ESCALA_TEXTO,
                        cor
                    );
                }
            }
        }

        texto++;
        x += ILI9341_LARGURA_FONTE;
    }
}

static int indice_fonte(char caractere)
{
    caractere = (char)toupper((unsigned char)caractere);

    if (caractere >= 'A' && caractere <= 'Z') {
        return caractere - 'A' + 1;
    }

    if (caractere >= '0' && caractere <= '9') {
        return caractere - '0' + 27;
    }

    if (caractere == '+') {
        return 37;
    }

    if (caractere == '-') {
        return 38;
    }

    if (caractere == '|') {
        return 39;
    }

    if (caractere == '#') {
        return 40;
    }

    if (caractere == '*') {
        return 41;
    }

    return 0;
}
