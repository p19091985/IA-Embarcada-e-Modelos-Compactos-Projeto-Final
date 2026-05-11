#include "unity.h"

#include "serie_temporal.h"

void test_serie_temporal_buffer_circular(void)
{
    serie_temporal_t serie;
    serie_temporal_amostra_t amostra;

    serie_temporal_iniciar(&serie);

    for (int i = 0; i < SERIE_TEMPORAL_TAMANHO + 2; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(&serie, i, i + 10, i + 20));
    }

    TEST_ASSERT_TRUE(serie_temporal_cheia(&serie));
    TEST_ASSERT_EQUAL_UINT(SERIE_TEMPORAL_TAMANHO, serie_temporal_quantidade(&serie));

    TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_obter(&serie, 0, &amostra));
    TEST_ASSERT_EQUAL_INT16(2, amostra.ax);
    TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_obter(&serie, SERIE_TEMPORAL_TAMANHO - 1, &amostra));
    TEST_ASSERT_EQUAL_INT16(SERIE_TEMPORAL_TAMANHO + 1, amostra.ax);
}

void test_serie_temporal_calcula_features(void)
{
    serie_temporal_t serie;
    serie_temporal_features_t features;

    serie_temporal_iniciar(&serie);
    TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(&serie, 0, 100, 200));
    TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(&serie, 1000, -100, 300));
    TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(&serie, -500, 400, -100));

    TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_calcular_features(&serie, &features));
    TEST_ASSERT_EQUAL_INT32(1500, features.amplitude_ax);
    TEST_ASSERT_EQUAL_INT32(500, features.amplitude_ay);
    TEST_ASSERT_EQUAL_INT32(400, features.amplitude_az);
    TEST_ASSERT_EQUAL_INT32(1500, features.maior_amplitude);
    TEST_ASSERT_TRUE(features.media_abs_delta > 0);
}

void test_serie_temporal_preprocessa_int8(void)
{
    serie_temporal_t serie;
    int8_t entrada[SERIE_TEMPORAL_ENTRADA_TINYML];

    serie_temporal_iniciar(&serie);
    for (int i = 0; i < SERIE_TEMPORAL_TAMANHO; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(&serie, i * 256, 0, -i * 256));
    }

    TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_preprocessar_int8(&serie, entrada, sizeof(entrada)));
    TEST_ASSERT_EQUAL_INT8(0, entrada[0]);
    TEST_ASSERT_EQUAL_INT8(0, entrada[1]);
    TEST_ASSERT_EQUAL_INT8(0, entrada[2]);
    TEST_ASSERT_EQUAL_INT8(1, entrada[3]);
    TEST_ASSERT_EQUAL_INT8(0, entrada[4]);
    TEST_ASSERT_EQUAL_INT8(-1, entrada[5]);
}
