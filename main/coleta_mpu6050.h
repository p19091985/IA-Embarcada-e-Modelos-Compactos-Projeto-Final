#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "mpu6050.h"

#define COLETA_MPU6050_CABECALHO_CSV "timestamp_ms,ax,ay,az,label"
#define COLETA_MPU6050_FREQ_HZ 50
#define COLETA_MPU6050_PERIODO_MS 20
#define COLETA_MPU6050_LABEL_REPOUSO 0
#define COLETA_MPU6050_LABEL_CONFIRMAR 1

typedef struct {
    mpu6050_t *mpu;
    volatile bool ativa;
    volatile int label;
} coleta_mpu6050_t;

void coleta_mpu6050_configurar(coleta_mpu6050_t *coleta, mpu6050_t *mpu);
void coleta_mpu6050_parar(coleta_mpu6050_t *coleta);
esp_err_t coleta_mpu6050_definir_label(coleta_mpu6050_t *coleta, int label);
bool coleta_mpu6050_label_valido(int label);
esp_err_t coleta_mpu6050_formatar_linha_csv(char *buffer,
                                             size_t tamanho,
                                             uint32_t timestamp_ms,
                                             int16_t ax,
                                             int16_t ay,
                                             int16_t az,
                                             int label);
void mpu_data_collection_task(void *parametro);
