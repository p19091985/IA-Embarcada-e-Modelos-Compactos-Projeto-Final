#include "unity.h"

#include <string.h>

#include "jogo_interface.h"

void test_interacao_libera_teclado_quando_sensor_de_presenca_indisponivel(void)
{
    TEST_ASSERT_TRUE(jogo_interacao_liberada_por_presenca(false, false, false));
}

void test_interacao_bloqueia_teclado_enquanto_presenca_nao_tem_leitura_valida(void)
{
    TEST_ASSERT_FALSE(jogo_interacao_liberada_por_presenca(true, false, false));
    TEST_ASSERT_FALSE(jogo_interacao_liberada_por_presenca(true, false, true));
}

void test_interacao_bloqueia_teclado_quando_jogador_ausente(void)
{
    TEST_ASSERT_FALSE(jogo_interacao_liberada_por_presenca(true, true, false));
}

void test_interacao_libera_teclado_quando_jogador_presente(void)
{
    TEST_ASSERT_TRUE(jogo_interacao_liberada_por_presenca(true, true, true));
}

void test_interacao_ignora_todos_os_botoes_quando_jogador_ausente(void)
{
    const char teclas[] = {'A', 'B', 'C', 'D', '0', '1', '5', '9', '*', '#'};

    for (size_t i = 0; i < sizeof(teclas); i++) {
        TEST_ASSERT_FALSE(jogo_interacao_tecla_liberada_por_presenca(teclas[i], true, true, false));
    }
}

void test_interacao_nao_bloqueia_ausencia_de_tecla(void)
{
    TEST_ASSERT_TRUE(jogo_interacao_tecla_liberada_por_presenca(0, true, true, false));
}

void test_interacao_libera_todos_os_botoes_quando_jogador_presente(void)
{
    const char teclas[] = {'A', '1', '*', '#'};

    for (size_t i = 0; i < sizeof(teclas); i++) {
        TEST_ASSERT_TRUE(jogo_interacao_tecla_liberada_por_presenca(teclas[i], true, true, true));
    }
}

void test_estatisticas_lcd_mostra_media_em_decimos_de_segundo(void)
{
    char linha0[JOGO_INTERFACE_LCD_TAMANHO_LINHA];
    char linha1[JOGO_INTERFACE_LCD_TAMANHO_LINHA];

    jogo_interface_formatar_estatisticas_lcd(6000, 9, 6000, linha0, linha1);

    TEST_ASSERT_EQUAL_STRING("Tempo 00:06", linha0);
    TEST_ASSERT_EQUAL_STRING("Jgd:09 Med:0.7s", linha1);
}

void test_estatisticas_lcd_mostra_zero_sem_jogadas(void)
{
    char linha0[JOGO_INTERFACE_LCD_TAMANHO_LINHA];
    char linha1[JOGO_INTERFACE_LCD_TAMANHO_LINHA];

    jogo_interface_formatar_estatisticas_lcd(0, 0, 0, linha0, linha1);

    TEST_ASSERT_EQUAL_STRING("Tempo 00:00", linha0);
    TEST_ASSERT_EQUAL_STRING("Jgd:00 Med:0.0s", linha1);
}

void test_estatisticas_lcd_preserva_largura_de_16_colunas(void)
{
    char linha0[JOGO_INTERFACE_LCD_TAMANHO_LINHA];
    char linha1[JOGO_INTERFACE_LCD_TAMANHO_LINHA];

    jogo_interface_formatar_estatisticas_lcd(123000, 12, 123000, linha0, linha1);

    TEST_ASSERT_EQUAL_STRING("Tempo 02:03", linha0);
    TEST_ASSERT_EQUAL_STRING("Jgd:12 Med:10.3s", linha1);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(JOGO_INTERFACE_LCD_COLUNAS, (uint32_t)strlen(linha0));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(JOGO_INTERFACE_LCD_COLUNAS, (uint32_t)strlen(linha1));
}
