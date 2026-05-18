#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#define SSD1306_LARGURA 128
#define SSD1306_ALTURA 64
#define SSD1306_PAGINAS (SSD1306_ALTURA / 8)
#define SSD1306_BUFFER_TAMANHO (SSD1306_LARGURA * SSD1306_PAGINAS)

typedef struct {
    i2c_port_num_t porta_i2c;
    gpio_num_t pino_sda;
    gpio_num_t pino_scl;
    uint8_t endereco;
    uint32_t frequencia_hz;
} ssd1306_config_t;

typedef struct {
    i2c_master_bus_handle_t barramento;
    i2c_master_dev_handle_t dispositivo;
    uint8_t buffer[SSD1306_BUFFER_TAMANHO];
} ssd1306_t;

esp_err_t ssd1306_iniciar(ssd1306_t *display, const ssd1306_config_t *configuracao);
esp_err_t ssd1306_limpar(ssd1306_t *display);
esp_err_t ssd1306_atualizar(ssd1306_t *display);
void ssd1306_desenhar_texto(ssd1306_t *display, int x, int y, const char *texto);
esp_err_t ssd1306_mostrar_linhas(ssd1306_t *display, const char *linhas[], int quantidade);
