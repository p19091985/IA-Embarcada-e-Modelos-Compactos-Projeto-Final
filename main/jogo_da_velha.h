#pragma once

#include <stdbool.h>
#include <stdint.h>

#define JOGO_TAMANHO 3
#define JOGO_CELULAS (JOGO_TAMANHO * JOGO_TAMANHO)

typedef struct {
    int linha;
    int coluna;
} jogo_jogada_t;

typedef struct {
    char casas[JOGO_TAMANHO][JOGO_TAMANHO];
    uint16_t vitorias_jogador;
    uint16_t vitorias_computador;
    uint16_t empates;
    bool mostrar_numeros;
} jogo_estado_t;

#ifdef __cplusplus
extern "C" {
#endif

void jogo_iniciar(jogo_estado_t *jogo);
void jogo_resetar_tabuleiro(jogo_estado_t *jogo);
bool jogo_posicao_valida(int posicao);
bool jogo_aplicar_posicao(jogo_estado_t *jogo, int posicao, char jogador);
bool jogo_aplicar_jogada(jogo_estado_t *jogo, jogo_jogada_t jogada, char jogador);
bool jogo_verificar_vitoria_tabuleiro(const char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO], char jogador);
bool jogo_verificar_vitoria(const jogo_estado_t *jogo, char jogador);
bool jogo_verificar_empate_tabuleiro(const char tabuleiro[JOGO_TAMANHO][JOGO_TAMANHO]);
bool jogo_verificar_empate(const jogo_estado_t *jogo);
void jogo_zerar_placar(jogo_estado_t *jogo);
void jogo_formatar_linha_tabuleiro(const jogo_estado_t *jogo, int linha, char *saida, int tamanho);
void jogo_formatar_linha_tabuleiro_expandida(const jogo_estado_t *jogo, int linha, char *saida, int tamanho);

#ifdef __cplusplus
}
#endif
