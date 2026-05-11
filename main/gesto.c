#include "gesto.h"

#include <string.h>

void gesto_detector_iniciar(gesto_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    memset(detector, 0, sizeof(*detector));
    serie_temporal_iniciar(&detector->janela);
    detector->debounce_ms = GESTO_DEBOUNCE_MS;
}

void gesto_detector_definir_classificador(gesto_detector_t *detector, gesto_classificador_t classificador, void *contexto)
{
    if (detector == NULL) {
        return;
    }

    detector->classificador = classificador;
    detector->contexto_classificador = contexto;
}

bool gesto_heuristica_detectar(const serie_temporal_t *janela)
{
    serie_temporal_features_t features;

    if (janela == NULL || !serie_temporal_cheia(janela)) {
        return false;
    }

    if (serie_temporal_calcular_features(janela, &features) != ESP_OK) {
        return false;
    }

    return features.maior_amplitude >= GESTO_LIMIAR_AMPLITUDE &&
           features.media_abs_delta >= GESTO_LIMIAR_DELTA_MEDIO;
}

gesto_evento_t gesto_detector_processar_amostra(gesto_detector_t *detector,
                                                uint32_t timestamp_ms,
                                                int16_t ax,
                                                int16_t ay,
                                                int16_t az)
{
    bool detectado = false;

    if (detector == NULL) {
        return GESTO_EVENTO_NENHUM;
    }

    if (serie_temporal_adicionar(&detector->janela, ax, ay, az) != ESP_OK || !serie_temporal_cheia(&detector->janela)) {
        return GESTO_EVENTO_NENHUM;
    }

    if (detector->classificador != NULL) {
        detectado = detector->classificador(&detector->janela, detector->contexto_classificador);
    } else {
        detectado = gesto_heuristica_detectar(&detector->janela);
    }

    if (!detectado) {
        return GESTO_EVENTO_NENHUM;
    }

    if (detector->tem_evento_anterior &&
        (uint32_t)(timestamp_ms - detector->ultimo_evento_ms) < detector->debounce_ms) {
        return GESTO_EVENTO_NENHUM;
    }

    detector->ultimo_evento_ms = timestamp_ms;
    detector->tem_evento_anterior = true;
    return GESTO_EVENTO_CONFIRMAR;
}
