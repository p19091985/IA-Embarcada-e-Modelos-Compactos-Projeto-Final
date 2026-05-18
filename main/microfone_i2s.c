#include "microfone_i2s.h"

#include <math.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "microfone_i2s";

/* INMP441 envia 24 bits em slot de 32 bits.
 * Lemos em chunks para evitar buffer raw de 64 KB. */
#define AUDIO_CHUNK_FRAMES 512
static int32_t s_raw_chunk[AUDIO_CHUNK_FRAMES]; /* 2 KB */

static i2s_chan_handle_t s_rx_handle = NULL;

/* Frequencias centrais das 8 bandas (Hz) — escala Mel aproximada.
 * Definidas aqui como file-static (nao exportadas no header). */
static const float s_freq_bands[AUDIO_N_BANDS] = {
    250.0f, 400.0f, 600.0f, 900.0f,
    1300.0f, 1900.0f, 2700.0f, 3800.0f,
};

esp_err_t microfone_iniciar(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_frame_num = AUDIO_CHUNK_FRAMES;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel falhou: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)AUDIO_GPIO_SCK,
            .ws   = (gpio_num_t)AUDIO_GPIO_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)AUDIO_GPIO_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(s_rx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode falhou: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return err;
    }

    err = i2s_channel_enable(s_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable falhou: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return err;
    }

    ESP_LOGI(TAG, "INMP441 pronto: WS=%d SCK=%d SD=%d @ %d Hz",
             AUDIO_GPIO_WS, AUDIO_GPIO_SCK, AUDIO_GPIO_SD, AUDIO_SAMPLE_RATE);
    return ESP_OK;
}

void microfone_desiniciar(void)
{
    if (s_rx_handle) {
        i2s_channel_disable(s_rx_handle);
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
    }
}

esp_err_t microfone_capturar(int16_t *amostras, int n_amostras)
{
    if (!s_rx_handle || !amostras || n_amostras <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Le em chunks de AUDIO_CHUNK_FRAMES para manter o buffer raw pequeno (2 KB). */
    int offset = 0;
    while (offset < n_amostras) {
        int frames = n_amostras - offset;
        if (frames > AUDIO_CHUNK_FRAMES) {
            frames = AUDIO_CHUNK_FRAMES;
        }

        size_t bytes_lidos = 0;
        esp_err_t err = i2s_channel_read(s_rx_handle,
                                         s_raw_chunk,
                                         (size_t)frames * sizeof(int32_t),
                                         &bytes_lidos,
                                         pdMS_TO_TICKS(3000));
        if (err != ESP_OK) {
            return err;
        }

        /* INMP441: dado de 24 bits MSB no word de 32 bits — desloca 16 bits */
        int lidos = (int)(bytes_lidos / sizeof(int32_t));
        for (int i = 0; i < lidos && (offset + i) < n_amostras; i++) {
            amostras[offset + i] = (int16_t)(s_raw_chunk[i] >> 16);
        }
        /* Preenche com silencio se leitura incompleta */
        for (int i = lidos; i < frames && (offset + i) < n_amostras; i++) {
            amostras[offset + i] = 0;
        }
        offset += frames;
    }
    return ESP_OK;
}

/* ---------- Algoritmo de Goertzel ----------
 * Calcula energia espectral em uma frequencia sem FFT completa.
 * Implementacao identica ao pipeline Python (pipeline_audio.py). */
static float goertzel_energia(const int16_t *amostras, int n, float freq)
{
    int   k     = (int)((float)n * freq / (float)AUDIO_SAMPLE_RATE + 0.5f);
    float omega = 2.0f * (float)M_PI * (float)k / (float)n;
    float coeff = 2.0f * cosf(omega);
    float s = 0.0f, s_prev = 0.0f;

    for (int i = 0; i < n; i++) {
        float x    = (float)amostras[i] / 32768.0f;
        float s_novo = x + coeff * s - s_prev;
        s_prev = s;
        s      = s_novo;
    }
    return s_prev * s_prev + s * s - coeff * s_prev * s;
}

void audio_extrair_features(const int16_t *amostras, float features[AUDIO_N_FEATURES])
{
    int frame_size = AUDIO_SAMPLES / AUDIO_N_FRAMES;

    for (int f = 0; f < AUDIO_N_FRAMES; f++) {
        const int16_t *frame = amostras + f * frame_size;
        for (int b = 0; b < AUDIO_N_BANDS; b++) {
            float energia = goertzel_energia(frame, frame_size, s_freq_bands[b]);
            features[f * AUDIO_N_BANDS + b] = logf(energia + 1e-8f);
        }
    }
}

bool microfone_tem_atividade_voz(const int16_t *amostras, int n)
{
    int64_t soma = 0;
    for (int i = 0; i < n; i++) {
        int32_t s = amostras[i];
        soma += s * s;
    }
    float rms = sqrtf((float)soma / (float)n);
    return rms > 400.0f;
}
