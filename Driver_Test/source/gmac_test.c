/*
 * gmac_test.c — GMAC 测试
 *
 *   GmacTest — 测试入口 (menu_test 第 23 项, 二级菜单)
 *     测试 1: MDIO 全地址扫描 (0~31), ID 对照 + reg0/reg1 已知明文旁证
 *     测试 2: 响应地址寄存器栅格图 (reg0~31 全量转储, 定位寻址偏差)
 *     测试 3: INER(reg18) 写读回环, 验证写路径与采样对齐
 *     测试 4: 完整激活 — 找 PHY → 等自协商/link → SWR → MAC/DMA 启动
 *     测试 5: 收包监控 ~12s (轮询 RI, 回显帧长/状态/头部字节)
 *     测试 6: 发一帧 60 字节最小广播, 回显写回状态与 MMC 计数
 *     测试 7: MAC 内部回环 (LM 位 + 内部 rx 参考钟, 免网线验证收发管线)
 *
 *  Created on: 2026-8-26
 *      Author: gjsbr
 */

#include "stm32mp1xx_gmac.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_syscfg.h"
#include "stm32mp1xx_btimer.h"
#include "common_test.h"


/* ================================================================== */
/*  测试配置 (板级参数, 待确认后填写)                                    */
/* ================================================================== */

#define GMAC_TEST_ETH_EN_BIT   10     /* MP_AHB6ENSETR: ETHMACEN 总线时钟 (hclk6/aclk) */
#define GMAC_TEST_ETH_CKEN_BIT 7      /* MP_AHB6ENSETR: ETHCKEN 内核时钟 (eth_ker_ck) */
#define GMAC_TEST_ETH_TXEN_BIT 8      /* MP_AHB6ENSETR: ETHTXEN TX 时钟路径 (GTX_CLK+clk_tx_i) */
#define GMAC_TEST_ETH_RXEN_BIT 9      /* MP_AHB6ENSETR: ETHRXEN RX 时钟路径 (clk_rx_i) */
#define GMAC_TEST_ETH_RST_BIT  10     /* AHB6RSTSETR: ETH 复位位号 (与使能同号) */

/* MDC/MDIO 引脚: PC1/PA2, AF11 */
#define GMAC_TEST_MDC_PORT     GPIO_C
#define GMAC_TEST_MDC_PIN      1
#define GMAC_TEST_MDIO_PORT    GPIO_A
#define GMAC_TEST_MDIO_PIN     2
#define GMAC_TEST_MDIO_AF      11

/* MDC 分频: HCLK≤266MHz 时 /124 得 ≤2.1MHz, 满足 IEEE ≤2.5MHz (待确认) */
#define GMAC_TEST_MDC_DIV      124

/* 内核时钟源: ETHCKSELR.ETHSRC, 0 = pll4_p_ck (125MHz, RGMII 千兆, 测试时配置)
 * ETH 与 LTDC 不同时使用: 跑 GMAC 测试时 PLL4_P=125MHz 给 ETH, Q 不使能;
 * 跑显示测试时 PLL4_Q=74.25MHz 给 LTDC, P 不使能 (P/Q 整数分频无法同时精确) */
#define GMAC_TEST_ETH_SRC      0

/* INER = 中断使能寄存器 (reg18): 复位默认全 0, 几乎全 RW, 拿来做写读回环最安全 */
#define GMAC_TEST_REG_INER     18

/* RTL8211F 出厂身份 (Rev1.1 手册 §8.4.3/8.4.4):
 * ID1 恒 0x001C; ID2 = OUI_LSB(110010)+Model(010001)+Rev(0110) = 0xC916,
 * revision 位允许批次差异, 只比对高 12 位 */


/* ================================================================== */
/*  公共初始化                                                         */
/* ================================================================== */

/*
 * PLL4: 24MHz HSE → 125MHz P 输出 (RGMII 千兆内核时钟)
 *   Fref = 24 / (3+1) = 6MHz
 *   DIVN=124, 整数模式 → ×125 → VCO=750MHz (规格 400-800MHz 内)
 *   DIVP=5 → /6 → PLL4_P = 125MHz
 *   Q/R 不使能 (显示测试时由 DisplayTest_ClockCfg 重配 PLL4 出 74.25MHz 给 LTDC)
 */
static void GmacPll4Init(void)
{
    RccPll34Cfg_t pll4;

    /* PLL4 source: HSE */
    RccPll4SrcSelect(RCC_PLLSRC_HSE);

    pll4.divm   = 3;
    pll4.divn   = 124;
    pll4.fracv  = 0;
    pll4.divp   = 5;
    pll4.en_p   = 1;
    pll4.divq   = 0;
    pll4.en_q   = 0;
    pll4.divr   = 0;
    pll4.en_r   = 0;
    pll4.ifrge  = 0;
    RccPll4Init(&pll4);
}


/*
 * RCC: ETH 时钟源选择 + 时钟使能 + 复位期间配置 SYSCFG + 释放复位
 *   BIT10 ETHMACEN = 总线时钟 (hclk6/aclk, 寄存器访问+DMA)
 *   BIT7  ETHCKEN  = 内核时钟 (eth_ker_ck, 软复位需要, 源由 ETHCKSELR 选)
 *   ETH_SEL/ETH_CLK_SEL 必须在 ETH 处于复位期间、释放复位前配置 (手册 Note).
 */
static void GmacClockEnable(void)
{
    /* 0. 先配好 PLL4_P=125MHz, 再选作 ETH 内核时钟源 */
    GmacPll4Init();

    /* 1. 内核时钟源: ETHSRC = pll4_p_ck (125MHz) */
    RCC->ETHCKSELR = (RCC->ETHCKSELR & ~(3U << 0)) | ((uint32_t)GMAC_TEST_ETH_SRC << 0);

    /* 2. 使能 ETH 全部时钟路径: 总线 + 内核 + TX/RX (少一个 SWR 复位链都过不去) */
    RCC->MP_AHB6ENSETR |= (1U << GMAC_TEST_ETH_EN_BIT)    /* ETHMACEN */
                        | (1U << GMAC_TEST_ETH_CKEN_BIT)  /* ETHCKEN  */
                        | (1U << GMAC_TEST_ETH_TXEN_BIT)  /* ETHTXEN  */
                        | (1U << GMAC_TEST_ETH_RXEN_BIT); /* ETHRXEN  */

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
 * PHY 复位: PH2 低脉冲 + 释放后等待, 时序用 TIM7 精确毫秒延时.
 * 手册 §7.17: 拉低 ≥10ms, 释放后再等 ≥30ms 才能访问寄存器.
 * 延时实现复制 btimer_test 的用法 (64MHz 内核时钟, 已实测).
 */
static void GmacTim7Init(void)
{
    BasicTimerCfg_t cfg;

    RCC->MP_APB1ENSETR |= 1 << 5;       /* TIM7 时钟使能 (APB1 bit5) */

    cfg.tim_psc  = 6399;                /* 64MHz / 6400 = 10kHz 计数 */
    cfg.tim_arr  = 9;                   /* 10 计数一次更新 = 1ms */
    cfg.tim_arpe = 1;
    cfg.tim_opm  = 0;
    cfg.tim_urs  = 0;
    cfg.tim_udis = 0;
    cfg.tim_ude  = 0;
    cfg.tim_uie  = 0;

    BasicTimerCfg(TIM7, &cfg);
    BasicTimerUg(TIM7);                 /* 装载 PSC/ARR 并复位计数 */
}

static void GmacTim7StartDelay(uint32_t ms)
{
    uint32_t uif = 0;

    TIM7->SR |= 1;                      /* 清掉残留 UIF (含 Ug 产生的) */
    BasicTimerStart(TIM7);

    while (uif < ms)
    {
        if (TIM7->SR & 1)
        {
            TIM7->SR |= 1;
            uif++;
        }
    }

    BasicTimerStop(TIM7);
}

/* TIM7 只需配一次 (菜单反复进出测试) */
static int g_tim7_inited = 0;

/*
 * PHY 复位时序 (docs/rtl8211f_datasheet.md §7.17):
 *   拉低 ≥10ms (内部稳压器完全起效)
 *   释放后等 ≥30ms (晶振起振+PLL 锁定+内部稳态), 之后才能访问寄存器,
 *   否则紧接着的软复位会因 RX 时钟域无时钟而超时.
 */
static void GmacPhyReset(void)
{
    if (!g_tim7_inited)
    {
        GmacTim7Init();
        g_tim7_inited = 1;
    }

    GpioOutLow(GPIO_H, 2);
    GmacTim7StartDelay(12);

    GpioOutHi(GPIO_H, 2);
    GmacTim7StartDelay(35);
}


/*
 * 公共初始化 (仅 MDIO 探测所需最小集): 时钟 + GPIO + PHY 复位 + MDC 分频
 * @return 1 = 恒成功 (探测测试不依赖 MAC 内部状态)
 *
 * 注意: 此处不做 GMAC 软复位 —— 按启动流程, SWR 应放在链路建立、PHY 回送
 * RX_CLK 之后执行 (DMAMR.SWR 自清零需要所有激活时钟域的时钟在场),
 * 无 link 时提前调用必然超时. MDIO/SMA 只依赖 CSR(eth_hclk) 时钟域
 * 和 RCC 总线使能, 复位释放后的寄存器默认态即可工作, 无需 SWR.
 */
static int GmacTest_Init(void)
{
    GmacMdioCfg_t mdio_cfg;

    GmacClockEnable();
    GmacGpioInit();
    GmacPhyReset();

    mdio_cfg.gmac_mdc_div = GMAC_TEST_MDC_DIV;
    GmacMdioInit(ETH, &mdio_cfg);
    return 1;
}


/* ================================================================== */
/*  子测试 1: MDIO 全地址扫描                                          */
/* ================================================================== */

#define RTL_ID1_EXPECT   0x001C
#define RTL_ID2_BASEMASK 0xFFF0
#define RTL_ID2_BASEVAL  0xC910

/*
 * 判断 ID 对是否符合 RTL8211F 家族出厂数据 (屏蔽 revision 位).
 */
static int RtlIdMatch(uint16_t id1, uint16_t id2)
{
    return (id1 == RTL_ID1_EXPECT)
        && ((id2 & RTL_ID2_BASEMASK) == RTL_ID2_BASEVAL);
}

/* 该地址是否在总线上应答 (读到非全 1 且不超时) */
static int MdioAddrAlive(uint32_t phy, uint16_t *id1_out, uint16_t *id2_out)
{
    uint16_t id1, id2;

    if (!GmacMdioRead(ETH, phy, 2, &id1))
        return 0;
    if (id1 == 0xFFFF)
        return 0;
    if (!GmacMdioRead(ETH, phy, 3, &id2))
        return 0;
    if (id2 == 0xFFFF)
        return 0;

    *id1_out = id1;
    *id2_out = id2;
    return 1;
}

/*
 * 测试 1: MDIO 全地址扫描 (0~31), 每个地址都打印, 不静默跳过.
 *   超时 / FF 无应答 / 真实应答 三种结果分开显示,
 *   应答者额外读 reg0(BMCR)/reg1(BMSR) 作已知明文旁证:
 *   复位默认应为 ~0x1140 / ~0x79xx (docs/rtl8211f_datasheet.md 附录A).
 *
 *   注意: 地址 0 是广播保留值, RTL8211F 默认应答广播命令 (Reg24.13=1),
 *   addr0 有响应 ≠ 设备真实地址; strap 决定的真实地址应在 1~7 内.
 */
static void TestGmacMdioScan(void)
{
    char buf[16];
    int i;

    if (!GmacTest_Init())
        return;

    /* MDIOAR 回读: CR 分频域写入是否仍生效 */
    PRINT("  MDIOAR=0x");
    PrintHex32(buf, ETH->MAC.MDIOAR);
    PRINT(buf);
    PRINT("\r\n");

    PRINT("  -- addr  ID1     ID2     BMCR    BMSR    verdict\r\n");

    for (i = 0; i < 32; i++)
    {
        uint16_t id1, id2, bmcr = 0, bmsr = 0;

        if (!MdioAddrAlive(i, &id1, &id2))
        {
            PRINT("  ");
            NumToStr(buf, i);
            PRINT(buf);
            PRINT(":       -- (no response / idle)\r\n");
            continue;
        }

        GmacMdioRead(ETH, i, 0, &bmcr);
        GmacMdioRead(ETH, i, 1, &bmsr);

        PRINT("  ");
        NumToStr(buf, i);
        PRINT(buf);
        PRINT(":     0x");
        PrintHex32(buf, id1);
        PRINT(buf);
        PRINT("  0x");
        PrintHex32(buf, id2);
        PRINT(buf);
        PRINT("  0x");
        PrintHex32(buf, bmcr);
        PRINT(buf);
        PRINT("  0x");
        PrintHex32(buf, bmsr);
        PRINT(buf);
        PRINT("  ");

        if (RtlIdMatch(id1, id2))
            PRINT("[RTL8211F confirmed]\r\n");
        else if ((id1 == RTL_ID1_EXPECT) ||
                 ((id2 & RTL_ID2_BASEMASK) == RTL_ID2_BASEVAL))
            PRINT("[Realtek family, odd ID]\r\n");
        else
            PRINT("[unknown responder]\r\n");
    }
}


/* ================================================================== */
/*  子测试 2: 响应地址寄存器栅格图                                      */
/* ================================================================== */

/*
 * 测试 2: 对每个应答地址把 reg0~31 全量转储一遍.
 *   用途: 若 MDIOAR 的 PB(PHY 地址)/RB(寄存器地址)字段组装有偏差,
 *   整表会呈现"行间搬运"指纹 (读 reg N 得到别的物理寄存器),
 *   与附录 A 的复位默认值形态直接对照即可定位偏移量.
 *   锚点行带提示注解.
 */
static void TestGmacRegDump(void)
{
    static const struct { uint8_t reg; const char *hint; } anchor[] = {
        { 0,  "<BMCR  exp~1140>" },
        { 1,  "<BMSR  exp~79xx>" },
        { 2,  "<ID1   exp 001C>" },
        { 3,  "<ID2   exp C91x>" },
        { 15, "<GBESR exp 2000>" },
        { 18, "<INER  def 0000>" },
        { 19, "<INSR  ro/rc>" },
        { 24, "<PHYCR1 bcast=en>" },
        { 25, "<PHYCR2 RXC-EN!>" },
        { 26, "<PHYSR realtime>" },
        { 31, "<PAGSR page>" },
    };
    char buf[16];
    int i, r, k;
    uint16_t v;

    if (!GmacTest_Init())
        return;

    for (i = 0; i < 32; i++)
    {
        uint16_t id1, id2;

        if (!MdioAddrAlive(i, &id1, &id2))
            continue;

        PRINT("  ---- PHY @addr ");
        NumToStr(buf, i);
        PRINT(buf);
        PRINT("  ID 0x");
        PrintHex32(buf, id1);
        PRINT(buf);
        PRINT("/0x");
        PrintHex32(buf, id2);
        PRINT(buf);
        PRINT(" ----\r\n");

        for (r = 0; r < 32; r++)
        {
            if (!GmacMdioRead(ETH, i, r, &v))
                continue;

            PRINT("  ");
            if (r < 10)
                PRINT(" ");
            NumToStr(buf, r);
            PRINT(buf);
            PRINT(": 0x");
            PrintHex32(buf, v);
            PRINT(buf);

            for (k = 0; k < (int)(sizeof(anchor) / sizeof(anchor[0])); k++)
            {
                if (anchor[k].reg == r)
                {
                    PRINT("  ");
                    PRINT(anchor[k].hint);
                }
            }

            PRINT("\r\n");
        }
    }
}


/* ================================================================== */
/*  子测试 3: INER 写读回环                                             */
/* ================================================================== */

/*
 * 测试 3: INER(reg18) 写读回环, 验证写路径与采样对齐.
 *   选 INER 当小白鼠: 默认全 0, 几乎全域 RW, 写错顶多误开中断且最后恢复.
 *   判据: 两个异构模式都能原样回显 => 读写双向 OK, 问题只剩身份判定;
 *         回显有规律变形 => 写路径/TA 采样有问题.
 */
static void TestGmacInerEcho(void)
{
    char buf[16];
    int i;
    int t;
    uint16_t v;
    const uint16_t pattern[2] = { 0x55AA, 0xAA55 };

    if (!GmacTest_Init())
        return;

    for (i = 0; i < 32; i++)
    {
        uint16_t id1, id2;

        if (!MdioAddrAlive(i, &id1, &id2))
            continue;

        PRINT("  echo test on PHY @addr ");
        NumToStr(buf, i);
        PRINT(buf);
        PRINT("\r\n");

        for (t = 0; t < 2; t++)
        {
            GmacMdioWrite(ETH, i, GMAC_TEST_REG_INER, pattern[t]);
            GmacMdioRead(ETH, i, GMAC_TEST_REG_INER, &v);

            PRINT("    write 0x");
            PrintHex32(buf, pattern[t]);
            PRINT(buf);
            PRINT(" -> read 0x");
            PrintHex32(buf, v);
            PRINT(buf);
            PRINT("  ");

            if (v == pattern[t])
                PRINT("[OK]\r\n");
            else
                PRINT("[MISMATCH]\r\n");
        }

        /* 恢复默认: 中断全部禁用 */
        GmacMdioWrite(ETH, i, GMAC_TEST_REG_INER, 0);
        GmacMdioRead(ETH, i, GMAC_TEST_REG_INER, &v);

        PRINT("    restore 0x");
        PrintHex32(buf, v);
        PRINT(buf);
        if (v == 0)
            PRINT(" [OK]\r\n");
        else
            PRINT(" [FAIL]\r\n");
        return;
    }

    PRINT("  no responder found\r\n");
}


/* ================================================================== */
/*  完整链路激活 (验身份 → 等 link → SWR → MAC/DMA)                     */
/* ================================================================== */

/* 描述符预填位 (docs/gmac_desc.md) */
#define DESC_OWN    (1U << 31)
#define DESC_IOC    (1U << 30)
#define DESC_FD     (1U << 29)
#define DESC_LD     (1U << 28)
#define DESC_BUF1V  (1U << 24)

/* 通道状态位 (ETH_DMACxSR, rc_w1) */
#define CHAN_SR_TI  (1U << 0)
#define CHAN_SR_RI  (1U << 6)

/* PHY 寄存器 */
#define REG_BMSR        1
#define BMSR_AN_COMPLETE (1U << 5)
#define REG_PHYSR       26

static const uint8_t g_mac_addr[6] = { 0x02, 0x12, 0x34, 0x56, 0x78, 0x90 };

#define GMAC_TEST_PHY_ADDR   1      /* strap 默认 001b; 以原理图为准 */
#define RX_RING_N            4
#define TX_RING_N            2
#define RX_BUF_SZ            1536   /* RBSZ: 8 的倍数 */

static GmacDesc_t rx_desc[RX_RING_N] __attribute__((aligned(8)));
static uint8_t    rx_buf[RX_RING_N][RX_BUF_SZ] __attribute__((aligned(8)));
static GmacDesc_t tx_desc[TX_RING_N] __attribute__((aligned(8)));
static uint8_t    tx_buf[TX_RING_N][64] __attribute__((aligned(8)));

static int      g_activated = 0;         /* 激活流程已跑过 */
static int      g_rx_cur    = 0;         /* 收包服务游标 */
static int      g_tx_next   = 0;         /* 下一次发送占用的槽位 */

/*
 * 找 RTL8211F: 先试配置地址, 失败则全地址段扫描按出厂 ID 匹配.
 * @return >=0 = PHY 地址, -1 = 总线上没有匹配者
 */
static int FindRtlPhy(void)
{
    char buf[16];
    int i;
    uint16_t id1, id2;

    if (MdioAddrAlive(GMAC_TEST_PHY_ADDR, &id1, &id2) && RtlIdMatch(id1, id2))
        return GMAC_TEST_PHY_ADDR;

    for (i = 0; i < 32; i++)
    {
        if (!MdioAddrAlive(i, &id1, &id2))
            continue;
        if (!RtlIdMatch(id1, id2))
            continue;
        PRINT("  note: expected addr ");
        NumToStr(buf, GMAC_TEST_PHY_ADDR);
        PRINT(buf);
        PRINT(", found ID match @ ");
        NumToStr(buf, i);
        PRINT(buf);
        PRINT("\r\n");
        return i;
    }
    return -1;
}

/*
 * 等自协商完成 + link up. 链路状态以 MAC 侧 ETH_MACPHYCSR 为准
 * (它读的是 RGMII 接口上 PHY 实际回送的状态信号), PHYSR 原值一并打印作对照.
 *
 * @param phy        PHY 地址
 * @param spd_out/fdx_out 输出: 协商速率 Mbps / 全双工标志
 * @return 0 = link up 并解出速率双工; -1 超时无对端
 */
static int WaitPhyLink(int phy, uint32_t *spd_out, uint32_t *fdx_out)
{
    char buf[16];
    int i;
    uint16_t physr = 0;

    PRINT("  waiting AN complete (~4s max, 需对端在位如光猫/交换机)\r\n");
    for (i = 0; i < 200; i++)
    {
        uint16_t bmsr;
        if (!GmacMdioRead(ETH, phy, REG_BMSR, &bmsr))
            return -1;
        if (bmsr & BMSR_AN_COMPLETE)
            break;
        GmacTim7StartDelay(20);
    }
    if (i >= 200)
    {
        PRINT("  [FAIL] AN not completed (check cable / peer)\r\n");
        return -1;
    }

    for (i = 0; i < 60; i++)
    {
        if (ETH->MAC.PHYCSR & (1U << 19))          /* LNKSTS */
            break;
        GmacMdioRead(ETH, phy, REG_PHYSR, &physr); /* 顺带刷新原值供打印 */
        GmacTim7StartDelay(50);
    }
    if (i >= 60)
    {
        PRINT("  [FAIL] no RGMII link signal\r\n");
        return -1;
    }

    /* MAC 视角: LNKSPEED[18:17]: 00=2.5MHz/10M, 01=25MHz/100M, 10=125MHz/1000M
     *           LNKMOD[16]: 1=全双工 */
    {
        static const uint32_t spd_tab[3] = { 10, 100, 1000 };
        uint32_t spd_idx = (ETH->MAC.PHYCSR >> 17) & 3U;

        *fdx_out = (ETH->MAC.PHYCSR >> 16) & 1U;
        *spd_out = spd_tab[(spd_idx > 2u) ? 2u : spd_idx];
    }

    PRINT("  PHYSR(reg26)=0x");
    PrintHex32(buf, physr);
    PRINT(buf);
    PRINT("  (对照 rtl 笔记, 位图待核)\r\n");
    return 0;
}

/*
 * 测试 4: 完整激活 — 验 PHY 身份 → 等自协商/link → SWR → MAC 配置 → DMA 启动
 * 通过后链路进入可用状态, 测试 5/6 在其之上收发.
 */
static void TestGmacActivate(void)
{
    volatile GmacDesc_t *d;
    char buf[16];
    int phy, i;
    uint32_t speed = 1000, fdx = 1;

    GmacTest_Init();

    phy = FindRtlPhy();
    if (phy < 0)
    {
        PRINT("  [STOP] no RTL8211F responded (run test 1 for details)\r\n");
        return;
    }
    PRINT("  PHY @addr ");
    NumToStr(buf, phy);
    PRINT(buf);
    PRINT(", ID OK\r\n");

    if (WaitPhyLink(phy, &speed, &fdx) != 0)
        return;
    PRINT("  link ");
    NumToStr(buf, (int)speed);
    PRINT(buf);
    PRINT(fdx ? "Mbps full-duplex\r\n" : "Mbps half-duplex\r\n");

    /* ---- SWR: 此刻 RX_CLK 在场, 应当干净通过 ---- */
    if (!GmacSoftReset(ETH))
    {
        PRINT("  [STOP] soft reset timeout even with link?!\r\n");
        return;
    }
    PRINT("  soft reset OK\r\n");

    /* ---- MDIO 分频被 SWR 清掉, 重配 ---- */
    {
        GmacMdioCfg_t mdio_cfg;
        mdio_cfg.gmac_mdc_div = GMAC_TEST_MDC_DIV;
        GmacMdioInit(ETH, &mdio_cfg);
    }

    /* ---- MAC 按协商结果配置 (63.9.3 步骤顺序), RE/TE 暂不使能 ---- */
    {
        GmacMacCfg_t mac_cfg;
        uint8_t k;

        for (k = 0; k < 6; k++)
            mac_cfg.gmac_mac_addr[k] = g_mac_addr[k];
        mac_cfg.gmac_speed     = speed;
        mac_cfg.gmac_duplex    = fdx;
        mac_cfg.gmac_jumbo     = 0;
        mac_cfg.gmac_strip_crc = 0;
        GmacMacInit(ETH, &mac_cfg);
    }

    /* ---- Rx 环预填 + 启动 ---- */
    for (i = 0; i < (int)RX_RING_N; i++)
    {
        d = &rx_desc[i];
        d->DES0 = (uint32_t)&rx_buf[i][0];
        d->DES1 = 0;
        d->DES2 = 0;
        d->DES3 = DESC_OWN | DESC_IOC | DESC_BUF1V;
    }
    GmacDmaRxStart(ETH, rx_desc, RX_RING_N, RX_BUF_SZ);

    /* ---- Tx 环清零待提交 ---- */
    for (i = 0; i < (int)TX_RING_N; i++)
    {
        d = &tx_desc[i];
        d->DES0 = (uint32_t)&tx_buf[i][0];
        d->DES1 = 0;
        d->DES2 = 0;
        d->DES3 = 0;
    }
    GmacDmaTxStart(ETH, tx_desc, TX_RING_N);

    /* ---- 最后一步: RE/TE ---- */
    GmacMacStart(ETH);

    g_activated = 1;
    g_rx_cur = 0;
    g_tx_next = 0;

    PRINT("  ACTIVATED. rings rx=");
    PrintHex32(buf, (uint32_t)rx_desc);
    PRINT(buf);
    PRINT(" tx=");
    PrintHex32(buf, (uint32_t)tx_desc);
    PRINT(buf);
    PRINT("\r\n  use test 5 to watch incoming broadcast,\r\n"
          "       test 6 to send a frame\r\n");
}

/*
 * 回收一个已关闭描述符并推进尾指针重灌窗口.
 * 尾指针指向被回收槽位的下一格: HW 光标之前就掠过了它,
 * 重置 OWN 后推进 tail 即把它重新交给硬件.
 */
static void RxServiceRing(void)
{
    volatile GmacDesc_t *d;
    char buf[16];
    int served;

    for (served = 0; served < RX_RING_N; served++)
    {
        volatile uint8_t *p;
        uint32_t w3;
        int n, k;

        d = &rx_desc[g_rx_cur];
        if (d->DES3 & DESC_OWN)
            break;                          /* 该格仍归 DMA, 后面必然更没到货 */

        w3 = d->DES3;
        if ((w3 & DESC_LD) == 0)
        {
            /* 罕见: 单缓冲配置不应出现跨描述符包, 直接丢弃重灌 */
            d->DES3 = DESC_OWN | DESC_IOC | DESC_BUF1V;
        }
        else
        {
            uint32_t len = w3 & 0x7FFFU;
            uint32_t es  = (w3 >> 15) & 1U;

            PRINT("  RX slot ");
            NumToStr(buf, g_rx_cur);
            PRINT(buf);
            PRINT(" len=");
            NumToStr(buf, (int)(es ? (len - 4u) : len)); /* ES 时 CRC 已剥另计 */
            PRINT(buf);
            PRINT(es ? " ES(err!)" : " ok");

            p = &rx_buf[g_rx_cur][0];
            for (n = 0; n < 16 && (uint32_t)n < len; n++)
            {
                static const char hd[17] = "0123456789ABCDEF";
                char pair[3];

                k = p[n];
                pair[0] = hd[(k >> 4) & 0xF];
                pair[1] = hd[k & 0xF];
                pair[2] = '\0';
                if (n % 16 == 0)
                    PRINT("\r\n      ");
                else if (n % 8 == 0)
                    PRINT(" ");
                PRINT(pair);
            }
            PRINT("\r\n");
        }

        d->DES3 = DESC_OWN | DESC_IOC | DESC_BUF1V;  /* 归还硬件复用 */
        g_rx_cur = (g_rx_cur + 1) % RX_RING_N;
        ETH->DMA.CH[0].RXDTPR =
            (uint32_t)&rx_desc[g_rx_cur];            /* 尾指针跟进新边界 */
        break;                                       /* 其余下轮再看 */
    }
}

/*
 * 测试 5: 收包监控 ~12s. 理想素材 = 光猫的广播帧 (ARP/IPv6 RA 等).
 * 轮询通道 RI 而非走中断, 免去 EXTI/GIC 配置, 与现有测试框架同构.
 */
static void TestGmacRxMonitor(void)
{
    if (!g_activated)
    {
        PRINT("  run test 4 first\r\n");
        return;
    }

    PRINT("  monitoring ~12s ...\r\n");
    {
        int t;
        for (t = 0; t < 240; t++)
        {
            if (ETH->DMA.CH[0].SR & CHAN_SR_RI)
            {
                ETH->DMA.CH[0].SR |= CHAN_SR_RI;     /* rc_w1 */
                RxServiceRing();
            }
            GmacTim7StartDelay(50);
        }
    }
    PRINT("  monitor done\r\n");
}

static void BuildTestFrame(uint8_t *f);

/*
 * 测试 6: 发一帧最小以太网广播 (dst=FF*6, ethertype 0x88B5 本地实验用).
 * 长度给 60 字节, CPC=00 让 MAC 自动补 Pad+CRC → 上线即合法最小帧.
 * 发送用双槽交替, 每次等上一槽完成再提交下一槽.
 */
static void TestGmacTxBroadcast(void)
{
    volatile GmacDesc_t *d;
    char buf[16];
    uint8_t *f;
    int s, t;

    if (!g_activated)
    {
        PRINT("  run test 4 first\r\n");
        return;
    }

    s = g_tx_next;
    f = &tx_buf[s][0];

    /* 组帧 (与回环测试同一字节模式): 广播目的 + 板卡源 MAC + 0x88B5 + 花样 */
    BuildTestFrame(f);

    d = &tx_desc[s];
    d->DES0 = (uint32_t)f;
    d->DES1 = 0;
    d->DES2 = DESC_IOC | 60u;                           /* B1L=60, 非 TSO 长度放这里 */
    d->DES3 = DESC_OWN | DESC_FD | DESC_LD;             /* CPC=00: 补 Pad+CRC */

    /* 提交: 尾指针 = 提交格的下一格; 双槽交替时窗口恰好只含本槽 */
    ETH->DMA.CH[0].TXDTPR = (uint32_t)&tx_desc[s ^ 1];

    /* 等完成 (TI 或 OWN 自清, 500ms 上限) */
    for (t = 0; t < 100; t++)
    {
        if (!(d->DES3 & DESC_OWN))
            break;
        GmacTim7StartDelay(5);
    }

    if (d->DES3 & DESC_OWN)
    {
        PRINT("  [TIMEOUT] desc not closed\r\n");
        return;
    }

    PRINT("  TX done. TDES3_WB=0x");
    PrintHex32(buf, d->DES3 & 0xFFFFU);
    PRINT(buf);
    PRINT((d->DES3 & (1U << 15)) ? " ES error bits:" : " ok");
    if (d->DES3 & (1U << 15))
    {
        if (d->DES3 & (1U << 2))  PRINT(" UF");
        if (d->DES3 & (1U << 9))  PRINT(" LC");
        if (d->DES3 & (1U << 10)) PRINT(" NC");
        if (d->DES3 & (1U << 8))  PRINT(" EC");
    }
    PRINT("\r\n");

    g_tx_next = s ^ 1;
}


/*
 * 组帧公用: 60 字节最小广播帧布局, Tx 测试与回环测试共用同一字节模式,
 * 便于回环比对逐字节一致性.
 */
static void BuildTestFrame(uint8_t *f)
{
    static const uint8_t dst[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    int k;

    for (k = 0; k < 6; k++)
    {
        f[k]     = dst[k];
        f[k + 6] = g_mac_addr[k];
    }
    f[12] = 0x88;
    f[13] = 0xB5;                       /* ether type 本地实验段 */
    for (k = 14; k < 60; k++)
        f[k] = (uint8_t)(0xA0 | k);
}

/*
 * 测试 7: MAC 内部回环 (无需网线/PHY link).
 *
 * 原理: MACCR.LM(bit12)=1 时发送数据在 MAC 内直连接收路径, 不出 RGMII 引脚,
 * 用于验证 描述符环+DMA+MAC 组帧(CRC 补齐)+Rx 过滤 的完整管线.
 * 手册约束: 需要 eth_rx_clk 在场 —— 通过 SYSCFG_ETH_REF_CLK_SEL 选内部
 * ETH_CLK 绕开对 PHY RX_CLK 的依赖 (该选择器手册标注面向 RMII, RGMII 下
 * 实际效果以本测试结果为准, 这正是本用例存在的意义).
 *
 * 注意: 无 link 时 SWR 无法完成, 故此测试不做软复位 —— RCC 总线复位
 * (GmacClockEnable 内) 已将全部寄存器带回默认态, 等效于干净基线.
 */
static void TestGmacMacLoop(void)
{
    volatile GmacDesc_t *txd, *rxd;
    char buf[16];
    int t, ok;

    g_activated = 0;                    /* 与真链路激活态互斥, 防误入测试5/6 */

    GmacTest_Init();

    SyscfgEthRefClkSelSet(1);           /* eth_rx_clk ← 内部 ETH_CLK */

    /* ---- MAC 配置 (千兆全双工 + 混杂), 先配置再开回环 ---- */
    {
        GmacMacCfg_t mac_cfg;
        uint8_t k;

        for (k = 0; k < 6; k++)
            mac_cfg.gmac_mac_addr[k] = g_mac_addr[k];
        mac_cfg.gmac_speed     = 1000;
        mac_cfg.gmac_duplex    = 1;
        mac_cfg.gmac_jumbo     = 0;
        mac_cfg.gmac_strip_crc = 0;     /* ACS=0: 回环收到的是含 CRC 全帧 */
        GmacMacInit(ETH, &mac_cfg);
    }

    /* ---- Rx/Tx 环 ---- */
    {
        int i;

        for (i = 0; i < (int)RX_RING_N; i++)
        {
            rxd = &rx_desc[i];
            rxd->DES0 = (uint32_t)&rx_buf[i][0];
            rxd->DES1 = 0;
            rxd->DES2 = 0;
            rxd->DES3 = DESC_OWN | DESC_IOC | DESC_BUF1V;
        }
        GmacDmaRxStart(ETH, rx_desc, RX_RING_N, RX_BUF_SZ);

        for (i = 0; i < (int)TX_RING_N; i++)
        {
            txd = &tx_desc[i];
            txd->DES0 = (uint32_t)&tx_buf[i][0];
            txd->DES1 = 0;
            txd->DES2 = 0;
            txd->DES3 = 0;
        }
        GmacDmaTxStart(ETH, tx_desc, TX_RING_N);
    }

    /* ---- 开回环 + RE/TE (顺序: 配置完最后使能) ---- */
    ETH->MAC.CR |= (1U << 12);          /* MACCR.LM */
    GmacMacStart(ETH);
    GmacTim7StartDelay(2);

    /* ---- 提交一帧 (槽0), CPC=00 → MAC 会补 Pad+CRC 上线 64 字节 ---- */
    BuildTestFrame(&tx_buf[0][0]);
    txd = &tx_desc[0];
    txd->DES0 = (uint32_t)&tx_buf[0][0];
    txd->DES1 = 0;
    txd->DES2 = DESC_IOC | 60u;
    txd->DES3 = DESC_OWN | DESC_FD | DESC_LD;
    ETH->DMA.CH[0].TXDTPR = (uint32_t)&tx_desc[1];

    for (t = 0; t < 100 && (txd->DES3 & DESC_OWN); t++)   /* 发送闭环 ≤500ms */
        GmacTim7StartDelay(5);
    if (txd->DES3 & DESC_OWN)
    {
        PRINT("  [FAIL] tx never completed (check DMADSR.RPS)\r\n");
        goto cleanup;
    }

    /* ---- 等回环帧落到 Rx 描述符 (槽0 预期, 1s 上限) ---- */
    rxd = &rx_desc[g_rx_cur];           /* 此前无流量, 游标应在槽0 */
    for (t = 0; t < 200 && (rxd->DES3 & DESC_OWN); t++)
        GmacTim7StartDelay(5);
    if (rxd->DES3 & DESC_OWN)
    {
        PRINT("  [FAIL] no looped-back frame (check ref-clk & LM setup)\r\n");
        goto cleanup;
    }

    ok = ((rxd->DES3 & DESC_LD) != 0)
       && ((rxd->DES3 & DESC_FD) != 0)
       && (((rxd->DES3 >> 15) & 1U) == 0)               /* ES=0 */
       && ((((rxd->DES3 >> 24) & 1U)) == 0);            /* CE=0 */

    PRINT("  loopback got len=");
    NumToStr(buf, (int)((ok ? (rxd->DES3 & 0x7FFFU) - 4u : (rxd->DES3 & 0x7FFFU))));
    PRINT(buf);
    PRINT(", WIRE=");
    NumToStr(buf, (int)(rxd->DES3 & 0x7FFFU));
    PRINT(buf);

    if (ok)
    {
        volatile uint8_t *p   = &rx_buf[g_rx_cur][0];
        const uint8_t    *ref = &tx_buf[0][0];
        int bad = 0;

        for (t = 0; t < 60; t++)
            if (p[t] != ref[t]) { bad++; break; }
        if (bad == 0)
            PRINT("  payload-match ");
        PRINT("\r\n  [");
        PRINT(bad ? "FAIL payload mismatch]" : "PASS mac loopback]");
        PRINT("\r\n");
    }
    else
    {
        PRINT("\r\n  [FAIL] status word: see ES/CE/LD/FD bits\r\n");
        goto noown_cleanup;
    }

noown_cleanup:
cleanup:
    ETH->MAC.CR &= ~(1U << 12);         /* 撤销回环, 还原真实链路路径 */
    PRINT("  LM cleared, done\r\n");
}


/* ================================================================== */
/*  子测试菜单 + 入口                                                   */
/* ================================================================== */

static const MenuEntry_t g_gmac_menu[] = {
    { "1", "MDIO scan (all addr)",   TestGmacMdioScan },
    { "2", "PHY reg grid dump",      TestGmacRegDump },
    { "3", "INER write/read echo",   TestGmacInerEcho },
    { "7", "MAC internal loopback",  TestGmacMacLoop },
    { "4", "Activate (link+SWR+MAC)",TestGmacActivate },
    { "5", "RX monitor (~12s)",      TestGmacRxMonitor },
    { "6", "TX broadcast frame",     TestGmacTxBroadcast },
    { "7", "MAC Loopback",           TestGmacMacLoop },
};

void GmacTest(void)
{
    RunSubMenu("GMAC Ethernet Test",
               g_gmac_menu, sizeof(g_gmac_menu) / sizeof(g_gmac_menu[0]));
}
