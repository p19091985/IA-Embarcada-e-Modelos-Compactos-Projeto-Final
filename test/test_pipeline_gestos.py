from pathlib import Path
import importlib.util


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pipeline_gestos", ROOT / "ml" / "pipeline_gestos.py")
pipeline = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pipeline)


def test_cria_janelas_com_label_majoritario():
    amostras = []
    for i in range(pipeline.TAMANHO_JANELA):
        amostras.append(
            {
                "timestamp_ms": i * 20,
                "ax": i * 256,
                "ay": 0,
                "az": 16384,
                "label": 1 if i >= 4 else 0,
            }
        )

    janelas = pipeline.criar_janelas(amostras, tamanho=pipeline.TAMANHO_JANELA, passo=4)

    assert len(janelas) == 1
    assert len(janelas[0]) == pipeline.ENTRADA_TINYML + 1
    assert janelas[0][0] == 0
    assert janelas[0][3] == 1
    assert janelas[0][-1] == 1


def test_carrega_csv_bruto(tmp_path):
    csv_path = tmp_path / "gestos_raw.csv"
    csv_path.write_text(
        "timestamp_ms,ax,ay,az,label\n"
        "0,1,2,3,0\n"
        "20,4,5,6,1\n",
        encoding="utf-8",
    )

    amostras = pipeline.carregar_csv_bruto(csv_path)

    assert amostras[0]["ax"] == 1
    assert amostras[1]["label"] == 1


def test_exporta_header_sem_tensorflow(tmp_path):
    header = tmp_path / "gesto_model_data.h"

    pipeline.exportar_header(tmp_path / "ausente.tflite", header)

    texto = header.read_text(encoding="utf-8")
    assert "GESTO_MODEL_WINDOW_SIZE 16" in texto
    assert "GESTO_MODEL_LIMIAR_AMPLITUDE" in texto
