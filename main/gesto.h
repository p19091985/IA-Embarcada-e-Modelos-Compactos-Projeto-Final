#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "serie_temporal.h"

#define GESTO_DEBOUNCE_MS 700
#define GESTO_LIMIAR_AMPLITUDE 3500
#define GESTO_LIMIAR_DELTA_MEDIO 700

typedef enum {
    GESTO_EVENTO_NENHUM = 0,
    GESTO_EVENTO_CONFIRMAR = 1,
} gesto_evento_t;

typedef bool (*gesto_classificador_t)(const serie_temporal_t *janela, void *contexto);

typedef struct {
    serie_temporal_t janela;
    uint32_t ultimo_evento_ms;
    uint32_t debounce_ms;
    bool tem_evento_anterior;
    gesto_classificador_t classificador;
    void *contexto_classificador;
} gesto_detector_t;

#ifdef __cplusplus
extern "C" {
#endif

void gesto_detector_iniciar(gesto_detector_t *detector);
void gesto_detector_definir_classificador(gesto_detector_t *detector, gesto_classificador_t classificador, void *contexto);
bool gesto_heuristica_detectar(const serie_temporal_t *janela);
gesto_evento_t gesto_detector_processar_amostra(gesto_detector_t *detector,
                                                uint32_t timestamp_ms,
                                                int16_t ax,
                                                int16_t ay,
                                                int16_t az);

#ifdef __cplusplus
}
#endif
