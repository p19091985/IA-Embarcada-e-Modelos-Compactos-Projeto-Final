#include "unity.h"

#include "jogo_auditoria.h"

void test_auditoria_serializa_tabuleiro_com_pontos_para_vazio(void)
{
    jogo_estado_t jogo;
    char tabuleiro[JOGO_AUDITORIA_TABULEIRO_TAMANHO];

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'X'));

    jogo_auditoria_serializar_tabuleiro(&jogo, tabuleiro, sizeof(tabuleiro));

    TEST_ASSERT_EQUAL_STRING("O...X....", tabuleiro);
}

void test_auditoria_detecta_vitoria_do_jogador_e_linha(void)
{
    jogo_estado_t jogo;
    jogo_auditoria_resultado_t resultado;

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 3, 'O'));

    resultado = jogo_auditoria_analisar(&jogo);

    TEST_ASSERT_TRUE(resultado.vitoria_o);
    TEST_ASSERT_FALSE(resultado.vitoria_x);
    TEST_ASSERT_FALSE(resultado.empate);
    TEST_ASSERT_EQUAL_UINT8(3, resultado.casas_ocupadas);
    TEST_ASSERT_EQUAL_STRING("L1", resultado.linha_o);
    TEST_ASSERT_EQUAL_STRING("-", resultado.linha_x);
}

void test_auditoria_nao_declara_vencedor_antes_de_trinca_real(void)
{
    jogo_estado_t jogo;
    jogo_auditoria_resultado_t resultado;

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 9, 'O'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'X'));

    resultado = jogo_auditoria_analisar(&jogo);

    TEST_ASSERT_FALSE(resultado.vitoria_o);
    TEST_ASSERT_FALSE(resultado.vitoria_x);
    TEST_ASSERT_FALSE(resultado.empate);
    TEST_ASSERT_EQUAL_UINT8(4, resultado.casas_ocupadas);
}

void test_auditoria_converte_jogada_para_posicao_1a9(void)
{
    jogo_jogada_t jogada = {
        .linha = 2,
        .coluna = 1,
    };

    TEST_ASSERT_EQUAL_INT(8, jogo_auditoria_posicao_da_jogada(jogada));
    jogada.linha = -1;
    TEST_ASSERT_EQUAL_INT(0, jogo_auditoria_posicao_da_jogada(jogada));
}
