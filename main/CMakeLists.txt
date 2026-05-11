idf_component_register(
    SRCS
        "main.c"
        "jogo_da_velha.c"
        "ia_jogo_da_velha.c"
        "teclado_matricial.c"
        "leds.c"
        "buzzer.c"
        "ssd1306_i2c.c"
        "lcd1602_i2c.c"
        "mpu6050.c"
        "coleta_mpu6050.c"
        "serie_temporal.c"
        "gesto.c"
        "gesto_tflite.cc"
        "auto_scan.c"
        "ia_tflite.cc"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_rom esp_timer esp-tflite-micro
)
