#pragma once

#include <stdbool.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#define LDR_ADC_UNIT ADC_UNIT_1
#define LDR_ADC_CHANNEL ADC_CHANNEL_9
#define LDR_GPIO_NUM 10
#define LDR_LIMIAR_ESCURO 1600
#define LDR_HISTERESE_ADC 200

typedef struct {
    adc_oneshot_unit_handle_t unidade;
    adc_channel_t canal;
    int limiar_escuro;
    bool pronto;
} ldr_t;

esp_err_t ldr_iniciar(ldr_t *ldr);
esp_err_t ldr_ler_bruto(ldr_t *ldr, int *valor_bruto);
bool ldr_ambiente_escuro(int valor_bruto);
bool ldr_deve_atualizar_iluminacao_automatica(bool sensor_pronto, bool modo_manual);
bool ldr_calcular_led_automatico(int valor_bruto, int *estado_histerese);
