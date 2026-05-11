#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "ia_jogo_da_velha.h"
#include "tictactoe_model_data.h"

typedef struct {
    bool pronto;
    bool forcar_falha;
    int arena_bytes;
} ia_tflite_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ia_tflite_iniciar(ia_tflite_t *ia);
void ia_tflite_forcar_falha(ia_tflite_t *ia, bool forcar);
void ia_tflite_tabuleiro_para_entrada(const jogo_estado_t *jogo, int8_t entrada[TICTACTOE_MODEL_INPUT_SIZE]);
esp_err_t ia_tflite_inferir_scores(ia_tflite_t *ia, const jogo_estado_t *jogo, int8_t scores[TICTACTOE_MODEL_OUTPUT_SIZE]);
int ia_tflite_escolher_indice_com_mascara(const int8_t scores[TICTACTOE_MODEL_OUTPUT_SIZE], const jogo_estado_t *jogo);
ia_resultado_t ia_tflite_escolher_jogada(ia_tflite_t *ia, jogo_estado_t *jogo);

#ifdef __cplusplus
}
#endif
