#include "unity.h"

extern void test_jogo_detecta_vitoria_em_linha(void);
extern void test_jogo_rejeita_casa_ocupada(void);
extern void test_jogo_detecta_empate(void);
extern void test_ia_faz_jogada_vencedora(void);
extern void test_ia_bloqueia_vitoria_do_jogador(void);
extern void test_mapa_do_teclado_documentado(void);
extern void test_linha_do_tabuleiro_cabe_no_oled(void);

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_jogo_detecta_vitoria_em_linha);
    RUN_TEST(test_jogo_rejeita_casa_ocupada);
    RUN_TEST(test_jogo_detecta_empate);
    RUN_TEST(test_ia_faz_jogada_vencedora);
    RUN_TEST(test_ia_bloqueia_vitoria_do_jogador);
    RUN_TEST(test_mapa_do_teclado_documentado);
    RUN_TEST(test_linha_do_tabuleiro_cabe_no_oled);
    UNITY_END();
}
