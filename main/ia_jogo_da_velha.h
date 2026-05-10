#pragma once

#include "jogo_da_velha.h"

typedef enum {
    IA_MINIMAX = 0,
    IA_MCTS,
    IA_MINIMAX_PURO,
    IA_HEURISTICA,
    IA_FORCA_BRUTA,
    IA_GENETICO,
} ia_algoritmo_t;

typedef struct {
    jogo_jogada_t jogada;
    ia_algoritmo_t algoritmo;
    const char *nome_curto;
} ia_resultado_t;

ia_resultado_t ia_escolher_jogada(jogo_estado_t *jogo);
const char *ia_nome(ia_algoritmo_t algoritmo);
