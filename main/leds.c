#include "leds.h"

esp_err_t leds_iniciar(leds_t *leds)
{
    leds->azul = GPIO_NUM_11;
    leds->verde = GPIO_NUM_12;
    leds->vermelho = GPIO_NUM_35;
    leds->dourado = GPIO_NUM_13;
    leds->azul_ligado = true;
    leds->verde_ligado = true;
    leds->vermelho_ligado = false;
    leds->dourado_ligado = false;

    gpio_config_t config = {
        .pin_bit_mask = (1ULL << leds->azul) | (1ULL << leds->verde) |
                        (1ULL << leds->vermelho) | (1ULL << leds->dourado),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t resultado = gpio_config(&config);
    if (resultado != ESP_OK) {
        return resultado;
    }

    leds_atualizar(leds);
    return ESP_OK;
}

void leds_definir_dourado(leds_t *leds, bool ligado)
{
    leds->dourado_ligado = ligado;
    leds_atualizar(leds);
}

void leds_definir_status_rodando(leds_t *leds)
{
    leds->verde_ligado = true;
    leds->vermelho_ligado = false;
    leds_atualizar(leds);
}

void leds_definir_status_parado(leds_t *leds)
{
    leds->verde_ligado = false;
    leds->vermelho_ligado = true;
    leds_atualizar(leds);
}

void leds_atualizar(const leds_t *leds)
{
    gpio_set_level(leds->azul, leds->azul_ligado ? 1 : 0);
    gpio_set_level(leds->verde, leds->verde_ligado ? 1 : 0);
    gpio_set_level(leds->vermelho, leds->vermelho_ligado ? 1 : 0);
    gpio_set_level(leds->dourado, leds->dourado_ligado ? 1 : 0);
}
