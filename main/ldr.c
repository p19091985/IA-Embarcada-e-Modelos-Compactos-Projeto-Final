#include "ldr.h"

esp_err_t ldr_iniciar(ldr_t *ldr)
{
    if (ldr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ldr->unidade = NULL;
    ldr->canal = LDR_ADC_CHANNEL;
    ldr->limiar_escuro = LDR_LIMIAR_ESCURO;
    ldr->pronto = false;

    adc_oneshot_unit_init_cfg_t config_unidade = {
        .unit_id = LDR_ADC_UNIT,
    };
    esp_err_t erro = adc_oneshot_new_unit(&config_unidade, &ldr->unidade);
    if (erro != ESP_OK) {
        return erro;
    }

    adc_oneshot_chan_cfg_t config_canal = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    erro = adc_oneshot_config_channel(ldr->unidade, ldr->canal, &config_canal);
    if (erro != ESP_OK) {
        return erro;
    }

    ldr->pronto = true;
    return ESP_OK;
}

esp_err_t ldr_ler_bruto(ldr_t *ldr, int *valor_bruto)
{
    if (ldr == NULL || valor_bruto == NULL || !ldr->pronto) {
        return ESP_ERR_INVALID_ARG;
    }

    return adc_oneshot_read(ldr->unidade, ldr->canal, valor_bruto);
}

bool ldr_ambiente_escuro(int valor_bruto)
{
    return valor_bruto > LDR_LIMIAR_ESCURO;
}

bool ldr_deve_atualizar_iluminacao_automatica(bool sensor_pronto, bool modo_manual)
{
    return sensor_pronto && !modo_manual;
}

bool ldr_calcular_led_automatico(int valor_bruto, int *estado_histerese)
{
    if (estado_histerese == NULL) {
        return ldr_ambiente_escuro(valor_bruto);
    }

    int limiar_liga = LDR_LIMIAR_ESCURO + LDR_HISTERESE_ADC;
    int limiar_desliga = LDR_LIMIAR_ESCURO - LDR_HISTERESE_ADC;

    if (*estado_histerese == 0 && valor_bruto > limiar_liga) {
        *estado_histerese = 1;
    } else if (*estado_histerese != 0 && valor_bruto < limiar_desliga) {
        *estado_histerese = 0;
    }

    return *estado_histerese != 0;
}
