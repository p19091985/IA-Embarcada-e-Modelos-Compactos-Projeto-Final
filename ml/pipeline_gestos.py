#!/usr/bin/env python3
"""Pipeline do modelo compacto de gestos com MPU6050."""

from __future__ import annotations

import argparse
import csv
import math
import tempfile
from collections import Counter
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
RAW_PADRAO = ROOT / "ml" / "datasets" / "gestos_raw.csv"
JANELAS_PADRAO = ROOT / "ml" / "datasets" / "gestos_janelas.csv"
MODELO_FLOAT_PADRAO = ROOT / "ml" / "models" / "gesto_float.tflite"
MODELO_INT8_PADRAO = ROOT / "ml" / "models" / "gesto_int8.tflite"
HEADER_PADRAO = ROOT / "main" / "gesto_model_data.h"

TAMANHO_JANELA = 16
EIXOS = 3
ENTRADA_TINYML = TAMANHO_JANELA * EIXOS
LIMIAR_AMPLITUDE_PADRAO = 3500
LIMIAR_DELTA_PADRAO = 700


def gerar_raw_exemplo(caminho: Path = RAW_PADRAO, repeticoes: int = 36) -> int:
    """Gera um CSV no mesmo contrato produzido pelo firmware.

    Os sinais modelam padroes tipicos de aceleracao do MPU6050 com variacao
    de amplitude, frequencia e ruido gaussiano, adequados para validacao da
    pipeline de treinamento quando a coleta real nao esta disponivel.
    """
    import random

    rng = random.Random(42)
    caminho.parent.mkdir(parents=True, exist_ok=True)
    linhas: list[list[int]] = []
    timestamp = 0

    for repeticao in range(repeticoes):
        # Baseline drift simulando micro-deslocamento do sensor entre repeticoes
        drift_ax = rng.randint(-60, 60)
        drift_ay = rng.randint(-40, 40)
        drift_az = rng.randint(-30, 30)

        # Repouso: valores proximos a gravidade com ruido gaussiano
        for i in range(TAMANHO_JANELA):
            ruido_ax = int(rng.gauss(0, 45))
            ruido_ay = int(rng.gauss(0, 35))
            ruido_az = int(rng.gauss(0, 50))
            ax = drift_ax + ruido_ax + int(25 * math.sin(i / 4.0))
            ay = drift_ay + ruido_ay + int(20 * math.cos(i / 5.0))
            az = 16384 + drift_az + ruido_az
            linhas.append([timestamp, ax, ay, az, 0])
            timestamp += 20

        # Gesto de confirmacao: pulso bidirecional com variacao por repeticao
        amp_base = 3600 + rng.randint(-500, 800)
        freq_fator = 1.0 + rng.uniform(-0.15, 0.15)
        for i in range(TAMANHO_JANELA):
            direcao = 1 if i % 2 == 0 else -1
            envelope = math.sin(math.pi * i / TAMANHO_JANELA)
            pulso = int(direcao * amp_base * envelope * freq_fator)
            ruido_ax = int(rng.gauss(0, 120))
            ruido_ay = int(rng.gauss(0, 80))
            ruido_az = int(rng.gauss(0, 90))
            ax = pulso + ruido_ax
            ay = -pulso // 2 + ruido_ay + int(200 * math.sin(i * freq_fator / 2.0))
            az = 16384 + int(600 * math.sin(i * freq_fator / 3.0)) + ruido_az
            linhas.append([timestamp, ax, ay, az, 1])
            timestamp += 20

    with caminho.open("w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.writer(arquivo)
        escritor.writerow(["timestamp_ms", "ax", "ay", "az", "label"])
        escritor.writerows(linhas)

    return len(linhas)


def carregar_csv_bruto(caminho: Path) -> list[dict[str, int]]:
    amostras: list[dict[str, int]] = []

    with caminho.open(newline="", encoding="utf-8") as arquivo:
        leitor = csv.DictReader(arquivo)
        campos = {"timestamp_ms", "ax", "ay", "az", "label"}
        if set(leitor.fieldnames or []) < campos:
            raise ValueError(f"CSV precisa conter colunas: {sorted(campos)}")

        for linha in leitor:
            amostras.append({campo: int(linha[campo]) for campo in campos})

    return amostras


def normalizar_valor(valor: int) -> int:
    valor = int(valor / 256)
    return max(-128, min(127, valor))


def janela_para_entrada(janela: list[dict[str, int]]) -> list[int]:
    base = janela[0]
    entrada: list[int] = []

    for amostra in janela:
        entrada.append(normalizar_valor(amostra["ax"] - base["ax"]))
        entrada.append(normalizar_valor(amostra["ay"] - base["ay"]))
        entrada.append(normalizar_valor(amostra["az"] - base["az"]))

    return entrada


def label_majoritario(janela: list[dict[str, int]]) -> int:
    contador = Counter(amostra["label"] for amostra in janela)
    return contador.most_common(1)[0][0]


def criar_janelas(amostras: list[dict[str, int]], tamanho: int = TAMANHO_JANELA, passo: int = 4) -> list[list[int]]:
    if tamanho <= 0 or passo <= 0:
        raise ValueError("tamanho e passo precisam ser positivos")

    janelas: list[list[int]] = []
    for inicio in range(0, max(0, len(amostras) - tamanho + 1), passo):
        janela = amostras[inicio : inicio + tamanho]
        janelas.append([*janela_para_entrada(janela), label_majoritario(janela)])

    return janelas


def salvar_janelas_csv(janelas: list[list[int]], caminho: Path = JANELAS_PADRAO) -> None:
    caminho.parent.mkdir(parents=True, exist_ok=True)

    with caminho.open("w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.writer(arquivo)
        escritor.writerow([*(f"x{i}" for i in range(ENTRADA_TINYML)), "label"])
        escritor.writerows(janelas)


def preparar_janelas(raw: Path = RAW_PADRAO, saida: Path = JANELAS_PADRAO) -> int:
    amostras = carregar_csv_bruto(raw)
    janelas = criar_janelas(amostras)
    salvar_janelas_csv(janelas, saida)
    return len(janelas)


def carregar_janelas(caminho: Path = JANELAS_PADRAO) -> tuple[list[list[int]], list[int]]:
    entradas: list[list[int]] = []
    labels: list[int] = []

    with caminho.open(newline="", encoding="utf-8") as arquivo:
        leitor = csv.DictReader(arquivo)
        for linha in leitor:
            entradas.append([int(linha[f"x{i}"]) for i in range(ENTRADA_TINYML)])
            labels.append(int(linha["label"]))

    return entradas, labels


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

    acuracia = acertos / len(x_teste) if len(x_teste) > 0 else 0.0
    tamanho_kb = caminho_modelo.stat().st_size / 1024
    return {"acuracia": acuracia, "tamanho_kb": tamanho_kb}


def treinar_tensorflow(
    janelas_csv: Path = JANELAS_PADRAO,
    modelo_float: Path = MODELO_FLOAT_PADRAO,
    modelo_int8: Path = MODELO_INT8_PADRAO,
) -> dict[str, object] | bool:
    try:
        import numpy as np
        import tensorflow as tf
    except Exception:
        return False

    if not janelas_csv.exists():
        return False

    entradas, labels = carregar_janelas(janelas_csv)
    if not entradas:
        return False

    x = np.array(entradas, dtype=np.float32)
    y = np.array(labels, dtype=np.int64)

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
            tf.keras.layers.Input(shape=(ENTRADA_TINYML,)),
            tf.keras.layers.Dense(24, activation="relu"),
            tf.keras.layers.Dense(2),
        ]
    )
    modelo.compile(optimizer="adam", loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True), metrics=["accuracy"])
    historico = modelo.fit(x_treino, y_treino, epochs=60, batch_size=16, verbose=0,
                           validation_data=(x_teste, y_teste) if n_teste >= 2 else None)

    # Metricas de treino
    val_acc = historico.history.get("val_accuracy", [None])[-1]
    treino_acc = historico.history.get("accuracy", [None])[-1]
    print(f"\n--- Avaliacao do modelo de gestos ---")
    print(f"Amostras: {n_total} total, {len(x_treino)} treino, {n_teste} teste")
    if treino_acc is not None:
        print(f"Acuracia treino (ultima epoca): {treino_acc:.4f}")
    if val_acc is not None:
        print(f"Acuracia validacao (ultima epoca): {val_acc:.4f}")

    modelo_float.parent.mkdir(parents=True, exist_ok=True)
    saved_model_dir = Path(tempfile.mkdtemp(prefix="gesto_saved_model_"))
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

    print(f"\n{'Modelo':<18} {'Acuracia':>10} {'Tamanho':>10}")
    print("-" * 40)
    if metricas_float:
        print(f"{'Float':<18} {metricas_float['acuracia']:>9.4f} {metricas_float['tamanho_kb']:>8.1f} KB")
    if metricas_int8:
        print(f"{'INT8 (quantizado)':<18} {metricas_int8['acuracia']:>9.4f} {metricas_int8['tamanho_kb']:>8.1f} KB")
    if metricas_float and metricas_int8:
        delta_acc = metricas_int8["acuracia"] - metricas_float["acuracia"]
        reducao = (1.0 - metricas_int8["tamanho_kb"] / metricas_float["tamanho_kb"]) * 100
        print(f"\nDelta acuracia (INT8 - float): {delta_acc:+.4f}")
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
    bytes_modelo = modelo_int8.read_bytes() if modelo_int8.exists() else b"GESTO_INT8_FALLB"
    array = _formatar_array_c(bytes_modelo)

    conteudo = f"""#pragma once

#include <stdint.h>

#define GESTO_MODEL_WINDOW_SIZE {TAMANHO_JANELA}
#define GESTO_MODEL_INPUT_SIZE {ENTRADA_TINYML}
#define GESTO_MODEL_TENSOR_ARENA_BYTES 8192

static const int32_t GESTO_MODEL_LIMIAR_AMPLITUDE = {LIMIAR_AMPLITUDE_PADRAO};
static const int32_t GESTO_MODEL_LIMIAR_DELTA_MEDIO = {LIMIAR_DELTA_PADRAO};
static const int32_t GESTO_MODEL_PESO_AMPLITUDE = 2;
static const int32_t GESTO_MODEL_PESO_DELTA = 5;
static const int32_t GESTO_MODEL_BIAS = -10500;

static const unsigned char GESTO_MODEL_TFLITE[] = {{
{array}
}};

static const unsigned int GESTO_MODEL_TFLITE_LEN = sizeof(GESTO_MODEL_TFLITE);
"""
    header.write_text(conteudo, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Prepara janelas, treina e exporta modelo de gestos.")
    parser.add_argument("--raw", type=Path, default=RAW_PADRAO)
    parser.add_argument("--janelas", type=Path, default=JANELAS_PADRAO)
    parser.add_argument("--modelo-float", type=Path, default=MODELO_FLOAT_PADRAO)
    parser.add_argument("--modelo-int8", type=Path, default=MODELO_INT8_PADRAO)
    parser.add_argument("--header", type=Path, default=HEADER_PADRAO)
    parser.add_argument("--gerar-exemplo", action="store_true", help="gera um CSV inicial no formato do firmware")
    parser.add_argument("--sem-treino", action="store_true", help="gera header sem tentar TensorFlow")
    args = parser.parse_args()

    total = 0
    usou_exemplo = False
    if args.gerar_exemplo and not args.raw.exists():
        amostras = gerar_raw_exemplo(args.raw)
        print(f"raw exemplo: {args.raw} ({amostras} amostras)")
        usou_exemplo = True

    if args.raw.exists():
        total = preparar_janelas(args.raw, args.janelas)

    if usou_exemplo or (args.gerar_exemplo and args.raw.exists()):
        print("\n--- Dataset de gestos gerado por modelagem algoritmica. ---")
        print("--- Os sinais simulam padroes tipicos do MPU6050 com ruido ---")
        print(f"--- gaussiano para validacao da pipeline de treinamento. ---\n")

    resultado = False if args.sem_treino else treinar_tensorflow(args.janelas, args.modelo_float, args.modelo_int8)
    exportar_header(args.modelo_int8, args.header)

    treinou = isinstance(resultado, dict) and resultado.get("treinou", False)
    print(f"janelas: {args.janelas} ({total} linhas)")
    print(f"treino tensorflow: {'ok' if treinou else 'nao executado'}")
    print(f"header: {args.header}")


if __name__ == "__main__":
    main()
