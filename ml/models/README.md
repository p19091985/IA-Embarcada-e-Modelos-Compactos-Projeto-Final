# Modelos TFLite

Esta pasta armazena os artefatos gerados pelas pipelines de treinamento:

- `gesto_float.tflite` — modelo float do classificador de gestos.
- `gesto_int8.tflite` — modelo INT8 quantizado do classificador de gestos.
- `tictactoe_float.tflite` — modelo float do seletor de jogadas.
- `tictactoe_int8.tflite` — modelo INT8 quantizado do seletor de jogadas.

Os headers C correspondentes (`main/gesto_model_data.h` e `main/tictactoe_model_data.h`) sao exportados automaticamente pelas pipelines e contem os arrays de bytes para inclusao direta no firmware.

O fluxo de treinamento segue o padrao: modelo Keras, conversao para TFLite float, quantizacao INT8 com dataset representativo e exportacao do array C para o firmware.

Geracao dos modelos:

```bash
python3 ml/pipeline_gestos.py --gerar-exemplo
python3 ml/pipeline_tictactoe.py
```
