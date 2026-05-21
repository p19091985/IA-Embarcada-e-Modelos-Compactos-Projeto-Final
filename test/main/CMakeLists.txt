set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

idf_component_register(
    SRCS
        "test_runner.c"
        "../test_jogo_da_velha.c"
        "../test_jogo_auditoria.c"
        "../test_jogo_interface.c"
        "../test_teclado_matricial.c"
        "../test_formatacao_telas.c"
        "../test_hcsr04.c"
        "../test_ldr.c"
        "../test_ia_tflite.c"
        "../test_presenca_tflite.c"
        "../test_gameplay_alpha0.c"
        "${PROJECT_ROOT}/main/jogo_da_velha.c"
        "${PROJECT_ROOT}/main/jogo_auditoria.c"
        "${PROJECT_ROOT}/main/jogo_interface.c"
        "${PROJECT_ROOT}/main/ia_jogo_da_velha.c"
        "${PROJECT_ROOT}/main/lcd1602_i2c.c"
        "${PROJECT_ROOT}/main/hcsr04.c"
        "${PROJECT_ROOT}/main/ldr.c"
        "${PROJECT_ROOT}/main/ia_tflite.cc"
        "${PROJECT_ROOT}/main/presenca_tflite.cc"
    INCLUDE_DIRS
        "."
        "${PROJECT_ROOT}/main"
    REQUIRES unity esp_adc esp_driver_gpio esp_driver_i2c esp_rom esp_timer esp-tflite-micro
)
