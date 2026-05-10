#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    gpio_num_t pino;
} buzzer_t;

esp_err_t buzzer_iniciar(buzzer_t *buzzer);
void buzzer_tocar_tom(uint32_t frequencia_hz, uint32_t duracao_ms);
void buzzer_som_tecla(void);
void buzzer_som_inicial(void);
void buzzer_som_vitoria(void);
