#pragma once

#include <stdbool.h>
#include <stdint.h>

#define JOGO_INTERFACE_LCD_COLUNAS 16
#define JOGO_INTERFACE_LCD_TAMANHO_LINHA (JOGO_INTERFACE_LCD_COLUNAS + 1)

bool jogo_interacao_liberada_por_presenca(bool sensor_pronto, bool leitura_valida, bool presente);
bool jogo_interacao_tecla_liberada_por_presenca(char tecla,
                                                bool sensor_pronto,
                                                bool leitura_valida,
                                                bool presente);
void jogo_interface_formatar_estatisticas_lcd(uint32_t duracao_ms,
                                              uint32_t jogadas,
                                              uint32_t soma_intervalos_jogada_ms,
                                              char linha0[JOGO_INTERFACE_LCD_TAMANHO_LINHA],
                                              char linha1[JOGO_INTERFACE_LCD_TAMANHO_LINHA]);
