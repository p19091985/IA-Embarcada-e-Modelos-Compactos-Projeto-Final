#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define MPU6050_ENDERECO 0x68
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_ACELERACAO_BYTES 6
#define MPU6050_FREQ_HZ 400000

typedef struct {
    i2c_master_bus_handle_t barramento;
    i2c_master_dev_handle_t dispositivo;
} mpu6050_t;

esp_err_t mpu6050_iniciar(mpu6050_t *mpu, i2c_master_bus_handle_t barramento);
esp_err_t mpu6050_ler_aceleracao(mpu6050_t *mpu, int16_t *ax, int16_t *ay, int16_t *az);

int16_t mpu6050_unir_bytes(uint8_t byte_alto, uint8_t byte_baixo);
esp_err_t mpu6050_converter_bytes_aceleracao(const uint8_t bytes[MPU6050_ACELERACAO_BYTES], int16_t *ax, int16_t *ay, int16_t *az);
