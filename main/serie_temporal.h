#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define SERIE_TEMPORAL_TAMANHO 16
#define SERIE_TEMPORAL_EIXOS 3
#define SERIE_TEMPORAL_ENTRADA_TINYML (SERIE_TEMPORAL_TAMANHO * SERIE_TEMPORAL_EIXOS)

typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
} serie_temporal_amostra_t;

typedef struct {
    serie_temporal_amostra_t amostras[SERIE_TEMPORAL_TAMANHO];
    size_t inicio;
    size_t quantidade;
} serie_temporal_t;

typedef struct {
    int32_t amplitude_ax;
    int32_t amplitude_ay;
    int32_t amplitude_az;
    int32_t maior_amplitude;
    int32_t energia_delta;
    int32_t media_abs_delta;
} serie_temporal_features_t;

#ifdef __cplusplus
extern "C" {
#endif

void serie_temporal_iniciar(serie_temporal_t *serie);
esp_err_t serie_temporal_adicionar(serie_temporal_t *serie, int16_t ax, int16_t ay, int16_t az);
bool serie_temporal_cheia(const serie_temporal_t *serie);
size_t serie_temporal_quantidade(const serie_temporal_t *serie);
esp_err_t serie_temporal_obter(const serie_temporal_t *serie, size_t indice, serie_temporal_amostra_t *amostra);
esp_err_t serie_temporal_calcular_features(const serie_temporal_t *serie, serie_temporal_features_t *features);
esp_err_t serie_temporal_preprocessar_int8(const serie_temporal_t *serie, int8_t *saida, size_t tamanho);

#ifdef __cplusplus
}
#endif
