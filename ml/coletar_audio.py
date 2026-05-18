#!/usr/bin/env python3
"""Ferramenta de coleta de dataset de audio para os digitos em portugues.

Grava amostras de audio do microfone do PC e organiza por label:
    ml/datasets/audio/0/  -> silencio
    ml/datasets/audio/1/  -> "um"
    ml/datasets/audio/2/  -> "dois"
    ...
    ml/datasets/audio/9/  -> "nove"

Uso:
    python ml/coletar_audio.py

Instale as dependencias antes:
    pip install sounddevice soundfile numpy
"""

from __future__ import annotations

import sys
import time
import threading
from pathlib import Path

try:
    import numpy as np
    import sounddevice as sd
    import soundfile as sf
except ImportError:
    print("Instale: pip install sounddevice soundfile numpy")
    sys.exit(1)

SAMPLE_RATE   = 16000
DURACAO_S     = 1.5       # segundos por amostra
SILENCIO_S    = 0.3       # pausa antes de gravar
DATASET_PASTA = Path(__file__).resolve().parents[1] / "ml" / "datasets" / "audio"

ROTULOS = {
    "0": "silencio",
    "1": "um",
    "2": "dois",
    "3": "tres",
    "4": "quatro",
    "5": "cinco",
    "6": "seis",
    "7": "sete",
    "8": "oito",
    "9": "nove",
}


def contar_amostras() -> dict[str, int]:
    contagem: dict[str, int] = {}
    for label in ROTULOS:
        pasta = DATASET_PASTA / label
        contagem[label] = len(list(pasta.glob("*.wav"))) if pasta.exists() else 0
    return contagem


def gravar_amostra(label: str) -> Path:
    pasta = DATASET_PASTA / label
    pasta.mkdir(parents=True, exist_ok=True)
    n = len(list(pasta.glob("*.wav")))
    caminho = pasta / f"{label}_{n:04d}.wav"

    print(f"  Gravando em {DURACAO_S:.1f}s... ", end="", flush=True)
    time.sleep(SILENCIO_S)
    audio = sd.rec(int(DURACAO_S * SAMPLE_RATE),
                   samplerate=SAMPLE_RATE, channels=1, dtype="int16")
    sd.wait()
    sf.write(str(caminho), audio, SAMPLE_RATE, subtype="PCM_16")
    print(f"salvo em {caminho.name}")
    return caminho


def menu() -> None:
    print("\n" + "=" * 55)
    print("  Coleta de Audio — Digitos PT-BR para o Jogo da Velha")
    print("=" * 55)
    print("  Tecle o DIGITO que quer gravar (0-9)")
    print("  Tecle Q para sair\n")

    contagem = contar_amostras()
    for label, rotulo in ROTULOS.items():
        n = contagem.get(label, 0)
        print(f"  [{label}] '{rotulo:<8}' — {n} amostras")

    print("\n  Recomendado: >= 30 amostras por digito (3 falantes x 10 cada)")
    print("=" * 55)


def main() -> None:
    menu()
    while True:
        try:
            tecla = input("\n  Digito (0-9) ou Q para sair: ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\nSaindo.")
            break

        if tecla == "q":
            print("Coleta encerrada.")
            break

        if tecla not in ROTULOS:
            print("  Tecla invalida. Use 0-9 ou Q.")
            continue

        rotulo = ROTULOS[tecla]
        print(f"\n  FALE '{rotulo}' quando indicado...")
        gravar_amostra(tecla)

        contagem = contar_amostras()
        print(f"  Total '{rotulo}': {contagem[tecla]} amostras")


if __name__ == "__main__":
    main()
