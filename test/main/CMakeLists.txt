set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

idf_component_register(
    SRCS
        "test_runner.c"
        "../test_jogo_da_velha.c"
        "../test_ia_jogo_da_velha.c"
        "../test_teclado_matricial.c"
        "../test_formatacao_telas.c"
        "${PROJECT_ROOT}/main/jogo_da_velha.c"
        "${PROJECT_ROOT}/main/ia_jogo_da_velha.c"
    INCLUDE_DIRS
        "."
        "${PROJECT_ROOT}/main"
    REQUIRES unity esp_driver_gpio
)
