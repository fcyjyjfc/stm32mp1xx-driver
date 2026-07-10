/*
 * sdio_test.c — SDIO 协议层驱动测试框架
 *
 *  Created on: 2026-7-9
 *      Author: gjsbr
 */

#include <string.h>
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_rcc.h"
#include "sdio_card.h"
#include "common_test.h"

static SdioCard_t g_sdio;
static uint8_t g_buf[512] __attribute__((aligned(4)));


/* SDMMC3 GPIO: PD0(CMD,AF10) + PG15(CLK,AF10) + PF0/4(D0/D1,AF9) + PD5/7(D2/D3,AF10) */
static void SdioGpioInit(void)
{
    /* 使能 GPIOD, F, G (AHB4 bits 3,5,6) */
    RCC->MP_AHB4ENSETR |= (1U << 3) | (1U << 5) | (1U << 6);

    /* PD0 = CMD, AF10 */
    GpioMode(GPIO_D, 0, GPIO_MODER_AF);
    GpioAf(GPIO_D, 0, 10);
    GpioOtype(GPIO_D, 0, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_D, 0, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_D, 0, GPIO_PUPDR_PULL_UP);

    /* PG15 = CLK, AF10 */
    GpioMode(GPIO_G, 15, GPIO_MODER_AF);
    GpioAf(GPIO_G, 15, 10);
    GpioOtype(GPIO_G, 15, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_G, 15, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_G, 15, GPIO_PUPDR_NO);

    /* PF0 = D0, AF9 */
    GpioMode(GPIO_F, 0, GPIO_MODER_AF);
    GpioAf(GPIO_F, 0, 9);
    GpioOtype(GPIO_F, 0, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_F, 0, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_F, 0, GPIO_PUPDR_PULL_UP);

    /* PF4 = D1, AF9 */
    GpioMode(GPIO_F, 4, GPIO_MODER_AF);
    GpioAf(GPIO_F, 4, 9);
    GpioOtype(GPIO_F, 4, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_F, 4, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_F, 4, GPIO_PUPDR_PULL_UP);

    /* PD5 = D2, AF10 */
    GpioMode(GPIO_D, 5, GPIO_MODER_AF);
    GpioAf(GPIO_D, 5, 10);
    GpioOtype(GPIO_D, 5, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_D, 5, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_D, 5, GPIO_PUPDR_PULL_UP);

    /* PD7 = D3, AF10 */
    GpioMode(GPIO_D, 7, GPIO_MODER_AF);
    GpioAf(GPIO_D, 7, 10);
    GpioOtype(GPIO_D, 7, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_D, 7, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_D, 7, GPIO_PUPDR_PULL_UP);
}

static void SdioClkInit(void)
{
    /* ker_ck 选 HCLK2 (SDMMC3CKSELR bits[2:0]=0) */
    RCC->SDMMC3CKSELR = 3;
    /* 使能 SDMMC3 (AHB2 bit16) */
    RCC->MP_AHB2ENSETR = (1U << 16);
}

static void SdioWlRegOn(void)
{
    /* 读 PD4 当前电平 (AP6212 WL_REG_ON 复位脚) */
    RCC->MP_AHB4ENSETR |= (1U << 3);  /* GPIOD 时钟 */

    /* 拉高, 给 AP6212 上电 */
    GpioMode(GPIO_D, 4, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_D, 4, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_D, 4, GPIO_OSPEED_LOW);
    GpioOutHi(GPIO_D, 4);

    /* 等待 AP6212 内部上电完成 (约 50ms @ 64MHz) */
    volatile uint32_t delay = 30000000;
    while (delay--);
}

static int SdioHwInit(void)
{
    SdioGpioInit();
    SdioWlRegOn();
    SdioClkInit();

    g_sdio.sdmmc    = SDMMC3;
    g_sdio.ker_ck_hz = 64000000;  /* HCLK2=64MHz, 需按实际调整 */

    SdmmcPowerOn(g_sdio.sdmmc, g_sdio.ker_ck_hz);

    SdioCardErr_t err = SdioCardIdentify(&g_sdio);
    if (err != SDIO_OK)
        return -1;

    /* 读卡信息: CCCR + CIS */
    err = SdioCardReadInfo(&g_sdio);
    if (err != SDIO_OK)
        return -1;

    /* 使能 Function 1 (WiFi) */
    err = SdioCardWriteB(&g_sdio, SDIO_CCCR_IO_ENABLE, 0x02);
    if (err != SDIO_OK)
        return -1;

    /* 轮询 IO_READY 等待 Function 1 就绪 */
    {
        uint8_t ready;
        int poll;
        for (poll = 0; poll < 10000; poll++)
        {
            if (SdioCardReadB(&g_sdio, SDIO_CCCR_IO_READY, &ready) != SDIO_OK)
                return -1;
            if (ready & 0x02)
                break;
        }
        if (poll == 10000)
            return -1;
    }

    /* 切 4-bit 总线: BUS_CTRL bit0=1 */
    {
        uint8_t bus;
        if (SdioCardReadB(&g_sdio, SDIO_CCCR_BUS_CTRL, &bus) != SDIO_OK)
            return -1;
        bus |= 0x01;
        if (SdioCardWriteB(&g_sdio, SDIO_CCCR_BUS_CTRL, bus) != SDIO_OK)
            return -1;
        SdmmcSetBusWidth(g_sdio.sdmmc, SDMMC_BUS_4BIT);
    }

    /* 切高速: BUS_SPEED=1 (SDR25), 提升时钟至 25MHz */
    {
        uint8_t speed = 0x01;
        if (SdioCardWriteB(&g_sdio, SDIO_CCCR_BUS_SPEED, speed) != SDIO_OK)
            return -1;
        SdmmcSetClock(g_sdio.sdmmc, g_sdio.ker_ck_hz, 25000000);
    }

    return 0;
}


/**
 * @brief 打印 SDIO 卡信息
 *
 * 在 SdioHwInit 成功后调用, 输出卡参数及 CIS 信息.
 */
static void SdioPrintInfo(void)
{
    char dec[16];

    PRINT("\r\n===== SDIO Card Info =====\r\n");

    PRINT("  Functions: ");
    PrintDec(dec, g_sdio.nf);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  Combo: ");
    PRINT(g_sdio.mp ? "yes (Memory+IO)" : "no (IO-only)");
    PRINT("\r\n");

    PRINT("  S18A: ");
    PRINT(g_sdio.s18a ? "yes" : "no");
    PRINT("\r\n");

    PRINT("  CCCR Rev: ");
    PrintHex8(g_sdio.cccr_rev);
    PRINT("  SDIO Rev: ");
    PrintHex8(g_sdio.sdio_rev);
    PRINT("  SD Spec Rev: ");
    PrintHex8(g_sdio.sd_spec_rev);
    PRINT("\r\n");

    PRINT("  RCA: 0x");
    PrintHex8((uint8_t)(g_sdio.rca >> 8));
    PrintHex8((uint8_t)g_sdio.rca);
    PRINT("\r\n");

    PRINT("  CIS @ 0x");
    PrintHex8((uint8_t)(g_sdio.cis.cis_ptr >> 16));
    PrintHex8((uint8_t)(g_sdio.cis.cis_ptr >> 8));
    PrintHex8((uint8_t)g_sdio.cis.cis_ptr);
    PRINT("  SDIO Ver: ");
    PrintDec(dec, (int)g_sdio.cis.sdio_ver);
    PRINT(dec);
    PRINT("  Funcs in CIS: ");
    PrintDec(dec, (int)g_sdio.cis.func_cnt);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  Manufacturer: 0x");
    PrintHex8((uint8_t)(g_sdio.cis.manf_id >> 8));
    PrintHex8((uint8_t)g_sdio.cis.manf_id);
    PRINT("  Info: 0x");
    PrintHex8((uint8_t)(g_sdio.cis.manf_info >> 8));
    PrintHex8((uint8_t)g_sdio.cis.manf_info);
    PRINT("\r\n");

    {
        int fn;
        for (fn = 0; fn < (int)g_sdio.nf; fn++)
        {
            if (g_sdio.cis.func_blksz[fn])
            {
                PRINT("  F");
                PrintDec(dec, fn + 1);
                PRINT(dec);
                PRINT(" max blksz: ");
                PrintDec(dec, (int)g_sdio.cis.func_blksz[fn]);
                PRINT(dec);
                PRINT("\r\n");
            }
        }
    }
}


/* ================================
 *  1. CCCR 读测试 — 读 0x00~0x08 寄存器
 * ================================ */
static void CccrReadTest(void)
{
    char dec[16];
    uint8_t val;

    PRINT("\r\n----- CCCR Read -----\r\n");

    static const struct {
        uint16_t addr;
        const char *name;
    } regs[] = {
        { 0x00, "CCCR_REV"      },
        { 0x01, "SD_REV"        },
        { 0x02, "IO_ENABLE"     },
        { 0x03, "IO_READY"      },
        { 0x04, "INT_ENABLE"    },
        { 0x05, "INT_PENDING"   },
        { 0x07, "BUS_CTRL"      },
        { 0x08, "CARD_CAPS"     },
        { 0x13, "BUS_SPEED"     },
        { 0x14, "UHS_SUPPORT"   },
    };

    for (int i = 0; i < sizeof(regs)/sizeof(regs[0]); i++)
    {
        IwdgKickDog(IWDG2);
        if (SdioCardReadB(&g_sdio, regs[i].addr, &val) == SDIO_OK)
        {
            PRINT("  [0x");
            PrintHex8(regs[i].addr);
            PRINT("] ");
            PRINT(regs[i].name);
            PRINT(" = 0x");
            PrintHex8(val);
            PRINT("\r\n");
        }
        else
        {
            PRINT("  [0x");
            PrintHex8(regs[i].addr);
            PRINT("] ");
            PRINT(regs[i].name);
            PRINT(" FAILED\r\n");
        }
    }
}


/* ================================
 *  2. CMD52 回环测试 — 写 CCCR 保留区再读回
 *     仅对有写权限且不影响功能的寄存器操作
 * ================================ */
static void Cmd52LoopbackTest(void)
{
    char dec[16];
    uint8_t orig, test, readback;

    PRINT("\r\n----- CMD52 RAW Loopback -----\r\n");
    PRINT("  Purpose: verify CMD52 read-after-write (RAW=1)\r\n");

    /* INT_ENABLE(0x04) 可读可写 */
    if (SdioCardReadB(&g_sdio, SDIO_CCCR_INT_ENABLE, &orig) != SDIO_OK)
    {
        PRINT("  Read original FAILED\r\n");
        return;
    }
    PRINT("  Original INT_ENABLE = 0x");
    PrintHex8(orig);
    PRINT("\r\n");

    test = orig ^ 0x01;
    if (SdioCardWriteReadB(&g_sdio, SDIO_CCCR_INT_ENABLE, test, &readback) != SDIO_OK)
    {
        PRINT("  RAW write+read FAILED\r\n");
        return;
    }
    PRINT("  Write 0x");
    PrintHex8(test);
    PRINT(" -> readback 0x");
    PrintHex8(readback);
    PRINT("\r\n");

    if (readback == test)
    {
        PRINT("  Write verified, restoring original...\r\n");
        if (SdioCardWriteB(&g_sdio, SDIO_CCCR_INT_ENABLE, orig) != SDIO_OK)
        {
            PRINT("  Restore write FAILED\r\n");
            return;
        }
        /* 读回确认恢复 */
        readback = 0;
        if (SdioCardReadB(&g_sdio, SDIO_CCCR_INT_ENABLE, &readback) != SDIO_OK)
        {
            PRINT("  Restore read FAILED\r\n");
            return;
        }
        if (readback == orig)
            PRINT("  RAW Loopback PASSED\r\n");
        else
        {
            PRINT("  Restore FAILED (0x");
            PrintHex8(readback);
            PRINT(" != 0x");
            PrintHex8(orig);
            PRINT(")\r\n");
        }
    }
    else
    {
        PRINT("  RAW Loopback FAILED (readback != written)\r\n");
    }
}


/* ================================
 *  GIC 配置 / 清理
 * ================================ */
static void SdioGicConfig(void)
{
    GicdSetGroup(GIC_SDMMC3);
    GicdSetPriority(GIC_SDMMC3, 5);
    GicdSetTarget(GIC_SDMMC3, 1);
    GicdSetTrigMode(GIC_SDMMC3, 0);
    GicRegisterIrq(GIC_SDMMC3, Sdmmc3IrqHandler);
    GicdEnableInt(GIC_SDMMC3);
}

static void SdioGicCleanup(void)
{
    GicdDisableInt(GIC_SDMMC3);
}


/* ================================
 *  3. CIS Dump — 逐元组打印原始内容
 * ================================ */
static void CisDumpTest(void)
{
    char dec[16];
    uint8_t b[3];
    uint32_t addr;
    SdioCardErr_t err;

    PRINT("\r\n----- CIS Dump -----\r\n");

    /* 读 CIS 指针 */
    err  = SdioCardReadB(&g_sdio, SDIO_CCCR_CIS_PTR,     &b[0]);
    err |= SdioCardReadB(&g_sdio, SDIO_CCCR_CIS_PTR + 1, &b[1]);
    err |= SdioCardReadB(&g_sdio, SDIO_CCCR_CIS_PTR + 2, &b[2]);
    if (err != SDIO_OK) { PRINT("CIS ptr read FAILED\r\n"); return; }

    addr = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
    PRINT("CIS @ 0x");
    PrintHex8((uint8_t)(addr >> 16));
    PrintHex8((uint8_t)(addr >> 8));
    PrintHex8((uint8_t)addr);
    PRINT("\r\n\r\n");

    while (addr < 0x10000)
    {
        uint8_t tuple, link;

        IwdgKickDog(IWDG2);
        err = SdioCardReadB(&g_sdio, (uint16_t)addr, &tuple);
        if (err != SDIO_OK) break;

        if (tuple == SDIO_CISTPL_END)
        {
            PRINT("  [END] 0xFF\r\n");
            break;
        }

        err = SdioCardReadB(&g_sdio, (uint16_t)(addr + 1), &link);
        if (err != SDIO_OK) break;

        PRINT("  0x");
        PrintHex8(tuple);
        PRINT(" [len=");
        PrintDec(dec, link);
        PRINT(dec);
        PRINT("] @0x");
        PrintHex8((uint8_t)(addr >> 16));
        PrintHex8((uint8_t)(addr >> 8));
        PrintHex8((uint8_t)addr);
        PRINT(": ");

        /* 打印 body 前 16 字节 */
        int max = (link < 16) ? link : 16;
        for (int i = 0; i < max; i++)
        {
            uint8_t d;
            if (SdioCardReadB(&g_sdio, (uint16_t)(addr + 2 + i), &d) != SDIO_OK)
                break;
            PrintHex8(d);
            PRINT(" ");
        }
        if (link > 16) PRINT("...");

        /* 元组名 */
        if (tuple == 0x00) PRINT(" (NULL)");
        else if (tuple == 0x15) PRINT(" (VERS_1)");
        else if (tuple == 0x20) PRINT(" (MANFID)");
        else if (tuple == 0x22) PRINT(" (FUNCE)");
        else if (tuple == 0x91) PRINT(" (SDIO_STD)");
        else if (tuple == 0x92) PRINT(" (SDIO_FUNC)");
        PRINT("\r\n");

        addr += 2 + link;
    }

    PRINT("CIS Dump done.\r\n");
}


static const MenuEntry_t sdio_menu[] = {
    { "1", "CCCR Read",      CccrReadTest },
    { "2", "CMD52 Loopback", Cmd52LoopbackTest },
    { "3", "CIS Dump",       CisDumpTest },
};


void SdioTest(void)
{
    if (SdioHwInit() != 0)
    {
        PRINT("SdioHwInit FAILED\r\n");
        return;
    }

    SdioPrintInfo();
    RunSubMenu("SDIO Test", sdio_menu, sizeof(sdio_menu) / sizeof(sdio_menu[0]));
}
