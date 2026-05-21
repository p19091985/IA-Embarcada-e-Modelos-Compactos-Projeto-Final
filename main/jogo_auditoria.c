#include "jogo_auditoria.h"

#include <stdio.h>

static bool encontrar_linha_vencedora(const jogo_estado_t *jogo,
                                      char jogador,
                                      char *saida,
                                      size_t tamanho)
{
    if (jogo == NULL || saida == NULL || tamanho == 0) {
        return false;
    }

    snprintf(saida, tamanho, "-");

    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        if (jogo->casas[linha][0] == jogador &&
            jogo->casas[linha][1] == jogador &&
            jogo->casas[linha][2] == jogador) {
            snprintf(saida, tamanho, "L%d", linha + 1);
            return true;
        }
    }

    for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
        if (jogo->casas[0][coluna] == jogador &&
            jogo->casas[1][coluna] == jogador &&
            jogo->casas[2][coluna] == jogador) {
            snprintf(saida, tamanho, "C%d", coluna + 1);
            return true;
        }
    }

    if (jogo->casas[0][0] == jogador &&
        jogo->casas[1][1] == jogador &&
        jogo->casas[2][2] == jogador) {
        snprintf(saida, tamanho, "D1");
        return true;
    }

    if (jogo->casas[0][2] == jogador &&
        jogo->casas[1][1] == jogador &&
        jogo->casas[2][0] == jogador) {
        snprintf(saida, tamanho, "D2");
        return true;
    }

    return false;
}

void jogo_auditoria_serializar_tabuleiro(const jogo_estado_t *jogo, char *saida, size_t tamanho)
{
    if (saida == NULL || tamanho == 0) {
        return;
    }

    if (jogo == NULL || tamanho < JOGO_AUDITORIA_TABULEIRO_TAMANHO) {
        snprintf(saida, tamanho, "-");
        return;
    }

    for (int i = 0; i < JOGO_CELULAS; i++) {
        char casa = jogo->casas[i / JOGO_TAMANHO][i % JOGO_TAMANHO];
        saida[i] = (casa == ' ') ? '.' : casa;
    }
    saida[JOGO_CELULAS] = '\0';
}

jogo_auditoria_resultado_t jogo_auditoria_analisar(const jogo_estado_t *jogo)
{
    jogo_auditoria_resultado_t resultado = {
        .vitoria_o = false,
        .vitoria_x = false,
        .empate = false,
        .casas_ocupadas = 0,
        .linha_o = "-",
        .linha_x = "-",
    };

    if (jogo == NULL) {
        return resultado;
    }

    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
            if (jogo->casas[linha][coluna] != ' ') {
                resultado.casas_ocupadas++;
            }
        }
    }

    resultado.vitoria_o = encontrar_linha_vencedora(jogo,
                                                    'O',
                                                    resultado.linha_o,
                                                    sizeof(resultado.linha_o));
    resultado.vitoria_x = encontrar_linha_vencedora(jogo,
                                                    'X',
                                                    resultado.linha_x,
                                                    sizeof(resultado.linha_x));
    resultado.empate = jogo_verificar_empate(jogo);

    return resultado;
}

int jogo_auditoria_posicao_da_jogada(jogo_jogada_t jogada)
{
    if (jogada.linha < 0 || jogada.linha >= JOGO_TAMANHO ||
        jogada.coluna < 0 || jogada.coluna >= JOGO_TAMANHO) {
        return 0;
    }

    return jogada.linha * JOGO_TAMANHO + jogada.coluna + 1;
}
