#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "presenca_model_data.h"

typedef struct {
    bool pronto;
    bool runtime_tflite;
    int arena_bytes;
    int32_t ultimo_score;
} presenca_tflite_t;

typedef struct {
    bool leitura_valida;
    bool presente;
    uint16_t distancia_cm;
    uint32_t eco_us;
    int32_t score;
} presenca_tflite_resultado_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t presenca_tflite_iniciar(presenca_tflite_t *modelo);
bool presenca_tflite_classificar(presenca_tflite_t *modelo, uint16_t distancia_cm, uint32_t eco_us);
presenca_tflite_resultado_t presenca_tflite_avaliar_leitura_hcsr04(presenca_tflite_t *modelo,
                                                                    esp_err_t erro_leitura,
                                                                    uint16_t distancia_cm,
                                                                    uint32_t eco_us);
bool presenca_modelo_compacto_classificar(uint16_t distancia_cm, uint32_t eco_us);
int32_t presenca_modelo_compacto_score(uint16_t distancia_cm, uint32_t eco_us);

#ifdef __cplusplus
}
#endif
