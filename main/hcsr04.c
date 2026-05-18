#include "hcsr04.h"

#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool aguardar_nivel(gpio_num_t pino, int nivel, uint32_t timeout_us, uint32_t *timestamp_us)
{
    int64_t inicio = esp_timer_get_time();
    int iteracoes = 0;

    while ((uint32_t)(esp_timer_get_time() - inicio) <= timeout_us) {
        if (gpio_get_level(pino) == nivel) {
            if (timestamp_us != NULL) {
                *timestamp_us = (uint32_t)esp_timer_get_time();
            }
            return true;
        }

        if (++iteracoes % 100 == 0) {
            taskYIELD();
        }
    }

    return false;
}

esp_err_t hcsr04_iniciar(hcsr04_t *sensor, gpio_num_t trigger, gpio_num_t echo)
{
    if (sensor == NULL || trigger == GPIO_NUM_NC || echo == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_ARG;
    }

    sensor->trigger = trigger;
    sensor->echo = echo;
    sensor->timeout_us = HCSR04_TIMEOUT_US;

    gpio_config_t config_trigger = {
        .pin_bit_mask = 1ULL << trigger,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t erro = gpio_config(&config_trigger);
    if (erro != ESP_OK) {
        return erro;
    }

    gpio_config_t config_echo = {
        .pin_bit_mask = 1ULL << echo,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    erro = gpio_config(&config_echo);
    if (erro != ESP_OK) {
        return erro;
    }

    gpio_set_level(trigger, 0);
    return ESP_OK;
}

esp_err_t hcsr04_ler_distancia(hcsr04_t *sensor, uint16_t *distancia_cm, uint32_t *eco_us)
{
    if (sensor == NULL || distancia_cm == NULL || eco_us == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(sensor->trigger, 0);
    esp_rom_delay_us(2);
    gpio_set_level(sensor->trigger, 1);
    esp_rom_delay_us(10);
    gpio_set_level(sensor->trigger, 0);

    uint32_t inicio_eco = 0;
    uint32_t fim_eco = 0;
    if (!aguardar_nivel(sensor->echo, 1, sensor->timeout_us, &inicio_eco)) {
        return ESP_ERR_TIMEOUT;
    }

    if (!aguardar_nivel(sensor->echo, 0, sensor->timeout_us, &fim_eco)) {
        return ESP_ERR_TIMEOUT;
    }

    *eco_us = fim_eco - inicio_eco;
    *distancia_cm = hcsr04_echo_us_para_cm(*eco_us);

    return hcsr04_distancia_valida_cm(*distancia_cm) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

uint16_t hcsr04_echo_us_para_cm(uint32_t eco_us)
{
    return (uint16_t)(eco_us / HCSR04_ECHO_US_POR_CM);
}

bool hcsr04_distancia_valida_cm(uint16_t distancia_cm)
{
    return distancia_cm > 0 && distancia_cm <= HCSR04_DISTANCIA_MAX_CM;
}
