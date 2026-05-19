from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]


ARQUIVOS_OBRIGATORIOS = [
    "codigo/Descrição do projeto final.pdf",
    "codigo/tflite_hello_world_training.ipynb",
    "main/ia_tflite.cc",
    "main/tictactoe_model_data.h",
    "main/presenca_tflite.cc",
    "main/presenca_model_data.h",
    "main/hcsr04.c",
    "main/ldr.c",
    "ml/pipeline_tictactoe.py",
    "ml/pipeline_presenca.py",
    "ml/models/tictactoe_int8.tflite",
    "ml/models/presenca_int8.tflite",
    "ml/relatorios/tictactoe_training_report.json",
    "ml/relatorios/presenca_training_report.json",
    "ml/relatorios/README.md",
    "ml/datasets/dataset_tictactoe.csv",
    "ml/datasets/presenca_hcsr04.csv",
    "test/test_ia_tflite.c",
    "test/test_presenca_tflite.c",
]


@pytest.mark.parametrize("relativo", ARQUIVOS_OBRIGATORIOS)
def test_arquivos_obrigatorios_do_projeto_existem(relativo):
    caminho = ROOT / relativo
    assert caminho.exists(), relativo
    assert caminho.stat().st_size > 0, relativo


TRECHOS_OBRIGATORIOS = [
    ("main/main.c", '#include "presenca_tflite.h"'),
    ("main/main.c", "atualizar_presenca_ambiente();"),
    ("main/main.c", "Presenca HC-SR04"),
    ("main/main.c", "D=Creditos"),
    ("main/main.c", "coletar_dados_hcsr04"),
    ("main/main.c", 'LCD_AUTORES_TEXTO "Autores: Janiel - Joao Sanmartin - Patrik"'),
    ("main/main.c", "atualizar_lcd_status_ia(false);"),
    ("main/main.c", "lcd1602_formatar_janela_scroll"),
    ("main/main.c", "Janiel - Joao"),
    ("main/main.c", "mostrar_hello_world_treinamento();"),
    ("main/main.c", "Projeto Final: Jogo da Velha com IA (ESP32-S3)"),
    ("main/main.c", "Modelos Embarcados Ativos:"),
    ("main/main.c", "Taxa de Jogada Otima"),
    ("main/main.c", "Inferencia TFLite jogo: posicao=%d score_int8=%d"),
    ("main/main.c", "[i] Monitoramento do OLED ativado"),
    ("main/main.c", "espelhar_oled_console(origem, linhas, quantidade);"),
    ("main/main.c", "=== Tela OLED: %s ==="),
    ("main/CMakeLists.txt", '"presenca_tflite.cc"'),
    ("test/main/CMakeLists.txt", "../test_presenca_tflite.c"),
    ("test/main/CMakeLists.txt", "${PROJECT_ROOT}/main/presenca_tflite.cc"),
    ("test/main/test_runner.c", "test_presenca_tflite_inicia_com_modelo_int8_real"),
    ("iniciar.sh", "test/test_pipeline_presenca.py"),
    ("iniciar.bat", "test\\test_pipeline_presenca.py"),
    ("README.md", "classificador de presença"),
    ("README.md", "O display LCD1602 mostra em tempo real"),
    ("README.md", "Keras em Python"),
    ("README.md", "ml/relatorios/"),
    ("README.md", "todas as jogadas ótimas de um algoritmo minimax offline"),
    ("README.md", "espelhar tudo que vai pro OLED direto no console"),
    ("docs/projeto.md", "coleta de dados de sensor"),
    ("docs/projeto.md", "conversao e compressao"),
    ("docs/projeto.md", "pipeline de inferencia"),
    ("docs/projeto.md", "Janiel - Joao Sanmartin - Patrik"),
    ("docs/projeto.md", "linha 2 rola"),
    ("docs/projeto.md", "TensorFlow Lite Micro Hello World"),
    ("docs/projeto.md", "matriz de confusao"),
    ("docs/projeto.md", "full_integer_int8"),
    ("docs/projeto.md", "Todas as telas do OLED tambem sao espelhadas no console"),
    ("ml/models/README.md", "presenca_int8.tflite"),
    ("ml/models/README.md", "SHA-256"),
    ("ml/datasets/README.md", "presenca_hcsr04.csv"),
    ("ml/datasets/README.md", "politica multi-alvo"),
    ("ml/relatorios/README.md", "contrato de quantizacao"),
    ("test/test_formatacao_telas.c", "test_lcd1602_scroll_autores_move_para_esquerda"),
    ("diagram.json", "coleta CSV opcional"),
    ("diagram.json", "unico algoritmo de IA"),
]


@pytest.mark.parametrize(("relativo", "trecho"), TRECHOS_OBRIGATORIOS)
def test_trechos_obrigatorios_estao_documentados_ou_ligados(relativo, trecho):
    texto = (ROOT / relativo).read_text(encoding="utf-8")
    assert trecho in texto


@pytest.mark.parametrize(
    "trecho_antigo",
    [
        "IA_MCTS",
        "IA_MINIMAX_FALLBACK",
        "TICTACTOE_MODELO_PRIORIDADES",
        "pipeline_gestos.py",
        "gesto_tflite",
    ],
)
def test_algoritmos_e_fluxos_antigos_nao_voltaram_ao_backend(trecho_antigo):
    arquivos = [
        ROOT / "main" / "main.c",
        ROOT / "main" / "ia_tflite.cc",
        ROOT / "main" / "ia_jogo_da_velha.h",
        ROOT / "test" / "main" / "test_runner.c",
    ]
    conteudo = "\n".join(arquivo.read_text(encoding="utf-8") for arquivo in arquivos)
    assert trecho_antigo not in conteudo


def test_oled_tem_um_unico_ponto_de_saida_com_espelho_no_console():
    texto = (ROOT / "main" / "main.c").read_text(encoding="utf-8")

    assert texto.count("ssd1306_mostrar_linhas(") == 1
    assert "static void exibir_oled_linhas" in texto
    assert "static void espelhar_oled_console" in texto
    assert texto.count("exibir_oled_linhas(") >= 8


def test_hello_world_e_a_primeira_acao_do_app_main():
    texto = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
    inicio = texto.index("void app_main(void)")
    corpo = texto[inicio : texto.index("ssd1306_config_t config_oled", inicio)]

    assert "mostrar_hello_world_treinamento();" in corpo
    assert corpo.index("mostrar_hello_world_treinamento();") < corpo.index("mostrar_manual_serial();")
