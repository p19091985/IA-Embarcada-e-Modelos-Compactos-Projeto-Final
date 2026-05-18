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
        "hcsr04.c"
        "ldr.c"
        "ia_tflite.cc"
        "presenca_tflite.cc"
        "microfone_i2s.c"
        "audio_classificador.cc"
    INCLUDE_DIRS "."
    REQUIRES esp_adc esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_ledc esp_rom esp_timer esp-tflite-micro
)
