#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool    pronto;
    bool    runtime_tflite;
    int32_t ultimo_score;   /* diferenca top1 - top2 (confianca) */
    int     ultimo_digito;  /* 0=silencio/incerto, 1-9=digito reconhecido */
} audio_classificador_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_classificador_iniciar(audio_classificador_t *modelo);

/*
 * Captura 1 segundo de audio e retorna o digito reconhecido (1-9).
 * Retorna 0 se silencio, incerto ou microfone indisponivel.
 */
int audio_classificador_capturar_e_classificar(audio_classificador_t *modelo);

#ifdef __cplusplus
}
#endif
