#include "jogo_da_velha.h"

#include <stdio.h>

void jogo_iniciar(jogo_estado_t *jogo)
{
    jogo->vitorias_jogador = 0;
    jogo->vitorias_computador = 0;
    jogo->empates = 0;
    jogo_resetar_tabuleiro(jogo);
}

void jogo_resetar_tabuleiro(jogo_estado_t *jogo)
{
    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
            jogo->casas[linha][coluna] = ' ';
        }
    }

    jogo->mostrar_numeros = true;
}

bool jogo_posicao_valida(int posicao)
{
    return posicao >= 1 && posicao <= JOGO_CELULAS;
}

bool jogo_aplicar_posicao(jogo_estado_t *jogo, int posicao, char jogador)
{
    if (!jogo_posicao_valida(posicao)) {
        return false;
    }

    jogo_jogada_t jogada = {
        .linha = (posicao - 1) / JOGO_TAMANHO,
        .coluna = (posicao - 1) % JOGO_TAMANHO,
    };

    return jogo_aplicar_jogada(jogo, jogada, jogador);
}

bool jogo_aplicar_jogada(jogo_estado_t *jogo, jogo_jogada_t jogada, char jogador)
{
    if (jogada.linha < 0 || jogada.linha >= JOGO_TAMANHO || jogada.coluna < 0 || jogada.coluna >= JOGO_TAMANHO) {
        return false;
    }

    if (jogo->casas[jogada.linha][jogada.coluna] != ' ') {
        return false;
    }

    jogo->casas[jogada.linha][jogada.coluna] = jogador;
    jogo->mostrar_numeros = false;
    return true;
}

bool jogo_verificar_vitoria_tabuleiro(const char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO], char jogador)
{
    for (int i = 0; i < JOGO_TAMANHO; i++) {
        if (tabuleiro[i][0] == jogador && tabuleiro[i][1] == jogador && tabuleiro[i][2] == jogador) {
            return true;
        }

        if (tabuleiro[0][i] == jogador && tabuleiro[1][i] == jogador && tabuleiro[2][i] == jogador) {
            return true;
        }
    }

    if (tabuleiro[0][0] == jogador && tabuleiro[1][1] == jogador && tabuleiro[2][2] == jogador) {
        return true;
    }

    return tabuleiro[0][2] == jogador && tabuleiro[1][1] == jogador && tabuleiro[2][0] == jogador;
}

bool jogo_verificar_vitoria(const jogo_estado_t *jogo, char jogador)
{
    return jogo_verificar_vitoria_tabuleiro(jogo->casas, jogador);
}

bool jogo_verificar_empate_tabuleiro(const char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO])
{
    if (jogo_verificar_vitoria_tabuleiro(tabuleiro, 'X') || jogo_verificar_vitoria_tabuleiro(tabuleiro, 'O')) {
        return false;
    }

    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
            if (tabuleiro[linha][coluna] == ' ') {
                return false;
            }
        }
    }

    return true;
}

bool jogo_verificar_empate(const jogo_estado_t *jogo)
{
    return jogo_verificar_empate_tabuleiro(jogo->casas);
}

void jogo_zerar_placar(jogo_estado_t *jogo)
{
    jogo->vitorias_jogador = 0;
    jogo->vitorias_computador = 0;
    jogo->empates = 0;
}

void jogo_formatar_linha_tabuleiro(const jogo_estado_t *jogo, int linha, char *saida, int tamanho)
{
    char a = jogo->casas[linha][0];
    char b = jogo->casas[linha][1];
    char c = jogo->casas[linha][2];

    if (jogo->mostrar_numeros) {
        a = (char)('1' + linha * JOGO_TAMANHO);
        b = (char)('2' + linha * JOGO_TAMANHO);
        c = (char)('3' + linha * JOGO_TAMANHO);
    }

    snprintf(saida, tamanho, "%c|%c|%c", a, b, c);
}

void jogo_formatar_linha_tabuleiro_expandida(const jogo_estado_t *jogo, int linha, char *saida, int tamanho)
{
    char a = jogo->casas[linha][0];
    char b = jogo->casas[linha][1];
    char c = jogo->casas[linha][2];

    if (jogo->mostrar_numeros) {
        a = (char)('1' + linha * JOGO_TAMANHO);
        b = (char)('2' + linha * JOGO_TAMANHO);
        c = (char)('3' + linha * JOGO_TAMANHO);
    }

    snprintf(saida, tamanho, " %c | %c | %c ", a, b, c);
}
