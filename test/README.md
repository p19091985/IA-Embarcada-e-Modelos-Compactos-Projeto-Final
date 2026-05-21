# Testes

Esta pasta contem os testes automatizados do jogo da velha embarcado para ESP32-S3/ESP-IDF.

Arquivos:

- `test_diagram_json.py`: teste de host para conferir se o `diagram.json` tem os componentes e GPIOs esperados.
- `test_pipeline_tictactoe.py`: testes da geracao de dataset, politica otima, header e relatorio de treino do modelo do jogo.
- `test_pipeline_presenca.py`: testes do dataset HC-SR04, header e relatorio de treino do classificador de presenca.
- `test_requisitos_sistema.py`: testes de rastreabilidade entre PDF, notebook, firmware, modelos e documentacao.
- `test_jogo_da_velha.c`: testes Unity para a logica do tabuleiro.
- `test_jogo_interface.c`: testes Unity para bloqueio por presenca e formatacao do LCD de estatisticas.
- `test_ia_tflite.c`: testes Unity para a unica IA ativa no firmware.
- `test_teclado_matricial.c`: testes Unity para o mapa do teclado.
- `test_formatacao_telas.c`: testes Unity para os textos de tela.
- `test_hcsr04.c`: testes Unity para conversao e faixa do sensor ultrassonico.
- `test_ldr.c`: testes Unity para limiar de luminosidade.
- `test_presenca_tflite.c`: testes Unity para o classificador TFLite de presenca.
- `main/test_runner.c`: runner Unity embarcado que chama os testes C.

O teste Python pode ser executado com:

```bash
. /home/patrik/.espressif/v6.0.1/esp-idf/export.sh
python -m pytest test/test_diagram_json.py
```

Os testes C sao compilados como um projeto ESP-IDF separado:

```bash
. /home/patrik/.espressif/v6.0.1/esp-idf/export.sh
cd test
idf.py -B build_tests set-target esp32s3
idf.py -B build_tests build
```

Para executar os testes de fato, grave o firmware de teste em uma placa ou rode em um ambiente que exponha o monitor serial:

```bash
idf.py -B build_tests flash monitor
```
