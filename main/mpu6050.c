#include "mpu6050.h"

#include <string.h>

#include "esp_check.h"

#define MPU6050_TIMEOUT_MS 1000

static const char *TAG = "mpu6050";

esp_err_t mpu6050_iniciar(mpu6050_t *mpu, i2c_master_bus_handle_t barramento)
{
    if (mpu == NULL || barramento == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(mpu, 0, sizeof(*mpu));
    mpu->barramento = barramento;

    i2c_device_config_t config_dispositivo = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ENDERECO,
        .scl_speed_hz = MPU6050_FREQ_HZ,
    };

    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(mpu->barramento, &config_dispositivo, &mpu->dispositivo),
                        TAG,
                        "falha ao adicionar MPU6050");

    const uint8_t acordar_sensor[] = {MPU6050_REG_PWR_MGMT_1, 0x00};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(mpu->dispositivo, acordar_sensor, sizeof(acordar_sensor), MPU6050_TIMEOUT_MS),
                        TAG,
                        "falha ao acordar MPU6050");

    return ESP_OK;
}

esp_err_t mpu6050_ler_aceleracao(mpu6050_t *mpu, int16_t *ax, int16_t *ay, int16_t *az)
{
    if (mpu == NULL || mpu->dispositivo == NULL || ax == NULL || ay == NULL || az == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t registrador = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t bytes[MPU6050_ACELERACAO_BYTES];

    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(mpu->dispositivo,
                                                    &registrador,
                                                    sizeof(registrador),
                                                    bytes,
                                                    sizeof(bytes),
                                                    MPU6050_TIMEOUT_MS),
                        TAG,
                        "falha ao ler aceleracao");

    return mpu6050_converter_bytes_aceleracao(bytes, ax, ay, az);
}

int16_t mpu6050_unir_bytes(uint8_t byte_alto, uint8_t byte_baixo)
{
    uint16_t valor = ((uint16_t)byte_alto << 8) | byte_baixo;

    if (valor >= 0x8000) {
        return (int16_t)(-((int32_t)(0x10000u - valor)));
    }

    return (int16_t)valor;
}

esp_err_t mpu6050_converter_bytes_aceleracao(const uint8_t bytes[MPU6050_ACELERACAO_BYTES], int16_t *ax, int16_t *ay, int16_t *az)
{
    if (bytes == NULL || ax == NULL || ay == NULL || az == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *ax = mpu6050_unir_bytes(bytes[0], bytes[1]);
    *ay = mpu6050_unir_bytes(bytes[2], bytes[3]);
    *az = mpu6050_unir_bytes(bytes[4], bytes[5]);

    return ESP_OK;
}
