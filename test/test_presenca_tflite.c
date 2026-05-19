#include "unity.h"

#include <string.h>

#include "presenca_tflite.h"

void test_presenca_compacto_detecta_jogador_proximo(void)
{
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(5, 5 * 58));
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(50, 50 * 58));
    TEST_ASSERT_TRUE(presenca_modelo_compacto_classificar(PRESENCA_MODEL_DISTANCIA_LIMIAR_CM,
                                                          PRESENCA_MODEL_ECO_LIMIAR_US));
}

void test_presenca_compacto_detecta_ausencia(void)
{
    TEST_ASSERT_FALSE(presenca_modelo_compacto_classificar(0, 0));
    TEST_ASSERT_FALSE(presenca_modelo_compacto_classificar(PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1,
                                                           (PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1) * 58));
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
    int32_t longe = presenca_modelo_compacto_score(200, 200 * 58);

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
    TEST_ASSERT_TRUE(presenca_tflite_classificar(&modelo, 60, 60 * 58));
}

void test_presenca_tflite_classifica_amostra_distante(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, 250, 250 * 58));
}

void test_presenca_tflite_rejeita_distancias_invalidas_antes_da_inferencia(void)
{
    presenca_tflite_t modelo;

    TEST_ASSERT_EQUAL(ESP_OK, presenca_tflite_iniciar(&modelo));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, 0, 0));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1,
                                                  (PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM - 1) * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(&modelo, PRESENCA_MODEL_DISTANCIA_MAX_CM + 1,
                                                  (PRESENCA_MODEL_DISTANCIA_MAX_CM + 1) * 58));
}

void test_presenca_tflite_fallback_compacto_quando_modelo_nulo(void)
{
    TEST_ASSERT_TRUE(presenca_tflite_classificar(NULL, 40, 40 * 58));
    TEST_ASSERT_FALSE(presenca_tflite_classificar(NULL, 260, 260 * 58));
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
}
