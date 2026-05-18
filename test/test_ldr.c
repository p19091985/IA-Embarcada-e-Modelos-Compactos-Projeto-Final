#include "unity.h"

#include "ldr.h"

void test_ldr_detecta_ambiente_escuro_por_limiar(void)
{
    TEST_ASSERT_TRUE(ldr_ambiente_escuro(0));
    TEST_ASSERT_TRUE(ldr_ambiente_escuro(LDR_LIMIAR_ESCURO - 1));
    TEST_ASSERT_FALSE(ldr_ambiente_escuro(LDR_LIMIAR_ESCURO));
    TEST_ASSERT_FALSE(ldr_ambiente_escuro(LDR_LIMIAR_ESCURO + 1));
    TEST_ASSERT_FALSE(ldr_ambiente_escuro(4095));
}

void test_ldr_constantes_do_adc(void)
{
    TEST_ASSERT_EQUAL_INT(10, LDR_GPIO_NUM);
    TEST_ASSERT_EQUAL_INT(ADC_UNIT_1, LDR_ADC_UNIT);
    TEST_ASSERT_EQUAL_INT(ADC_CHANNEL_9, LDR_ADC_CHANNEL);
}

void test_ldr_configura_estado_padrao(void)
{
    ldr_t ldr;

    TEST_ASSERT_EQUAL(ESP_OK, ldr_iniciar(&ldr));
    TEST_ASSERT_EQUAL_INT(LDR_LIMIAR_ESCURO, ldr.limiar_escuro);
}

void test_ldr_rejeita_argumentos_invalidos(void)
{
    int leitura = 0;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ldr_iniciar(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ldr_ler_bruto(NULL, &leitura));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ldr_ler_bruto((ldr_t *)1, NULL));
}
