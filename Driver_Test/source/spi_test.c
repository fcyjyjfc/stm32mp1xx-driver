#include <string.h>
#include "stm32mp1xx_spi.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_rcc.h"
#include "w25qxx.h"
#include "m74hc595.h"
#include "test_common.h"

static SpiCfg_t Spi4Cfg;


/* 一次性的 GPIO + 时钟初始化 */
static void SpiGpioInit(void)
{
    RCC->MP_APB2ENSETR |= 1 << 9;   // SPI4 clock
    RCC->MP_AHB4ENSETR |= 0x7f;      // GPIOA-G enable

    GpioMode(GPIO_E, 11, GPIO_MODER_AF);
    GpioMode(GPIO_E, 12, GPIO_MODER_AF);
    GpioMode(GPIO_E, 13, GPIO_MODER_AF);
    GpioMode(GPIO_E, 14, GPIO_MODER_AF);
    GpioAf(GPIO_E, 11, 5);
    GpioAf(GPIO_E, 12, 5);
    GpioAf(GPIO_E, 13, 5);
    GpioAf(GPIO_E, 14, 5);
    GpioOtype(GPIO_E, 11, GPIO_OTYPE_PUSH_PULL);
    GpioOtype(GPIO_E, 12, GPIO_OTYPE_PUSH_PULL);
    GpioOtype(GPIO_E, 13, GPIO_OTYPE_PUSH_PULL);
    GpioOtype(GPIO_E, 14, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_E, 11, GPIO_OSPEED_MEDIUM);
    GpioOspeed(GPIO_E, 12, GPIO_OSPEED_MEDIUM);
    GpioOspeed(GPIO_E, 13, GPIO_OSPEED_MEDIUM);
    GpioOspeed(GPIO_E, 14, GPIO_OSPEED_MEDIUM);
    GpioPullUpDown(GPIO_E, 11, GPIO_PUPDR_NO);
    GpioPullUpDown(GPIO_E, 12, GPIO_PUPDR_NO);
    GpioPullUpDown(GPIO_E, 13, GPIO_PUPDR_NO);
    GpioPullUpDown(GPIO_E, 14, GPIO_PUPDR_NO);
}


/* 初始化 SPI（GPIO 只做一次） */
static void SpiInitComm(SpiCommMode_t comm_mode, SpiSsMgmt_t ss_mgmt)
{
    SpiGpioInit();

    Spi4Cfg.spi_baud_reate_div = 6;
    Spi4Cfg.spi_clk_cfg        = SPI_CLK_IDLE0_DELAY;
    Spi4Cfg.spi_comm_mode      = comm_mode;
    Spi4Cfg.spi_master         = SPI_MASTER;
    Spi4Cfg.spi_protocol       = SPI_PROTOCOL_MOTOROLA;
    Spi4Cfg.spi_shift          = SPI_SHIFT_MSB_FIRST;
    Spi4Cfg.spi_word_len       = 7;
    Spi4Cfg.spi_ss_mgmt        = ss_mgmt;

    SpiCfg(SPI4, &Spi4Cfg);
    W25Q_Init(SPI4);
    Led_Init(SPI4);
}


/* ================================
 *  1. 全双工测试 — W25QXX Flash
 * ================================ */

static void FlashTest(void)
{
    uint8_t id[3];
    uint8_t buf[256];
    uint8_t verify[256];
    char dec[16];
    int i;

    PRINT("\r\n===== Full Duplex: W25QXX Flash =====\r\n");

    SpiInitComm(SPI_COMM_FULL_DUPLEX, SPI_SSM_SW);
    IwdgKickDog(IWDG2);

    W25Q_ReadJEDECID(id);
    PRINT("[1] JEDEC ID: \r\n     ");
    PrintHex8(id[0]);
    PRINT(" ");
    PrintHex8(id[1]);
    PRINT(" ");
    PrintHex8(id[2]);
    PRINT("\r\n");

    if (id[0] == 0xFF && id[1] == 0xFF && id[2] == 0xFF)
    {
        PRINT("Flash not responding (all 0xFF).\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    {
        uint8_t sr = W25Q_ReadSR1();
        PRINT("[2] SR1: \r\n     ");
        PrintHex8(sr);
        PRINT("\r\n");
    }
    IwdgKickDog(IWDG2);

    PRINT("[3] Erasing sector 0...\r\n");
    W25Q_SectorErase(0);
    PRINT("     Erase done.\r\n");
    IwdgKickDog(IWDG2);

    W25Q_ReadData(0, verify, 256);
    PRINT("[4] Verify erase...\r\n");
    {
        int err = 0;
        for (i = 0; i < 256; i++)
        {
            if (verify[i] != 0xFF)
                err = 1;
        }
        if (err)
            PRINT("     Erase verify FAILED\r\n");
        else
            PRINT("     Erase verify PASSED\r\n");
    }
    IwdgKickDog(IWDG2);

    for (i = 0; i < 256; i++)
        buf[i] = i;

    PRINT("[5] Programming page 0...\r\n");
    W25Q_PageProgram(0, buf, 256);
    PRINT("     Program done.\r\n");
    IwdgKickDog(IWDG2);

    PRINT("[6] Read & verify...\r\n");
    W25Q_ReadData(0, verify, 256);

    int errors = 0;
    for (i = 0; i < 256; i++)
    {
        if (verify[i] != (uint8_t)i)
        {
            if (errors < 8)
            {
                PRINT("Mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": W=");
                PrintHex8(i);
                PRINT(" R=");
                PrintHex8(verify[i]);
                PRINT("\r\n");
            }
            errors++;
        }
    }
    IwdgKickDog(IWDG2);

    if (errors == 0)
    {
        PRINT("     Program Verify PASSED (0..255).\r\n");
        PRINT("     Data[0..31]:");
        for (i = 0; i < 32; i++)
        {
            if ((i & 0xF) == 0)
            {
                PRINT("\r\n     ");
                PrintHex8(i);
                PRINT(":");
            }
            PRINT(" ");
            PrintHex8(verify[i]);
        }
        PRINT("\r\n");
    }
    else
    {
        PRINT("     Program Verify FAILED: ");
        PrintDec(dec, errors);
        PRINT(dec);
        PRINT(" errors\r\n");
    }
}


/* ================================
 *  2. LED Display Test
 * ================================ */

static void LedTest(void)
{
    volatile int d;
    uint8_t num;
    int j;

    PRINT("\r\n===== LED Display Test =====\r\n");

    SpiInitComm(SPI_COMM_FULL_DUPLEX, SPI_SSM_HW);
    Led_Clear();

    /* 1. 4 个 LED 同时依次显示 0~F */
    PRINT("All digits 0~F simultaneously:\r\n");
    for (num = 0; num <= 15; num++)
    {
        Led_DisplayAll(num);
        for (d = 0; d < 300000; d++)
            ;
    }

    /* 2. 全部关闭 */
    Led_Clear();
    PRINT("All off.\r\n");
    for (d = 0; d < 300000; d++)
        ;

    /* 3. 显示多个 4 位数 */
    uint16_t numbers[] = { 0x1234, 0x5678, 0x9ABC, 0xDEF0, 0x0000 };
    int n;

    IwdgKickDog(IWDG2);
    PRINT("Displaying numbers:\r\n");
    for (n = 0; n < 5; n++)
    {
        uint16_t val = numbers[n];
        /* 每位显示约 1.5 秒（快速扫描保持视觉暂留） */
        for (d = 0; d < 300; d++)
        {
            for (j = 0; j < 4; j++)
            {
                uint8_t digit_val = (val >> (12 - j * 4)) & 0xF;
                Led_DisplayDigit(j + 1, digit_val);
                volatile int k;
                for (k = 0; k < 2000; k++)
                    ;
            }
            IwdgKickDog(IWDG2);
        }
    }
    IwdgKickDog(IWDG2);

    /* 4. 清除退出 */
    Led_Clear();
    PRINT("Done.\r\n");
}


/* ===== Submenu ===== */

static const MenuEntry_t spi_menu[] = {
    { "1", "W25QXX Flash", FlashTest },
    { "2", "LED Display",  LedTest },
};

void SpiTest(void)
{
    RunSubMenu("SPI Test", spi_menu, sizeof(spi_menu) / sizeof(spi_menu[0]));
}
