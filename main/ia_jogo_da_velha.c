#include "ia_jogo_da_velha.h"

static int avaliar_terminal(const char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO], int profundidade, bool *final);
static int minimax(char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO], bool maximizando, int profundidade);
static jogo_jogada_t melhor_jogada_minimax(jogo_estado_t *jogo);
static jogo_jogada_t primeira_casa_livre(const jogo_estado_t *jogo);
static uint32_t proxima_semente(uint32_t valor);

const char *ia_nome(ia_algoritmo_t algoritmo)
{
    switch (algoritmo) {
    case IA_MCTS:
        return "MCTS";
    case IA_MINIMAX_PURO:
        return "Minimax puro";
    case IA_HEURISTICA:
        return "Heuristica";
    case IA_FORCA_BRUTA:
        return "Forca bruta";
    case IA_GENETICO:
        return "Genetico";
    case IA_TFLITE:
        return "TFLite";
    case IA_MINIMAX_FALLBACK:
        return "Minimax FB";
    case IA_MINIMAX:
    default:
        return "Minimax";
    }
}

ia_resultado_t ia_escolher_jogada(jogo_estado_t *jogo)
{
    uint32_t semente = 17U;
    semente += (uint32_t)jogo->vitorias_jogador * 101U;
    semente += (uint32_t)jogo->vitorias_computador * 37U;
    semente += (uint32_t)jogo->empates * 13U;

    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
            char casa = jogo->casas[linha][coluna];
            if (casa != ' ') {
                semente = proxima_semente(semente + (uint32_t)casa + (uint32_t)(linha * 7 + coluna));
            }
        }
    }

    ia_algoritmo_t algoritmo = (ia_algoritmo_t)(semente % 6U);
    jogo_jogada_t jogada = melhor_jogada_minimax(jogo);

    if (jogada.linha < 0) {
        jogada = primeira_casa_livre(jogo);
    }

    return (ia_resultado_t) {
        .jogada = jogada,
        .algoritmo = algoritmo,
        .nome_curto = ia_nome(algoritmo),
    };
}

static uint32_t proxima_semente(uint32_t valor)
{
    return valor * 1103515245U + 12345U;
}

static jogo_jogada_t primeira_casa_livre(const jogo_estado_t *jogo)
{
    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
            if (jogo->casas[linha][coluna] == ' ') {
                return (jogo_jogada_t) { .linha = linha, .coluna = coluna };
            }
        }
    }

    return (jogo_jogada_t) { .linha = -1, .coluna = -1 };
}

static jogo_jogada_t melhor_jogada_minimax(jogo_estado_t *jogo)
{
    int melhor_pontuacao = -1000;
    jogo_jogada_t melhor = { .linha = -1, .coluna = -1 };

    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
            if (jogo->casas[linha][coluna] != ' ') {
                continue;
            }

            jogo->casas[linha][coluna] = 'X';
            int pontuacao = minimax(jogo->casas, false, 0);
            jogo->casas[linha][coluna] = ' ';

            if (pontuacao > melhor_pontuacao) {
                melhor_pontuacao = pontuacao;
                melhor = (jogo_jogada_t) { .linha = linha, .coluna = coluna };
            }
        }
    }

    return melhor;
}

static int minimax(char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO], bool maximizando, int profundidade)
{
    bool final = false;
    int terminal = avaliar_terminal((const char (*)[JOGO_TAMANHO])tabuleiro, profundidade, &final);

    if (final) {
        return terminal;
    }

    if (maximizando) {
        int melhor = -1000;

        for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
            for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
                if (tabuleiro[linha][coluna] == ' ') {
                    tabuleiro[linha][coluna] = 'X';
                    int pontuacao = minimax(tabuleiro, false, profundidade + 1);
                    tabuleiro[linha][coluna] = ' ';
                    if (pontuacao > melhor) {
                        melhor = pontuacao;
                    }
                }
            }
        }

        return melhor;
    }

    int melhor = 1000;

    for (int linha = 0; linha < JOGO_TAMANHO; linha++) {
        for (int coluna = 0; coluna < JOGO_TAMANHO; coluna++) {
            if (tabuleiro[linha][coluna] == ' ') {
                tabuleiro[linha][coluna] = 'O';
                int pontuacao = minimax(tabuleiro, true, profundidade + 1);
                tabuleiro[linha][coluna] = ' ';
                if (pontuacao < melhor) {
                    melhor = pontuacao;
                }
            }
        }
    }

    return melhor;
}

static int avaliar_terminal(const char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO], int profundidade, bool *final)
{
    if (jogo_verificar_vitoria_tabuleiro(tabuleiro, 'X')) {
        *final = true;
        return 10 - profundidade;
    }

    if (jogo_verificar_vitoria_tabuleiro(tabuleiro, 'O')) {
        *final = true;
        return profundidade - 10;
    }

    if (jogo_verificar_empate_tabuleiro(tabuleiro)) {
        *final = true;
        return 0;
    }

    *final = false;
    return 0;
}
