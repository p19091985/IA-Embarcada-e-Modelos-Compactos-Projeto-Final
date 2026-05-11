#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "jogo_da_velha.h"

#define AUTO_SCAN_INTERVALO_MS 700

typedef struct {
    int posicao_atual;
    uint32_t ultimo_avanco_ms;
    uint32_t intervalo_ms;
    bool ativo;
} auto_scan_t;

void auto_scan_iniciar(auto_scan_t *scan, const jogo_estado_t *jogo, uint32_t agora_ms);
bool auto_scan_atualizar(auto_scan_t *scan, const jogo_estado_t *jogo, uint32_t agora_ms);
int auto_scan_posicao_atual(const auto_scan_t *scan);
int auto_scan_proxima_posicao_livre(const jogo_estado_t *jogo, int posicao_atual);
bool auto_scan_confirmar(auto_scan_t *scan, jogo_estado_t *jogo, char jogador);
