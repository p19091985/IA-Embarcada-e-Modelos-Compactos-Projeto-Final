#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#define LCD1602_COLUNAS 16

typedef struct {
    i2c_port_num_t porta_i2c;
    gpio_num_t pino_sda;
    gpio_num_t pino_scl;
    uint8_t endereco;
    uint32_t frequencia_hz;
} lcd1602_config_t;

typedef struct {
    i2c_master_bus_handle_t barramento;
    i2c_master_dev_handle_t dispositivo;
} lcd1602_t;

esp_err_t lcd1602_iniciar(lcd1602_t *lcd, const lcd1602_config_t *configuracao);
esp_err_t lcd1602_escrever_linha(lcd1602_t *lcd, uint8_t linha, const char *texto);
void lcd1602_formatar_janela_scroll(const char *texto, uint32_t passo, char saida[LCD1602_COLUNAS + 1]);
