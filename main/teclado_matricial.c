#include "teclado_matricial.h"

#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const gpio_num_t linhas_padrao[TECLADO_LINHAS] = {
    GPIO_NUM_2,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
};

static const gpio_num_t colunas_padrao[TECLADO_COLUNAS] = {
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_8,
    GPIO_NUM_9,
};

static const char mapa_padrao[TECLADO_LINHAS][TECLADO_COLUNAS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

esp_err_t teclado_matricial_iniciar(teclado_matricial_t *teclado)
{
    for (int i = 0; i < TECLADO_LINHAS; i++) {
        teclado->linhas[i] = linhas_padrao[i];
        gpio_config_t config_linha = {
            .pin_bit_mask = 1ULL << linhas_padrao[i],
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };

        esp_err_t resultado = gpio_config(&config_linha);
        if (resultado != ESP_OK) {
            return resultado;
        }
        gpio_set_level(linhas_padrao[i], 1);
    }

    for (int i = 0; i < TECLADO_COLUNAS; i++) {
        teclado->colunas[i] = colunas_padrao[i];
        gpio_config_t config_coluna = {
            .pin_bit_mask = 1ULL << colunas_padrao[i],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };

        esp_err_t resultado = gpio_config(&config_coluna);
        if (resultado != ESP_OK) {
            return resultado;
        }
    }

    for (int linha = 0; linha < TECLADO_LINHAS; linha++) {
        for (int coluna = 0; coluna < TECLADO_COLUNAS; coluna++) {
            teclado->mapa[linha][coluna] = mapa_padrao[linha][coluna];
        }
    }

    teclado->ultima_tecla = 0;
    return ESP_OK;
}

char teclado_matricial_ler(teclado_matricial_t *teclado)
{
    char tecla_atual = 0;

    for (int linha = 0; linha < TECLADO_LINHAS; linha++) {
        gpio_set_level(teclado->linhas[linha], 0);
        esp_rom_delay_us(50);

        for (int coluna = 0; coluna < TECLADO_COLUNAS; coluna++) {
            if (gpio_get_level(teclado->colunas[coluna]) == 0) {
                tecla_atual = teclado->mapa[linha][coluna];
                break;
            }
        }

        gpio_set_level(teclado->linhas[linha], 1);

        if (tecla_atual != 0) {
            break;
        }
    }

    if (tecla_atual == 0) {
        teclado->ultima_tecla = 0;
        return 0;
    }

    if (tecla_atual == teclado->ultima_tecla) {
        return 0;
    }

    teclado->ultima_tecla = tecla_atual;
    vTaskDelay(pdMS_TO_TICKS(30));
    return tecla_atual;
}
