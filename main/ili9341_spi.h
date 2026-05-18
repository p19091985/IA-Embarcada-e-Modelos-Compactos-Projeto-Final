#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#define ILI9341_LARGURA 320
#define ILI9341_ALTURA 240

typedef struct {
    spi_host_device_t host;
    gpio_num_t pino_mosi;
    gpio_num_t pino_sck;
    gpio_num_t pino_cs;
    gpio_num_t pino_dc;
    int frequencia_hz;
} ili9341_config_t;

typedef struct {
    spi_device_handle_t dispositivo;
    gpio_num_t pino_dc;
} ili9341_t;

esp_err_t ili9341_iniciar(ili9341_t *display, const ili9341_config_t *configuracao);
esp_err_t ili9341_limpar(ili9341_t *display, uint16_t cor);
esp_err_t ili9341_mostrar_linhas(ili9341_t *display, const char *linhas[], int quantidade);
