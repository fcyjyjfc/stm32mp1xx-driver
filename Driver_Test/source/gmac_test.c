/*
 * gmac_test.c — GMAC 测试
 *
 *   GmacTest — 测试入口 (menu_test 第 23 项, 二级菜单)
 *     测试 1: MDIO 扫描 0~31, 读 PHY ID1/ID2, 找到板载 PHY
 *
 *  Created on: 2026-8-26
 *      Author: gjsbr
 */

#include "stm32mp1xx_gmac.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_syscfg.h"
#include "common_test.h"


/* ================================================================== */
/*  测试配置 (板级参数, 待确认后填写)                                    */
/* ================================================================== */

#define GMAC_TEST_ETH_EN_BIT   10     /* MP_AHB6ENSETR: ETHMACEN 总线时钟 (hclk6/aclk) */
#define GMAC_TEST_ETH_CKEN_BIT 7      /* MP_AHB6ENSETR: ETHCKEN 内核时钟 (eth_ker_ck) */
#define GMAC_TEST_ETH_RST_BIT  10     /* AHB6RSTSETR: ETH 复位位号 (与使能同号) */

/* MDC/MDIO 引脚: PC1/PA2, AF11 */
#define GMAC_TEST_MDC_PORT     GPIO_C
#define GMAC_TEST_MDC_PIN      1
#define GMAC_TEST_MDIO_PORT    GPIO_A
#define GMAC_TEST_MDIO_PIN     2
#define GMAC_TEST_MDIO_AF      11

/* MDC 分频: HCLK≤266MHz 时 /124 得 ≤2.1MHz, 满足 IEEE ≤2.5MHz (待确认) */
#define GMAC_TEST_MDC_DIV      124

/* 内核时钟源: ETHCKSELR.ETHSRC, 0 = pll4_p_ck, 1 = pll3_q_ck (RGMII 需 125MHz) */
#define GMAC_TEST_ETH_SRC      0


/* ================================================================== */
/*  公共初始化                                                         */
/* ================================================================== */

/*
 * RCC: ETH 时钟源选择 + 时钟使能 + 复位期间配置 SYSCFG + 释放复位
 *   BIT10 ETHMACEN = 总线时钟 (hclk6/aclk, 寄存器访问+DMA)
 *   BIT7  ETHCKEN  = 内核时钟 (eth_ker_ck, 软复位需要, 源由 ETHCKSELR 选)
 *   ETH_SEL/ETH_CLK_SEL 必须在 ETH 处于复位期间、释放复位前配置 (手册 Note).
 */
static void GmacClockEnable(void)
{
    /* 1. 内核时钟源: ETHSRC = pll4_p_ck (先选 pll4, 若软复位超时改 pll3_q_ck) */
    RCC->ETHCKSELR = (RCC->ETHCKSELR & ~(3U << 0)) | ((uint32_t)GMAC_TEST_ETH_SRC << 0);

    /* 2. 使能 ETH 总线/内核时钟 */
    RCC->MP_AHB6ENSETR |= (1U << GMAC_TEST_ETH_EN_BIT)   /* ETHMACEN */
                        | (1U << GMAC_TEST_ETH_CKEN_BIT); /* ETHCKEN  */

    /* 3. ETH 进复位 */
    RCC->AHB6RSTSETR = (1U << GMAC_TEST_ETH_RST_BIT);

    /* 4. 复位期间配置 SYSCFG: RGMII 接口 + 内部 125MHz (GTX_CLK 由 MAC 输出) */
    SyscfgEthModeSet(SYSCFG_ETH_RGMII);
    SyscfgEthClkSelSet(1);

    /* 5. 释放复位 */
    RCC->AHB6RSTCLRR = (1U << GMAC_TEST_ETH_RST_BIT);
}


/*
 * GPIO: 全部 ETH 引脚 (RGMII, AF11)
 *   PH2 = PHY 复位 — AF11 是 CRS, 复位必须用 GPIO 推挽输出做低脉冲
 */
static void GmacGpioInit(void)
{
    RCC->MP_AHB4ENSETR |= (1U << 0) | (1U << 1) | (1U << 2)
                        | (1U << 4) | (1U << 6) | (1U << 7); /* GPIOA,B,C,E,G,H */

    /* ---- PH2 = PHY 复位脚 (GPIO 推挽输出, 复位时序见 GmacPhyReset) ---- */
    GpioMode(GPIO_H, 2, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_H, 2, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_H, 2, GPIO_OSPEED_HI);

    /* ---- PC1 = MDC (输出) ---- */
    GpioMode(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_MODER_AF);
    GpioAf(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GMAC_TEST_MDIO_AF);
    GpioOtype(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_OSPEED_HI);
    GpioPullUpDown(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_PUPDR_NO);

    /* ---- PA2 = MDIO (双向) ---- */
    GpioMode(GMAC_TEST_MDIO_PORT, GMAC_TEST_MDIO_PIN, GPIO_MODER_AF);
    GpioAf(GMAC_TEST_MDIO_PORT, GMAC_TEST_MDIO_PIN, GMAC_TEST_MDIO_AF);
    GpioOtype(GMAC_TEST_MDIO_PORT, GMAC_TEST_MDIO_PIN, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GMAC_TEST_MDIO_PORT, GMAC_TEST_MDIO_PIN, GPIO_OSPEED_HI);
    GpioPullUpDown(GMAC_TEST_MDIO_PORT, GMAC_TEST_MDIO_PIN, GPIO_PUPDR_NO);

    /* ---- TX 输出 (AF11) ---- */
    GpioMode(GPIO_G, 13, GPIO_MODER_AF);  /* PG13 = TXD0 */
    GpioAf(GPIO_G, 13, 11);
    GpioOtype(GPIO_G, 13, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_G, 13, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_G, 13, GPIO_PUPDR_NO);

    GpioMode(GPIO_G, 14, GPIO_MODER_AF);  /* PG14 = TXD1 */
    GpioAf(GPIO_G, 14, 11);
    GpioOtype(GPIO_G, 14, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_G, 14, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_G, 14, GPIO_PUPDR_NO);

    GpioMode(GPIO_C, 2, GPIO_MODER_AF);   /* PC2 = TXD2 */
    GpioAf(GPIO_C, 2, 11);
    GpioOtype(GPIO_C, 2, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_C, 2, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_C, 2, GPIO_PUPDR_NO);

    GpioMode(GPIO_E, 2, GPIO_MODER_AF);   /* PE2 = TXD3 */
    GpioAf(GPIO_E, 2, 11);
    GpioOtype(GPIO_E, 2, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_E, 2, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_E, 2, GPIO_PUPDR_NO);

    GpioMode(GPIO_B, 11, GPIO_MODER_AF);  /* PB11 = TX_EN */
    GpioAf(GPIO_B, 11, 11);
    GpioOtype(GPIO_B, 11, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_B, 11, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_B, 11, GPIO_PUPDR_NO);

    GpioMode(GPIO_G, 4, GPIO_MODER_AF);   /* PG4 = GTX_CLK (125MHz 输出) */
    GpioAf(GPIO_G, 4, 11);
    GpioOtype(GPIO_G, 4, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_G, 4, GPIO_OSPEED_VERY_HI);
    GpioPullUpDown(GPIO_G, 4, GPIO_PUPDR_NO);

    /* ---- RX 输入 (AF11) ---- */
    GpioMode(GPIO_C, 4, GPIO_MODER_AF);   /* PC4 = RXD0 */
    GpioAf(GPIO_C, 4, 11);
    GpioOspeed(GPIO_C, 4, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_C, 4, GPIO_PUPDR_NO);

    GpioMode(GPIO_C, 5, GPIO_MODER_AF);   /* PC5 = RXD1 */
    GpioAf(GPIO_C, 5, 11);
    GpioOspeed(GPIO_C, 5, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_C, 5, GPIO_PUPDR_NO);

    GpioMode(GPIO_B, 0, GPIO_MODER_AF);   /* PB0 = RXD2 */
    GpioAf(GPIO_B, 0, 11);
    GpioOspeed(GPIO_B, 0, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_B, 0, GPIO_PUPDR_NO);

    GpioMode(GPIO_B, 1, GPIO_MODER_AF);   /* PB1 = RXD3 */
    GpioAf(GPIO_B, 1, 11);
    GpioOspeed(GPIO_B, 1, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_B, 1, GPIO_PUPDR_NO);

    GpioMode(GPIO_A, 7, GPIO_MODER_AF);   /* PA7 = RX_DV */
    GpioAf(GPIO_A, 7, 11);
    GpioOspeed(GPIO_A, 7, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_A, 7, GPIO_PUPDR_NO);

    GpioMode(GPIO_A, 1, GPIO_MODER_AF);   /* PA1 = RX_CLK */
    GpioAf(GPIO_A, 1, 11);
    GpioOspeed(GPIO_A, 1, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_A, 1, GPIO_PUPDR_NO);

    GpioMode(GPIO_G, 5, GPIO_MODER_AF);   /* PG5 = CLK125 */
    GpioAf(GPIO_G, 5, 11);
    GpioOspeed(GPIO_G, 5, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_G, 5, GPIO_PUPDR_NO);

    /* ---- PG12 = PHY 中断 (AF11 输入, 后续接 EXTI) ---- */
    GpioMode(GPIO_G, 12, GPIO_MODER_AF);
    GpioAf(GPIO_G, 12, 11);
    GpioOspeed(GPIO_G, 12, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_G, 12, GPIO_PUPDR_NO);
}


/*
 * PHY 复位: PH2 低脉冲 ~20ms 后拉高, 等 PHY 内部初始化完成.
 * 与 PHY 配置同属 PHY 操作, 后续 PHY init/link 测试沿用.
 */
static void GmacPhyReset(void)
{
    GpioOutLow(GPIO_H, 2);
    {
        volatile uint32_t delay = 10000000;
        while (delay--);
    }
    GpioOutHi(GPIO_H, 2);
}


/*
 * 公共初始化: 时钟 + GPIO + PHY 复位 + 软复位 + MDC 分频
 * @return 1 = 成功, 0 = 软复位超时 (PHY 时钟不在场)
 *
 * 软复位完成不了说明 PHY 输入时钟未配置, 后续 MDIO 不会有响应.
 */
static int GmacTest_Init(void)
{
    GmacMdioCfg_t mdio_cfg;

    GmacClockEnable();
    GmacGpioInit();
    GmacPhyReset();

    /* 软复位失败 = SWR 未自清零 = 复位超时 (PHY 时钟未配置) */
    if (!GmacSoftReset(ETH))
    {
        PRINT("  WARN: soft reset timeout (PHY clk?)\r\n");
        return 0;
    }

    mdio_cfg.gmac_mdc_div = GMAC_TEST_MDC_DIV;
    GmacMdioInit(ETH, &mdio_cfg);
    return 1;
}


/* ================================================================== */
/*  子测试 1: MDIO 扫描 PHY                                             */
/* ================================================================== */

/*
 * 扫描 0~31 地址, 读 PHY ID1/ID2 (reg 2/3).
 * 读到非 0xFFFF 即认为该地址有 PHY, 打印地址 + ID.
 */
static void TestGmacMdioScan(void)
{
    char buf[16];
    int found = 0;
    int i;

    if (!GmacTest_Init())
        return;

    for (i = 0; i < 32; i++)
    {
        uint16_t id1, id2;

        /* 读失败 (超时) 或读到全 1 都视为该地址无 PHY */
        if (!GmacMdioRead(ETH, i, 2, &id1) || !GmacMdioRead(ETH, i, 3, &id2))
            continue;
        if ((id1 == 0xFFFF) && (id2 == 0xFFFF))
            continue;

        found = 1;
        PRINT("  PHY found @ addr ");
        NumToStr(buf, i);
        PRINT(buf);
        PRINT("  ID1=0x");
        PrintHex32(buf, id1);
        PRINT(buf);
        PRINT("  ID2=0x");
        PrintHex32(buf, id2);
        PRINT(buf);
        PRINT("\r\n");
    }

    if (!found)
        PRINT("  no PHY found\r\n");
}


/* ================================================================== */
/*  子测试菜单 + 入口                                                   */
/* ================================================================== */

static const MenuEntry_t g_gmac_menu[] = {
    { "1", "MDIO scan PHY",  TestGmacMdioScan },
};

void GmacTest(void)
{
    RunSubMenu("GMAC Ethernet Test",
               g_gmac_menu, sizeof(g_gmac_menu) / sizeof(g_gmac_menu[0]));
}
