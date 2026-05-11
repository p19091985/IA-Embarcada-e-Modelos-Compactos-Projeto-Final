#include <string.h>

#include "unity.h"

#include "mpu6050.h"

/* ========================================================================== */
/* Constantes e registradores I2C                                             */
/* ========================================================================== */

void test_mpu6050_constantes_do_driver(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x68, MPU6050_ENDERECO);
    TEST_ASSERT_EQUAL_HEX8(0x3B, MPU6050_REG_ACCEL_XOUT_H);
    TEST_ASSERT_EQUAL_HEX8(0x6B, MPU6050_REG_PWR_MGMT_1);
    TEST_ASSERT_EQUAL_UINT(6, MPU6050_ACELERACAO_BYTES);
    TEST_ASSERT_EQUAL_UINT(400000, MPU6050_FREQ_HZ);
}

/* ========================================================================== */
/* Conversao de dois bytes big-endian para int16_t com sinal                  */
/* ========================================================================== */

void test_mpu6050_converte_bytes_assinados(void)
{
    /* Zero */
    TEST_ASSERT_EQUAL_INT16(0, mpu6050_unir_bytes(0x00, 0x00));

    /* Positivos */
    TEST_ASSERT_EQUAL_INT16(1, mpu6050_unir_bytes(0x00, 0x01));
    TEST_ASSERT_EQUAL_INT16(255, mpu6050_unir_bytes(0x00, 0xFF));
    TEST_ASSERT_EQUAL_INT16(256, mpu6050_unir_bytes(0x01, 0x00));
    TEST_ASSERT_EQUAL_INT16(1000, mpu6050_unir_bytes(0x03, 0xE8));

    /* Limite positivo (max int16_t) */
    TEST_ASSERT_EQUAL_INT16(32767, mpu6050_unir_bytes(0x7F, 0xFF));

    /* Gravidade em repouso no eixo Z (~16384 = +1g na faixa ±2g) */
    TEST_ASSERT_EQUAL_INT16(16384, mpu6050_unir_bytes(0x40, 0x00));

    /* Negativos */
    TEST_ASSERT_EQUAL_INT16(-1, mpu6050_unir_bytes(0xFF, 0xFF));
    TEST_ASSERT_EQUAL_INT16(-2, mpu6050_unir_bytes(0xFF, 0xFE));
    TEST_ASSERT_EQUAL_INT16(-256, mpu6050_unir_bytes(0xFF, 0x00));
    TEST_ASSERT_EQUAL_INT16(-1000, mpu6050_unir_bytes(0xFC, 0x18));

    /* Limite negativo (min int16_t) */
    TEST_ASSERT_EQUAL_INT16(-32768, mpu6050_unir_bytes(0x80, 0x00));

    /* Fronteira do bit de sinal: 0x7FFF e 0x8000 */
    TEST_ASSERT_EQUAL_INT16(32767, mpu6050_unir_bytes(0x7F, 0xFF));
    TEST_ASSERT_EQUAL_INT16(-32768, mpu6050_unir_bytes(0x80, 0x00));

    /* 0x8001 = -32767 */
    TEST_ASSERT_EQUAL_INT16(-32767, mpu6050_unir_bytes(0x80, 0x01));
}

/* ========================================================================== */
/* Conversao do vetor de 6 bytes para 3 eixos                                */
/* ========================================================================== */

void test_mpu6050_converte_vetor_de_aceleracao(void)
{
    int16_t ax = 0, ay = 0, az = 0;

    /* Cenario 1: valores positivo, negativo e gravidade */
    const uint8_t bytes_misto[6] = {0x00, 0x64, 0xFF, 0x9C, 0x40, 0x00};
    TEST_ASSERT_EQUAL(ESP_OK, mpu6050_converter_bytes_aceleracao(bytes_misto, &ax, &ay, &az));
    TEST_ASSERT_EQUAL_INT16(100, ax);
    TEST_ASSERT_EQUAL_INT16(-100, ay);
    TEST_ASSERT_EQUAL_INT16(16384, az);

    /* Cenario 2: todos os bytes zero */
    const uint8_t bytes_zero[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ax = 99; ay = 99; az = 99;
    TEST_ASSERT_EQUAL(ESP_OK, mpu6050_converter_bytes_aceleracao(bytes_zero, &ax, &ay, &az));
    TEST_ASSERT_EQUAL_INT16(0, ax);
    TEST_ASSERT_EQUAL_INT16(0, ay);
    TEST_ASSERT_EQUAL_INT16(0, az);

    /* Cenario 3: limites extremos em todos os eixos */
    const uint8_t bytes_extremos[6] = {0x7F, 0xFF, 0x80, 0x00, 0xFF, 0xFF};
    TEST_ASSERT_EQUAL(ESP_OK, mpu6050_converter_bytes_aceleracao(bytes_extremos, &ax, &ay, &az));
    TEST_ASSERT_EQUAL_INT16(32767, ax);
    TEST_ASSERT_EQUAL_INT16(-32768, ay);
    TEST_ASSERT_EQUAL_INT16(-1, az);

    /* Cenario 4: todos os bytes 0xFF */
    const uint8_t bytes_ff[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_EQUAL(ESP_OK, mpu6050_converter_bytes_aceleracao(bytes_ff, &ax, &ay, &az));
    TEST_ASSERT_EQUAL_INT16(-1, ax);
    TEST_ASSERT_EQUAL_INT16(-1, ay);
    TEST_ASSERT_EQUAL_INT16(-1, az);

    /* Cenario 5: repouso tipico (ax~0, ay~0, az~+16384) */
    const uint8_t bytes_repouso[6] = {0x00, 0x10, 0xFF, 0xF0, 0x40, 0x00};
    TEST_ASSERT_EQUAL(ESP_OK, mpu6050_converter_bytes_aceleracao(bytes_repouso, &ax, &ay, &az));
    TEST_ASSERT_EQUAL_INT16(16, ax);
    TEST_ASSERT_EQUAL_INT16(-16, ay);
    TEST_ASSERT_EQUAL_INT16(16384, az);

    /* Cenario 6: posicao byte alto vs byte baixo (ordem big-endian) */
    const uint8_t bytes_be[6] = {0x01, 0x00, 0x00, 0x01, 0x10, 0x00};
    TEST_ASSERT_EQUAL(ESP_OK, mpu6050_converter_bytes_aceleracao(bytes_be, &ax, &ay, &az));
    TEST_ASSERT_EQUAL_INT16(256, ax);
    TEST_ASSERT_EQUAL_INT16(1, ay);
    TEST_ASSERT_EQUAL_INT16(4096, az);
}

/* ========================================================================== */
/* Rejeicao de argumentos nulos                                               */
/* ========================================================================== */

void test_mpu6050_rejeita_argumentos_invalidos(void)
{
    const uint8_t bytes[6] = {0};
    int16_t eixo = 0;

    /* mpu6050_iniciar: ambos NULL */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mpu6050_iniciar(NULL, NULL));

    /* mpu6050_ler_aceleracao: struct NULL */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mpu6050_ler_aceleracao(NULL, &eixo, &eixo, &eixo));

    /* mpu6050_converter_bytes_aceleracao: cada ponteiro NULL individualmente */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mpu6050_converter_bytes_aceleracao(NULL, &eixo, &eixo, &eixo));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mpu6050_converter_bytes_aceleracao(bytes, NULL, &eixo, &eixo));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mpu6050_converter_bytes_aceleracao(bytes, &eixo, NULL, &eixo));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, mpu6050_converter_bytes_aceleracao(bytes, &eixo, &eixo, NULL));

    /* mpu6050_ler_aceleracao: struct nao-NULL mas sem dispositivo */
    mpu6050_t mpu_sem_dispositivo;
    memset(&mpu_sem_dispositivo, 0, sizeof(mpu_sem_dispositivo));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      mpu6050_ler_aceleracao(&mpu_sem_dispositivo, &eixo, &eixo, &eixo));
}
