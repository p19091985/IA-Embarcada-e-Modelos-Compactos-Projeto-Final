#include "unity.h"

#include <string.h>

#include "ia_tflite.h"

void test_ia_tflite_converte_tabuleiro_para_vetor(void)
{
    jogo_estado_t jogo;
    int8_t entrada[TICTACTOE_MODEL_INPUT_SIZE];

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'X'));

    ia_tflite_tabuleiro_para_entrada(&jogo, entrada);

    TEST_ASSERT_EQUAL_INT8(-1, entrada[0]);
    TEST_ASSERT_EQUAL_INT8(1, entrada[4]);
    TEST_ASSERT_EQUAL_INT8(0, entrada[8]);
}

void test_ia_tflite_mascara_casas_ocupadas(void)
{
    jogo_estado_t jogo;
    int8_t scores[TICTACTOE_MODEL_OUTPUT_SIZE] = {
        127, 10, 9,
        8, 7, 6,
        5, 4, 3,
    };

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'O'));

    TEST_ASSERT_EQUAL_INT(1, ia_tflite_escolher_indice_com_mascara(scores, &jogo));
}

void test_ia_tflite_nao_usa_fallback_quando_inferencia_falha(void)
{
    jogo_estado_t jogo;
    ia_tflite_t ia;
    ia_resultado_t resultado;

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'O'));

    TEST_ASSERT_EQUAL(ESP_OK, ia_tflite_iniciar(&ia));
    ia_tflite_forcar_falha(&ia, true);
    resultado = ia_tflite_escolher_jogada(&ia, &jogo);

    TEST_ASSERT_EQUAL(IA_TFLITE, resultado.algoritmo);
    TEST_ASSERT_EQUAL_INT(-1, resultado.jogada.linha);
    TEST_ASSERT_EQUAL_INT(-1, resultado.jogada.coluna);
}

void test_ia_tflite_prefere_vitoria_imediata(void)
{
    jogo_estado_t jogo;
    ia_tflite_t ia;
    ia_resultado_t resultado;

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'O'));

    TEST_ASSERT_EQUAL(ESP_OK, ia_tflite_iniciar(&ia));
    resultado = ia_tflite_escolher_jogada(&ia, &jogo);

    TEST_ASSERT_EQUAL(IA_TFLITE, resultado.algoritmo);
    TEST_ASSERT_EQUAL_INT(0, resultado.jogada.linha);
    TEST_ASSERT_EQUAL_INT(2, resultado.jogada.coluna);
}

void test_ia_tflite_header_tem_metadados_de_treinamento(void)
{
    TEST_ASSERT_GREATER_THAN_INT(1000, TICTACTOE_MODEL_DATASET_ROWS);
    TEST_ASSERT_EQUAL_STRING("full_integer_int8", TICTACTOE_MODEL_QUANTIZATION);
    TEST_ASSERT_EQUAL_STRING("codigo/tflite_hello_world_training.ipynb", TICTACTOE_MODEL_NOTEBOOK);
    TEST_ASSERT_EQUAL_INT(64, strlen(TICTACTOE_MODEL_INT8_SHA256));
    TEST_ASSERT_GREATER_THAN_INT(9000, TICTACTOE_MODEL_OPTIMAL_MOVE_PERMYRIAD);
}
