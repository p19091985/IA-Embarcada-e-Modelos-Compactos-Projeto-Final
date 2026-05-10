#include "unity.h"

#include "jogo_da_velha.h"

void test_linha_do_tabuleiro_cabe_no_oled(void)
{
    jogo_estado_t jogo;
    char linha[16];

    jogo_iniciar(&jogo);
    jogo_formatar_linha_tabuleiro(&jogo, 0, linha, sizeof(linha));

    TEST_ASSERT_EQUAL_STRING("1|2|3", linha);
}
