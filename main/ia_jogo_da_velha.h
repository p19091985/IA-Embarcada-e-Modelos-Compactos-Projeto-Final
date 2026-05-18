#pragma once

#include "jogo_da_velha.h"

typedef enum {
    IA_TFLITE = 0,
} ia_algoritmo_t;

typedef struct {
    jogo_jogada_t jogada;
    ia_algoritmo_t algoritmo;
    const char *nome_curto;
} ia_resultado_t;

#ifdef __cplusplus
extern "C" {
#endif

const char *ia_nome(ia_algoritmo_t algoritmo);

#ifdef __cplusplus
}
#endif
