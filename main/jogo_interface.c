#include "jogo_interface.h"

#include <stdio.h>

bool jogo_interacao_liberada_por_presenca(bool sensor_pronto, bool leitura_valida, bool presente)
{
    if (!sensor_pronto) {
        return true;
    }

    return leitura_valida && presente;
}

bool jogo_interacao_tecla_liberada_por_presenca(char tecla,
                                                bool sensor_pronto,
                                                bool leitura_valida,
                                                bool presente)
{
    if (tecla == 0) {
        return true;
    }

    return jogo_interacao_liberada_por_presenca(sensor_pronto, leitura_valida, presente);
}

void jogo_interface_formatar_estatisticas_lcd(uint32_t duracao_ms,
                                              uint32_t jogadas,
                                              uint32_t soma_intervalos_jogada_ms,
                                              char linha0[JOGO_INTERFACE_LCD_TAMANHO_LINHA],
                                              char linha1[JOGO_INTERFACE_LCD_TAMANHO_LINHA])
{
    if (linha0 == NULL || linha1 == NULL) {
        return;
    }

    uint32_t total_segundos = duracao_ms / 1000U;
    uint32_t minutos = total_segundos / 60U;
    uint32_t segundos = total_segundos % 60U;
    uint32_t media_decimos = 0;

    if (jogadas > 0) {
        uint32_t media_ms = soma_intervalos_jogada_ms / jogadas;
        media_decimos = (media_ms + 50U) / 100U;
        if (media_decimos > 999U) {
            media_decimos = 999U;
        }
    }

    unsigned int minutos_lcd = (unsigned int)(minutos % 100U);
    unsigned int segundos_lcd = (unsigned int)(segundos % 60U);
    unsigned int jogadas_lcd = (unsigned int)(jogadas % 100U);
    unsigned int media_segundos_lcd = (unsigned int)(media_decimos / 10U);
    unsigned int media_decimal_lcd = (unsigned int)(media_decimos % 10U);

    snprintf(linha0, JOGO_INTERFACE_LCD_TAMANHO_LINHA, "Tempo %02u:%02u", minutos_lcd, segundos_lcd);
    snprintf(linha1,
             JOGO_INTERFACE_LCD_TAMANHO_LINHA,
             "Jgd:%02u Med:%u.%us",
             jogadas_lcd,
             media_segundos_lcd,
             media_decimal_lcd);
}
