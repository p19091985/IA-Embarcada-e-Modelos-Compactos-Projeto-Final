#include "unity.h"

#include "jogo_da_velha.h"

void test_jogo_detecta_vitoria_em_linha(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 3, 'O'));
    TEST_ASSERT_TRUE(jogo_verificar_vitoria(&jogo, 'O'));
}

void test_jogo_rejeita_casa_ocupada(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'O'));
    TEST_ASSERT_FALSE(jogo_aplicar_posicao(&jogo, 5, 'X'));
}

void test_jogo_detecta_empate(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    const char casas[JOGO_CELULAS] = {'O', 'X', 'O', 'O', 'X', 'X', 'X', 'O', 'O'};
    for (int i = 0; i < JOGO_CELULAS; i++) {
        TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, i + 1, casas[i]));
    }

    TEST_ASSERT_TRUE(jogo_verificar_empate(&jogo));
}
