#include "serie_temporal.h"

#include <string.h>

static int32_t abs32(int32_t valor)
{
    return valor < 0 ? -valor : valor;
}

static int8_t normalizar_int8(int32_t valor)
{
    int32_t normalizado = valor / 256;

    if (normalizado > 127) {
        return 127;
    }

    if (normalizado < -128) {
        return -128;
    }

    return (int8_t)normalizado;
}

void serie_temporal_iniciar(serie_temporal_t *serie)
{
    if (serie == NULL) {
        return;
    }

    memset(serie, 0, sizeof(*serie));
}

esp_err_t serie_temporal_adicionar(serie_temporal_t *serie, int16_t ax, int16_t ay, int16_t az)
{
    if (serie == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    serie_temporal_amostra_t amostra = {
        .ax = ax,
        .ay = ay,
        .az = az,
    };

    if (serie->quantidade < SERIE_TEMPORAL_TAMANHO) {
        size_t indice = (serie->inicio + serie->quantidade) % SERIE_TEMPORAL_TAMANHO;
        serie->amostras[indice] = amostra;
        serie->quantidade++;
        return ESP_OK;
    }

    serie->amostras[serie->inicio] = amostra;
    serie->inicio = (serie->inicio + 1) % SERIE_TEMPORAL_TAMANHO;
    return ESP_OK;
}

bool serie_temporal_cheia(const serie_temporal_t *serie)
{
    return serie != NULL && serie->quantidade == SERIE_TEMPORAL_TAMANHO;
}

size_t serie_temporal_quantidade(const serie_temporal_t *serie)
{
    return serie == NULL ? 0 : serie->quantidade;
}

esp_err_t serie_temporal_obter(const serie_temporal_t *serie, size_t indice, serie_temporal_amostra_t *amostra)
{
    if (serie == NULL || amostra == NULL || indice >= serie->quantidade) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t indice_fisico = (serie->inicio + indice) % SERIE_TEMPORAL_TAMANHO;
    *amostra = serie->amostras[indice_fisico];
    return ESP_OK;
}

esp_err_t serie_temporal_calcular_features(const serie_temporal_t *serie, serie_temporal_features_t *features)
{
    if (serie == NULL || features == NULL || serie->quantidade == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    serie_temporal_amostra_t primeira;
    esp_err_t erro = serie_temporal_obter(serie, 0, &primeira);
    if (erro != ESP_OK) {
        return erro;
    }

    int16_t min_ax = primeira.ax;
    int16_t max_ax = primeira.ax;
    int16_t min_ay = primeira.ay;
    int16_t max_ay = primeira.ay;
    int16_t min_az = primeira.az;
    int16_t max_az = primeira.az;
    int32_t energia_delta = 0;
    serie_temporal_amostra_t anterior = primeira;

    for (size_t i = 1; i < serie->quantidade; i++) {
        serie_temporal_amostra_t atual;
        erro = serie_temporal_obter(serie, i, &atual);
        if (erro != ESP_OK) {
            return erro;
        }

        if (atual.ax < min_ax) {
            min_ax = atual.ax;
        }
        if (atual.ax > max_ax) {
            max_ax = atual.ax;
        }
        if (atual.ay < min_ay) {
            min_ay = atual.ay;
        }
        if (atual.ay > max_ay) {
            max_ay = atual.ay;
        }
        if (atual.az < min_az) {
            min_az = atual.az;
        }
        if (atual.az > max_az) {
            max_az = atual.az;
        }

        energia_delta += abs32((int32_t)atual.ax - anterior.ax);
        energia_delta += abs32((int32_t)atual.ay - anterior.ay);
        energia_delta += abs32((int32_t)atual.az - anterior.az);
        anterior = atual;
    }

    memset(features, 0, sizeof(*features));
    features->amplitude_ax = (int32_t)max_ax - min_ax;
    features->amplitude_ay = (int32_t)max_ay - min_ay;
    features->amplitude_az = (int32_t)max_az - min_az;
    features->maior_amplitude = features->amplitude_ax;

    if (features->amplitude_ay > features->maior_amplitude) {
        features->maior_amplitude = features->amplitude_ay;
    }
    if (features->amplitude_az > features->maior_amplitude) {
        features->maior_amplitude = features->amplitude_az;
    }

    features->energia_delta = energia_delta;
    if (serie->quantidade > 1) {
        features->media_abs_delta = energia_delta / (int32_t)(serie->quantidade - 1);
    }

    return ESP_OK;
}

esp_err_t serie_temporal_preprocessar_int8(const serie_temporal_t *serie, int8_t *saida, size_t tamanho)
{
    if (serie == NULL || saida == NULL || tamanho < SERIE_TEMPORAL_ENTRADA_TINYML || !serie_temporal_cheia(serie)) {
        return ESP_ERR_INVALID_ARG;
    }

    serie_temporal_amostra_t base;
    esp_err_t erro = serie_temporal_obter(serie, 0, &base);
    if (erro != ESP_OK) {
        return erro;
    }

    for (size_t i = 0; i < SERIE_TEMPORAL_TAMANHO; i++) {
        serie_temporal_amostra_t amostra;
        erro = serie_temporal_obter(serie, i, &amostra);
        if (erro != ESP_OK) {
            return erro;
        }

        saida[i * SERIE_TEMPORAL_EIXOS + 0] = normalizar_int8((int32_t)amostra.ax - base.ax);
        saida[i * SERIE_TEMPORAL_EIXOS + 1] = normalizar_int8((int32_t)amostra.ay - base.ay);
        saida[i * SERIE_TEMPORAL_EIXOS + 2] = normalizar_int8((int32_t)amostra.az - base.az);
    }

    return ESP_OK;
}
