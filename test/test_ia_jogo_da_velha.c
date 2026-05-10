#include "unity.h"

#include "ia_jogo_da_velha.h"

void test_ia_faz_jogada_vencedora(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'O'));

    ia_resultado_t resultado = ia_escolher_jogada(&jogo);
    TEST_ASSERT_EQUAL_INT(0, resultado.jogada.linha);
    TEST_ASSERT_EQUAL_INT(2, resultado.jogada.coluna);
}

void test_ia_bloqueia_vitoria_do_jogador(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'X'));

    ia_resultado_t resultado = ia_escolher_jogada(&jogo);
    TEST_ASSERT_EQUAL_INT(0, resultado.jogada.linha);
    TEST_ASSERT_EQUAL_INT(2, resultado.jogada.coluna);
}
