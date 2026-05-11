#!/usr/bin/env python3
"""Pipeline do modelo compacto para o cerebro do Jogo da Velha.

O fluxo principal gera um dataset proprio a partir do minimax, tenta treinar
uma MLP com TensorFlow quando a dependencia esta instalada e exporta um header
C que o firmware consegue embarcar mesmo quando o treino completo nao foi
executado na maquina local.
"""

from __future__ import annotations

import argparse
import csv
import itertools
import tempfile
from pathlib import Path
from typing import Iterable, Sequence


ROOT = Path(__file__).resolve().parents[1]
DATASET_PADRAO = ROOT / "ml" / "datasets" / "dataset_tictactoe.csv"
MODELO_FLOAT_PADRAO = ROOT / "ml" / "models" / "tictactoe_float.tflite"
MODELO_INT8_PADRAO = ROOT / "ml" / "models" / "tictactoe_int8.tflite"
HEADER_PADRAO = ROOT / "main" / "tictactoe_model_data.h"

VAZIO = 0
X_COMPUTADOR = 1
O_JOGADOR = -1
CELULAS = 9
LINHAS_VITORIA = (
    (0, 1, 2),
    (3, 4, 5),
    (6, 7, 8),
    (0, 3, 6),
    (1, 4, 7),
    (2, 5, 8),
    (0, 4, 8),
    (2, 4, 6),
)
PRIORIDADES = (24, 16, 24, 16, 32, 16, 24, 16, 24)


def vencedor(tabuleiro: Sequence[int], jogador: int) -> bool:
    return any(all(tabuleiro[i] == jogador for i in linha) for linha in LINHAS_VITORIA)


def tabuleiro_valido(tabuleiro: Sequence[int]) -> bool:
    x = tabuleiro.count(X_COMPUTADOR)
    o = tabuleiro.count(O_JOGADOR)

    if o < x or o > x + 1:
        return False

    x_venceu = vencedor(tabuleiro, X_COMPUTADOR)
    o_venceu = vencedor(tabuleiro, O_JOGADOR)

    if x_venceu and o_venceu:
        return False
    if x_venceu and o != x:
        return False
    if o_venceu and o != x + 1:
        return False

    return True


def terminal(tabuleiro: Sequence[int]) -> bool:
    return vencedor(tabuleiro, X_COMPUTADOR) or vencedor(tabuleiro, O_JOGADOR) or VAZIO not in tabuleiro


def pontuacao_terminal(tabuleiro: Sequence[int], profundidade: int) -> int | None:
    if vencedor(tabuleiro, X_COMPUTADOR):
        return 10 - profundidade
    if vencedor(tabuleiro, O_JOGADOR):
        return profundidade - 10
    if VAZIO not in tabuleiro:
        return 0
    return None


def minimax(tabuleiro: tuple[int, ...], vez_x: bool, profundidade: int = 0) -> int:
    terminal_score = pontuacao_terminal(tabuleiro, profundidade)
    if terminal_score is not None:
        return terminal_score

    jogador = X_COMPUTADOR if vez_x else O_JOGADOR
    scores = []
    for indice, valor in enumerate(tabuleiro):
        if valor != VAZIO:
            continue
        proximo = list(tabuleiro)
        proximo[indice] = jogador
        scores.append(minimax(tuple(proximo), not vez_x, profundidade + 1))

    return max(scores) if vez_x else min(scores)


def melhor_jogada(tabuleiro: Sequence[int]) -> int:
    melhor_indice = -1
    melhor_score = -1000

    for indice, valor in enumerate(tabuleiro):
        if valor != VAZIO:
            continue
        proximo = list(tabuleiro)
        proximo[indice] = X_COMPUTADOR
        score = minimax(tuple(proximo), False, 0)
        if score > melhor_score:
            melhor_score = score
            melhor_indice = indice

    return melhor_indice


def gerar_linhas_dataset() -> list[list[int]]:
    linhas: list[list[int]] = []

    for tabuleiro in itertools.product((O_JOGADOR, VAZIO, X_COMPUTADOR), repeat=CELULAS):
        if not tabuleiro_valido(tabuleiro) or terminal(tabuleiro):
            continue
        if tabuleiro.count(O_JOGADOR) != tabuleiro.count(X_COMPUTADOR) + 1:
            continue
        jogada = melhor_jogada(tabuleiro)
        if jogada >= 0:
            linhas.append([*tabuleiro, jogada])

    return linhas


def salvar_dataset(caminho: Path = DATASET_PADRAO) -> int:
    caminho.parent.mkdir(parents=True, exist_ok=True)
    linhas = gerar_linhas_dataset()

    with caminho.open("w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.writer(arquivo)
        escritor.writerow([*(f"c{i}" for i in range(CELULAS)), "best_move"])
        escritor.writerows(linhas)

    return len(linhas)


def carregar_dataset(caminho: Path = DATASET_PADRAO) -> tuple[list[list[int]], list[int]]:
    entradas: list[list[int]] = []
    saidas: list[int] = []

    with caminho.open(newline="", encoding="utf-8") as arquivo:
        leitor = csv.DictReader(arquivo)
        for linha in leitor:
            entradas.append([int(linha[f"c{i}"]) for i in range(CELULAS)])
            saidas.append(int(linha["best_move"]))

    return entradas, saidas


def _avaliar_tflite(caminho_modelo: Path, x_teste, y_teste) -> dict[str, float]:
    """Avalia um modelo .tflite no conjunto de teste e retorna metricas."""
    try:
        import numpy as np
        import tensorflow as tf
    except Exception:
        return {}

    if not caminho_modelo.exists():
        return {}

    interprete = tf.lite.Interpreter(model_path=str(caminho_modelo))
    interprete.allocate_tensors()
    entrada_info = interprete.get_input_details()[0]
    saida_info = interprete.get_output_details()[0]

    acertos = 0
    acertos_mascara = 0
    for i in range(len(x_teste)):
        amostra = np.array([x_teste[i]], dtype=np.float32)
        if entrada_info["dtype"] == np.int8:
            escala, zero = entrada_info["quantization"]
            amostra = np.round(amostra / escala + zero).astype(np.int8)
        interprete.set_tensor(entrada_info["index"], amostra)
        interprete.invoke()
        saida = interprete.get_tensor(saida_info["index"])[0]
        if np.argmax(saida) == y_teste[i]:
            acertos += 1
        # Acuracia com mascara de casas ocupadas
        scores = saida.copy().astype(float)
        for j in range(CELULAS):
            if x_teste[i][j] != VAZIO:
                scores[j] = -1e9
        if np.argmax(scores) == y_teste[i]:
            acertos_mascara += 1

    n = len(x_teste) if len(x_teste) > 0 else 1
    tamanho_kb = caminho_modelo.stat().st_size / 1024
    return {
        "acuracia": acertos / n,
        "acuracia_mascara": acertos_mascara / n,
        "tamanho_kb": tamanho_kb,
    }


def treinar_tensorflow(
    dataset: Path = DATASET_PADRAO,
    modelo_float: Path = MODELO_FLOAT_PADRAO,
    modelo_int8: Path = MODELO_INT8_PADRAO,
) -> dict[str, object] | bool:
    try:
        import numpy as np
        import tensorflow as tf
    except Exception:
        return False

    if not dataset.exists():
        salvar_dataset(dataset)

    entradas, saidas = carregar_dataset(dataset)
    x = np.array(entradas, dtype=np.float32)
    y = np.array(saidas, dtype=np.int64)

    # Separar treino e teste (80/20) para avaliacao do modelo
    n_total = len(x)
    n_teste = max(1, int(n_total * 0.2))
    indices = np.arange(n_total)
    np.random.seed(42)
    np.random.shuffle(indices)
    idx_teste = indices[:n_teste]
    idx_treino = indices[n_teste:]
    x_treino, y_treino = x[idx_treino], y[idx_treino]
    x_teste, y_teste = x[idx_teste], y[idx_teste]

    modelo = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(CELULAS,)),
            tf.keras.layers.Dense(32, activation="relu"),
            tf.keras.layers.Dense(32, activation="relu"),
            tf.keras.layers.Dense(CELULAS),
        ]
    )
    modelo.compile(optimizer="adam", loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True), metrics=["accuracy"])
    historico = modelo.fit(x_treino, y_treino, epochs=80, batch_size=32, verbose=0,
                           validation_data=(x_teste, y_teste))

    # Metricas de treino
    val_acc = historico.history.get("val_accuracy", [None])[-1]
    treino_acc = historico.history.get("accuracy", [None])[-1]
    print(f"\n--- Avaliacao do modelo do jogo da velha ---")
    print(f"Amostras: {n_total} total, {len(x_treino)} treino, {n_teste} teste")
    if treino_acc is not None:
        print(f"Acuracia treino (ultima epoca): {treino_acc:.4f}")
    if val_acc is not None:
        print(f"Acuracia validacao (ultima epoca): {val_acc:.4f}")

    modelo_float.parent.mkdir(parents=True, exist_ok=True)
    saved_model_dir = Path(tempfile.mkdtemp(prefix="tictactoe_saved_model_"))
    modelo.export(saved_model_dir)

    conversor = tf.lite.TFLiteConverter.from_saved_model(str(saved_model_dir))
    modelo_float.write_bytes(conversor.convert())

    def representante() -> Iterable[list[object]]:
        for amostra in x_treino[: min(100, len(x_treino))]:
            yield [np.array([amostra], dtype=np.float32)]

    conversor = tf.lite.TFLiteConverter.from_saved_model(str(saved_model_dir))
    conversor.optimizations = [tf.lite.Optimize.DEFAULT]
    conversor.representative_dataset = representante
    conversor.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conversor.inference_input_type = tf.int8
    conversor.inference_output_type = tf.int8
    modelo_int8.write_bytes(conversor.convert())

    # Avaliacao comparativa float vs INT8
    metricas_float = _avaliar_tflite(modelo_float, x_teste, y_teste)
    metricas_int8 = _avaliar_tflite(modelo_int8, x_teste, y_teste)

    print(f"\n{'Modelo':<18} {'Acuracia':>10} {'c/ Mascara':>11} {'Tamanho':>10}")
    print("-" * 51)
    if metricas_float:
        print(f"{'Float':<18} {metricas_float['acuracia']:>9.4f} {metricas_float['acuracia_mascara']:>10.4f} {metricas_float['tamanho_kb']:>8.1f} KB")
    if metricas_int8:
        print(f"{'INT8 (quantizado)':<18} {metricas_int8['acuracia']:>9.4f} {metricas_int8['acuracia_mascara']:>10.4f} {metricas_int8['tamanho_kb']:>8.1f} KB")
    if metricas_float and metricas_int8:
        delta_acc = metricas_int8["acuracia"] - metricas_float["acuracia"]
        delta_mascara = metricas_int8["acuracia_mascara"] - metricas_float["acuracia_mascara"]
        reducao = (1.0 - metricas_int8["tamanho_kb"] / metricas_float["tamanho_kb"]) * 100
        print(f"\nDelta acuracia (INT8 - float): {delta_acc:+.4f}")
        print(f"Delta acuracia c/ mascara:     {delta_mascara:+.4f}")
        print(f"Reducao de tamanho: {reducao:.0f}%")
    print()

    return {"treinou": True, "float": metricas_float, "int8": metricas_int8}


def _formatar_array_c(bytes_modelo: bytes) -> str:
    linhas = []
    for i in range(0, len(bytes_modelo), 12):
        chunk = bytes_modelo[i : i + 12]
        linhas.append("    " + ", ".join(f"0x{valor:02x}" for valor in chunk) + ",")
    return "\n".join(linhas)


def exportar_header(modelo_int8: Path = MODELO_INT8_PADRAO, header: Path = HEADER_PADRAO) -> None:
    header.parent.mkdir(parents=True, exist_ok=True)
    bytes_modelo = modelo_int8.read_bytes() if modelo_int8.exists() else b"TICTACTOE_INT8_F"
    array = _formatar_array_c(bytes_modelo)
    prioridades = ", ".join(str(valor) for valor in PRIORIDADES)

    conteudo = f"""#pragma once

#include <stdint.h>

#define TICTACTOE_MODEL_INPUT_SIZE 9
#define TICTACTOE_MODEL_OUTPUT_SIZE 9
#define TICTACTOE_MODEL_TENSOR_ARENA_BYTES 8192

static const int8_t TICTACTOE_MODELO_PRIORIDADES[TICTACTOE_MODEL_OUTPUT_SIZE] = {{
    {prioridades},
}};

static const unsigned char TICTACTOE_MODEL_TFLITE[] = {{
{array}
}};

static const unsigned int TICTACTOE_MODEL_TFLITE_LEN = sizeof(TICTACTOE_MODEL_TFLITE);
"""
    header.write_text(conteudo, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Gera dataset, modelo e header C do Jogo da Velha.")
    parser.add_argument("--dataset", type=Path, default=DATASET_PADRAO)
    parser.add_argument("--modelo-float", type=Path, default=MODELO_FLOAT_PADRAO)
    parser.add_argument("--modelo-int8", type=Path, default=MODELO_INT8_PADRAO)
    parser.add_argument("--header", type=Path, default=HEADER_PADRAO)
    parser.add_argument("--sem-treino", action="store_true", help="gera dataset/header sem tentar TensorFlow")
    args = parser.parse_args()

    total = salvar_dataset(args.dataset)
    resultado = False if args.sem_treino else treinar_tensorflow(args.dataset, args.modelo_float, args.modelo_int8)
    exportar_header(args.modelo_int8, args.header)

    treinou = isinstance(resultado, dict) and resultado.get("treinou", False)
    print(f"dataset: {args.dataset} ({total} linhas)")
    print(f"treino tensorflow: {'ok' if treinou else 'nao executado'}")
    print(f"header: {args.header}")


if __name__ == "__main__":
    main()
