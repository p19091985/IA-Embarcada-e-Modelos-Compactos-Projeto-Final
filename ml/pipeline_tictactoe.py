#!/usr/bin/env python3
"""Pipeline do unico modelo compacto para o cerebro do Jogo da Velha.

O fluxo segue a estrutura do notebook `codigo/tflite_hello_world_training.ipynb`:
modelo Keras pequeno, conversao para TFLite float, quantizacao INT8 com dataset
representativo e exportacao do array C para o firmware. O minimax aparece apenas
como gerador das respostas de treino; no firmware, somente o modelo TFLite roda.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
import tempfile
from collections import Counter
from functools import lru_cache
from pathlib import Path
from typing import Iterable, Sequence


ROOT = Path(__file__).resolve().parents[1]
DATASET_PADRAO = ROOT / "ml" / "datasets" / "dataset_tictactoe.csv"
MODELO_FLOAT_PADRAO = ROOT / "ml" / "models" / "tictactoe_float.tflite"
MODELO_INT8_PADRAO = ROOT / "ml" / "models" / "tictactoe_int8.tflite"
HEADER_PADRAO = ROOT / "main" / "tictactoe_model_data.h"
RELATORIO_PADRAO = ROOT / "ml" / "relatorios" / "tictactoe_training_report.json"
NOTEBOOK_BASE = "codigo/tflite_hello_world_training.ipynb"
PDF_REQUISITOS = "codigo/Descrição do projeto final.pdf"
SEMENTE = 42
QUANTIZACAO = "full_integer_int8"

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


@lru_cache(maxsize=None)
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


def score_jogada_x(tabuleiro: Sequence[int], indice: int) -> int:
    if indice < 0 or indice >= CELULAS or tabuleiro[indice] != VAZIO:
        return -1000

    proximo = list(tabuleiro)
    proximo[indice] = X_COMPUTADOR
    return minimax(tuple(proximo), False, 0)


def jogada_otima(tabuleiro: Sequence[int], indice: int) -> bool:
    if indice < 0 or indice >= CELULAS or tabuleiro[indice] != VAZIO:
        return False

    melhor_score = max(score_jogada_x(tabuleiro, candidato) for candidato, valor in enumerate(tabuleiro) if valor == VAZIO)
    return score_jogada_x(tabuleiro, indice) == melhor_score


def politica_otima(tabuleiro: Sequence[int]) -> list[float]:
    scores = [score_jogada_x(tabuleiro, indice) if valor == VAZIO else -1000 for indice, valor in enumerate(tabuleiro)]
    melhor_score = max(scores)
    otimas = [indice for indice, score in enumerate(scores) if score == melhor_score]
    peso = 1.0 / len(otimas)
    return [peso if indice in otimas else 0.0 for indice in range(CELULAS)]


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


def sha256_arquivo(caminho: Path) -> str:
    return hashlib.sha256(caminho.read_bytes()).hexdigest() if caminho.exists() else ""


def auditar_dataset(linhas: list[list[int]]) -> dict[str, object]:
    jogadas = Counter(linha[CELULAS] for linha in linhas)
    ocupadas = Counter(sum(1 for valor in linha[:CELULAS] if valor != VAZIO) for linha in linhas)

    return {
        "linhas": len(linhas),
        "features": CELULAS,
        "rotulo": "best_move",
        "classes": {str(indice): jogadas.get(indice, 0) for indice in range(CELULAS)},
        "casas_ocupadas": {str(chave): ocupadas[chave] for chave in sorted(ocupadas)},
        "origem_rotulo": "minimax offline usado somente para gerar o dataset",
    }


def selecionar_representativos(x_treino, y_treino, max_amostras: int = 512):
    """Seleciona amostras variadas por classe para calibrar a quantizacao INT8."""
    try:
        import numpy as np
    except Exception:
        return x_treino[:max_amostras]

    selecionados: list[int] = []
    por_classe = max(1, max_amostras // CELULAS)
    for classe in range(CELULAS):
        indices = np.where(y_treino == classe)[0][:por_classe]
        selecionados.extend(int(indice) for indice in indices)

    if len(selecionados) < min(max_amostras, len(x_treino)):
        usados = set(selecionados)
        for indice in range(len(x_treino)):
            if indice not in usados:
                selecionados.append(indice)
            if len(selecionados) >= min(max_amostras, len(x_treino)):
                break

    return x_treino[selecionados[:max_amostras]]


def contrato_quantizacao(caminho_modelo: Path) -> dict[str, object]:
    try:
        import tensorflow as tf
    except Exception:
        return {}

    if not caminho_modelo.exists():
        return {}

    interprete = tf.lite.Interpreter(model_path=str(caminho_modelo))
    interprete.allocate_tensors()
    entrada = interprete.get_input_details()[0]
    saida = interprete.get_output_details()[0]

    return {
        "tipo": QUANTIZACAO,
        "entrada_dtype": str(entrada["dtype"]),
        "saida_dtype": str(saida["dtype"]),
        "entrada_shape": [int(valor) for valor in entrada["shape"]],
        "saida_shape": [int(valor) for valor in saida["shape"]],
        "entrada_quantization": [float(entrada["quantization"][0]), int(entrada["quantization"][1])],
        "saida_quantization": [float(saida["quantization"][0]), int(saida["quantization"][1])],
    }


def _avaliar_tflite(caminho_modelo: Path, x_teste, y_teste) -> dict[str, object]:
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
    jogadas_otimas = 0
    matriz = np.zeros((CELULAS, CELULAS), dtype=np.int64)
    for i in range(len(x_teste)):
        amostra = np.array([x_teste[i]], dtype=np.float32)
        if entrada_info["dtype"] == np.int8:
            escala, zero = entrada_info["quantization"]
            amostra = np.round(amostra / escala + zero).astype(np.int8)
        interprete.set_tensor(entrada_info["index"], amostra)
        interprete.invoke()
        saida = interprete.get_tensor(saida_info["index"])[0]
        previsto = int(np.argmax(saida))
        real = int(y_teste[i])
        matriz[real][previsto] += 1
        if previsto == real:
            acertos += 1
        # Acuracia com mascara de casas ocupadas
        scores = saida.copy().astype(float)
        for j in range(CELULAS):
            if x_teste[i][j] != VAZIO:
                scores[j] = -1e9
        previsto_mascarado = int(np.argmax(scores))
        if previsto_mascarado == real:
            acertos_mascara += 1
        if jogada_otima([int(valor) for valor in x_teste[i]], previsto_mascarado):
            jogadas_otimas += 1

    n = len(x_teste) if len(x_teste) > 0 else 1
    tamanho_kb = caminho_modelo.stat().st_size / 1024
    return {
        "acuracia": acertos / n,
        "acuracia_mascara": acertos_mascara / n,
        "jogada_otima": jogadas_otimas / n,
        "acertos": acertos,
        "acertos_mascara": acertos_mascara,
        "acertos_jogada_otima": jogadas_otimas,
        "amostras": len(x_teste),
        "matriz_confusao": matriz.tolist(),
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
    y_politica = np.array([politica_otima(tabuleiro) for tabuleiro in entradas], dtype=np.float32)

    # Separar treino e teste (80/20) para avaliacao do modelo
    n_total = len(x)
    n_teste = max(1, int(n_total * 0.2))
    indices = np.arange(n_total)
    np.random.seed(SEMENTE)
    np.random.shuffle(indices)
    idx_teste = indices[:n_teste]
    idx_treino = indices[n_teste:]
    x_treino, y_treino = x[idx_treino], y[idx_treino]
    x_teste, y_teste = x[idx_teste], y[idx_teste]
    y_politica_treino = y_politica[idx_treino]
    y_politica_teste = y_politica[idx_teste]
    tf.keras.utils.set_random_seed(SEMENTE)
    try:
        tf.config.experimental.enable_op_determinism()
    except Exception:
        pass

    modelo = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(CELULAS,)),
            tf.keras.layers.Dense(32, activation="relu"),
            tf.keras.layers.Dense(32, activation="relu"),
            tf.keras.layers.Dense(CELULAS),
        ]
    )
    modelo.compile(
        optimizer="adam",
        loss=tf.keras.losses.CategoricalCrossentropy(from_logits=True),
        metrics=[tf.keras.metrics.CategoricalAccuracy(name="policy_accuracy")],
    )
    parada = tf.keras.callbacks.EarlyStopping(
        monitor="val_policy_accuracy",
        mode="max",
        patience=24,
        restore_best_weights=True,
    )
    historico = modelo.fit(
        x_treino,
        y_politica_treino,
        epochs=160,
        batch_size=32,
        verbose=0,
        validation_data=(x_teste, y_politica_teste),
        callbacks=[parada],
        shuffle=True,
    )

    # Metricas de treino
    val_acc = historico.history.get("val_policy_accuracy", [None])[-1]
    treino_acc = historico.history.get("policy_accuracy", [None])[-1]
    print(f"\n--- Avaliacao do modelo do jogo da velha ---")
    print(f"Amostras: {n_total} total, {len(x_treino)} treino, {n_teste} teste")
    if treino_acc is not None:
        print(f"Acuracia politica treino (ultima epoca): {treino_acc:.4f}")
    if val_acc is not None:
        print(f"Acuracia politica validacao (ultima epoca): {val_acc:.4f}")

    modelo_float.parent.mkdir(parents=True, exist_ok=True)
    saved_model_dir = Path(tempfile.mkdtemp(prefix="tictactoe_saved_model_"))
    modelo.export(saved_model_dir)

    conversor = tf.lite.TFLiteConverter.from_saved_model(str(saved_model_dir))
    modelo_float.write_bytes(conversor.convert())

    representativos = selecionar_representativos(x_treino, y_treino)

    def representante() -> Iterable[list[object]]:
        for amostra in representativos:
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
    quantizacao = contrato_quantizacao(modelo_int8)

    print(f"\n{'Modelo':<18} {'Acuracia':>10} {'c/ Mascara':>11} {'Otima':>9} {'Tamanho':>10}")
    print("-" * 62)
    if metricas_float:
        print(f"{'Float':<18} {metricas_float['acuracia']:>9.4f} {metricas_float['acuracia_mascara']:>10.4f} {metricas_float['jogada_otima']:>9.4f} {metricas_float['tamanho_kb']:>8.1f} KB")
    if metricas_int8:
        print(f"{'INT8 (quantizado)':<18} {metricas_int8['acuracia']:>9.4f} {metricas_int8['acuracia_mascara']:>10.4f} {metricas_int8['jogada_otima']:>9.4f} {metricas_int8['tamanho_kb']:>8.1f} KB")
    if metricas_float and metricas_int8:
        delta_acc = metricas_int8["acuracia"] - metricas_float["acuracia"]
        delta_mascara = metricas_int8["acuracia_mascara"] - metricas_float["acuracia_mascara"]
        delta_otima = metricas_int8["jogada_otima"] - metricas_float["jogada_otima"]
        reducao = (1.0 - metricas_int8["tamanho_kb"] / metricas_float["tamanho_kb"]) * 100
        print(f"\nDelta acuracia (INT8 - float): {delta_acc:+.4f}")
        print(f"Delta acuracia c/ mascara:     {delta_mascara:+.4f}")
        print(f"Delta jogada otima:            {delta_otima:+.4f}")
        print(f"Reducao de tamanho: {reducao:.0f}%")
    print()

    return {
        "treinou": True,
        "float": metricas_float,
        "int8": metricas_int8,
        "split": {"treino": int(len(x_treino)), "teste": int(n_teste), "semente": SEMENTE},
        "epocas_executadas": len(historico.history.get("loss", [])),
        "amostras_representativas": int(len(representativos)),
        "quantizacao": quantizacao,
    }


def _formatar_array_c(bytes_modelo: bytes) -> str:
    linhas = []
    for i in range(0, len(bytes_modelo), 12):
        chunk = bytes_modelo[i : i + 12]
        linhas.append("    " + ", ".join(f"0x{valor:02x}" for valor in chunk) + ",")
    return "\n".join(linhas)


def metrica_permyriad(metricas: dict[str, object] | None, chave: str) -> int:
    if not metricas or chave not in metricas:
        return -1
    return int(round(float(metricas[chave]) * 10000))


def artefato(caminho: Path) -> dict[str, object]:
    return {
        "path": str(caminho.relative_to(ROOT)),
        "bytes": caminho.stat().st_size if caminho.exists() else 0,
        "sha256": sha256_arquivo(caminho),
    }


def salvar_relatorio(
    relatorio: Path,
    dataset: Path,
    modelo_float: Path,
    modelo_int8: Path,
    header: Path,
    resultado: dict[str, object] | bool,
) -> None:
    linhas = gerar_linhas_dataset()
    treinou = isinstance(resultado, dict) and resultado.get("treinou", False)
    metricas_float = resultado.get("float", {}) if treinou else {}
    metricas_int8 = resultado.get("int8", {}) if treinou else {}
    split = resultado.get("split", {}) if treinou else {}
    quantizacao = resultado.get("quantizacao", {}) if treinou else contrato_quantizacao(modelo_int8)

    documento = {
        "modelo": "jogo_da_velha",
        "objetivo": "Selecionar a jogada do computador mantendo o jogo legado intacto.",
        "notebook_base": NOTEBOOK_BASE,
        "pdf_requisitos": PDF_REQUISITOS,
        "experiencia_usuario": "inalterada: teclado, OLED, LCD, LEDs e buzzer seguem o fluxo Alpha0",
        "pipeline": [
            "geracao do dataset supervisionado",
            "treinamento Keras pequeno no padrao Hello World",
            "conversao para TFLite float",
            "quantizacao inteira INT8 com dataset representativo",
            "exportacao do FlatBuffer como array C",
            "inferencia TFLite Micro no ESP32-S3",
        ],
        "dataset": auditar_dataset(linhas),
        "treinamento": {
            "executado": bool(treinou),
            "semente": SEMENTE,
            "split": split,
            "epocas_executadas": resultado.get("epocas_executadas", 0) if treinou else 0,
            "alvo": "politica multi-alvo com todas as jogadas minimax otimas",
            "arquitetura": ["Input(9)", "Dense(32,relu)", "Dense(32,relu)", "Dense(9,logits)"],
        },
        "metricas": {
            "float": metricas_float,
            "int8": metricas_int8,
            "delta_int8_menos_float": {
                "acuracia": float(metricas_int8.get("acuracia", 0.0)) - float(metricas_float.get("acuracia", 0.0))
                if metricas_float and metricas_int8
                else None,
                "acuracia_mascara": float(metricas_int8.get("acuracia_mascara", 0.0)) -
                float(metricas_float.get("acuracia_mascara", 0.0))
                if metricas_float and metricas_int8
                else None,
                "jogada_otima": float(metricas_int8.get("jogada_otima", 0.0)) -
                float(metricas_float.get("jogada_otima", 0.0))
                if metricas_float and metricas_int8
                else None,
            },
        },
        "quantizacao": {
            "tipo": QUANTIZACAO,
            "amostras_representativas": resultado.get("amostras_representativas", 0) if treinou else 0,
            "contrato_tflite": quantizacao,
        },
        "artefatos": {
            "dataset": artefato(dataset),
            "modelo_float": artefato(modelo_float),
            "modelo_int8": artefato(modelo_int8),
            "header": artefato(header),
        },
        "requisitos_pdf": {
            "treinamento": "modelo treinado com dataset gerado e validado",
            "conversao_compressao": "TFLite float e TFLite INT8 full integer",
            "pipeline_inferencia": "main/ia_tflite.cc executa TFLite Micro e mascara casas ocupadas",
            "coleta_sensor": "cumprida pelo classificador de presenca HC-SR04 em pipeline separada",
        },
    }

    relatorio.parent.mkdir(parents=True, exist_ok=True)
    relatorio.write_text(json.dumps(documento, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def exportar_header(
    modelo_int8: Path = MODELO_INT8_PADRAO,
    header: Path = HEADER_PADRAO,
    dataset_rows: int | None = None,
    metricas_int8: dict[str, object] | None = None,
) -> None:
    header.parent.mkdir(parents=True, exist_ok=True)
    bytes_modelo = modelo_int8.read_bytes() if modelo_int8.exists() else b"TICTACTOE_INT8_F"
    array = _formatar_array_c(bytes_modelo)
    linhas_dataset = dataset_rows if dataset_rows is not None else len(gerar_linhas_dataset())
    sha256 = hashlib.sha256(bytes_modelo).hexdigest()
    acuracia = metrica_permyriad(metricas_int8, "acuracia")
    acuracia_mascara = metrica_permyriad(metricas_int8, "acuracia_mascara")
    jogada_otima = metrica_permyriad(metricas_int8, "jogada_otima")

    conteudo = f"""#pragma once

#include <stdint.h>

#define TICTACTOE_MODEL_INPUT_SIZE 9
#define TICTACTOE_MODEL_OUTPUT_SIZE 9
#define TICTACTOE_MODEL_TENSOR_ARENA_BYTES 8192
#define TICTACTOE_MODEL_DATASET_ROWS {linhas_dataset}
#define TICTACTOE_MODEL_QUANTIZATION "{QUANTIZACAO}"
#define TICTACTOE_MODEL_NOTEBOOK "{NOTEBOOK_BASE}"
#define TICTACTOE_MODEL_INT8_SHA256 "{sha256}"
#define TICTACTOE_MODEL_TEST_ACCURACY_PERMYRIAD {acuracia}
#define TICTACTOE_MODEL_MASKED_ACCURACY_PERMYRIAD {acuracia_mascara}
#define TICTACTOE_MODEL_OPTIMAL_MOVE_PERMYRIAD {jogada_otima}

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
    parser.add_argument("--relatorio", type=Path, default=RELATORIO_PADRAO)
    parser.add_argument("--sem-treino", action="store_true", help="gera dataset/header sem tentar TensorFlow")
    args = parser.parse_args()

    total = salvar_dataset(args.dataset)
    resultado = False if args.sem_treino else treinar_tensorflow(args.dataset, args.modelo_float, args.modelo_int8)
    metricas_int8 = resultado.get("int8", {}) if isinstance(resultado, dict) else {}
    exportar_header(args.modelo_int8, args.header, total, metricas_int8)
    salvar_relatorio(args.relatorio, args.dataset, args.modelo_float, args.modelo_int8, args.header, resultado)

    treinou = isinstance(resultado, dict) and resultado.get("treinou", False)
    print(f"dataset: {args.dataset} ({total} linhas)")
    print(f"treino tensorflow: {'ok' if treinou else 'nao executado'}")
    print(f"header: {args.header}")
    print(f"relatorio: {args.relatorio}")


if __name__ == "__main__":
    main()
