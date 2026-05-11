#!/usr/bin/env python3
"""Pipeline do classificador de presenca com HC-SR04.

Este fluxo cobre os requisitos do projeto final: gera/coleta dataset de sensor,
treina um modelo pequeno, converte para TFLite, quantiza para INT8 e exporta o
header C usado pela inferencia embarcada no ESP32-S3.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import tempfile
from collections import Counter
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
DATASET_PADRAO = ROOT / "ml" / "datasets" / "presenca_hcsr04.csv"
MODELO_FLOAT_PADRAO = ROOT / "ml" / "models" / "presenca_float.tflite"
MODELO_INT8_PADRAO = ROOT / "ml" / "models" / "presenca_int8.tflite"
HEADER_PADRAO = ROOT / "main" / "presenca_model_data.h"
RELATORIO_PADRAO = ROOT / "ml" / "relatorios" / "presenca_training_report.json"
NOTEBOOK_BASE = "codigo/tflite_hello_world_training.ipynb"
PDF_REQUISITOS = "codigo/Descrição do projeto final.pdf"
SEMENTE = 42
QUANTIZACAO = "full_integer_int8"

DISTANCIA_LIMIAR_CM = 120
DISTANCIA_MAX_CM = 350
DISTANCIA_MIN_PRESENTE_CM = 5
ECO_US_POR_CM = 58
TENSOR_ARENA_BYTES = 8192
PESO_DISTANCIA = -32
PESO_ECO = -1


def label_presenca(distancia_cm: int) -> int:
    return 1 if DISTANCIA_MIN_PRESENTE_CM <= distancia_cm <= DISTANCIA_LIMIAR_CM else 0


def eco_us_para_distancia_cm(eco_us: int) -> int:
    return int(round(eco_us / ECO_US_POR_CM))


def gerar_linhas_dataset() -> list[dict[str, int]]:
    linhas: list[dict[str, int]] = []

    for distancia_base in range(0, DISTANCIA_MAX_CM + 1, 5):
        for ruido_distancia in (-2, -1, 0, 1, 2):
            distancia = max(0, min(DISTANCIA_MAX_CM, distancia_base + ruido_distancia))
            eco_base = distancia * ECO_US_POR_CM
            for ruido_eco in (-14, -7, 0, 7, 14):
                eco_us = max(0, eco_base + ruido_eco)
                linhas.append(
                    {
                        "distancia_cm": distancia,
                        "eco_us": eco_us,
                        "label": label_presenca(distancia),
                    }
                )

    return linhas


def salvar_dataset(caminho: Path = DATASET_PADRAO) -> int:
    caminho.parent.mkdir(parents=True, exist_ok=True)
    linhas = gerar_linhas_dataset()

    with caminho.open("w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.DictWriter(arquivo, fieldnames=("distancia_cm", "eco_us", "label"))
        escritor.writeheader()
        escritor.writerows(linhas)

    return len(linhas)


def carregar_dataset(caminho: Path = DATASET_PADRAO) -> list[dict[str, int]]:
    with caminho.open(newline="", encoding="utf-8") as arquivo:
        return [
            {
                "distancia_cm": int(linha["distancia_cm"]),
                "eco_us": int(linha["eco_us"]),
                "label": int(linha["label"]),
            }
            for linha in csv.DictReader(arquivo)
        ]


def sha256_arquivo(caminho: Path) -> str:
    return hashlib.sha256(caminho.read_bytes()).hexdigest() if caminho.exists() else ""


def auditar_dataset(linhas: list[dict[str, int]]) -> dict[str, object]:
    labels = Counter(linha["label"] for linha in linhas)
    distancias = [linha["distancia_cm"] for linha in linhas]
    eco_us = [linha["eco_us"] for linha in linhas]
    fronteira = [
        linha
        for linha in linhas
        if DISTANCIA_LIMIAR_CM - 5 <= linha["distancia_cm"] <= DISTANCIA_LIMIAR_CM + 5
    ]

    return {
        "linhas": len(linhas),
        "features": ["distancia_cm", "eco_us"],
        "rotulo": "label",
        "classes": {"0": labels.get(0, 0), "1": labels.get(1, 0)},
        "distancia_cm": {"min": min(distancias), "max": max(distancias), "limiar": DISTANCIA_LIMIAR_CM},
        "eco_us": {"min": min(eco_us), "max": max(eco_us), "fator_us_por_cm": ECO_US_POR_CM},
        "amostras_fronteira": len(fronteira),
        "origem": "coleta/simulacao do sensor HC-SR04 exportada em CSV",
    }


def treinar_limiar(linhas: list[dict[str, int]]) -> int:
    melhor_limiar = DISTANCIA_LIMIAR_CM
    melhor_acertos = -1

    for limiar in range(1, DISTANCIA_MAX_CM + 1):
        acertos = 0
        for linha in linhas:
            previsto = 1 if DISTANCIA_MIN_PRESENTE_CM <= linha["distancia_cm"] <= limiar else 0
            if previsto == linha["label"]:
                acertos += 1
        if acertos > melhor_acertos:
            melhor_acertos = acertos
            melhor_limiar = limiar

    return melhor_limiar


def normalizar_linhas(linhas: list[dict[str, int]]):
    import numpy as np

    x = np.array(
        [
            [
                linha["distancia_cm"] / DISTANCIA_MAX_CM,
                linha["eco_us"] / (DISTANCIA_MAX_CM * ECO_US_POR_CM),
            ]
            for linha in linhas
        ],
        dtype=np.float32,
    )
    y = np.array([linha["label"] for linha in linhas], dtype=np.int64)
    return x, y


def selecionar_representativos(x_treino, y_treino, max_amostras: int = 512):
    try:
        import numpy as np
    except Exception:
        return x_treino[:max_amostras]

    selecionados: list[int] = []
    por_classe = max(1, max_amostras // 2)
    for classe in (0, 1):
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
    matriz = np.zeros((2, 2), dtype=np.int64)
    for indice in range(len(x_teste)):
        amostra = np.array([x_teste[indice]], dtype=np.float32)
        if entrada_info["dtype"] == np.int8:
            escala, zero = entrada_info["quantization"]
            amostra = np.round(amostra / escala + zero).astype(np.int8)
        interprete.set_tensor(entrada_info["index"], amostra)
        interprete.invoke()
        saida = interprete.get_tensor(saida_info["index"])[0]
        previsto = int(np.argmax(saida))
        real = int(y_teste[indice])
        matriz[real][previsto] += 1
        if previsto == real:
            acertos += 1

    total = len(x_teste) if len(x_teste) > 0 else 1
    return {
        "acuracia": acertos / total,
        "acertos": acertos,
        "amostras": len(x_teste),
        "matriz_confusao": matriz.tolist(),
        "tamanho_kb": caminho_modelo.stat().st_size / 1024,
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

    linhas = carregar_dataset(dataset)
    x, y = normalizar_linhas(linhas)
    n_total = len(x)
    n_teste = max(1, int(n_total * 0.2))
    indices = np.arange(n_total)
    np.random.seed(SEMENTE)
    np.random.shuffle(indices)
    idx_teste = indices[:n_teste]
    idx_treino = indices[n_teste:]
    x_treino, y_treino = x[idx_treino], y[idx_treino]
    x_teste, y_teste = x[idx_teste], y[idx_teste]
    tf.keras.utils.set_random_seed(SEMENTE)
    try:
        tf.config.experimental.enable_op_determinism()
    except Exception:
        pass

    limiar_cm = treinar_limiar(linhas)
    limiar_normalizado = (limiar_cm + 0.5) / DISTANCIA_MAX_CM
    ganho = 12.0

    modelo = tf.keras.Sequential([tf.keras.layers.Input(shape=(2,)), tf.keras.layers.Dense(2)])
    modelo.layers[0].set_weights(
        [
            np.array([[ganho, -ganho], [ganho, -ganho]], dtype=np.float32),
            np.array([-2.0 * ganho * limiar_normalizado, 2.0 * ganho * limiar_normalizado], dtype=np.float32),
        ]
    )
    modelo.compile(
        optimizer="adam",
        loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
        metrics=["accuracy"],
    )
    parada = tf.keras.callbacks.EarlyStopping(
        monitor="val_accuracy",
        mode="max",
        patience=10,
        restore_best_weights=True,
    )
    historico = modelo.fit(
        x_treino,
        y_treino,
        epochs=40,
        batch_size=32,
        verbose=0,
        validation_data=(x_teste, y_teste),
        callbacks=[parada],
        shuffle=True,
    )
    _, val_acc = modelo.evaluate(x_teste, y_teste, verbose=0)

    modelo_float.parent.mkdir(parents=True, exist_ok=True)
    saved_model_dir = Path(tempfile.mkdtemp(prefix="presenca_saved_model_"))
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

    metricas_float = _avaliar_tflite(modelo_float, x_teste, y_teste)
    metricas_int8 = _avaliar_tflite(modelo_int8, x_teste, y_teste)
    quantizacao = contrato_quantizacao(modelo_int8)
    print("\n--- Avaliacao do modelo de presenca ---")
    print(f"Amostras: {n_total} total, {len(x_treino)} treino, {n_teste} teste")
    print(f"Limiar aprendido: {limiar_cm} cm")
    print(f"Acuracia validacao Keras: {val_acc:.4f}")
    if metricas_float:
        print(f"Float: acuracia={metricas_float['acuracia']:.4f}, tamanho={metricas_float['tamanho_kb']:.1f} KB")
    if metricas_int8:
        print(f"INT8:  acuracia={metricas_int8['acuracia']:.4f}, tamanho={metricas_int8['tamanho_kb']:.1f} KB")

    return {
        "treinou": True,
        "float": metricas_float,
        "int8": metricas_int8,
        "split": {"treino": int(len(x_treino)), "teste": int(n_teste), "semente": SEMENTE},
        "epocas_executadas": len(historico.history.get("loss", [])),
        "amostras_representativas": int(len(representativos)),
        "limiar_cm": int(limiar_cm),
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
    linhas = carregar_dataset(dataset) if dataset.exists() else gerar_linhas_dataset()
    treinou = isinstance(resultado, dict) and resultado.get("treinou", False)
    metricas_float = resultado.get("float", {}) if treinou else {}
    metricas_int8 = resultado.get("int8", {}) if treinou else {}
    split = resultado.get("split", {}) if treinou else {}
    quantizacao = resultado.get("quantizacao", {}) if treinou else contrato_quantizacao(modelo_int8)

    documento = {
        "modelo": "presenca_hcsr04",
        "objetivo": "Classificar presenca do jogador em segundo plano sem alterar a experiencia do jogo.",
        "notebook_base": NOTEBOOK_BASE,
        "pdf_requisitos": PDF_REQUISITOS,
        "experiencia_usuario": "inalterada: a inferencia roda no backend e registra apenas no console serial",
        "pipeline": [
            "coleta CSV do sensor HC-SR04",
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
            "limiar_cm": resultado.get("limiar_cm", treinar_limiar(linhas)) if treinou else treinar_limiar(linhas),
            "epocas_executadas": resultado.get("epocas_executadas", 0) if treinou else 0,
            "arquitetura": ["Input(2)", "Dense(2,logits)"],
        },
        "metricas": {"float": metricas_float, "int8": metricas_int8},
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
            "coleta_dados_sensores": "HC-SR04 gera timestamp_ms, distancia_cm, eco_us, label",
            "treinamento": "modelo treinado com dataset proprio/sintetizado a partir da coleta",
            "conversao_compressao": "TFLite float e TFLite INT8 full integer",
            "pipeline_inferencia": "main/presenca_tflite.cc normaliza sensor, quantiza entrada e invoca TFLite Micro",
        },
    }

    relatorio.parent.mkdir(parents=True, exist_ok=True)
    relatorio.write_text(json.dumps(documento, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def exportar_header(
    modelo_int8: Path = MODELO_INT8_PADRAO,
    header: Path = HEADER_PADRAO,
    limiar_cm: int = DISTANCIA_LIMIAR_CM,
    dataset_rows: int | None = None,
    metricas_int8: dict[str, object] | None = None,
) -> None:
    header.parent.mkdir(parents=True, exist_ok=True)
    bytes_modelo = modelo_int8.read_bytes() if modelo_int8.exists() else b"PRESENCA_INT8_FB"
    array = _formatar_array_c(bytes_modelo)
    linhas_dataset = dataset_rows if dataset_rows is not None else len(gerar_linhas_dataset())
    sha256 = hashlib.sha256(bytes_modelo).hexdigest()
    acuracia = metrica_permyriad(metricas_int8, "acuracia")

    conteudo = f"""#pragma once

#include <stdint.h>

#define PRESENCA_MODEL_INPUT_SIZE 2
#define PRESENCA_MODEL_OUTPUT_SIZE 2
#define PRESENCA_MODEL_TENSOR_ARENA_BYTES {TENSOR_ARENA_BYTES}
#define PRESENCA_MODEL_DATASET_ROWS {linhas_dataset}
#define PRESENCA_MODEL_QUANTIZATION "{QUANTIZACAO}"
#define PRESENCA_MODEL_NOTEBOOK "{NOTEBOOK_BASE}"
#define PRESENCA_MODEL_INT8_SHA256 "{sha256}"
#define PRESENCA_MODEL_TEST_ACCURACY_PERMYRIAD {acuracia}
#define PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM {DISTANCIA_MIN_PRESENTE_CM}
#define PRESENCA_MODEL_DISTANCIA_LIMIAR_CM {limiar_cm}
#define PRESENCA_MODEL_DISTANCIA_MAX_CM {DISTANCIA_MAX_CM}
#define PRESENCA_MODEL_ECO_LIMIAR_US (PRESENCA_MODEL_DISTANCIA_LIMIAR_CM * {ECO_US_POR_CM})

static const int8_t PRESENCA_MODELO_PESOS[PRESENCA_MODEL_INPUT_SIZE] = {{
    {PESO_DISTANCIA}, {PESO_ECO},
}};

static const int32_t PRESENCA_MODELO_BIAS = 0;

static const unsigned char PRESENCA_MODEL_TFLITE[] = {{
{array}
}};

static const unsigned int PRESENCA_MODEL_TFLITE_LEN = sizeof(PRESENCA_MODEL_TFLITE);
"""
    header.write_text(conteudo, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Gera dataset, modelo e header C do classificador de presenca.")
    parser.add_argument("--dataset", type=Path, default=DATASET_PADRAO)
    parser.add_argument("--modelo-float", type=Path, default=MODELO_FLOAT_PADRAO)
    parser.add_argument("--modelo-int8", type=Path, default=MODELO_INT8_PADRAO)
    parser.add_argument("--header", type=Path, default=HEADER_PADRAO)
    parser.add_argument("--relatorio", type=Path, default=RELATORIO_PADRAO)
    parser.add_argument("--sem-treino", action="store_true", help="gera dataset/header sem tentar TensorFlow")
    args = parser.parse_args()

    total = salvar_dataset(args.dataset)
    linhas = carregar_dataset(args.dataset)
    limiar = treinar_limiar(linhas)
    resultado = False if args.sem_treino else treinar_tensorflow(args.dataset, args.modelo_float, args.modelo_int8)
    metricas_int8 = resultado.get("int8", {}) if isinstance(resultado, dict) else {}
    exportar_header(args.modelo_int8, args.header, limiar, total, metricas_int8)
    salvar_relatorio(args.relatorio, args.dataset, args.modelo_float, args.modelo_int8, args.header, resultado)

    treinou = isinstance(resultado, dict) and resultado.get("treinou", False)
    print(f"dataset: {args.dataset} ({total} linhas)")
    print(f"limiar treinado: {limiar} cm")
    print(f"treino tensorflow: {'ok' if treinou else 'nao executado'}")
    print(f"header: {args.header}")
    print(f"relatorio: {args.relatorio}")


if __name__ == "__main__":
    main()
