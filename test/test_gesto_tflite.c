#include "unity.h"

#include "gesto_tflite.h"

void test_gesto_tflite_inicia_modelo_embarcado(void)
{
    gesto_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, gesto_tflite_iniciar(&modelo));
    TEST_ASSERT_TRUE(modelo.pronto);
    TEST_ASSERT_TRUE(modelo.usar_fallback_heuristico);
}

void test_gesto_tflite_classifica_janela_de_confirmacao(void)
{
    gesto_tflite_t modelo;
    serie_temporal_t serie;

    TEST_ASSERT_EQUAL(ESP_OK, gesto_tflite_iniciar(&modelo));
    serie_temporal_iniciar(&serie);

    for (int i = 0; i < SERIE_TEMPORAL_TAMANHO; i++) {
        int16_t pulso = (i % 2 == 0) ? 0 : 5000;
        TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(&serie, pulso, -pulso, 16384));
    }

    TEST_ASSERT_TRUE(gesto_tflite_classificar(&serie, &modelo));
    TEST_ASSERT_TRUE(modelo.ultimo_score > 0);
}

void test_gesto_tflite_fallback_sem_modelo(void)
{
    serie_temporal_t serie;

    serie_temporal_iniciar(&serie);
    for (int i = 0; i < SERIE_TEMPORAL_TAMANHO; i++) {
        int16_t pulso = (i % 2 == 0) ? 0 : 5000;
        TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(&serie, pulso, -pulso, 16384));
    }

    TEST_ASSERT_TRUE(gesto_tflite_classificar(&serie, NULL));
}
