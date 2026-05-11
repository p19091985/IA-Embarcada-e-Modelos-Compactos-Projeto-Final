#include "unity.h"

#include "jogo_da_velha.h"
#include "lcd1602_i2c.h"

void test_linha_do_tabuleiro_cabe_no_oled(void)
{
    jogo_estado_t jogo;
    char linha[16];

    jogo_iniciar(&jogo);
    jogo_formatar_linha_tabuleiro(&jogo, 0, linha, sizeof(linha));

    TEST_ASSERT_EQUAL_STRING("1|2|3", linha);
}

void test_linha_do_tabuleiro_expandida_preserva_espacamento(void)
{
    jogo_estado_t jogo;
    char linha[16];

    jogo_iniciar(&jogo);
    jogo_formatar_linha_tabuleiro_expandida(&jogo, 0, linha, sizeof(linha));

    TEST_ASSERT_EQUAL_STRING(" 1 | 2 | 3 ", linha);
}

void test_lcd1602_scroll_autores_primeira_janela(void)
{
    char linha[LCD1602_COLUNAS + 1];

    lcd1602_formatar_janela_scroll("Autores : Janiel e Patrik", 0, linha);

    TEST_ASSERT_EQUAL_STRING("Autores : Janiel", linha);
}

void test_lcd1602_scroll_autores_move_para_esquerda(void)
{
    char linha[LCD1602_COLUNAS + 1];

    lcd1602_formatar_janela_scroll("Autores : Janiel e Patrik", 1, linha);

    TEST_ASSERT_EQUAL_STRING("utores : Janiel ", linha);
}

void test_lcd1602_scroll_autores_reinicia_com_espaco(void)
{
    char linha[LCD1602_COLUNAS + 1];

    lcd1602_formatar_janela_scroll("Autores : Janiel e Patrik", 26, linha);

    TEST_ASSERT_EQUAL_STRING("   Autores : Jan", linha);
}
