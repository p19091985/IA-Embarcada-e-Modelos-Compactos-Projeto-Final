#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "jogo_da_velha.h"

#define JOGO_AUDITORIA_TABULEIRO_TAMANHO 10
#define JOGO_AUDITORIA_LINHA_TAMANHO 4

typedef struct {
    bool vitoria_o;
    bool vitoria_x;
    bool empate;
    uint8_t casas_ocupadas;
    char linha_o[JOGO_AUDITORIA_LINHA_TAMANHO];
    char linha_x[JOGO_AUDITORIA_LINHA_TAMANHO];
} jogo_auditoria_resultado_t;

#ifdef __cplusplus
extern "C" {
#endif

void jogo_auditoria_serializar_tabuleiro(const jogo_estado_t *jogo, char *saida, size_t tamanho);
jogo_auditoria_resultado_t jogo_auditoria_analisar(const jogo_estado_t *jogo);
int jogo_auditoria_posicao_da_jogada(jogo_jogada_t jogada);

#ifdef __cplusplus
}
#endif
