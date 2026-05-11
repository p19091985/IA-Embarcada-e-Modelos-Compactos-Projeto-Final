set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

idf_component_register(
    SRCS
        "test_runner.c"
        "../test_jogo_da_velha.c"
        "../test_ia_jogo_da_velha.c"
        "../test_teclado_matricial.c"
        "../test_formatacao_telas.c"
        "../test_mpu6050.c"
        "../test_coleta_mpu6050.c"
        "../test_serie_temporal.c"
        "../test_gesto.c"
        "../test_gesto_tflite.c"
        "../test_auto_scan.c"
        "../test_ia_tflite.c"
        "${PROJECT_ROOT}/main/jogo_da_velha.c"
        "${PROJECT_ROOT}/main/ia_jogo_da_velha.c"
        "${PROJECT_ROOT}/main/mpu6050.c"
        "${PROJECT_ROOT}/main/coleta_mpu6050.c"
        "${PROJECT_ROOT}/main/serie_temporal.c"
        "${PROJECT_ROOT}/main/gesto.c"
        "${PROJECT_ROOT}/main/gesto_tflite.cc"
        "${PROJECT_ROOT}/main/auto_scan.c"
        "${PROJECT_ROOT}/main/ia_tflite.cc"
    INCLUDE_DIRS
        "."
        "${PROJECT_ROOT}/main"
    REQUIRES unity esp_driver_gpio esp_driver_i2c esp_timer esp-tflite-micro
)
