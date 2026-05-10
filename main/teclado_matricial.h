#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#define TECLADO_LINHAS 4
#define TECLADO_COLUNAS 4

typedef struct {
    gpio_num_t linhas[TECLADO_LINHAS];
    gpio_num_t colunas[TECLADO_COLUNAS];
    char mapa[TECLADO_LINHAS][TECLADO_COLUNAS];
    char ultima_tecla;
} teclado_matricial_t;

esp_err_t teclado_matricial_iniciar(teclado_matricial_t *teclado);
char teclado_matricial_ler(teclado_matricial_t *teclado);
