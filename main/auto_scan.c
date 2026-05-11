#include "auto_scan.h"

#include <string.h>

static bool posicao_livre(const jogo_estado_t *jogo, int posicao)
{
    if (jogo == NULL || !jogo_posicao_valida(posicao)) {
        return false;
    }

    int linha = (posicao - 1) / JOGO_TAMANHO;
    int coluna = (posicao - 1) % JOGO_TAMANHO;
    return jogo->casas[linha][coluna] == ' ';
}

void auto_scan_iniciar(auto_scan_t *scan, const jogo_estado_t *jogo, uint32_t agora_ms)
{
    if (scan == NULL) {
        return;
    }

    memset(scan, 0, sizeof(*scan));
    scan->intervalo_ms = AUTO_SCAN_INTERVALO_MS;
    scan->ultimo_avanco_ms = agora_ms;
    scan->posicao_atual = auto_scan_proxima_posicao_livre(jogo, 0);
    scan->ativo = scan->posicao_atual != 0;
}

bool auto_scan_atualizar(auto_scan_t *scan, const jogo_estado_t *jogo, uint32_t agora_ms)
{
    int proxima = 0;

    if (scan == NULL || jogo == NULL || !scan->ativo) {
        return false;
    }

    if (!posicao_livre(jogo, scan->posicao_atual)) {
        proxima = auto_scan_proxima_posicao_livre(jogo, scan->posicao_atual);
        scan->posicao_atual = proxima;
        scan->ativo = proxima != 0;
        scan->ultimo_avanco_ms = agora_ms;
        return true;
    }

    if ((uint32_t)(agora_ms - scan->ultimo_avanco_ms) < scan->intervalo_ms) {
        return false;
    }

    proxima = auto_scan_proxima_posicao_livre(jogo, scan->posicao_atual);
    if (proxima == 0) {
        scan->ativo = false;
        scan->posicao_atual = 0;
        return true;
    }

    scan->ultimo_avanco_ms = agora_ms;
    if (proxima == scan->posicao_atual) {
        return false;
    }

    scan->posicao_atual = proxima;
    return true;
}

int auto_scan_posicao_atual(const auto_scan_t *scan)
{
    return scan == NULL || !scan->ativo ? 0 : scan->posicao_atual;
}

int auto_scan_proxima_posicao_livre(const jogo_estado_t *jogo, int posicao_atual)
{
    if (jogo == NULL) {
        return 0;
    }

    int inicio = jogo_posicao_valida(posicao_atual) ? posicao_atual : 0;

    for (int deslocamento = 1; deslocamento <= JOGO_CELULAS; deslocamento++) {
        int posicao = ((inicio + deslocamento - 1) % JOGO_CELULAS) + 1;
        if (posicao_livre(jogo, posicao)) {
            return posicao;
        }
    }

    return 0;
}

bool auto_scan_confirmar(auto_scan_t *scan, jogo_estado_t *jogo, char jogador)
{
    int posicao = auto_scan_posicao_atual(scan);

    if (posicao == 0) {
        return false;
    }

    return jogo_aplicar_posicao(jogo, posicao, jogador);
}
