#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* GPIOs do INMP441 no ESP32-S3 */
#define AUDIO_GPIO_WS   21   /* Word Select (LRCK) */
#define AUDIO_GPIO_SCK  38   /* Bit Clock (BCLK)   */
#define AUDIO_GPIO_SD   39   /* Data (SD)           */

/* Parametros de captura */
#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_SAMPLES       16000  /* 1 segundo de audio */

/* Parametros das features Goertzel */
#define AUDIO_N_FRAMES      5
#define AUDIO_N_BANDS       8
#define AUDIO_N_FEATURES    (AUDIO_N_FRAMES * AUDIO_N_BANDS)  /* 40 */

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t microfone_iniciar(void);
void      microfone_desiniciar(void);

/* Captura AUDIO_SAMPLES amostras (1 s). Bloqueia ate 3 s. */
esp_err_t microfone_capturar(int16_t *amostras, int n_amostras);

/* Extrai AUDIO_N_FEATURES (40) features Goertzel em log-energia. */
void audio_extrair_features(const int16_t *amostras, float features[AUDIO_N_FEATURES]);

/* Retorna true se a energia RMS indicar presenca de audio (nao silencio). */
bool microfone_tem_atividade_voz(const int16_t *amostras, int n);

#ifdef __cplusplus
}
#endif
