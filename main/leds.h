#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    gpio_num_t azul;
    gpio_num_t verde;
    gpio_num_t dourado;
    bool azul_ligado;
    bool verde_ligado;
    bool dourado_ligado;
} leds_t;

esp_err_t leds_iniciar(leds_t *leds);
void leds_definir_dourado(leds_t *leds, bool ligado);
void leds_atualizar(const leds_t *leds);
