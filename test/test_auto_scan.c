#include "unity.h"

#include "auto_scan.h"

void test_auto_scan_inicia_na_primeira_casa_livre(void)
{
    jogo_estado_t jogo;
    auto_scan_t scan;

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'X'));
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'O'));

    auto_scan_iniciar(&scan, &jogo, 100);

    TEST_ASSERT_TRUE(scan.ativo);
    TEST_ASSERT_EQUAL_INT(3, auto_scan_posicao_atual(&scan));
}

void test_auto_scan_avanca_e_pula_ocupadas(void)
{
    jogo_estado_t jogo;
    auto_scan_t scan;

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 2, 'X'));

    auto_scan_iniciar(&scan, &jogo, 0);
    TEST_ASSERT_EQUAL_INT(1, auto_scan_posicao_atual(&scan));

    TEST_ASSERT_TRUE(auto_scan_atualizar(&scan, &jogo, AUTO_SCAN_INTERVALO_MS));
    TEST_ASSERT_EQUAL_INT(3, auto_scan_posicao_atual(&scan));
}

void test_auto_scan_confirmar_aplica_jogada(void)
{
    jogo_estado_t jogo;
    auto_scan_t scan;

    jogo_iniciar(&jogo);
    auto_scan_iniciar(&scan, &jogo, 0);

    TEST_ASSERT_TRUE(auto_scan_confirmar(&scan, &jogo, 'O'));
    TEST_ASSERT_EQUAL_CHAR('O', jogo.casas[0][0]);
    TEST_ASSERT_FALSE(auto_scan_confirmar(&scan, &jogo, 'X'));
}
