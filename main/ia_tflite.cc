#include "ia_tflite.h"

#include <string.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static bool casa_livre(const jogo_estado_t *jogo, int indice)
{
    if (jogo == NULL || indice < 0 || indice >= JOGO_CELULAS) {
        return false;
    }

    return jogo->casas[indice / JOGO_TAMANHO][indice % JOGO_TAMANHO] == ' ';
}

static int8_t limitar_score(int valor)
{
    if (valor > 127) {
        return 127;
    }

    if (valor < -128) {
        return -128;
    }

    return (int8_t)valor;
}

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

static int8_t quantizar_para_tensor(float valor, const TfLiteTensor *tensor)
{
    if (tensor == nullptr || tensor->params.scale <= 0.0f) {
        return limitar_score((int)valor);
    }

    return limitar_score(arredondar(valor / tensor->params.scale) + tensor->params.zero_point);
}

namespace {
alignas(16) uint8_t tensor_arena[TICTACTOE_MODEL_TENSOR_ARENA_BYTES];
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

    if (TICTACTOE_MODEL_INPUT_SIZE != JOGO_CELULAS ||
        TICTACTOE_MODEL_OUTPUT_SIZE != JOGO_CELULAS ||
        TICTACTOE_MODEL_TFLITE_LEN == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    modelo_tflite = tflite::GetModel(TICTACTOE_MODEL_TFLITE);
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
        tensor_num_elementos(entrada_tflite) != TICTACTOE_MODEL_INPUT_SIZE ||
        tensor_num_elementos(saida_tflite) != TICTACTOE_MODEL_OUTPUT_SIZE) {
        interpretador = nullptr;
        entrada_tflite = nullptr;
        saida_tflite = nullptr;
        return ESP_ERR_INVALID_SIZE;
    }

    runtime_pronto = true;
    return ESP_OK;
}
} // namespace

static ia_resultado_t resultado_invalido(void)
{
    return (ia_resultado_t) {
        .jogada = {
            .linha = -1,
            .coluna = -1,
        },
        .algoritmo = IA_TFLITE,
        .nome_curto = ia_nome(IA_TFLITE),
    };
}

esp_err_t ia_tflite_iniciar(ia_tflite_t *ia)
{
    if (ia == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(ia, 0, sizeof(*ia));
    ia->arena_bytes = TICTACTOE_MODEL_TENSOR_ARENA_BYTES;
    ia->ultimo_indice = -1;
    ia->ultimo_score = -128;

    esp_err_t erro = iniciar_runtime_tflite();
    if (erro != ESP_OK) {
        return erro;
    }

    ia->pronto = true;
    return ESP_OK;
}

void ia_tflite_forcar_falha(ia_tflite_t *ia, bool forcar)
{
    if (ia != NULL) {
        ia->forcar_falha = forcar;
    }
}

void ia_tflite_tabuleiro_para_entrada(const jogo_estado_t *jogo, int8_t entrada[TICTACTOE_MODEL_INPUT_SIZE])
{
    if (jogo == NULL || entrada == NULL) {
        return;
    }

    for (int i = 0; i < JOGO_CELULAS; i++) {
        char casa = jogo->casas[i / JOGO_TAMANHO][i % JOGO_TAMANHO];
        if (casa == 'X') {
            entrada[i] = 1;
        } else if (casa == 'O') {
            entrada[i] = -1;
        } else {
            entrada[i] = 0;
        }
    }
}

esp_err_t ia_tflite_inferir_scores(ia_tflite_t *ia, const jogo_estado_t *jogo, int8_t scores[TICTACTOE_MODEL_OUTPUT_SIZE])
{
    if (ia == NULL || jogo == NULL || scores == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!ia->pronto || ia->forcar_falha) {
        return ESP_FAIL;
    }

    if (!runtime_pronto || interpretador == nullptr || entrada_tflite == nullptr || saida_tflite == nullptr) {
        return ESP_FAIL;
    }

    int8_t entrada_tabuleiro[TICTACTOE_MODEL_INPUT_SIZE];
    ia_tflite_tabuleiro_para_entrada(jogo, entrada_tabuleiro);
    for (int i = 0; i < TICTACTOE_MODEL_INPUT_SIZE; i++) {
        entrada_tflite->data.int8[i] = quantizar_para_tensor((float)entrada_tabuleiro[i], entrada_tflite);
    }

    if (interpretador->Invoke() != kTfLiteOk) {
        return ESP_FAIL;
    }

    for (int i = 0; i < JOGO_CELULAS; i++) {
        if (!casa_livre(jogo, i)) {
            scores[i] = -128;
            continue;
        }

        scores[i] = saida_tflite->data.int8[i];
    }

    return ESP_OK;
}

int ia_tflite_escolher_indice_com_mascara(const int8_t scores[TICTACTOE_MODEL_OUTPUT_SIZE], const jogo_estado_t *jogo)
{
    int melhor_indice = -1;
    int8_t melhor_score = -128;

    if (scores == NULL || jogo == NULL) {
        return -1;
    }

    for (int i = 0; i < JOGO_CELULAS; i++) {
        if (!casa_livre(jogo, i)) {
            continue;
        }

        if (melhor_indice < 0 || scores[i] > melhor_score) {
            melhor_indice = i;
            melhor_score = scores[i];
        }
    }

    return melhor_indice;
}

ia_resultado_t ia_tflite_escolher_jogada(ia_tflite_t *ia, jogo_estado_t *jogo)
{
    int8_t scores[TICTACTOE_MODEL_OUTPUT_SIZE];
    int indice = -1;

    if (ia_tflite_inferir_scores(ia, jogo, scores) != ESP_OK) {
        if (ia != NULL) {
            ia->ultimo_indice = -1;
            ia->ultimo_score = -128;
        }
        return resultado_invalido();
    }

    indice = ia_tflite_escolher_indice_com_mascara(scores, jogo);
    if (indice < 0) {
        if (ia != NULL) {
            ia->ultimo_indice = -1;
            ia->ultimo_score = -128;
        }
        return resultado_invalido();
    }

    if (ia != NULL) {
        ia->ultimo_indice = indice;
        ia->ultimo_score = scores[indice];
    }

    return (ia_resultado_t) {
        .jogada = {
            .linha = indice / JOGO_TAMANHO,
            .coluna = indice % JOGO_TAMANHO,
        },
        .algoritmo = IA_TFLITE,
        .nome_curto = ia_nome(IA_TFLITE),
    };
}
