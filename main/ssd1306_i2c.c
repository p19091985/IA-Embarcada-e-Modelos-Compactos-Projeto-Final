#include "ssd1306_i2c.h"

#include <ctype.h>
#include <string.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306_font.h"

#define SSD1306_TIMEOUT_MS 1000
#define SSD1306_CONTROLE_COMANDO 0x00
#define SSD1306_CONTROLE_DADOS 0x40

static const char *TAG = "ssd1306";

static esp_err_t ssd1306_comando(ssd1306_t *display, uint8_t comando);
static esp_err_t ssd1306_comandos(ssd1306_t *display, const uint8_t *comandos, int quantidade);
static int indice_fonte(char caractere);
static void desenhar_char(ssd1306_t *display, int x, int y, char caractere);

esp_err_t ssd1306_iniciar(ssd1306_t *display, const ssd1306_config_t *configuracao)
{
    i2c_master_bus_config_t config_barramento = {
        .i2c_port = configuracao->porta_i2c,
        .sda_io_num = configuracao->pino_sda,
        .scl_io_num = configuracao->pino_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_device_config_t config_dispositivo = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = configuracao->endereco,
        .scl_speed_hz = configuracao->frequencia_hz,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config_barramento, &display->barramento), TAG, "falha no barramento I2C");
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(display->barramento, &config_dispositivo, &display->dispositivo), TAG, "falha ao adicionar OLED");

    vTaskDelay(pdMS_TO_TICKS(50));

    const uint8_t comandos[] = {
        0xAE,       /* display off */
        0x20, 0x00, /* horizontal addressing */
        0x40,
        0xA1,
        0xA8, 0x3F,
        0xC8,
        0xD3, 0x00,
        0xDA, 0x12,
        0xD5, 0x80,
        0xD9, 0xF1,
        0xDB, 0x30,
        0x81, 0xFF,
        0xA4,
        0xA6,
        0x8D, 0x14,
        0x2E,
        0xAF,
    };

    ESP_RETURN_ON_ERROR(ssd1306_comandos(display, comandos, sizeof(comandos)), TAG, "falha init OLED");
    return ssd1306_limpar(display);
}

esp_err_t ssd1306_limpar(ssd1306_t *display)
{
    memset(display->buffer, 0, sizeof(display->buffer));
    return ssd1306_atualizar(display);
}

esp_err_t ssd1306_atualizar(ssd1306_t *display)
{
    const uint8_t comandos[] = {
        0x21, 0, SSD1306_LARGURA - 1,
        0x22, 0, SSD1306_PAGINAS - 1,
    };

    ESP_RETURN_ON_ERROR(ssd1306_comandos(display, comandos, sizeof(comandos)), TAG, "falha ao posicionar OLED");

    uint8_t dados[SSD1306_BUFFER_TAMANHO + 1];
    dados[0] = SSD1306_CONTROLE_DADOS;
    memcpy(&dados[1], display->buffer, SSD1306_BUFFER_TAMANHO);

    return i2c_master_transmit(display->dispositivo, dados, sizeof(dados), SSD1306_TIMEOUT_MS);
}

void ssd1306_desenhar_texto(ssd1306_t *display, int x, int y, const char *texto)
{
    while (*texto != '\0' && x <= SSD1306_LARGURA - 8) {
        desenhar_char(display, x, y, *texto);
        texto++;
        x += 8;
    }
}

esp_err_t ssd1306_mostrar_linhas(ssd1306_t *display, const char *linhas[], int quantidade)
{
    memset(display->buffer, 0, sizeof(display->buffer));

    for (int i = 0; i < quantidade && i < 8; i++) {
        ssd1306_desenhar_texto(display, 0, i * 8, linhas[i]);
    }

    return ssd1306_atualizar(display);
}

static esp_err_t ssd1306_comando(ssd1306_t *display, uint8_t comando)
{
    uint8_t pacote[] = {SSD1306_CONTROLE_COMANDO, comando};
    return i2c_master_transmit(display->dispositivo, pacote, sizeof(pacote), SSD1306_TIMEOUT_MS);
}

static esp_err_t ssd1306_comandos(ssd1306_t *display, const uint8_t *comandos, int quantidade)
{
    for (int i = 0; i < quantidade; i++) {
        ESP_RETURN_ON_ERROR(ssd1306_comando(display, comandos[i]), TAG, "falha ao enviar comando");
    }

    return ESP_OK;
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

static void desenhar_char(ssd1306_t *display, int x, int y, char caractere)
{
    if (x < 0 || y < 0 || x > SSD1306_LARGURA - 8 || y > SSD1306_ALTURA - 8) {
        return;
    }

    int pagina = y / 8;
    int posicao = pagina * SSD1306_LARGURA + x;
    int indice = indice_fonte(caractere) * 8;

    for (int coluna = 0; coluna < 8; coluna++) {
        display->buffer[posicao + coluna] = ssd1306_font_8x8[indice + coluna];
    }
}
