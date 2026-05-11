#include "lcd1602_i2c.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_TEMPO_LIMITE_I2C_MS 1000
#define LCD_LUZ_FUNDO 0x08
#define LCD_ATIVAR 0x04
#define LCD_REGISTRO_DADOS 0x01
#define LCD_SCROLL_ESPACOS 4

static const char *TAG_LCD = "lcd1602";

static esp_err_t lcd_enviar_byte(lcd1602_t *lcd, uint8_t byte);
static esp_err_t lcd_pulsar_enable(lcd1602_t *lcd, uint8_t byte);
static esp_err_t lcd_enviar_4_bits(lcd1602_t *lcd, uint8_t byte);
static esp_err_t lcd_enviar(lcd1602_t *lcd, uint8_t valor, uint8_t modo);
static esp_err_t lcd_comando(lcd1602_t *lcd, uint8_t comando);
static esp_err_t lcd_escrever_caractere(lcd1602_t *lcd, char caractere);
static esp_err_t lcd_posicionar_cursor(lcd1602_t *lcd, uint8_t coluna, uint8_t linha);

esp_err_t lcd1602_iniciar(lcd1602_t *lcd, const lcd1602_config_t *configuracao)
{
    i2c_master_bus_config_t configuracao_barramento = {
        .i2c_port = configuracao->porta_i2c,
        .sda_io_num = configuracao->pino_sda,
        .scl_io_num = configuracao->pino_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_device_config_t configuracao_dispositivo = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = configuracao->endereco,
        .scl_speed_hz = configuracao->frequencia_hz,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&configuracao_barramento, &lcd->barramento), TAG_LCD, "falha no barramento I2C");
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(lcd->barramento, &configuracao_dispositivo, &lcd->dispositivo), TAG_LCD, "falha ao adicionar LCD");

    // sequencia de inicializacao do HD44780 em modo 4 bits
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(lcd_enviar_4_bits(lcd, 0x30), TAG_LCD, "falha init 1");
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(lcd_enviar_4_bits(lcd, 0x30), TAG_LCD, "falha init 2");
    esp_rom_delay_us(150);
    ESP_RETURN_ON_ERROR(lcd_enviar_4_bits(lcd, 0x30), TAG_LCD, "falha init 3");
    ESP_RETURN_ON_ERROR(lcd_enviar_4_bits(lcd, 0x20), TAG_LCD, "falha modo 4 bits");

    ESP_RETURN_ON_ERROR(lcd_comando(lcd, 0x28), TAG_LCD, "falha configuracao");
    ESP_RETURN_ON_ERROR(lcd_comando(lcd, 0x08), TAG_LCD, "falha display off");
    ESP_RETURN_ON_ERROR(lcd_comando(lcd, 0x01), TAG_LCD, "falha limpar LCD");
    ESP_RETURN_ON_ERROR(lcd_comando(lcd, 0x06), TAG_LCD, "falha modo escrita");
    ESP_RETURN_ON_ERROR(lcd_comando(lcd, 0x0C), TAG_LCD, "falha display on");

    return ESP_OK;
}

esp_err_t lcd1602_escrever_linha(lcd1602_t *lcd, uint8_t linha, const char *texto)
{
    char texto_com_16_colunas[17];

    snprintf(texto_com_16_colunas, sizeof(texto_com_16_colunas), "%-16.16s", texto);
    ESP_RETURN_ON_ERROR(lcd_posicionar_cursor(lcd, 0, linha), TAG_LCD, "falha ao posicionar cursor");

    for (int coluna = 0; coluna < LCD1602_COLUNAS; coluna++) {
        ESP_RETURN_ON_ERROR(lcd_escrever_caractere(lcd, texto_com_16_colunas[coluna]), TAG_LCD, "falha ao escrever caractere");
    }

    return ESP_OK;
}

void lcd1602_formatar_janela_scroll(const char *texto, uint32_t passo, char saida[LCD1602_COLUNAS + 1])
{
    const char *conteudo = texto != NULL ? texto : "";
    size_t tamanho = strlen(conteudo);

    if (saida == NULL) {
        return;
    }

    if (tamanho == 0) {
        memset(saida, ' ', LCD1602_COLUNAS);
        saida[LCD1602_COLUNAS] = '\0';
        return;
    }

    size_t periodo = tamanho + LCD_SCROLL_ESPACOS;
    for (size_t coluna = 0; coluna < LCD1602_COLUNAS; coluna++) {
        size_t indice = (passo + coluna) % periodo;
        saida[coluna] = indice < tamanho ? conteudo[indice] : ' ';
    }
    saida[LCD1602_COLUNAS] = '\0';
}

static esp_err_t lcd_enviar_byte(lcd1602_t *lcd, uint8_t byte)
{
    uint8_t byte_com_luz = byte | LCD_LUZ_FUNDO;
    return i2c_master_transmit(lcd->dispositivo, &byte_com_luz, 1, LCD_TEMPO_LIMITE_I2C_MS);
}

static esp_err_t lcd_pulsar_enable(lcd1602_t *lcd, uint8_t byte)
{
    ESP_RETURN_ON_ERROR(lcd_enviar_byte(lcd, byte | LCD_ATIVAR), TAG_LCD, "falha ao ativar enable");
    esp_rom_delay_us(1);
    ESP_RETURN_ON_ERROR(lcd_enviar_byte(lcd, byte & ~LCD_ATIVAR), TAG_LCD, "falha ao desativar enable");
    esp_rom_delay_us(50);
    return ESP_OK;
}

static esp_err_t lcd_enviar_4_bits(lcd1602_t *lcd, uint8_t byte)
{
    ESP_RETURN_ON_ERROR(lcd_enviar_byte(lcd, byte), TAG_LCD, "falha ao enviar 4 bits");
    return lcd_pulsar_enable(lcd, byte);
}

static esp_err_t lcd_enviar(lcd1602_t *lcd, uint8_t valor, uint8_t modo)
{
    ESP_RETURN_ON_ERROR(lcd_enviar_4_bits(lcd, (valor & 0xF0) | modo), TAG_LCD, "falha no nibble alto");
    return lcd_enviar_4_bits(lcd, ((valor << 4) & 0xF0) | modo);
}

static esp_err_t lcd_comando(lcd1602_t *lcd, uint8_t comando)
{
    esp_err_t resultado = lcd_enviar(lcd, comando, 0);

    if (comando == 0x01 || comando == 0x02) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    return resultado;
}

static esp_err_t lcd_escrever_caractere(lcd1602_t *lcd, char caractere)
{
    return lcd_enviar(lcd, (uint8_t)caractere, LCD_REGISTRO_DADOS);
}

static esp_err_t lcd_posicionar_cursor(lcd1602_t *lcd, uint8_t coluna, uint8_t linha)
{
    const uint8_t inicio_linhas[] = {0x00, 0x40};

    if (linha > 1) {
        linha = 1;
    }
    if (coluna > 15) {
        coluna = 15;
    }

    return lcd_comando(lcd, 0x80 | (uint8_t)(coluna + inicio_linhas[linha]));
}
