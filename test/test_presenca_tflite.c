#include "unity.h"

#include <string.h>

#include "hcsr04.h"
#include "jogo_interface.h"
#include "presenca_tflite.h"

void test_presenca_compacto_detecta_jogador_proximo(void)
{
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(2, 2 * 58));
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(5, 5 * 58));
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(50, 50 * 58));
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(69, 69 * 58));
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(PRESENCA_MODEL_DISTANCIA_LIMIAR_CM,
                                                          PRESENCA_MODEL_ECO_LIMIAR_US));
}

void test_presenca_compacto_detecta_ausencia(void)
{
    TEST_ASSERT_FALSE(presenca_modelo_compacto_classificar(0, 0));
    TEST_ASSERT_FALSE(presenca_modelo_compacto_classificar(PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1,
                                                           (PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1) * 58));
    TEST_ASSERT_FALSE(presenca_modelo_compacto_classificar(PRESENCA_MODEL_DISTANCIA_AUSENTE_A_PARTIR_CM,
                                                           PRESENCA_MODEL_DISTANCIA_AUSENTE_A_PARTIR_CM * 58));
    TEST_ASSERT_FALSE(presenca_modelo_compacto_classificar(PRESENCA_MODEL_DISTANCIA_LIMIAR_CM + 30,
                                                           (PRESENCA_MODEL_DISTANCIA_LIMIAR_CM + 30) * 58));
    TEST_ASSERT_FALSE(presenca_modelo_compacto_classificar(PRESENCA_MODEL_DISTANCIA_MAX_CM + 1,
                                                           (PRESENCA_MODEL_DISTANCIA_MAX_CM + 1) * 58));
}

void test_presenca_compacto_score_cai_com_distancia(void)
{
    int32_t perto = presenca_modelo_compacto_score(30, 30 * 58);
    int32_t limiar = presenca_modelo_compacto_score(PRESENCA_MODEL_DISTANCIA_LIMIAR_CM,
                                                    PRESENCA_MODEL_ECO_LIMIAR_US);
    int32_t longe = presenca_modelo_compacto_score(250, 250 * 58);

    TEST_ASSERT_GREATER_THAN_INT32(limiar, perto);
    TEST_ASSERT_GREATER_THAN_INT32(longe, limiar);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, perto);
    TEST_ASSERT_LESS_THAN_INT32(0, longe);
}

void test_presenca_tflite_inicia_com_modelo_int8_real(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));
    TEST_ASSERT_TRUE(modelo.pronto);
    TEST_ASSERT_TRUE(modelo.runtime_tflite);
    TEST_ASSERT_EQUAL_INT(PRESENCA_MODEL_TENSOR_ARENA_BYTES, modelo.arena_bytes);
}

void test_presenca_tflite_classifica_amostra_proxima(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));
    TEST_ASSERT_TRUE(presenca_tflite_classificar(&modelo, 2, 2 * 58));
    TEST_ASSERT_TRUE(presenca_tflite_classificar(&modelo, 60, 60 * 58));
    TEST_ASSERT_TRUE(presenca_tflite_classificar(&modelo, 69, 69 * 58));
}

void test_presenca_tflite_classifica_amostra_distante(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, 70, 70 * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, 74, 74 * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, 250, 250 * 58));
}

void test_presenca_tflite_score_usa_saida_int8_do_modelo(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));
    TEST_ASSERT_TRUE(presenca_tflite_classificar(&modelo, 40, 40 * 58));
    TEST_ASSERT_GREATER_THAN_INT32(0, modelo.ultimo_score);
    TEST_ASSERT_LESS_OR_EQUAL_INT32(255, modelo.ultimo_score);
}

void test_presenca_tflite_classifica_ausencia_nas_fronteiras(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, 0, 0));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1,
                                                  (PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1) * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, PRESENCA_MODEL_DISTANCIA_AUSENTE_A_PARTIR_CM,
                                                  PRESENCA_MODEL_DISTANCIA_AUSENTE_A_PARTIR_CM * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, PRESENCA_MODEL_DISTANCIA_MAX_CM + 1,
                                                  (PRESENCA_MODEL_DISTANCIA_MAX_CM + 1) * 58));
}

void test_presenca_tflite_fallback_compacto_quando_modelo_nulo(void)
{
    TEST_ASSERT_TRUE(presenca_tflite_classificar(NULL, 2, 2 * 58));
    TEST_ASSERT_TRUE(presenca_tflite_classificar(NULL, 40, 40 * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(NULL, 70, 70 * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(NULL, 260, 260 * 58));
}

void test_presenca_timeout_do_hcsr04_substitui_leitura_antiga_por_ausencia(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));

    presenca_tflite_resultado_t leitura_antiga =
        presenca_tflite_avaliar_leitura_hcsr04(&modelo, ESP_OK, 31, 31 * HCSR04_ECHO_US_POR_CM);
    TEST_ASSERT_TRUE(leitura_antiga.leitura_valida);
    TEST_ASSERT_TRUE(leitura_antiga.presente);
    TEST_ASSERT_EQUAL_UINT16(31, leitura_antiga.distancia_cm);
    TEST_ASSERT_TRUE(jogo_interacao_liberada_por_presenca(true, leitura_antiga.leitura_valida, leitura_antiga.presente));

    presenca_tflite_resultado_t timeout =
        presenca_tflite_avaliar_leitura_hcsr04(&modelo, ESP_ERR_TIMEOUT, 31, 31 * HCSR04_ECHO_US_POR_CM);
    TEST_ASSERT_TRUE(timeout.leitura_valida);
    TEST_ASSERT_FALSE(timeout.presente);
    TEST_ASSERT_EQUAL_UINT16(HCSR04_DISTANCIA_MAX_CM, timeout.distancia_cm);
    TEST_ASSERT_EQUAL_UINT32(HCSR04_DISTANCIA_MAX_CM * HCSR04_ECHO_US_POR_CM, timeout.eco_us);
    TEST_ASSERT_LESS_THAN_INT32(0, timeout.score);
    TEST_ASSERT_EQUAL_INT32(timeout.score, modelo.ultimo_score);
    TEST_ASSERT_FALSE(jogo_interacao_liberada_por_presenca(true, timeout.leitura_valida, timeout.presente));
}

void test_presenca_quatro_metros_eh_ausente_e_bloqueia_teclado(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));

    presenca_tflite_resultado_t resultado =
        presenca_tflite_avaliar_leitura_hcsr04(&modelo,
                                               ESP_OK,
                                               HCSR04_DISTANCIA_MAX_CM,
                                               HCSR04_DISTANCIA_MAX_CM * HCSR04_ECHO_US_POR_CM);

    TEST_ASSERT_TRUE(resultado.leitura_valida);
    TEST_ASSERT_FALSE(resultado.presente);
    TEST_ASSERT_EQUAL_UINT16(400, resultado.distancia_cm);
    TEST_ASSERT_LESS_THAN_INT32(0, resultado.score);
    TEST_ASSERT_FALSE(jogo_interacao_liberada_por_presenca(true, resultado.leitura_valida, resultado.presente));
    TEST_ASSERT_FALSE(jogo_interacao_tecla_liberada_por_presenca('A',
                                                                 true,
                                                                 resultado.leitura_valida,
                                                                 resultado.presente));
}

void test_presenca_modelo_embarcado_tem_flatbuffer_real(void)
{
    TEST_ASSERT_GREATER_THAN_UINT32(512, PRESENCA_MODEL_TFLITE_LEN);
    TEST_ASSERT_EQUAL_UINT8(0x54, PRESENCA_MODEL_TFLITE[4]);
    TEST_ASSERT_EQUAL_UINT8(0x46, PRESENCA_MODEL_TFLITE[5]);
    TEST_ASSERT_EQUAL_UINT8(0x4c, PRESENCA_MODEL_TFLITE[6]);
    TEST_ASSERT_EQUAL_UINT8(0x33, PRESENCA_MODEL_TFLITE[7]);
}

void test_presenca_header_tem_metadados_de_treinamento(void)
{
    TEST_ASSERT_GREATER_THAN_INT(1000, PRESENCA_MODEL_DATASET_ROWS);
    TEST_ASSERT_EQUAL_STRING("full_integer_int8", PRESENCA_MODEL_QUANTIZATION);
    TEST_ASSERT_EQUAL_STRING("codigo/tflite_hello_world_training.ipynb", PRESENCA_MODEL_NOTEBOOK);
    TEST_ASSERT_EQUAL_INT(64, strlen(PRESENCA_MODEL_INT8_SHA256));
    TEST_ASSERT_GREATER_THAN_INT(9500, PRESENCA_MODEL_TEST_ACCURACY_PERMYRIAD);
    TEST_ASSERT_EQUAL_UINT16(70, PRESENCA_MODEL_DISTANCIA_AUSENTE_A_PARTIR_CM);
}
