# Modelos TFLite

Esta pasta armazena os artefatos gerados pelas pipelines de treinamento:

- `tictactoe_float.tflite` — modelo float do seletor de jogadas.
- `tictactoe_int8.tflite` — modelo INT8 quantizado do seletor de jogadas.
- `presenca_float.tflite` — modelo float do classificador de presenca.
- `presenca_int8.tflite` — modelo INT8 quantizado do classificador de presenca.

Os headers C correspondentes (`main/tictactoe_model_data.h` e `main/presenca_model_data.h`) sao exportados automaticamente pelas pipelines e contem os arrays para inclusao direta no firmware.

O fluxo de treinamento segue `codigo/tflite_hello_world_training.ipynb`: modelo Keras pequeno, conversao para TFLite float, quantizacao INT8 com dataset representativo e exportacao do array C para o firmware.

As pipelines tambem atualizam `ml/relatorios/*.json` com metricas float vs INT8, matriz de confusao, contrato de quantizacao `full_integer_int8`, tamanho e SHA-256 dos modelos. Os mesmos hashes e metricas principais sao exportados para os headers C e aparecem no console de boot.

Geracao dos modelos:

```bash
python3 ml/pipeline_tictactoe.py
python3 ml/pipeline_presenca.py
```
