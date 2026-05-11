#include "gesto_tflite.h"

#include <string.h>

#include "gesto_model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
alignas(16) uint8_t tensor_arena[GESTO_MODEL_TENSOR_ARENA_BYTES];
const tflite::Model *modelo_tflite = nullptr;
tflite::MicroInterpreter *interpretador = nullptr;
TfLiteTensor *entrada = nullptr;
TfLiteTensor *saida = nullptr;
bool runtime_pronto = false;

int tensor_num_elementos(const TfLiteTensor *tensor)
{
    if (tensor == nullptr || tensor->dims == nullptr) {
        return 0;
    }

    int elementos = 1;
    for (int i = 0; i < tensor->dims->size; i++) {
        elementos *= tensor->dims->data[i];
    }

    return elementos;
}

int8_t limitar_int8(int valor)
{
    if (valor > 127) {
        return 127;
    }

    if (valor < -128) {
        return -128;
    }

    return (int8_t)valor;
}

int arredondar(float valor)
{
    return valor >= 0.0f ? (int)(valor + 0.5f) : (int)(valor - 0.5f);
}

int8_t quantizar_para_tensor(float valor, const TfLiteTensor *tensor)
{
    if (tensor == nullptr || tensor->params.scale <= 0.0f) {
        return limitar_int8((int)valor);
    }

    return limitar_int8(arredondar(valor / tensor->params.scale) + tensor->params.zero_point);
}

esp_err_t iniciar_runtime_tflite()
{
    if (runtime_pronto) {
        return ESP_OK;
    }

    if (GESTO_MODEL_WINDOW_SIZE != SERIE_TEMPORAL_TAMANHO ||
        GESTO_MODEL_INPUT_SIZE != SERIE_TEMPORAL_ENTRADA_TINYML ||
        GESTO_MODEL_TFLITE_LEN == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    modelo_tflite = tflite::GetModel(GESTO_MODEL_TFLITE);
    if (modelo_tflite == nullptr || modelo_tflite->version() != TFLITE_SCHEMA_VERSION) {
        return ESP_ERR_INVALID_VERSION;
    }

    static tflite::MicroMutableOpResolver<1> resolvedor;
    static bool resolvedor_pronto = false;
    if (!resolvedor_pronto) {
        if (resolvedor.AddFullyConnected() != kTfLiteOk) {
            return ESP_FAIL;
        }
        resolvedor_pronto = true;
    }

    static tflite::MicroInterpreter interpretador_estatico(
        modelo_tflite, resolvedor, tensor_arena, sizeof(tensor_arena));
    interpretador = &interpretador_estatico;

    if (interpretador->AllocateTensors() != kTfLiteOk) {
        interpretador = nullptr;
        return ESP_ERR_NO_MEM;
    }

    entrada = interpretador->input(0);
    saida = interpretador->output(0);

    if (entrada == nullptr || saida == nullptr ||
        entrada->type != kTfLiteInt8 || saida->type != kTfLiteInt8 ||
        tensor_num_elementos(entrada) != GESTO_MODEL_INPUT_SIZE ||
        tensor_num_elementos(saida) != 2) {
        interpretador = nullptr;
        entrada = nullptr;
        saida = nullptr;
        return ESP_ERR_INVALID_SIZE;
    }

    runtime_pronto = true;
    return ESP_OK;
}
} // namespace

esp_err_t gesto_tflite_iniciar(gesto_tflite_t *modelo)
{
    if (modelo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(modelo, 0, sizeof(*modelo));
    modelo->usar_fallback_heuristico = true;

    esp_err_t erro = iniciar_runtime_tflite();
    if (erro != ESP_OK) {
        return erro;
    }

    modelo->pronto = true;
    return ESP_OK;
}

bool gesto_tflite_classificar(const serie_temporal_t *janela, void *contexto)
{
    gesto_tflite_t *modelo = (gesto_tflite_t *)contexto;
    int8_t dados_preprocessados[GESTO_MODEL_INPUT_SIZE];

    if (janela == NULL || !serie_temporal_cheia(janela)) {
        return false;
    }

    if (modelo == NULL || !modelo->pronto || !runtime_pronto ||
        interpretador == nullptr || entrada == nullptr || saida == nullptr) {
        return gesto_heuristica_detectar(janela);
    }

    if (serie_temporal_preprocessar_int8(janela, dados_preprocessados, GESTO_MODEL_INPUT_SIZE) != ESP_OK) {
        return gesto_heuristica_detectar(janela);
    }

    for (int i = 0; i < GESTO_MODEL_INPUT_SIZE; i++) {
        entrada->data.int8[i] = quantizar_para_tensor((float)dados_preprocessados[i], entrada);
    }

    if (interpretador->Invoke() != kTfLiteOk) {
        return gesto_heuristica_detectar(janela);
    }

    int8_t score_repouso = saida->data.int8[0];
    int8_t score_confirmar = saida->data.int8[1];
    modelo->ultimo_score = (int32_t)score_confirmar - (int32_t)score_repouso;

    if (modelo->ultimo_score > 0) {
        return true;
    }

    if (modelo->usar_fallback_heuristico) {
        bool detectado = gesto_heuristica_detectar(janela);
        if (detectado && modelo->ultimo_score <= 0) {
            modelo->ultimo_score = 1;
        }
        return detectado;
    }

    return false;
}
