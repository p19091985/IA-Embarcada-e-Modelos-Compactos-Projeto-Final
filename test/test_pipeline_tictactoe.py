from pathlib import Path
import importlib.util
import json

import pytest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pipeline_tictactoe", ROOT / "ml" / "pipeline_tictactoe.py")
pipeline = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pipeline)


def test_melhor_jogada_vencedora():
    tabuleiro = (
        pipeline.X_COMPUTADOR,
        pipeline.X_COMPUTADOR,
        pipeline.VAZIO,
        pipeline.VAZIO,
        pipeline.O_JOGADOR,
        pipeline.VAZIO,
        pipeline.O_JOGADOR,
        pipeline.VAZIO,
        pipeline.VAZIO,
    )

    assert pipeline.melhor_jogada(tabuleiro) == 2


def test_politica_otima_aceita_multiplas_jogadas_equivalentes():
    tabuleiro = (
        pipeline.O_JOGADOR,
        pipeline.VAZIO,
        pipeline.VAZIO,
        pipeline.VAZIO,
        pipeline.X_COMPUTADOR,
        pipeline.VAZIO,
        pipeline.VAZIO,
        pipeline.VAZIO,
        pipeline.O_JOGADOR,
    )

    politica = pipeline.politica_otima(tabuleiro)
    otimas = [indice for indice, peso in enumerate(politica) if peso > 0]

    assert pytest.approx(sum(politica), rel=1e-6) == 1.0
    assert len(otimas) >= 1
    assert all(pipeline.jogada_otima(tabuleiro, indice) for indice in otimas)


def test_dataset_tem_formato_e_jogadas_validas():
    linhas = pipeline.gerar_linhas_dataset()
    vistos = set()

    assert linhas
    assert len(linhas[0]) == 10
    for linha in linhas:
        tabuleiro = linha[:9]
        jogada = linha[9]
        assert pipeline.tabuleiro_valido(tabuleiro)
        assert tabuleiro.count(pipeline.O_JOGADOR) == tabuleiro.count(pipeline.X_COMPUTADOR) + 1
        assert tabuleiro[jogada] == pipeline.VAZIO
        assert tuple(tabuleiro) not in vistos
        vistos.add(tuple(tabuleiro))


def test_exporta_header_sem_tensorflow(tmp_path):
    header = tmp_path / "tictactoe_model_data.h"

    pipeline.exportar_header(tmp_path / "ausente.tflite", header)

    texto = header.read_text(encoding="utf-8")
    assert "TICTACTOE_MODEL_INPUT_SIZE 9" in texto
    assert "TICTACTOE_MODEL_TFLITE" in texto
    assert "TICTACTOE_MODEL_DATASET_ROWS" in texto
    assert "TICTACTOE_MODEL_QUANTIZATION \"full_integer_int8\"" in texto
    assert "TICTACTOE_MODEL_INT8_SHA256" in texto
    assert "TICTACTOE_MODEL_OPTIMAL_MOVE_PERMYRIAD" in texto
    assert "TICTACTOE_MODELO_PRIORIDADES" not in texto


@pytest.mark.parametrize(
    ("tabuleiro", "valido"),
    [
        ((0, 0, 0, 0, 0, 0, 0, 0, 0), True),
        ((-1, 0, 0, 0, 0, 0, 0, 0, 0), True),
        ((1, 0, 0, 0, 0, 0, 0, 0, 0), False),
        ((-1, 1, -1, 1, -1, 0, 0, 0, 0), True),
        ((1, 1, 1, -1, -1, 0, 0, 0, 0), False),
        ((-1, -1, -1, 1, 1, 0, 0, 0, 0), True),
        ((1, 1, 1, -1, -1, -1, 0, 0, 0), False),
        ((1, -1, 1, -1, 1, -1, -1, 1, -1), True),
    ],
)
def test_tabuleiro_valido_casos_de_regra(tabuleiro, valido):
    assert pipeline.tabuleiro_valido(tabuleiro) is valido


def test_relatorio_tictactoe_real_documenta_quantizacao_e_requisitos():
    relatorio = ROOT / "ml" / "relatorios" / "tictactoe_training_report.json"
    dados = json.loads(relatorio.read_text(encoding="utf-8"))

    assert dados["notebook_base"] == "codigo/tflite_hello_world_training.ipynb"
    assert dados["pdf_requisitos"] == "codigo/Descrição do projeto final.pdf"
    assert dados["dataset"]["linhas"] == len(pipeline.gerar_linhas_dataset())
    assert dados["treinamento"]["alvo"] == "politica multi-alvo com todas as jogadas minimax otimas"
    assert dados["quantizacao"]["tipo"] == "full_integer_int8"
    assert dados["quantizacao"]["contrato_tflite"]["entrada_dtype"].endswith("int8'>")
    assert dados["metricas"]["int8"]["jogada_otima"] >= 0.90
    assert len(dados["artefatos"]["modelo_int8"]["sha256"]) == 64
