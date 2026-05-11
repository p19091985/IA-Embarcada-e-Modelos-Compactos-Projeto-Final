#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#define HCSR04_TRIGGER_GPIO GPIO_NUM_19
#define HCSR04_ECHO_GPIO GPIO_NUM_20
#define HCSR04_TIMEOUT_US 30000U
#define HCSR04_ECHO_US_POR_CM 58U
#define HCSR04_DISTANCIA_MAX_CM 400U

typedef struct {
    gpio_num_t trigger;
    gpio_num_t echo;
    uint32_t timeout_us;
} hcsr04_t;

esp_err_t hcsr04_iniciar(hcsr04_t *sensor, gpio_num_t trigger, gpio_num_t echo);
esp_err_t hcsr04_ler_distancia(hcsr04_t *sensor, uint16_t *distancia_cm, uint32_t *eco_us);
uint16_t hcsr04_echo_us_para_cm(uint32_t eco_us);
bool hcsr04_distancia_valida_cm(uint16_t distancia_cm);
