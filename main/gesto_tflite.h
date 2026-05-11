#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "gesto.h"

typedef struct {
    bool pronto;
    bool usar_fallback_heuristico;
    int32_t ultimo_score;
} gesto_tflite_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gesto_tflite_iniciar(gesto_tflite_t *modelo);
bool gesto_tflite_classificar(const serie_temporal_t *janela, void *contexto);

#ifdef __cplusplus
}
#endif
