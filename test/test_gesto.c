#include "unity.h"

#include "gesto.h"

static void preencher_repouso(serie_temporal_t *serie)
{
    for (int i = 0; i < SERIE_TEMPORAL_TAMANHO; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(serie, 100, 50, 16384));
    }
}

static void preencher_confirmacao(serie_temporal_t *serie)
{
    for (int i = 0; i < SERIE_TEMPORAL_TAMANHO; i++) {
        int16_t pulso = (i % 2 == 0) ? 0 : 5000;
        TEST_ASSERT_EQUAL(ESP_OK, serie_temporal_adicionar(serie, pulso, -pulso, 16384));
    }
}

void test_gesto_heuristica_ignora_repouso(void)
{
    serie_temporal_t serie;

    serie_temporal_iniciar(&serie);
    preencher_repouso(&serie);

    TEST_ASSERT_FALSE(gesto_heuristica_detectar(&serie));
}

void test_gesto_heuristica_detecta_confirmacao(void)
{
    serie_temporal_t serie;

    serie_temporal_iniciar(&serie);
    preencher_confirmacao(&serie);

    TEST_ASSERT_TRUE(gesto_heuristica_detectar(&serie));
}

void test_gesto_detector_aplica_debounce(void)
{
    gesto_detector_t detector;
    gesto_evento_t evento = GESTO_EVENTO_NENHUM;

    gesto_detector_iniciar(&detector);

    for (int i = 0; i < SERIE_TEMPORAL_TAMANHO; i++) {
        int16_t pulso = (i % 2 == 0) ? 0 : 5000;
        evento = gesto_detector_processar_amostra(&detector, (uint32_t)(i * 20), pulso, -pulso, 16384);
    }

    TEST_ASSERT_EQUAL(GESTO_EVENTO_CONFIRMAR, evento);
    TEST_ASSERT_EQUAL(GESTO_EVENTO_NENHUM, gesto_detector_processar_amostra(&detector, 330, 5000, -5000, 16384));
    TEST_ASSERT_EQUAL(GESTO_EVENTO_CONFIRMAR, gesto_detector_processar_amostra(&detector, 1100, 0, 5000, 16384));
}
