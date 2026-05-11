from pathlib import Path
import importlib.util


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
    assert "TICTACTOE_MODELO_PRIORIDADES" in texto
