from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]


def assert_nao_depende_de_documentacao_auxiliar(caminho):
    caminho_resolvido = caminho.resolve()
    pasta_docs = (ROOT / "docs").resolve()

    assert caminho_resolvido.name.lower() != "readme.md", caminho
    assert caminho_resolvido != pasta_docs
    assert pasta_docs not in caminho_resolvido.parents, caminho


ARQUIVOS_OBRIGATORIOS = [
    "codigo/Descrição do projeto final.pdf",
    "codigo/tflite_hello_world_training.ipynb",
    "main/ia_tflite.cc",
    "main/tictactoe_model_data.h",
    "main/jogo_interface.c",
    "main/jogo_interface.h",
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
    "ml/datasets/dataset_tictactoe.csv",
    "ml/datasets/presenca_hcsr04.csv",
    "test/test_ia_tflite.c",
    "test/test_jogo_interface.c",
    "test/test_presenca_tflite.c",
]


@pytest.mark.parametrize("relativo", ARQUIVOS_OBRIGATORIOS)
def test_arquivos_obrigatorios_do_projeto_existem(relativo):
    caminho = ROOT / relativo
    assert_nao_depende_de_documentacao_auxiliar(caminho)
    assert caminho.exists(), relativo
    assert caminho.stat().st_size > 0, relativo


TRECHOS_OBRIGATORIOS = [
    ("main/main.c", '#include "presenca_tflite.h"'),
    ("main/main.c", "atualizar_presenca_ambiente();"),
    ("main/main.c", "Presenca HC-SR04"),
    ("main/main.c", "D=Creditos"),
    ("main/main.c", "iniciar_task_presenca();"),
    ("main/main.c", "LCD_PRESENCA_ENDERECO 0x26"),
    ("main/main.c", "LCD_ESTATISTICAS_ENDERECO 0x25"),
    ("main/lcd1602_i2c.c", "lcd_obter_barramento_compartilhado"),
    ("main/lcd1602_i2c.c", "xSemaphoreCreateMutex"),
    ("main/main.c", 'LCD_AUTORES_TEXTO "Autores: Janiel, Joao e Patrik"'),
    ("main/main.c", "atualizar_lcd_status_ia(false);"),
    ("main/main.c", "atualizar_lcd_estatisticas(false);"),
    ("main/main.c", "lcd1602_formatar_janela_scroll"),
    ("main/main.c", "Janiel, Joao"),
    ("main/main.c", "mostrar_hello_world_treinamento();"),
    ("main/main.c", "Projeto Final: Jogo da Velha com IA (ESP32-S3)"),
    ("main/main.c", "Modelos Embarcados Ativos:"),
    ("main/main.c", "Taxa de Jogada Otima"),
    ("main/main.c", "Inferencia TFLite jogo: posicao=%d score_int8=%d"),
    ("main/main.c", "[i] Monitoramento do OLED ativado"),
    ("main/main.c", "espelhar_oled_console(origem, linhas, quantidade);"),
    ("main/main.c", "=== Tela OLED: %s ==="),
    ("main/main.c", "tecla_liberada_para_interacao"),
    ("main/main.c", "Tecla ignorada: jogador ausente."),
    ("main/main.c", "Tecla ignorada na partida: jogador ausente."),
    ("main/jogo_interface.c", "jogo_interacao_tecla_liberada_por_presenca"),
    ("main/jogo_interface.c", "jogo_interacao_liberada_por_presenca"),
    ("main/main.c", "jogo_interface_formatar_estatisticas_lcd("),
    ("main/CMakeLists.txt", '"jogo_interface.c"'),
    ("main/jogo_interface.c", "media_decimos"),
    ("main/jogo_interface.c", "Jgd:%02u Med:%u.%us"),
    ("test/main/CMakeLists.txt", "../test_jogo_interface.c"),
    ("test/main/test_runner.c", "test_interacao_bloqueia_teclado_quando_jogador_ausente"),
    ("test/main/test_runner.c", "test_interacao_ignora_todos_os_botoes_quando_jogador_ausente"),
    ("test/test_jogo_interface.c", "Jgd:09 Med:0.7s"),
    ("test/test_jogo_interface.c", "'*'"),
    ("test/test_jogo_interface.c", "'#'"),
    ("main/CMakeLists.txt", '"presenca_tflite.cc"'),
    ("test/main/CMakeLists.txt", "../test_presenca_tflite.c"),
    ("test/main/CMakeLists.txt", "${PROJECT_ROOT}/main/presenca_tflite.cc"),
    ("test/main/test_runner.c", "test_presenca_tflite_inicia_com_modelo_int8_real"),
    ("iniciar.sh", "test/test_pipeline_presenca.py"),
    ("iniciar.bat", "test\\test_pipeline_presenca.py"),
    ("test/test_formatacao_telas.c", "test_lcd1602_scroll_autores_move_para_esquerda"),
]


@pytest.mark.parametrize(("relativo", "trecho"), TRECHOS_OBRIGATORIOS)
def test_trechos_obrigatorios_estao_documentados_ou_ligados(relativo, trecho):
    caminho = ROOT / relativo
    assert_nao_depende_de_documentacao_auxiliar(caminho)
    texto = caminho.read_text(encoding="utf-8")
    assert trecho in texto


@pytest.mark.parametrize(
    "trecho_antigo",
    [
        "IA_MCTS",
        "IA_MINIMAX_FALLBACK",
        "TICTACTOE_MODELO_PRIORIDADES",
        "pipeline_gestos.py",
        "gesto_tflite",
        "coletar_dados_hcsr04",
        "COLETA_HCSR04",
    ],
)
def test_algoritmos_e_fluxos_antigos_nao_voltaram_ao_backend(trecho_antigo):
    arquivos = [
        ROOT / "main" / "main.c",
        ROOT / "main" / "ia_tflite.cc",
        ROOT / "main" / "ia_jogo_da_velha.h",
        ROOT / "test" / "main" / "test_runner.c",
    ]
    for arquivo in arquivos:
        assert_nao_depende_de_documentacao_auxiliar(arquivo)
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


def test_presenca_bloqueia_tecla_antes_de_emitir_buzzer():
    texto = (ROOT / "main" / "main.c").read_text(encoding="utf-8")

    inicio_menu = texto.index("while (true) {", texto.index("mostrar_menu();"))
    trecho_menu = texto[inicio_menu : texto.index("switch (tecla)", inicio_menu)]
    assert trecho_menu.index("tecla_liberada_para_interacao(tecla)") < trecho_menu.index("buzzer_som_tecla();")

    inicio_partida = texto.index("static bool aguardar_jogada_teclado")
    trecho_partida = texto[inicio_partida : texto.index("if (tecla == '*')", inicio_partida)]
    assert trecho_partida.index("tecla_liberada_para_interacao(tecla)") < trecho_partida.index("buzzer_som_tecla();")


def test_falha_do_hcsr04_nao_mantem_presenca_antiga():
    texto = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
    teste_embarcado = (ROOT / "test" / "test_presenca_tflite.c").read_text(encoding="utf-8")

    inicio = texto.index("esp_err_t erro = hcsr04_ler_distancia")
    trecho = texto[inicio : texto.index("return;", texto.index("Sem eco valido", inicio))]

    assert "presenca_tflite_avaliar_leitura_hcsr04(&modelo_presenca, erro, distancia_cm, eco_us)" in trecho
    assert "if (erro != ESP_OK)" in trecho
    assert "atualizar_estado_presenca(" in trecho
    assert "resultado_presenca.presente" in trecho
    assert "Sem eco valido" in trecho
    assert "test_presenca_timeout_do_hcsr04_substitui_leitura_antiga_por_ausencia" in teste_embarcado
    assert "test_presenca_quatro_metros_eh_ausente_e_bloqueia_teclado" in teste_embarcado


def test_ldr_automatico_roda_ate_o_primeiro_comando_manual():
    texto = (ROOT / "main" / "main.c").read_text(encoding="utf-8")

    inicio_menu = texto.index("while (true) {", texto.index("mostrar_menu();"))
    trecho_menu = texto[inicio_menu : texto.index("char tecla = teclado_matricial_ler", inicio_menu)]
    assert "atualizar_luz_automatica();" in trecho_menu

    inicio_partida = texto.index("static bool aguardar_jogada_teclado")
    trecho_partida = texto[inicio_partida : texto.index("char tecla = teclado_matricial_ler", inicio_partida)]
    assert "atualizar_luz_automatica();" in trecho_partida

    assert "static void definir_luz_manual(bool ligada)" in texto
    assert texto.count("definir_luz_manual(true);") >= 2
    assert texto.count("definir_luz_manual(false);") >= 2
    assert "ldr_deve_atualizar_iluminacao_automatica(sensor_luz_pronto, luz_dourada_manual)" in texto


def test_auditoria_de_jogo_registra_jogadas_e_resultado_final():
    texto = (ROOT / "main" / "main.c").read_text(encoding="utf-8")
    log_base = (ROOT / "logs" / "jogo_auditoria.log").read_text(encoding="utf-8")

    assert "TAG_AUDIT" in texto
    assert "auditoria_jogo_inicio();" in texto
    assert "auditoria_jogo_jogada(\"HUMANO\"" in texto
    assert "auditoria_jogo_jogada(\"IA\"" in texto
    assert "auditoria_jogo_fim(\"VITORIA_HUMANO\", 'O');" in texto
    assert "tabuleiro=%s ocupadas=%u vitoria_O=%d linha_O=%s vitoria_X=%d linha_X=%s empate=%d" in texto
    assert "JOGO_AUDIT" in log_base
    assert "idf.py monitor | tee -a logs/jogo_auditoria.log" in log_base


def test_iniciar_sh_tem_modo_auditoria_com_log_serial_automatico():
    texto = (ROOT / "iniciar.sh").read_text(encoding="utf-8")

    assert "MONITOR_AUDITORIA=0" in texto
    assert "FLASH_AUDITORIA=0" in texto
    assert "AUDITORIA_WOKWI=0" in texto
    assert 'AUDITORIA_LOG="$PASTA/logs/jogo_auditoria.log"' in texto
    assert "./iniciar.sh auditoria" in texto
    assert "./iniciar.sh monitor" in texto
    assert "./iniciar.sh simular-auditoria" in texto
    assert "preparar_log_auditoria" in texto
    assert "preparar_auditoria_wokwi" in texto
    assert "abrir_monitor_auditoria" in texto
    assert 'idf.py flash monitor | tee -a "$AUDITORIA_LOG"' in texto
    assert 'idf.py monitor | tee -a "$AUDITORIA_LOG"' in texto
    assert 'preparar_log_auditoria "./iniciar.sh flash-testes"' in texto
    assert "Compilar, abrir VS Code e testar no Wokwi com auditoria" in texto
    assert "AUDITORIA_COMANDO=\"./iniciar.sh simular-auditoria\"" in texto
