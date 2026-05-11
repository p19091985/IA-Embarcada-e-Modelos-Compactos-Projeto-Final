#include "coleta_mpu6050.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "coleta_mpu6050";

void coleta_mpu6050_configurar(coleta_mpu6050_t *coleta, mpu6050_t *mpu)
{
    if (coleta == NULL) {
        return;
    }

    memset(coleta, 0, sizeof(*coleta));
    coleta->mpu = mpu;
    coleta->ativa = true;
    coleta->label = COLETA_MPU6050_LABEL_REPOUSO;
}

void coleta_mpu6050_parar(coleta_mpu6050_t *coleta)
{
    if (coleta != NULL) {
        coleta->ativa = false;
    }
}

esp_err_t coleta_mpu6050_definir_label(coleta_mpu6050_t *coleta, int label)
{
    if (coleta == NULL || !coleta_mpu6050_label_valido(label)) {
        return ESP_ERR_INVALID_ARG;
    }

    coleta->label = label;
    return ESP_OK;
}

bool coleta_mpu6050_label_valido(int label)
{
    return label == COLETA_MPU6050_LABEL_REPOUSO || label == COLETA_MPU6050_LABEL_CONFIRMAR;
}

esp_err_t coleta_mpu6050_formatar_linha_csv(char *buffer,
                                             size_t tamanho,
                                             uint32_t timestamp_ms,
                                             int16_t ax,
                                             int16_t ay,
                                             int16_t az,
                                             int label)
{
    if (buffer == NULL || tamanho == 0 || !coleta_mpu6050_label_valido(label)) {
        return ESP_ERR_INVALID_ARG;
    }

    int escritos = snprintf(buffer, tamanho, "%lu,%d,%d,%d,%d", (unsigned long)timestamp_ms, ax, ay, az, label);

    if (escritos < 0 || (size_t)escritos >= tamanho) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

void mpu_data_collection_task(void *parametro)
{
    coleta_mpu6050_t *coleta = (coleta_mpu6050_t *)parametro;

    if (coleta == NULL || coleta->mpu == NULL) {
        ESP_LOGE(TAG, "parametros invalidos para task de coleta");
        vTaskDelete(NULL);
        return;
    }

    printf("%s\n", COLETA_MPU6050_CABECALHO_CSV);

    TickType_t ultimo_despertar = xTaskGetTickCount();
    while (coleta->ativa) {
        int16_t ax = 0;
        int16_t ay = 0;
        int16_t az = 0;
        esp_err_t erro = mpu6050_ler_aceleracao(coleta->mpu, &ax, &ay, &az);

        if (erro == ESP_OK) {
            char linha[64];
            uint32_t timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
            int label = coleta->label;

            if (coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha), timestamp_ms, ax, ay, az, label) == ESP_OK) {
                printf("%s\n", linha);
            }
        } else {
            ESP_LOGW(TAG, "falha na leitura do MPU6050: %s", esp_err_to_name(erro));
        }

        vTaskDelayUntil(&ultimo_despertar, pdMS_TO_TICKS(COLETA_MPU6050_PERIODO_MS));
    }

    ESP_LOGI(TAG, "coleta finalizada");
    vTaskDelete(NULL);
}
