from pathlib import Path
import importlib.util
import json

import pytest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pipeline_presenca", ROOT / "ml" / "pipeline_presenca.py")
pipeline = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pipeline)


DISTANCIAS_ROTULADAS = [
    *range(0, 31),
    58,
    59,
    60,
    68,
    69,
    70,
    71,
    240,
    300,
    350,
    351,
    400,
]


@pytest.mark.parametrize("distancia_cm", DISTANCIAS_ROTULADAS)
def test_label_presenca_fronteiras(distancia_cm):
    esperado = 1 if 2 <= distancia_cm < 70 else 0
    assert pipeline.label_presenca(distancia_cm) == esperado


@pytest.mark.parametrize(
    ("eco_us", "distancia_cm"),
    [
        (0, 0),
        (58, 1),
        (116, 2),
        (580, 10),
        (4002, 69),
        (4060, 70),
        (20300, 350),
    ],
)
def test_eco_us_para_distancia_cm(eco_us, distancia_cm):
    assert pipeline.eco_us_para_distancia_cm(eco_us) == distancia_cm


def test_pipeline_presenca_gera_dataset_deterministico_e_balanceado():
    linhas = pipeline.gerar_linhas_dataset()
    labels = {linha["label"] for linha in linhas}

    assert len(linhas) == 1775
    assert labels == {0, 1}
    assert sum(linha["label"] for linha in linhas) > 0
    assert sum(1 for linha in linhas if linha["label"] == 0) > sum(1 for linha in linhas if linha["label"] == 1)


def test_pipeline_presenca_linhas_respeitam_formula_de_label():
    for linha in pipeline.gerar_linhas_dataset():
        esperado = 1 if 2 <= linha["distancia_cm"] < 70 else 0
        assert linha["label"] == esperado
        assert linha["eco_us"] >= 0


def test_pipeline_presenca_treina_limiar_esperado():
    assert pipeline.treinar_limiar(pipeline.gerar_linhas_dataset()) == 69


def test_pipeline_presenca_salva_e_carrega_dataset(tmp_path):
    dataset = tmp_path / "presenca.csv"

    total = pipeline.salvar_dataset(dataset)
    linhas = pipeline.carregar_dataset(dataset)

    assert total == 1775
    assert len(linhas) == total
    assert set(linhas[0]) == {"distancia_cm", "eco_us", "label"}


def test_pipeline_presenca_exporta_header_com_constantes(tmp_path):
    modelo = tmp_path / "modelo.tflite"
    modelo.write_bytes(b"\x20\x00\x00\x00TFL3modelo")
    header = tmp_path / "presenca_model_data.h"

    pipeline.exportar_header(modelo, header, 119)
    texto = header.read_text(encoding="utf-8")

    assert "PRESENCA_MODEL_INPUT_SIZE 2" in texto
    assert "PRESENCA_MODEL_OUTPUT_SIZE 2" in texto
    assert "PRESENCA_MODEL_DATASET_ROWS" in texto
    assert "PRESENCA_MODEL_QUANTIZATION \"full_integer_int8\"" in texto
    assert "PRESENCA_MODEL_INT8_SHA256" in texto
    assert "PRESENCA_MODEL_TEST_ACCURACY_PERMYRIAD" in texto
    assert "PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM 2" in texto
    assert "PRESENCA_MODEL_DISTANCIA_AUSENTE_A_PARTIR_CM 70" in texto
    assert "PRESENCA_MODEL_DISTANCIA_LIMIAR_CM 119" in texto
    assert "PRESENCA_MODEL_TFLITE" in texto


def test_modelo_presenca_int8_real_foi_exportado():
    modelo = ROOT / "ml" / "models" / "presenca_int8.tflite"
    header = ROOT / "main" / "presenca_model_data.h"

    assert modelo.exists()
    assert modelo.stat().st_size > 512
    assert b"TFL3" in modelo.read_bytes()[:16]
    assert b"PRESENCA_INT8_FB" not in header.read_bytes()
    texto = header.read_text(encoding="utf-8")
    assert "0x54, 0x46, 0x4c, 0x33" in texto
    assert "PRESENCA_MODEL_DISTANCIA_MIN_PRESENTE_CM 2" in texto
    assert "PRESENCA_MODEL_DISTANCIA_LIMIAR_CM 69" in texto
    assert "PRESENCA_MODEL_DISTANCIA_AUSENTE_A_PARTIR_CM 70" in texto


def test_relatorio_presenca_real_documenta_pdf_quantizacao_e_sensor():
    relatorio = ROOT / "ml" / "relatorios" / "presenca_training_report.json"
    dados = json.loads(relatorio.read_text(encoding="utf-8"))

    assert dados["notebook_base"] == "codigo/tflite_hello_world_training.ipynb"
    assert dados["pdf_requisitos"] == "codigo/Descrição do projeto final.pdf"
    assert dados["dataset"]["linhas"] == len(pipeline.gerar_linhas_dataset())
    assert dados["dataset"]["features"] == ["distancia_cm", "eco_us"]
    assert dados["dataset"]["distancia_cm"]["min_presente"] == 2
    assert dados["dataset"]["distancia_cm"]["ausente_a_partir"] == 70
    assert dados["dataset"]["distancia_cm"]["limiar"] == 69
    assert dados["treinamento"]["limiar_cm"] == 69
    assert dados["quantizacao"]["tipo"] == "full_integer_int8"
    assert dados["quantizacao"]["contrato_tflite"]["entrada_dtype"].endswith("int8'>")
    assert dados["metricas"]["int8"]["acuracia"] >= 0.95
    assert dados["requisitos_pdf"]["coleta_dados_sensores"].startswith("HC-SR04")
    assert len(dados["artefatos"]["modelo_int8"]["sha256"]) == 64
