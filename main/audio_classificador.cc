#include "audio_classificador.h"
#include "audio_model_data.h"
#include "microfone_i2s.h"

#include <cmath>
#include <cstring>

#include "esp_log.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char *TAG = "audio_classificador";

namespace {

alignas(16) static uint8_t s_tensor_arena[AUDIO_MODEL_TENSOR_ARENA_BYTES];

static const tflite::Model   *s_modelo   = nullptr;
static tflite::MicroInterpreter *s_interp = nullptr;
static TfLiteTensor *s_entrada = nullptr;
static TfLiteTensor *s_saida   = nullptr;
static bool          s_pronto  = false;

/* Buffer de audio int16 — 16000 amostras × 2 bytes = 32 KB */
static int16_t s_buffer_audio[AUDIO_SAMPLES];

static int tensor_n_elementos(const TfLiteTensor *t)
{
    if (!t || !t->dims) return 0;
    /* Ultimo eixo: shape pode ser [1, N] ou [N] */
    return t->dims->data[t->dims->size - 1];
}

static esp_err_t iniciar_runtime_tflite()
{
    if (s_pronto) return ESP_OK;

    if (AUDIO_MODEL_TFLITE_LEN < 16) {
        return ESP_ERR_INVALID_SIZE; /* modelo placeholder — execute pipeline_audio.py */
    }

    s_modelo = tflite::GetModel(AUDIO_MODEL_TFLITE);
    if (!s_modelo || s_modelo->version() != TFLITE_SCHEMA_VERSION) {
        return ESP_ERR_INVALID_VERSION;
    }

    static tflite::MicroMutableOpResolver<1> resolver;
    static bool resolver_pronto = false;
    if (!resolver_pronto) {
        if (resolver.AddFullyConnected() != kTfLiteOk) {
            return ESP_FAIL;
        }
        resolver_pronto = true;
    }

    static tflite::MicroInterpreter interp(
        s_modelo, resolver, s_tensor_arena, sizeof(s_tensor_arena));
    s_interp = &interp;

    if (s_interp->AllocateTensors() != kTfLiteOk) {
        s_interp = nullptr;
        return ESP_ERR_NO_MEM;
    }

    s_entrada = s_interp->input(0);
    s_saida   = s_interp->output(0);

    /* Valida tipos e dimensoes (aceita shape [N] ou [1, N]) */
    if (!s_entrada || !s_saida ||
        s_entrada->type != kTfLiteInt8 ||
        s_saida->type   != kTfLiteInt8 ||
        tensor_n_elementos(s_entrada) != AUDIO_MODEL_N_FEATURES ||
        tensor_n_elementos(s_saida)   != AUDIO_MODEL_N_CLASSES) {
        ESP_LOGE(TAG, "Shape do tensor incompativel: entrada=%d saida=%d",
                 tensor_n_elementos(s_entrada), tensor_n_elementos(s_saida));
        s_interp  = nullptr;
        s_entrada = nullptr;
        s_saida   = nullptr;
        return ESP_ERR_INVALID_SIZE;
    }

    s_pronto = true;
    ESP_LOGI(TAG, "TFLite audio pronto: arena=%d B", AUDIO_MODEL_TENSOR_ARENA_BYTES);
    return ESP_OK;
}

} /* namespace */

extern "C" esp_err_t audio_classificador_iniciar(audio_classificador_t *modelo)
{
    if (!modelo) return ESP_ERR_INVALID_ARG;
    memset(modelo, 0, sizeof(*modelo));
    modelo->pronto = true;

    esp_err_t err = iniciar_runtime_tflite();
    modelo->runtime_tflite = (err == ESP_OK);

    if (!modelo->runtime_tflite) {
        ESP_LOGW(TAG, "Modelo de audio indisponivel (%s) — treine com ml/pipeline_audio.py",
                 esp_err_to_name(err));
    }
    return ESP_OK;
}

extern "C" int audio_classificador_capturar_e_classificar(audio_classificador_t *modelo)
{
    if (!modelo || !modelo->pronto || !modelo->runtime_tflite) {
        return 0;
    }
    if (!s_pronto || !s_interp || !s_entrada || !s_saida) {
        return 0;
    }

    /* 1. Captura 1 segundo de audio via INMP441 */
    if (microfone_capturar(s_buffer_audio, AUDIO_SAMPLES) != ESP_OK) {
        return 0;
    }

    /* 2. VAD simples: ignora se nao houver audio relevante */
    if (!microfone_tem_atividade_voz(s_buffer_audio, AUDIO_SAMPLES)) {
        return 0;
    }

    /* 3. Extrai 40 features Goertzel (log-energia por banda por frame) */
    float features[AUDIO_N_FEATURES];
    audio_extrair_features(s_buffer_audio, features);

    /* 4. Normaliza com media/desvio calculados no treinamento */
    for (int i = 0; i < AUDIO_N_FEATURES; i++) {
        features[i] = (features[i] - AUDIO_FEATURE_MEAN[i]) / AUDIO_FEATURE_STD[i];
    }

    /* 5. Quantiza para INT8 usando parametros do tensor de entrada */
    float escala     = s_entrada->params.scale;
    int   zero_point = s_entrada->params.zero_point;
    for (int i = 0; i < AUDIO_N_FEATURES; i++) {
        int val = (int)roundf(features[i] / escala) + zero_point;
        if (val >  127) val =  127;
        if (val < -128) val = -128;
        s_entrada->data.int8[i] = (int8_t)val;
    }

    /* 6. Inferencia */
    if (s_interp->Invoke() != kTfLiteOk) {
        return 0;
    }

    /* 7. Argmax: classe com maior logit INT8 */
    int    melhor       = 0;
    int8_t melhor_score = s_saida->data.int8[0];
    for (int c = 1; c < AUDIO_MODEL_N_CLASSES; c++) {
        if (s_saida->data.int8[c] > melhor_score) {
            melhor_score = s_saida->data.int8[c];
            melhor = c;
        }
    }

    /* 8. Confianca = diferenca top-1 vs top-2 */
    int8_t segundo = -128;
    for (int c = 0; c < AUDIO_MODEL_N_CLASSES; c++) {
        if (c != melhor && s_saida->data.int8[c] > segundo) {
            segundo = s_saida->data.int8[c];
        }
    }
    int32_t score = (int32_t)melhor_score - (int32_t)segundo;
    modelo->ultimo_score  = score;
    modelo->ultimo_digito = melhor;

    ESP_LOGI(TAG, "Audio: classe=%d score=%ld confianca=%s",
             melhor, (long)score,
             score >= AUDIO_MODEL_CONFIDENCE_THRESHOLD ? "ALTA" : "BAIXA");

    if (melhor >= 1 && melhor <= 9 && score >= AUDIO_MODEL_CONFIDENCE_THRESHOLD) {
        return melhor;
    }
    return 0;
}
