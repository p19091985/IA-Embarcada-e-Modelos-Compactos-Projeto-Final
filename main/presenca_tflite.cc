#include "presenca_tflite.h"

#include <string.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static int tensor_num_elementos(const TfLiteTensor *tensor)
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

static int arredondar(float valor)
{
    return valor >= 0.0f ? (int)(valor + 0.5f) : (int)(valor - 0.5f);
}

static int8_t limitar_int8(int valor)
{
    if (valor > 127) {
        return 127;
    }
    if (valor < -128) {
        return -128;
    }
    return (int8_t)valor;
}

static int8_t quantizar_para_tensor(float valor, const TfLiteTensor *tensor)
{
    if (tensor == nullptr || tensor->params.scale <= 0.0f) {
        return limitar_int8(arredondar(valor));
    }

    return limitar_int8(arredondar(valor / tensor->params.scale) + tensor->params.zero_point);
}

namespace {
alignas(16) uint8_t tensor_arena[PRESENCA_MODEL_TENSOR_ARENA_BYTES];
const tflite::Model *modelo_tflite = nullptr;
tflite::MicroInterpreter *interpretador = nullptr;
TfLiteTensor *entrada_tflite = nullptr;
TfLiteTensor *saida_tflite = nullptr;
bool runtime_pronto = false;

esp_err_t iniciar_runtime_tflite()
{
    if (runtime_pronto) {
        return ESP_OK;
    }

    if (PRESENCA_MODEL_TFLITE_LEN < 16) {
        return ESP_ERR_INVALID_SIZE;
    }

    modelo_tflite = tflite::GetModel(PRESENCA_MODEL_TFLITE);
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

    entrada_tflite = interpretador->input(0);
    saida_tflite = interpretador->output(0);
    if (entrada_tflite == nullptr || saida_tflite == nullptr ||
        entrada_tflite->type != kTfLiteInt8 || saida_tflite->type != kTfLiteInt8 ||
        tensor_num_elementos(entrada_tflite) != PRESENCA_MODEL_INPUT_SIZE ||
        tensor_num_elementos(saida_tflite) != PRESENCA_MODEL_OUTPUT_SIZE) {
        interpretador = nullptr;
        entrada_tflite = nullptr;
        saida_tflite = nullptr;
        return ESP_ERR_INVALID_SIZE;
    }

    runtime_pronto = true;
    return ESP_OK;
}
} // namespace

esp_err_t presenca_tflite_iniciar(presenca_tflite_t *modelo)
{
    if (modelo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(modelo, 0, sizeof(*modelo));
    modelo->arena_bytes = PRESENCA_MODEL_TENSOR_ARENA_BYTES;

    esp_err_t erro = iniciar_runtime_tflite();
    modelo->pronto = true;
    modelo->runtime_tflite = erro == ESP_OK;
    modelo->ultimo_score = 0;
    return ESP_OK;
}

int32_t presenca_modelo_compacto_score(uint16_t distancia_cm, uint32_t eco_us)
{
    if (distancia_cm < PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM ||
        distancia_cm > PRESENCA_MODEL_DISTANCIA_MAX_CM) {
        return -100000;
    }

    int32_t distancia_delta = (int32_t)PRESENCA_MODEL_DISTANCIA_LIMIAR_CM - (int32_t)distancia_cm;
    int32_t eco_delta_cm = ((int32_t)PRESENCA_MODEL_ECO_LIMIAR_US - (int32_t)eco_us) / 58;

    return PRESENCA_MODELO_BIAS -
           (int32_t)PRESENCA_MODELO_PESOS[0] * distancia_delta -
           (int32_t)PRESENCA_MODELO_PESOS[1] * eco_delta_cm;
}

bool presenca_modelo_compacto_classificar(uint16_t distancia_cm, uint32_t eco_us)
{
    return presenca_modelo_compacto_score(distancia_cm, eco_us) >= 0;
}

bool presenca_tflite_classificar(presenca_tflite_t *modelo, uint16_t distancia_cm, uint32_t eco_us)
{
    if (distancia_cm < PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM ||
        distancia_cm > PRESENCA_MODEL_DISTANCIA_MAX_CM) {
        if (modelo != NULL) {
            modelo->ultimo_score = presenca_modelo_compacto_score(distancia_cm, eco_us);
        }
        return false;
    }

    if (modelo == NULL || !modelo->pronto || !modelo->runtime_tflite ||
        interpretador == nullptr || entrada_tflite == nullptr || saida_tflite == nullptr) {
        bool presente = presenca_modelo_compacto_classificar(distancia_cm, eco_us);
        if (modelo != NULL) {
            modelo->ultimo_score = presenca_modelo_compacto_score(distancia_cm, eco_us);
        }
        return presente;
    }

    float distancia_normalizada = (float)distancia_cm / (float)PRESENCA_MODEL_DISTANCIA_MAX_CM;
    float eco_normalizado = (float)eco_us / (float)(PRESENCA_MODEL_DISTANCIA_MAX_CM * 58);
    entrada_tflite->data.int8[0] = quantizar_para_tensor(distancia_normalizada, entrada_tflite);
    entrada_tflite->data.int8[1] = quantizar_para_tensor(eco_normalizado, entrada_tflite);

    if (interpretador->Invoke() != kTfLiteOk) {
        bool presente = presenca_modelo_compacto_classificar(distancia_cm, eco_us);
        modelo->ultimo_score = presenca_modelo_compacto_score(distancia_cm, eco_us);
        return presente;
    }

    int32_t score = (int32_t)saida_tflite->data.int8[1] - (int32_t)saida_tflite->data.int8[0];
    modelo->ultimo_score = score;
    if (score == 0) {
        return presenca_modelo_compacto_classificar(distancia_cm, eco_us);
    }

    return score > 0;
}
