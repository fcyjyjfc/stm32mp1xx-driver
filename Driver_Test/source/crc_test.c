#include <string.h>
#include "stm32mp1xx_crc.h"
#include "test_common.h"


/* CRC-32 (Ethernet) 已知值测试向量 */
static const uint8_t test_data[] = "123456789";
/* CRC-32("123456789") = 0xCBF43926 */
#define CRC32_CHECK_VALUE  0xCBF43926ul


static void TestDefaultPolynomial(void)
{
    CrcCfg_t cfg;
    cfg.polynomial    = 0x04C11DB7;
    cfg.initial_value = 0xFFFFFFFF;
    cfg.polysize      = 32;
    cfg.rev_in        = 3;   // word
    cfg.rev_out       = 1;
    CrcCfg(CRC1, &cfg);

    CrcReset(CRC1);

    /* 喂入测试数据 */
    for (int i = 0; i < 9; i++)
        *(volatile uint8_t *)&CRC1->DR = test_data[i];

    uint32_t result = CrcGet(CRC1) ^ 0xFFFFFFFF;

    char buf[32];
    PrintHex32(buf, result);
    PRINT("\r\n--- CRC-32 Default Polynomial Test ---\r\n");
    PRINT("CRC-32(\"123456789\"): 0x");
    PRINT(buf);
    PRINT("  expected: 0xCBF43926\r\n");

    if (result == CRC32_CHECK_VALUE)
        PRINT("PASS\r\n");
    else
        PRINT("FAIL\r\n");
    PRINT("--- Test done ---\r\n");
}


static void TestCalculateFunction(void)
{
    CrcCfg_t cfg;
    cfg.polynomial    = 0x04C11DB7;
    cfg.initial_value = 0xFFFFFFFF;
    cfg.polysize      = 32;
    cfg.rev_in        = 3;
    cfg.rev_out       = 1;
    CrcCfg(CRC1, &cfg);

    uint32_t result = CrcCalculate(CRC1, test_data, 9) ^ 0xFFFFFFFF;

    char buf[32];
    PrintHex32(buf, result);
    PRINT("\r\n--- CRC Calculate Function Test ---\r\n");
    PRINT("CrcCalculate(\"123456789\"): 0x");
    PRINT(buf);
    PRINT("  expected: 0xCBF43926\r\n");

    if (result == CRC32_CHECK_VALUE)
        PRINT("PASS\r\n");
    else
        PRINT("FAIL\r\n");
    PRINT("--- Test done ---\r\n");
}


static void TestCrc16(void)
{
    /* CRC-16-IBM: POLY=0x8005, INIT=0x0000, REV_IN=half, REV_OUT=1 */
    CrcCfg_t cfg;
    cfg.polynomial    = 0x8005;
    cfg.initial_value = 0x0000;
    cfg.polysize      = 16;
    cfg.rev_in        = 2;   // half-word
    cfg.rev_out       = 1;
    CrcCfg(CRC1, &cfg);

    CrcReset(CRC1);

    for (int i = 0; i < 9; i++)
        *(volatile uint8_t *)&CRC1->DR = test_data[i];

    uint32_t result = CrcGet(CRC1) & 0xFFFF;

    char buf[32];
    int p = 0;
    PrintHex32(buf, result);
    p = strlen(buf);
    buf[p] = '\0';

    PRINT("\r\n--- CRC-16-IBM Test ---\r\n");
    PRINT("CRC-16(\"123456789\"): 0x");
    PRINT(buf);
    PRINT("  expected: 0xBB3D\r\n");

    if ((result & 0xFFFF) == 0xBB3D)
        PRINT("PASS\r\n");
    else
        PRINT("FAIL\r\n");
    PRINT("--- Test done ---\r\n");
}


static void TestCrc8(void)
{
    /* CRC-8: POLY=0x07, INIT=0x00, REV_IN=byte, REV_OUT=0 */
    CrcCfg_t cfg;
    cfg.polynomial    = 0x07;
    cfg.initial_value = 0x00;
    cfg.polysize      = 8;
    cfg.rev_in        = 1;   // byte
    cfg.rev_out       = 0;
    CrcCfg(CRC1, &cfg);

    CrcReset(CRC1);

    for (int i = 0; i < 9; i++)
        *(volatile uint8_t *)&CRC1->DR = test_data[i];

    uint32_t result = CrcGet(CRC1) & 0xFF;

    char buf[32];
    PrintHex32(buf, result);

    PRINT("\r\n--- CRC-8 Test ---\r\n");
    PRINT("CRC-8(\"123456789\"): 0x");
    PRINT(buf);
    PRINT("  expected: 0xF4\r\n");

    if ((result & 0xFF) == 0xF4)
        PRINT("PASS\r\n");
    else
        PRINT("FAIL\r\n");
    PRINT("--- Test done ---\r\n");
}


static void TestEmptyData(void)
{
    CrcCfg_t cfg;
    cfg.polynomial    = 0x04C11DB7;
    cfg.initial_value = 0xFFFFFFFF;
    cfg.polysize      = 32;
    cfg.rev_in        = 3;
    cfg.rev_out       = 1;
    CrcCfg(CRC1, &cfg);

    /* 空数据复位后直接读 */
    CrcReset(CRC1);
    uint32_t after_reset = CRC1->DR;

    CrcReset(CRC1);
    uint32_t after_calc  = CrcCalculate(CRC1, (void *)"", 0);

    char buf[32];
    PRINT("\r\n--- CRC Empty Data Test ---\r\n");
    PrintHex32(buf, after_reset);
    PRINT("After reset: 0x");
    PRINT(buf);
    PRINT("\r\n");
    PrintHex32(buf, after_calc);
    PRINT("CrcCalculate(empty): 0x");
    PRINT(buf);
    PRINT("\r\n");

    /* CRC-32 of empty data = 0x00000000 (after XOR) */
    uint32_t result = after_calc ^ 0xFFFFFFFF;
    if (result == 0)
        PRINT("PASS\r\n");
    else
        PRINT("FAIL (expected 0x00000000)\r\n");
    PRINT("--- Test done ---\r\n");
}


static void TestBulkData(void)
{
    CrcCfg_t cfg;
    cfg.polynomial    = 0x04C11DB7;
    cfg.initial_value = 0xFFFFFFFF;
    cfg.polysize      = 32;
    cfg.rev_in        = 3;
    cfg.rev_out       = 1;
    CrcCfg(CRC1, &cfg);

    /* 构造 1024 字节测试数据 */
    uint8_t bulk[1024];
    for (int i = 0; i < 1024; i++)
        bulk[i] = (uint8_t)(i & 0xFF);

    uint32_t result = CrcCalculate(CRC1, bulk, 1024) ^ 0xFFFFFFFF;

    char buf[32];
    PrintHex32(buf, result);
    PRINT("\r\n--- CRC Bulk Data Test (1024 bytes) ---\r\n");
    PRINT("CRC-32(0..1023): 0x");
    PRINT(buf);
    PRINT("\r\n");
    PRINT("--- Test done ---\r\n");
}


static const MenuEntry_t crc_menu[] = {
    { "1", "CRC-32 default polynomial",     TestDefaultPolynomial },
    { "2", "CrcCalculate function",         TestCalculateFunction },
    { "3", "CRC-16-IBM",                    TestCrc16 },
    { "4", "CRC-8",                         TestCrc8 },
    { "5", "Empty data",                    TestEmptyData },
    { "6", "Bulk data (1024 bytes)",        TestBulkData },
};

void CrcTest(void)
{
    RunSubMenu("CRC Test", crc_menu, sizeof(crc_menu) / sizeof(crc_menu[0]));
}
