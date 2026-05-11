# Relatorios de treinamento

Esta pasta contem os relatorios JSON gerados pelas pipelines `ml/pipeline_tictactoe.py` e `ml/pipeline_presenca.py`.

Cada relatorio registra:

- origem e auditoria do dataset;
- split treino/teste e semente;
- arquitetura e metricas do modelo float e INT8;
- matriz de confusao;
- contrato de quantizacao `full_integer_int8`;
- tamanho e SHA-256 dos artefatos;
- ligacao com `codigo/tflite_hello_world_training.ipynb` e `codigo/Descricao do projeto final.pdf`.

Esses arquivos sao parte da demonstracao tecnica: o firmware imprime no console um resumo dos mesmos metadados sem mudar a experiencia visual do jogo.
