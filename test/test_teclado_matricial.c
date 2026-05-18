#include "unity.h"

#include "teclado_matricial.h"

void test_mapa_do_teclado_documentado(void)
{
    const char mapa[TECLADO_LINHAS][TECLADO_COLUNAS] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'},
    };

    TEST_ASSERT_EQUAL_CHAR('1', mapa[0][0]);
    TEST_ASSERT_EQUAL_CHAR('9', mapa[2][2]);
    TEST_ASSERT_EQUAL_CHAR('*', mapa[3][0]);
    TEST_ASSERT_EQUAL_CHAR('0', mapa[3][1]);
    TEST_ASSERT_EQUAL_CHAR('#', mapa[3][2]);
    TEST_ASSERT_EQUAL_CHAR('D', mapa[3][3]);
}
