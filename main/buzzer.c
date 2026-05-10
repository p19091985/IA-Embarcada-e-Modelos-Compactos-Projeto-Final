#include "buzzer.h"

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUZZER_MODO LEDC_LOW_SPEED_MODE
#define BUZZER_TIMER LEDC_TIMER_0
#define BUZZER_CANAL LEDC_CHANNEL_0
#define BUZZER_RESOLUCAO LEDC_TIMER_10_BIT
#define BUZZER_DUTY 512

static bool buzzer_configurado = false;

esp_err_t buzzer_iniciar(buzzer_t *buzzer)
{
    buzzer->pino = GPIO_NUM_18;

    ledc_timer_config_t timer = {
        .speed_mode = BUZZER_MODO,
        .timer_num = BUZZER_TIMER,
        .duty_resolution = BUZZER_RESOLUCAO,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t resultado = ledc_timer_config(&timer);
    if (resultado != ESP_OK) {
        return resultado;
    }

    ledc_channel_config_t canal = {
        .gpio_num = buzzer->pino,
        .speed_mode = BUZZER_MODO,
        .channel = BUZZER_CANAL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0,
    };

    resultado = ledc_channel_config(&canal);
    if (resultado == ESP_OK) {
        buzzer_configurado = true;
    }

    return resultado;
}

void buzzer_tocar_tom(uint32_t frequencia_hz, uint32_t duracao_ms)
{
    if (!buzzer_configurado || frequencia_hz == 0 || duracao_ms == 0) {
        return;
    }

    ledc_set_freq(BUZZER_MODO, BUZZER_TIMER, frequencia_hz);
    ledc_set_duty(BUZZER_MODO, BUZZER_CANAL, BUZZER_DUTY);
    ledc_update_duty(BUZZER_MODO, BUZZER_CANAL);
    vTaskDelay(pdMS_TO_TICKS(duracao_ms));
    ledc_set_duty(BUZZER_MODO, BUZZER_CANAL, 0);
    ledc_update_duty(BUZZER_MODO, BUZZER_CANAL);
}

void buzzer_som_tecla(void)
{
    buzzer_tocar_tom(1000, 45);
}

void buzzer_som_inicial(void)
{
    const uint32_t tons[] = {300, 600, 900, 1200, 1800, 1500, 1000, 400};

    for (int i = 0; i < (int)(sizeof(tons) / sizeof(tons[0])); i++) {
        buzzer_tocar_tom(tons[i], 70);
        vTaskDelay(pdMS_TO_TICKS(35));
    }
}

void buzzer_som_vitoria(void)
{
    const uint32_t notas[] = {523, 659, 784, 880, 784, 659, 523, 392, 784};
    const uint32_t duracoes[] = {120, 120, 120, 150, 120, 120, 120, 160, 260};

    for (int i = 0; i < (int)(sizeof(notas) / sizeof(notas[0])); i++) {
        buzzer_tocar_tom(notas[i], duracoes[i]);
        vTaskDelay(pdMS_TO_TICKS(35));
    }
}
