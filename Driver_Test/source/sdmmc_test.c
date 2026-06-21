#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_rcc.h"
#include "sd_card.h"
#include "test_common.h"

static SdCard_t g_card;
static uint8_t g_buf[512] __attribute__((aligned(4)));


/* SDMMC1 GPIO: PC8~PC12(AF12), PD2(AF12), 高速+上拉 */
static void SdmmcGpioInit(void)
{
    RCC->MP_AHB4ENSETR |= (1U << 2) | (1U << 3) | (1U << 7);  /* GPIOC, D, H */

    /* PC8=DAT0, PC9=DAT1, PC10=DAT2, PC11=DAT3, PC12=CLK */
    int pin;
    for (pin = 8; pin <= 12; pin++)
    {
        GpioMode(GPIO_C, pin, GPIO_MODER_AF);
        GpioAf(GPIO_C, pin, 12);
        GpioOtype(GPIO_C, pin, GPIO_OTYPE_PUSH_PULL);
        GpioOspeed(GPIO_C, pin, GPIO_OSPEED_VERY_HI);
        GpioPullUpDown(GPIO_C, pin, (pin == 12) ? GPIO_PUPDR_NO : GPIO_PUPDR_PULL_UP);
    }

    /* PD2=CMD */
    GpioMode(GPIO_D, 2, GPIO_MODER_AF);
    GpioAf(GPIO_D, 2, 12);
    GpioOtype(GPIO_D, 2, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_D, 2, GPIO_OSPEED_VERY_HI);
    GpioPullUpDown(GPIO_D, 2, GPIO_PUPDR_PULL_UP);
}

static void SdmmcClkInit(void)
{
    /* ker_ck 选 HSI (64MHz), 频率已知不依赖 PLL 配置 */
    RCC->SDMMC12CKSELR = 3;
    /* 使能 SDMMC1 (AHB6 bit16) */
    RCC->MP_AHB6ENSETR = (1U << 16);
}

/* 统一初始化: GPIO + 时钟 + 卡识别, 成功返回0 */
static int SdmmcHwInit(void)
{
    char dec[16];

    SdmmcGpioInit();
    SdmmcClkInit();

    g_card.sdmmc = SDMMC1;
    g_card.ker_ck_hz = 64000000;

    SdCardErr_t err = SdCardInit(&g_card);
    if (err != SD_OK)
    {
        PRINT("SdCardInit FAILED, err=");
        PrintDec(dec, (int32_t)err);
        PRINT(dec);
        PRINT("\r\n");
        return -1;
    }

    PRINT("Init OK!\r\n");
    PRINT("  Product: ");
    PRINT(g_card.product);
    PRINT(" v");
    PrintDec(dec, g_card.prv_major);
    PRINT(dec);
    PRINT(".");
    PrintDec(dec, g_card.prv_minor);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  OEM: ");
    PRINT(g_card.oem_id);
    PRINT("  MFR: 0x");
    PrintHex8(g_card.mfr_id);
    PRINT("\r\n");

    PRINT("  Serial: 0x");
    {
        char hex[12];
        PrintHex32(hex, g_card.serial);
        PRINT(hex);
    }
    PRINT("\r\n");

    PRINT("  Date: ");
    PrintDec(dec, g_card.mfr_year);
    PRINT(dec);
    PRINT("/");
    PrintDec(dec, g_card.mfr_month);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  Type: ");
    PRINT(g_card.is_sdhc ? "SDHC/SDXC" : "SDSC");
    PRINT("\r\n");

    PRINT("  Capacity: ");
    PrintDec(dec, (int32_t)g_card.capacity_mb);
    PRINT(dec);
    PRINT(" MB\r\n");

    PRINT("  Max Clock: ");
    PrintDec(dec, (int32_t)(g_card.max_clk_hz / 1000000));
    PRINT(dec);
    PRINT(" MHz\r\n");

    return 0;
}


/* ================================
 *  1. 读 MBR 测试 — 检查 0x55AA 签名
 * ================================ */
static void ReadMbrTest(void)
{
    PRINT("\r\n----- Read MBR (Block 0) -----\r\n");

    IwdgKickDog(IWDG2);
    SdCardErr_t err = SdCardReadBlocks(&g_card, 0, g_buf, 1);
    if (err != SD_OK)
    {
        PRINT("Read FAILED\r\n");
        return;
    }

    PRINT("  Block 0 first 16 bytes:\r\n    ");
    int i;
    for (i = 0; i < 16; i++)
    {
        PrintHex8(g_buf[i]);
        PRINT(" ");
    }
    PRINT("\r\n");

    PRINT("  Signature [510..511]: ");
    PrintHex8(g_buf[510]);
    PRINT(" ");
    PrintHex8(g_buf[511]);
    if (g_buf[510] == 0x55 && g_buf[511] == 0xAA)
        PRINT(" -> MBR Valid\r\n");
    else
        PRINT(" -> No MBR\r\n");
}


/* ================================
 *  2. 写回读测试 — 写入 pattern 并校验
 * ================================ */
static void WriteReadTest(void)
{
    char dec[16];
    uint32_t test_block = 2048;
    int i;

    PRINT("\r\n----- Write/Read Test (Block ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT(") -----\r\n");

    IwdgKickDog(IWDG2);

    /* 写入 pattern: 每字节 = 索引低8位 XOR 0xA5 */
    for (i = 0; i < 512; i++)
        g_buf[i] = (uint8_t)(i ^ 0xA5);

    PRINT("  Writing...\r\n");
    SdCardErr_t err = SdCardWriteBlocks(&g_card, test_block, g_buf, 1);
    if (err != SD_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 清缓冲区再读回 */
    memset(g_buf, 0, 512);
    PRINT("  Reading back...\r\n");
    err = SdCardReadBlocks(&g_card, test_block, g_buf, 1);
    if (err != SD_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 校验 */
    int errors = 0;
    for (i = 0; i < 512; i++)
    {
        if (g_buf[i] != (uint8_t)(i ^ 0xA5))
        {
            if (errors < 4)
            {
                PRINT("  Mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": expect ");
                PrintHex8((uint8_t)(i ^ 0xA5));
                PRINT(" got ");
                PrintHex8(g_buf[i]);
                PRINT("\r\n");
            }
            errors++;
        }
    }

    if (errors == 0)
        PRINT("  Verify PASSED\r\n");
    else
    {
        PRINT("  Verify FAILED: ");
        PrintDec(dec, errors);
        PRINT(dec);
        PRINT(" errors\r\n");
    }
}


/* ===== Submenu ===== */

static const MenuEntry_t sdmmc_menu[] = {
    { "1", "Read MBR",        ReadMbrTest },
    { "2", "Write/Read Test", WriteReadTest },
};

void SdmmcTest(void)
{
    PRINT("\r\n===== SDMMC Init =====\r\n");
    if (SdmmcHwInit() != 0)
        return;

    RunSubMenu("SDMMC Test", sdmmc_menu, sizeof(sdmmc_menu) / sizeof(sdmmc_menu[0]));
}
