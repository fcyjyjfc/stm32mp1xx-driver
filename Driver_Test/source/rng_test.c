#include <string.h>
#include "stm32mp1xx_rng.h"
#include "common_test.h"


static void TestBasicRead(void)
{
    RngCfg_t cfg;
    cfg.clock_error_detection = 0;    // CED=0: 时钟错误检测使能
    cfg.interrupt_enable = 0;         // IE=0: 轮询模式

    RngCfg(RNG1, &cfg);

    PRINT("\r\n--- RNG Basic Read Test ---\r\n");
    PRINT("Reading 16 random numbers:\r\n\r\n");

    for (int i = 0; i < 16; i++)
    {
        uint32_t val = RngGetRandom(RNG1);

        char buf[16];
        int p = 0;
        buf[p++] = '0' + i / 10;
        buf[p++] = '0' + i % 10;
        buf[p++] = ':';
        buf[p++] = ' ';
        buf[p] = '\0';

        PrintHex32(buf + p, val);
        p = strlen(buf);
        buf[p++] = '\r';
        buf[p++] = '\n';
        UsartWrite(USART4, (void *)buf, p);
    }

    RngDisable(RNG1);
    PRINT("\r\n--- Test done ---\r\n");
}


static void TestNonBlocking(void)
{
    RngCfg_t cfg;
    cfg.clock_error_detection = 0;
    cfg.interrupt_enable = 0;

    RngCfg(RNG1, &cfg);

    PRINT("\r\n--- RNG Non-blocking Read Test ---\r\n");
    PRINT("Read 10 random numbers (non-blocking, with timeout):\r\n\r\n");

    int count = 0;
    int timeout = 0;

    while (count < 10 && timeout < 100000)
    {
        uint32_t val;
        if (RngRead(RNG1, &val))
        {
            char buf[16];
            PrintHex32(buf, val);
            int p = strlen(buf);
            buf[p++] = '\r';
            buf[p++] = '\n';
            UsartWrite(USART4, (void *)buf, p);
            count++;
        }
        else
        {
            timeout++;
        }
    }

    if (count >= 10)
        PRINT("All 10 values read successfully.\r\n");
    else
        PRINT("Timeout reached.\r\n");

    RngDisable(RNG1);
    PRINT("--- Test done ---\r\n");
}


static void TestErrorRecovery(void)
{
    RngCfg_t cfg;
    cfg.clock_error_detection = 0;
    cfg.interrupt_enable = 0;

    RngCfg(RNG1, &cfg);

    PRINT("\r\n--- RNG Error Recovery Test ---\r\n");
    PRINT("Normal read before recovery:\r\n");

    uint32_t val = RngGetRandom(RNG1);
    if (val)
    {
        char buf[16];
        PrintHex32(buf, val);
        PRINT("  RNG_DR: ");
        PRINT(buf);
        PRINT("\r\n");
    }

    if (RngSeedErrorRecovery(RNG1))
        PRINT("Seed error recovery: PASS\r\n");
    else
        PRINT("Seed error recovery: FAIL (error persists)\r\n");

    PRINT("Normal read after recovery:\r\n");
    val = RngGetRandom(RNG1);
    if (val)
    {
        char buf[16];
        PrintHex32(buf, val);
        PRINT("  RNG_DR: ");
        PRINT(buf);
        PRINT("\r\n");
    }

    PRINT("--- Test done ---\r\n");
}


static void TestStatus(void)
{
    RngCfg_t cfg;
    cfg.clock_error_detection = 0;
    cfg.interrupt_enable = 0;

    RngCfg(RNG1, &cfg);

    PRINT("\r\n--- RNG Status Test ---\r\n");

    uint32_t sr = RngGetStatus(RNG1);
    char buf[16];
    PrintHex32(buf, sr);

    PRINT("RNG_SR: 0x");
    PRINT(buf);
    PRINT("\r\n  DRDY: ");
    buf[0] = (sr & RNG_SR_DRDY) ? '1' : '0';
    buf[1] = '\0';
    PRINT(buf);
    PRINT("\r\n  CECS: ");
    buf[0] = (sr & RNG_SR_CECS) ? '1' : '0';
    PRINT(buf);
    PRINT("\r\n  SECS: ");
    buf[0] = (sr & RNG_SR_SECS) ? '1' : '0';
    PRINT(buf);

    RngDisable(RNG1);
    PRINT("\r\n--- Test done ---\r\n");
}


static void TestBulkRead(void)
{
    RngCfg_t cfg;
    cfg.clock_error_detection = 0;
    cfg.interrupt_enable = 0;

    RngCfg(RNG1, &cfg);

    PRINT("\r\n--- RNG Bulk Read Test (100 values) ---\r\n");
    PRINT("Generating 100 random numbers...\r\n");

    uint32_t sum = 0;
    uint32_t min = 0xFFFFFFFF;
    uint32_t max = 0;
    int fail = 0;

    for (int i = 0; i < 100; i++)
    {
        uint32_t val = RngGetRandom(RNG1);
        if (val == 0)
        {
            fail++;
            continue;
        }
        sum  += val;
        if (val < min) min = val;
        if (val > max) max = val;
    }

    char buf[32];
    int p;

    PRINT("  Samples: 100\r\n");
    PRINT("  Failed:  ");
    p = 0;
    NumToStr(buf, fail);
    while (buf[p]) p++;
    UsartWrite(USART4, (void *)buf, p);
    PRINT("\r\n  Min:      ");
    PrintHex32(buf, min);
    PRINT(buf);
    PRINT("\r\n  Max:      ");
    PrintHex32(buf, max);
    p = 0;
    PRINT(buf);
    PRINT("\r\n  Sum[31:0]: ");
    PrintHex32(buf, sum);
    PRINT(buf);
    PRINT("\r\n");

    RngDisable(RNG1);
    PRINT("--- Test done ---\r\n");
}


static const MenuEntry_t rng_menu[] = {
    { "1", "Basic read (16 values)",      TestBasicRead },
    { "2", "Non-blocking read",           TestNonBlocking },
    { "3", "Status register",             TestStatus },
    { "4", "Bulk read (100 values)",      TestBulkRead },
    { "5", "Error recovery",              TestErrorRecovery },
};

void RngTest(void)
{
    RunSubMenu("RNG Test", rng_menu, sizeof(rng_menu) / sizeof(rng_menu[0]));
}
