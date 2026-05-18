#include "unity.h"

#include "hcsr04.h"

void test_hcsr04_converte_echo_para_distancia(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, hcsr04_echo_us_para_cm(0));
    TEST_ASSERT_EQUAL_UINT16(1, hcsr04_echo_us_para_cm(58));
    TEST_ASSERT_EQUAL_UINT16(1, hcsr04_echo_us_para_cm(115));
    TEST_ASSERT_EQUAL_UINT16(10, hcsr04_echo_us_para_cm(580));
    TEST_ASSERT_EQUAL_UINT16(120, hcsr04_echo_us_para_cm(6960));
    TEST_ASSERT_EQUAL_UINT16(400, hcsr04_echo_us_para_cm(23200));
}

void test_hcsr04_valida_faixa_de_distancia(void)
{
    TEST_ASSERT_FALSE(hcsr04_distancia_valida_cm(0));
    TEST_ASSERT_TRUE(hcsr04_distancia_valida_cm(1));
    TEST_ASSERT_TRUE(hcsr04_distancia_valida_cm(120));
    TEST_ASSERT_TRUE(hcsr04_distancia_valida_cm(400));
    TEST_ASSERT_FALSE(hcsr04_distancia_valida_cm(401));
}

void test_hcsr04_constantes_do_sensor(void)
{
    TEST_ASSERT_EQUAL_INT(GPIO_NUM_19, HCSR04_TRIGGER_GPIO);
    TEST_ASSERT_EQUAL_INT(GPIO_NUM_20, HCSR04_ECHO_GPIO);
    TEST_ASSERT_EQUAL_UINT32(30000, HCSR04_TIMEOUT_US);
    TEST_ASSERT_EQUAL_UINT32(58, HCSR04_ECHO_US_POR_CM);
}

void test_hcsr04_rejeita_argumentos_invalidos(void)
{
    hcsr04_t sensor;
    uint16_t distancia = 0;
    uint32_t eco_us = 0;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hcsr04_iniciar(NULL, HCSR04_TRIGGER_GPIO, HCSR04_ECHO_GPIO));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hcsr04_iniciar(&sensor, GPIO_NUM_NC, HCSR04_ECHO_GPIO));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hcsr04_iniciar(&sensor, HCSR04_TRIGGER_GPIO, GPIO_NUM_NC));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hcsr04_ler_distancia(NULL, &distancia, &eco_us));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hcsr04_ler_distancia(&sensor, NULL, &eco_us));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hcsr04_ler_distancia(&sensor, &distancia, NULL));
}
