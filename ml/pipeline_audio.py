#!/usr/bin/env python3
"""Pipeline de treinamento do classificador de audio (digitos PT-BR).

Etapas cobertas pelo projeto final:
  1. Coleta: ml/datasets/audio/{0-9}/*.wav  (feita com coletar_audio.py)
  2. Treinamento: MLP com features Goertzel identicas ao firmware C
  3. Conversao: TFLite float e TFLite INT8 full integer
  4. Exportacao: main/audio_model_data.h  com modelo + normalizacao

Uso:
    pip install tensorflow librosa soundfile numpy scipy
    python ml/pipeline_audio.py
"""

from __future__ import annotations

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np

ROOT           = Path(__file__).resolve().parents[1]
DATASET_PASTA  = ROOT / "ml" / "datasets" / "audio"
MODELO_FLOAT   = ROOT / "ml" / "models" / "audio_float.tflite"
MODELO_INT8    = ROOT / "ml" / "models" / "audio_int8.tflite"
HEADER_PADRAO  = ROOT / "main" / "audio_model_data.h"
RELATORIO      = ROOT / "ml" / "relatorios" / "audio_training_report.json"

SAMPLE_RATE    = 16000
N_SAMPLES      = 16000   # 1 segundo
N_FRAMES       = 5
N_BANDS        = 8
N_FEATURES     = N_FRAMES * N_BANDS   # 40
N_CLASSES      = 10                    # 0=silencio, 1-9=digitos
SEMENTE        = 42
TENSOR_ARENA   = 12288
CONF_THRESHOLD = 15

FREQ_BANDS = [250.0, 400.0, 600.0, 900.0, 1300.0, 1900.0, 2700.0, 3800.0]

ROTULOS = {
    "0": "silencio",
    "1": "um",   "2": "dois",   "3": "tres",  "4": "quatro", "5": "cinco",
    "6": "seis", "7": "sete",   "8": "oito",  "9": "nove",
}


# ---------------------------------------------------------------------------
# Feature extraction — identica ao firmware C (microfone_i2s.c)
# ---------------------------------------------------------------------------

def goertzel_energia(samples: np.ndarray, freq: float, sr: int = SAMPLE_RATE) -> float:
    """Energia espectral na frequencia `freq` via algoritmo de Goertzel."""
    n = len(samples)
    k = round(n * freq / sr)
    omega = 2.0 * np.pi * k / n
    coeff = 2.0 * np.cos(omega)
    s, s_prev = 0.0, 0.0
    for x in samples:
        s_novo = float(x) + coeff * s - s_prev
        s_prev = s
        s = s_novo
    return s_prev**2 + s**2 - coeff * s_prev * s


def extrair_features(audio: np.ndarray, sr: int = SAMPLE_RATE) -> np.ndarray:
    """Extrai N_FEATURES (40) features Goertzel em log-energia.
    Requer audio int16 ou float32 normalizado em [-1, 1].
    """
    if audio.dtype != np.float64 and audio.dtype != np.float32:
        audio = audio.astype(np.float64) / 32768.0

    total = len(audio)
    frame_size = total // N_FRAMES
    features = []

    for f in range(N_FRAMES):
        frame = audio[f * frame_size:(f + 1) * frame_size]
        for freq in FREQ_BANDS:
            energia = goertzel_energia(frame, freq, sr)
            features.append(np.log(energia + 1e-8))

    return np.array(features, dtype=np.float32)


# ---------------------------------------------------------------------------
# Dataset
# ---------------------------------------------------------------------------

def carregar_dataset(pasta: Path = DATASET_PASTA) -> tuple[np.ndarray, np.ndarray]:
    try:
        import soundfile as sf
    except ImportError:
        raise ImportError("pip install soundfile")

    X, y = [], []
    for label_str in sorted(ROTULOS.keys()):
        label = int(label_str)
        label_pasta = pasta / label_str
        if not label_pasta.exists():
            continue
        wavs = sorted(label_pasta.glob("*.wav"))
        for wav in wavs:
            audio, sr = sf.read(str(wav), dtype="int16", always_2d=False)
            if sr != SAMPLE_RATE:
                try:
                    import librosa
                    audio = librosa.resample(audio.astype(np.float32), orig_sr=sr, target_sr=SAMPLE_RATE)
                    audio = (audio * 32768).astype(np.int16)
                except ImportError:
                    print(f"  [aviso] {wav.name}: sr={sr}, esperado {SAMPLE_RATE}. Instale librosa para resampling.")
                    continue

            # Padeia ou corta para N_SAMPLES
            if len(audio) < N_SAMPLES:
                audio = np.pad(audio, (0, N_SAMPLES - len(audio)))
            else:
                audio = audio[:N_SAMPLES]

            X.append(extrair_features(audio))
            y.append(label)

    if not X:
        raise ValueError(
            f"Nenhuma amostra encontrada em {pasta}. "
            "Execute: python ml/coletar_audio.py"
        )

    return np.array(X, dtype=np.float32), np.array(y, dtype=np.int64)


def auditar_dataset(y: np.ndarray) -> dict:
    classes = {str(i): int(np.sum(y == i)) for i in range(N_CLASSES)}
    return {
        "total": len(y),
        "classes": classes,
        "features": N_FEATURES,
        "rotulos": ROTULOS,
        "freq_bands_hz": FREQ_BANDS,
        "n_frames": N_FRAMES,
        "n_bands": N_BANDS,
    }


# ---------------------------------------------------------------------------
# Treinamento
# ---------------------------------------------------------------------------

def treinar(X: np.ndarray, y: np.ndarray) -> tuple:
    """Treina MLP, normaliza dados e retorna (modelo, mean, std)."""
    import tensorflow as tf

    np.random.seed(SEMENTE)
    tf.keras.utils.set_random_seed(SEMENTE)

    idx = np.arange(len(X))
    np.random.shuffle(idx)
    n_teste = max(1, int(len(X) * 0.2))
    idx_teste, idx_treino = idx[:n_teste], idx[n_teste:]
    X_treino, y_treino = X[idx_treino], y[idx_treino]
    X_teste,  y_teste  = X[idx_teste],  y[idx_teste]

    mean = X_treino.mean(axis=0)
    std  = X_treino.std(axis=0) + 1e-8
    X_treino_n = (X_treino - mean) / std
    X_teste_n  = (X_teste  - mean) / std

    modelo = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(N_FEATURES,)),
        tf.keras.layers.Dense(64, activation="relu"),
        tf.keras.layers.Dense(32, activation="relu"),
        tf.keras.layers.Dense(N_CLASSES),
    ])
    modelo.compile(
        optimizer="adam",
        loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
        metrics=["accuracy"],
    )
    parada = tf.keras.callbacks.EarlyStopping(
        monitor="val_accuracy", patience=15, restore_best_weights=True
    )
    historico = modelo.fit(
        X_treino_n, y_treino,
        epochs=100, batch_size=16, verbose=0,
        validation_data=(X_teste_n, y_teste),
        callbacks=[parada],
        shuffle=True,
    )
    _, acc = modelo.evaluate(X_teste_n, y_teste, verbose=0)
    epocas = len(historico.history["loss"])
    print(f"  Acuracia validacao: {acc:.4f} ({epocas} epocas)")
    return modelo, mean, std, X_treino, X_treino_n, y_treino, X_teste_n, y_teste


def converter_tflite(modelo, X_rep_n: np.ndarray) -> tuple[bytes, bytes]:
    import tensorflow as tf

    saved = Path(tempfile.mkdtemp(prefix="audio_saved_"))
    modelo.export(str(saved))

    # Float
    conv = tf.lite.TFLiteConverter.from_saved_model(str(saved))
    float_bytes = conv.convert()

    # INT8 full integer
    def representativo():
        for amostra in X_rep_n[:256]:
            yield [np.array([amostra], dtype=np.float32)]

    conv = tf.lite.TFLiteConverter.from_saved_model(str(saved))
    conv.optimizations            = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset   = representativo
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type     = tf.int8
    conv.inference_output_type    = tf.int8
    int8_bytes = conv.convert()

    return float_bytes, int8_bytes


def avaliar_tflite(tflite_bytes: bytes, X_teste_n: np.ndarray, y_teste: np.ndarray) -> dict:
    import tensorflow as tf

    interp = tf.lite.Interpreter(model_content=tflite_bytes)
    interp.allocate_tensors()
    entrada_info = interp.get_input_details()[0]
    saida_info   = interp.get_output_details()[0]

    acertos = 0
    matriz = np.zeros((N_CLASSES, N_CLASSES), dtype=np.int64)
    for i in range(len(X_teste_n)):
        amostra = np.array([X_teste_n[i]], dtype=np.float32)
        if entrada_info["dtype"] == np.int8:
            s, z = entrada_info["quantization"]
            amostra = np.round(amostra / s + z).astype(np.int8)
        interp.set_tensor(entrada_info["index"], amostra)
        interp.invoke()
        saida = interp.get_tensor(saida_info["index"])[0]
        prev = int(np.argmax(saida))
        real = int(y_teste[i])
        matriz[real][prev] += 1
        if prev == real:
            acertos += 1

    total = len(y_teste) or 1
    return {
        "acuracia":        acertos / total,
        "acertos":         acertos,
        "amostras_teste":  int(total),
        "tamanho_kb":      len(tflite_bytes) / 1024,
        "matriz_confusao": matriz.tolist(),
    }


# ---------------------------------------------------------------------------
# Exportacao do header C
# ---------------------------------------------------------------------------

def _array_c(dados: bytes) -> str:
    linhas = []
    for i in range(0, len(dados), 12):
        chunk = dados[i:i + 12]
        linhas.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    return "\n".join(linhas)


def _array_float_c(vals: np.ndarray, nome: str) -> str:
    partes = []
    for i, v in enumerate(vals):
        if i % 8 == 0:
            partes.append("\n    ")
        partes.append(f"{v:.8f}f, ")
    return f"static const float {nome}[AUDIO_MODEL_N_FEATURES] = {{{' '.join(partes)}\n}};"


def exportar_header(
    int8_bytes: bytes,
    mean: np.ndarray,
    std: np.ndarray,
    metricas: dict,
    header: Path = HEADER_PADRAO,
) -> None:
    sha256 = hashlib.sha256(int8_bytes).hexdigest()
    acc_pm = int(round(metricas.get("acuracia", 0) * 10000))
    array  = _array_c(int8_bytes)

    mean_vals = ", ".join(f"{v:.8f}f" for v in mean)
    std_vals  = ", ".join(f"{v:.8f}f" for v in std)

    conteudo = f"""#pragma once

/*
 * Gerado por ml/pipeline_audio.py — NAO EDITE MANUALMENTE.
 * Modelo: classificador de digitos PT-BR (0=silencio, 1-9)
 * Acuracia INT8: {metricas.get('acuracia', 0):.4f}  ({acc_pm} permyriad)
 * Tamanho INT8:  {metricas.get('tamanho_kb', 0):.1f} KB
 * SHA-256:       {sha256}
 */

#include <stdint.h>

#define AUDIO_MODEL_N_FEATURES           {N_FEATURES}
#define AUDIO_MODEL_N_CLASSES            {N_CLASSES}
#define AUDIO_MODEL_TENSOR_ARENA_BYTES   {TENSOR_ARENA}
#define AUDIO_MODEL_CONFIDENCE_THRESHOLD {CONF_THRESHOLD}
#define AUDIO_MODEL_TFLITE_LEN           {len(int8_bytes)}
#define AUDIO_MODEL_INT8_SHA256          "{sha256}"
#define AUDIO_MODEL_TEST_ACCURACY_PERMYRIAD {acc_pm}

/* Estatisticas de normalizacao calculadas no treino */
static const float AUDIO_FEATURE_MEAN[AUDIO_MODEL_N_FEATURES] = {{
    {mean_vals}
}};

static const float AUDIO_FEATURE_STD[AUDIO_MODEL_N_FEATURES] = {{
    {std_vals}
}};

static const unsigned char AUDIO_MODEL_TFLITE[] = {{
{array}
}};
"""
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(conteudo, encoding="utf-8")


# ---------------------------------------------------------------------------
# Relatorio
# ---------------------------------------------------------------------------

def salvar_relatorio(
    y: np.ndarray,
    metricas_float: dict,
    metricas_int8: dict,
    epocas: int,
    caminho: Path = RELATORIO,
) -> None:
    doc = {
        "modelo":    "audio_digitos_ptbr",
        "objetivo":  "Classificar digitos falados em PT-BR (1-9) para selecionar posicao no jogo.",
        "pipeline": [
            "coleta de audio com coletar_audio.py",
            "features Goertzel 40D (8 bandas x 5 frames)",
            "treinamento MLP Keras",
            "conversao TFLite float e INT8 full integer",
            "exportacao do header C com modelo e normalizacao",
            "inferencia TFLite Micro no ESP32-S3 (audio_classificador.cc)",
        ],
        "sensor":     "INMP441 I2S — GPIO WS=21, SCK=38, SD=39",
        "sample_rate": SAMPLE_RATE,
        "features": {
            "n":         N_FEATURES,
            "tipo":      "Goertzel log-energia",
            "freq_bands_hz": FREQ_BANDS,
            "n_frames":  N_FRAMES,
            "n_bands":   N_BANDS,
        },
        "arquitetura": [f"Input({N_FEATURES})", "Dense(64,relu)", "Dense(32,relu)", f"Dense({N_CLASSES},logits)"],
        "dataset":      auditar_dataset(y),
        "treinamento":  {"epocas": epocas, "semente": SEMENTE, "optimizer": "adam"},
        "metricas":     {"float": metricas_float, "int8": metricas_int8},
        "requisitos_pdf": {
            "coleta_dados_sensores":  "INMP441 captura audio a 16 kHz via I2S",
            "treinamento":            "MLP treinado com dataset proprio de digitos PT-BR",
            "conversao_compressao":   "TFLite INT8 full integer",
            "pipeline_inferencia":    "audio_classificador.cc: captura -> Goertzel -> norm -> TFLite Micro",
        },
    }
    caminho.parent.mkdir(parents=True, exist_ok=True)
    caminho.write_text(json.dumps(doc, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description="Treina classificador de audio e exporta header C.")
    parser.add_argument("--dataset", type=Path, default=DATASET_PASTA)
    parser.add_argument("--header",  type=Path, default=HEADER_PADRAO)
    parser.add_argument("--relatorio", type=Path, default=RELATORIO)
    args = parser.parse_args()

    print("\n=== Pipeline Audio — Digitos PT-BR ===")
    print(f"Dataset: {args.dataset}")

    try:
        X, y = carregar_dataset(args.dataset)
    except ValueError as e:
        print(f"\nErro: {e}")
        return

    print(f"Amostras carregadas: {len(X)} ({np.bincount(y.astype(int)).tolist()})")
    print("Treinando modelo...")

    modelo, mean, std, X_treino, X_treino_n, y_treino, X_teste_n, y_teste = treinar(X, y)

    print("Convertendo para TFLite...")
    float_bytes, int8_bytes = converter_tflite(modelo, X_treino_n)

    MODELO_FLOAT.parent.mkdir(parents=True, exist_ok=True)
    MODELO_FLOAT.write_bytes(float_bytes)
    MODELO_INT8.write_bytes(int8_bytes)

    metricas_float = avaliar_tflite(float_bytes, X_teste_n, y_teste)
    metricas_int8  = avaliar_tflite(int8_bytes,  X_teste_n, y_teste)

    print(f"Float: acc={metricas_float['acuracia']:.4f}  {metricas_float['tamanho_kb']:.1f} KB")
    print(f"INT8:  acc={metricas_int8['acuracia']:.4f}  {metricas_int8['tamanho_kb']:.1f} KB")

    exportar_header(int8_bytes, mean, std, metricas_int8, args.header)
    salvar_relatorio(y, metricas_float, metricas_int8,
                     epocas=0, caminho=args.relatorio)

    print(f"\nHeader exportado: {args.header}")
    print(f"Relatorio: {args.relatorio}")
    print("\nRECOMPILE o firmware: idf.py build")


if __name__ == "__main__":
    main()
