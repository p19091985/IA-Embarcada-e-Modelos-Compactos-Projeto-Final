#include "unity.h"

#include "ia_tflite.h"
#include "jogo_da_velha.h"

/*
 * Testes de fidelidade ao jogo legado Alpha0.
 * Garante que a mecanica e a jogabilidade do jogo atual reproduzem o comportamento
 * do codigo original contido em codigo-ET/.../main.c.
 */

/* ========================================================================== */
/* Jogador sempre comeca como 'O', computador sempre joga como 'X'            */
/* ========================================================================== */

void test_alpha0_jogador_sempre_usa_O(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    /* Jogador marca posicao 5 (centro) como 'O' */
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'O'));
    TEST_ASSERT_EQUAL_CHAR('O', jogo.casas[1][1]);
}

void test_alpha0_computador_sempre_usa_X(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    /* Computador marca posicao 1 como 'X' */
    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 1, 'X'));
    TEST_ASSERT_EQUAL_CHAR('X', jogo.casas[0][0]);
}

/* ========================================================================== */
/* Tabuleiro inicial mostra numeros 1-9 na primeira exibicao                  */
/* ========================================================================== */

void test_alpha0_tabuleiro_inicial_mostra_numeros(void)
{
    jogo_estado_t jogo;
    char linha[16];

    jogo_iniciar(&jogo);
    TEST_ASSERT_TRUE(jogo.mostrar_numeros);

    jogo_formatar_linha_tabuleiro_expandida(&jogo, 0, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING(" 1 | 2 | 3 ", linha);

    jogo_formatar_linha_tabuleiro_expandida(&jogo, 1, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING(" 4 | 5 | 6 ", linha);

    jogo_formatar_linha_tabuleiro_expandida(&jogo, 2, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING(" 7 | 8 | 9 ", linha);
}

/* ========================================================================== */
/* Apos a primeira jogada, numeros sao substituidos pelos simbolos            */
/* ========================================================================== */

void test_alpha0_numeros_desaparecem_apos_primeira_jogada(void)
{
    jogo_estado_t jogo;
    char linha[16];

    jogo_iniciar(&jogo);
    jogo_aplicar_posicao(&jogo, 5, 'O');

    TEST_ASSERT_FALSE(jogo.mostrar_numeros);

    /* Posicoes vazias devem ter espaco, nao numeros */
    jogo_formatar_linha_tabuleiro_expandida(&jogo, 0, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING("   |   |   ", linha);

    /* Centro deve ter O */
    jogo_formatar_linha_tabuleiro_expandida(&jogo, 1, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING("   | O |   ", linha);
}

/* ========================================================================== */
/* Posicao ocupada e rejeitada (jogador nao pode sobrescrever)                */
/* ========================================================================== */

void test_alpha0_rejeita_posicao_ocupada(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, 5, 'O'));
    TEST_ASSERT_FALSE(jogo_aplicar_posicao(&jogo, 5, 'X'));
    TEST_ASSERT_FALSE(jogo_aplicar_posicao(&jogo, 5, 'O'));
    TEST_ASSERT_EQUAL_CHAR('O', jogo.casas[1][1]);
}

/* ========================================================================== */
/* Posicoes 1-9 mapeiam corretamente para o tabuleiro (como no Alpha0)        */
/* ========================================================================== */

void test_alpha0_mapeamento_posicao_1a9(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    /* Posicao 1 = [0][0], 2 = [0][1], ..., 9 = [2][2] */
    for (int pos = 1; pos <= 9; pos++) {
        jogo_resetar_tabuleiro(&jogo);
        TEST_ASSERT_TRUE(jogo_aplicar_posicao(&jogo, pos, 'X'));

        int esperado_linha = (pos - 1) / 3;
        int esperado_coluna = (pos - 1) % 3;
        TEST_ASSERT_EQUAL_CHAR('X', jogo.casas[esperado_linha][esperado_coluna]);
    }
}

/* ========================================================================== */
/* Posicoes invalidas (0, negativo, >9) sao rejeitadas                        */
/* ========================================================================== */

void test_alpha0_rejeita_posicao_invalida(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    TEST_ASSERT_FALSE(jogo_aplicar_posicao(&jogo, 0, 'O'));
    TEST_ASSERT_FALSE(jogo_aplicar_posicao(&jogo, -1, 'O'));
    TEST_ASSERT_FALSE(jogo_aplicar_posicao(&jogo, 10, 'O'));
}

/* ========================================================================== */
/* Vitoria detectada em todas as 8 combinacoes (3 linhas, 3 colunas, 2 diag.) */
/* ========================================================================== */

void test_alpha0_vitoria_todas_combinacoes(void)
{
    /* As 8 combinacoes vencedoras no jogo da velha 3x3 */
    int combinacoes[8][3] = {
        {1, 2, 3}, {4, 5, 6}, {7, 8, 9}, /* linhas */
        {1, 4, 7}, {2, 5, 8}, {3, 6, 9}, /* colunas */
        {1, 5, 9}, {3, 5, 7},            /* diagonais */
    };

    for (int c = 0; c < 8; c++) {
        jogo_estado_t jogo;
        jogo_iniciar(&jogo);

        for (int i = 0; i < 3; i++) {
            jogo_aplicar_posicao(&jogo, combinacoes[c][i], 'O');
        }

        TEST_ASSERT_TRUE_MESSAGE(
            jogo_verificar_vitoria(&jogo, 'O'),
            "Falhou ao detectar vitoria na combinacao"
        );
        TEST_ASSERT_FALSE(jogo_verificar_vitoria(&jogo, 'X'));
    }
}

/* ========================================================================== */
/* Empate correto: tabuleiro cheio sem vencedor                               */
/* ========================================================================== */

void test_alpha0_empate_tabuleiro_cheio(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    /* Tabuleiro classico de empate:
     * X | O | X
     * X | O | O
     * O | X | X
     */
    jogo.casas[0][0] = 'X'; jogo.casas[0][1] = 'O'; jogo.casas[0][2] = 'X';
    jogo.casas[1][0] = 'X'; jogo.casas[1][1] = 'O'; jogo.casas[1][2] = 'O';
    jogo.casas[2][0] = 'O'; jogo.casas[2][1] = 'X'; jogo.casas[2][2] = 'X';
    jogo.mostrar_numeros = false;

    TEST_ASSERT_TRUE(jogo_verificar_empate(&jogo));
    TEST_ASSERT_FALSE(jogo_verificar_vitoria(&jogo, 'X'));
    TEST_ASSERT_FALSE(jogo_verificar_vitoria(&jogo, 'O'));
}

/* ========================================================================== */
/* Placar: zerado ao iniciar, incrementa corretamente                         */
/* ========================================================================== */

void test_alpha0_placar_inicio_zerado(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    TEST_ASSERT_EQUAL_UINT16(0, jogo.vitorias_jogador);
    TEST_ASSERT_EQUAL_UINT16(0, jogo.vitorias_computador);
    TEST_ASSERT_EQUAL_UINT16(0, jogo.empates);
}

void test_alpha0_placar_zerar_preserva_tabuleiro(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    jogo.vitorias_jogador = 5;
    jogo.vitorias_computador = 3;
    jogo.empates = 2;
    jogo_aplicar_posicao(&jogo, 5, 'X');

    jogo_zerar_placar(&jogo);

    TEST_ASSERT_EQUAL_UINT16(0, jogo.vitorias_jogador);
    TEST_ASSERT_EQUAL_UINT16(0, jogo.vitorias_computador);
    TEST_ASSERT_EQUAL_UINT16(0, jogo.empates);
    /* Tabuleiro NAO e resetado ao zerar placar */
    TEST_ASSERT_EQUAL_CHAR('X', jogo.casas[1][1]);
}

/* ========================================================================== */
/* IA computador sempre faz jogada valida (casa vazia)                        */
/* ========================================================================== */

void test_alpha0_ia_nunca_joga_em_casa_ocupada(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    /* Preenche quase todo o tabuleiro, deixando so posicao 9 */
    jogo.casas[0][0] = 'X'; jogo.casas[0][1] = 'O'; jogo.casas[0][2] = 'X';
    jogo.casas[1][0] = 'O'; jogo.casas[1][1] = 'X'; jogo.casas[1][2] = 'O';
    jogo.casas[2][0] = 'O'; jogo.casas[2][1] = 'X'; /* [2][2] livre */
    jogo.mostrar_numeros = false;

    ia_tflite_t ia;
    TEST_ASSERT_EQUAL(ESP_OK, ia_tflite_iniciar(&ia));
    ia_resultado_t resultado = ia_tflite_escolher_jogada(&ia, &jogo);

    TEST_ASSERT_EQUAL_INT(2, resultado.jogada.linha);
    TEST_ASSERT_EQUAL_INT(2, resultado.jogada.coluna);
}

/* ========================================================================== */
/* Tabuleiro resetado entre partidas                                          */
/* ========================================================================== */

void test_alpha0_tabuleiro_resetado_entre_partidas(void)
{
    jogo_estado_t jogo;
    jogo_iniciar(&jogo);

    jogo_aplicar_posicao(&jogo, 1, 'X');
    jogo_aplicar_posicao(&jogo, 5, 'O');

    jogo_resetar_tabuleiro(&jogo);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            TEST_ASSERT_EQUAL_CHAR(' ', jogo.casas[i][j]);
        }
    }

    TEST_ASSERT_TRUE(jogo.mostrar_numeros);
}

/* ========================================================================== */
/* Formato expandido do tabuleiro: " X | O |   " com separador "---+---+---"  */
/* ========================================================================== */

void test_alpha0_formato_tabuleiro_expandido(void)
{
    jogo_estado_t jogo;
    char linha[16];

    jogo_iniciar(&jogo);
    jogo_aplicar_posicao(&jogo, 1, 'X');
    jogo_aplicar_posicao(&jogo, 5, 'O');
    jogo_aplicar_posicao(&jogo, 9, 'X');

    jogo_formatar_linha_tabuleiro_expandida(&jogo, 0, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING(" X |   |   ", linha);

    jogo_formatar_linha_tabuleiro_expandida(&jogo, 1, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING("   | O |   ", linha);

    jogo_formatar_linha_tabuleiro_expandida(&jogo, 2, linha, sizeof(linha));
    TEST_ASSERT_EQUAL_STRING("   |   | X ", linha);
}

/* ========================================================================== */
/* Algoritmo exibido no LCD: nomes devem corresponder ao enum                 */
/* ========================================================================== */

void test_alpha0_nomes_algoritmos_lcd(void)
{
    TEST_ASSERT_EQUAL_STRING("TFLite", ia_nome(IA_TFLITE));
}
