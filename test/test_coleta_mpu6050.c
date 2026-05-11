#include <string.h>

#include "unity.h"

#include "coleta_mpu6050.h"

/* ========================================================================== */
/* Constantes da coleta: cabecalho CSV, frequencia e periodo                  */
/* ========================================================================== */

void test_coleta_mpu6050_cabecalho_e_taxa(void)
{
    TEST_ASSERT_EQUAL_STRING("timestamp_ms,ax,ay,az,label", COLETA_MPU6050_CABECALHO_CSV);
    TEST_ASSERT_EQUAL_UINT(50, COLETA_MPU6050_FREQ_HZ);
    TEST_ASSERT_EQUAL_UINT(20, COLETA_MPU6050_PERIODO_MS);

    /* Consistencia: periodo = 1000 / frequencia */
    TEST_ASSERT_EQUAL_UINT(1000 / COLETA_MPU6050_FREQ_HZ, COLETA_MPU6050_PERIODO_MS);
}

/* ========================================================================== */
/* Validacao de labels: somente repouso (0) e confirmar (1) sao aceitos       */
/* ========================================================================== */

void test_coleta_mpu6050_labels_aceitos(void)
{
    TEST_ASSERT_TRUE(coleta_mpu6050_label_valido(COLETA_MPU6050_LABEL_REPOUSO));
    TEST_ASSERT_TRUE(coleta_mpu6050_label_valido(COLETA_MPU6050_LABEL_CONFIRMAR));
    TEST_ASSERT_EQUAL_INT(0, COLETA_MPU6050_LABEL_REPOUSO);
    TEST_ASSERT_EQUAL_INT(1, COLETA_MPU6050_LABEL_CONFIRMAR);

    /* Rejeita valores fora do range */
    TEST_ASSERT_FALSE(coleta_mpu6050_label_valido(-1));
    TEST_ASSERT_FALSE(coleta_mpu6050_label_valido(2));
    TEST_ASSERT_FALSE(coleta_mpu6050_label_valido(99));
    TEST_ASSERT_FALSE(coleta_mpu6050_label_valido(-100));
}

/* ========================================================================== */
/* Formatacao CSV com valores tipicos e negativos                             */
/* ========================================================================== */

void test_coleta_mpu6050_formata_linha_csv_com_negativos(void)
{
    char linha[64];

    /* Cenario 1: valores mistos com label confirmar */
    TEST_ASSERT_EQUAL(ESP_OK,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        12345, -100, 0, 16384,
                                                        COLETA_MPU6050_LABEL_CONFIRMAR));
    TEST_ASSERT_EQUAL_STRING("12345,-100,0,16384,1", linha);

    /* Cenario 2: todos os eixos negativos com label repouso */
    TEST_ASSERT_EQUAL(ESP_OK,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        0, -500, -300, -16384,
                                                        COLETA_MPU6050_LABEL_REPOUSO));
    TEST_ASSERT_EQUAL_STRING("0,-500,-300,-16384,0", linha);

    /* Cenario 3: limites extremos int16_t */
    TEST_ASSERT_EQUAL(ESP_OK,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        999999, 32767, -32768, 0,
                                                        COLETA_MPU6050_LABEL_CONFIRMAR));
    TEST_ASSERT_EQUAL_STRING("999999,32767,-32768,0,1", linha);

    /* Cenario 4: timestamp zero, todos os eixos zero */
    TEST_ASSERT_EQUAL(ESP_OK,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        0, 0, 0, 0,
                                                        COLETA_MPU6050_LABEL_REPOUSO));
    TEST_ASSERT_EQUAL_STRING("0,0,0,0,0", linha);
}

/* ========================================================================== */
/* Rejeicao de parametros invalidos na formatacao CSV                          */
/* ========================================================================== */

void test_coleta_mpu6050_rejeita_linha_csv_invalida(void)
{
    char linha[8];

    /* Buffer NULL */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      coleta_mpu6050_formatar_linha_csv(NULL, 0, 1, 2, 3, 4,
                                                        COLETA_MPU6050_LABEL_REPOUSO));

    /* Tamanho zero */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      coleta_mpu6050_formatar_linha_csv(linha, 0, 1, 2, 3, 4,
                                                        COLETA_MPU6050_LABEL_REPOUSO));

    /* Label invalido */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        1, 2, 3, 4, 9));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        1, 2, 3, 4, -1));

    /* Buffer pequeno demais para caber os dados */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        123456, 32767, -32768, 0,
                                                        COLETA_MPU6050_LABEL_REPOUSO));
}

/* ========================================================================== */
/* Configuracao do estado inicial da coleta                                   */
/* ========================================================================== */

void test_coleta_mpu6050_configura_estado_inicial(void)
{
    coleta_mpu6050_t coleta;
    mpu6050_t mpu;

    memset(&mpu, 0, sizeof(mpu));
    coleta_mpu6050_configurar(&coleta, &mpu);

    /* Estado inicial correto */
    TEST_ASSERT_EQUAL_PTR(&mpu, coleta.mpu);
    TEST_ASSERT_TRUE(coleta.ativa);
    TEST_ASSERT_EQUAL_INT(COLETA_MPU6050_LABEL_REPOUSO, coleta.label);

    /* Troca de label para confirmar */
    TEST_ASSERT_EQUAL(ESP_OK, coleta_mpu6050_definir_label(&coleta, COLETA_MPU6050_LABEL_CONFIRMAR));
    TEST_ASSERT_EQUAL_INT(COLETA_MPU6050_LABEL_CONFIRMAR, coleta.label);

    /* Volta para repouso */
    TEST_ASSERT_EQUAL(ESP_OK, coleta_mpu6050_definir_label(&coleta, COLETA_MPU6050_LABEL_REPOUSO));
    TEST_ASSERT_EQUAL_INT(COLETA_MPU6050_LABEL_REPOUSO, coleta.label);

    /* Rejeita label invalido — label nao muda */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, coleta_mpu6050_definir_label(&coleta, 5));
    TEST_ASSERT_EQUAL_INT(COLETA_MPU6050_LABEL_REPOUSO, coleta.label);

    /* Rejeita coleta NULL */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, coleta_mpu6050_definir_label(NULL, COLETA_MPU6050_LABEL_REPOUSO));

    /* Parar coleta */
    coleta_mpu6050_parar(&coleta);
    TEST_ASSERT_FALSE(coleta.ativa);

    /* Parar NULL nao causa crash */
    coleta_mpu6050_parar(NULL);
}

/* ========================================================================== */
/* Fluxo completo: bytes brutos -> int16_t -> formatacao CSV                  */
/* ========================================================================== */

void test_coleta_mpu6050_pipeline_bytes_ate_csv(void)
{
    /* Simula o fluxo real do firmware:
       1. Bytes brutos lidos do MPU6050
       2. Convertidos para int16_t pelo driver
       3. Formatados como linha CSV pela coleta */

    const uint8_t bytes_sensor[MPU6050_ACELERACAO_BYTES] = {
        0x00, 0x64,   /* ax = +100 */
        0xFF, 0x9C,   /* ay = -100 */
        0x40, 0x00,   /* az = +16384 (gravidade) */
    };

    int16_t ax = 0, ay = 0, az = 0;
    TEST_ASSERT_EQUAL(ESP_OK, mpu6050_converter_bytes_aceleracao(bytes_sensor, &ax, &ay, &az));

    char linha[64];
    TEST_ASSERT_EQUAL(ESP_OK,
                      coleta_mpu6050_formatar_linha_csv(linha, sizeof(linha),
                                                        5000, ax, ay, az,
                                                        COLETA_MPU6050_LABEL_REPOUSO));
    TEST_ASSERT_EQUAL_STRING("5000,100,-100,16384,0", linha);
}
