/*
 * gmac_test.c — GMAC 测试
 *
 *   GmacTest — 测试入口 (menu_test 第 23 项, 二级菜单)
 *     测试 1: MDIO 通信层验证 — 读 ID + 栅格图 + INER 写读回环 + 换页 (0xA43)
 *     测试 2: 完整激活 — 找 PHY → 等自协商/link → SWR → MAC/DMA 启动
 *     测试 3: 收包监控 ~12s (轮询 RI, 回显帧长/状态/头部字节)
 *     测试 4: 发一帧 60 字节最小广播, 回显写回状态与 MMC 计数
 *     测试 5: MAC 内部回环 (LM 位 + 内部 rx 参考钟, 免网线验证收发管线)
 *     测试 6: MDIO 位冲击读 (GPIO 模拟总线直调, 绕过 GMAC MDIO 控制器)
 *
 * 分层: MAC MDIO 控制器在 drivers/stm32mp1xx_gmac.c; 与 PHY 的通信在
 * devices/phy_mae0621a.c (黑盒); GPIO 位冲击总线在 devices/mdio_gpio.c.
 *
 *  Created on: 2026-8-26
 *      Author: gjsbr
 */

#include "stm32mp1xx_gmac.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_syscfg.h"
#include "stm32mp1xx_btimer.h"
#include "phy_mae0621a.h"
#include "mdio_gpio.h"
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

/* INER = 中断使能寄存器 (reg18): 复位默认 0x0020 (bit5=1), 拿来做写读回环
 * 实测可写位 0,2,3,4,5,7,9,10 = 0x06BD; 其余位 (1,6,8,11~15) 写入不回显,
 * 与手册保留位一致, 判据只比对可写位 */
#define GMAC_TEST_REG_INER     18
#define GMAC_TEST_INER_RW_MASK 0x06BD

/* PHY strap 地址: 实测应答 @0 —— Linux 侧同址 (stmmac-0:00), M4 各测试
 * MaePhyFind 轮轮命中 0. 早期按 001b 假设成 1, 向错误地址发 MDIO 会被静默
 * 丢弃 (读回 0xFFFF): clk_init 的 SWR 因此表现为 500ms 超时误报失败
 * (2026-09-07 回归教训), 地址必须与硬件一致. */
#define GMAC_TEST_PHY_ADDR     0


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
 *   PH2 = PHY 复位脚, 由 MaePhyReset 自行配置 (devices/phy_mae0621a.c)
 */
static void GmacGpioInit(void)
{
    RCC->MP_AHB4ENSETR |= (1U << 0) | (1U << 1) | (1U << 2)
                        | (1U << 4) | (1U << 6) | (1U << 7); /* GPIOA,B,C,E,G,H */

    /* ---- PC1 = MDC (输出) ---- */
    GpioMode(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_MODER_AF);
    GpioAf(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GMAC_TEST_MDIO_AF);
    GpioOtype(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_OSPEED_HI);
    GpioPullUpDown(GMAC_TEST_MDC_PORT, GMAC_TEST_MDC_PIN, GPIO_PUPDR_NO);

    /* ---- PA2 = MDIO (双向) ----
     * 必须推挽: 开漏时命令帧高电平只剩内部 40k 上拉, RC 上升沿 1.2µs
     * vs MDC 666kHz 位宽 1.5µs, PHY 采不到合法命令 → 全 0xFFFF (实测回归) */
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
 * 板级 PHY 实例: 绑定 MAC MDIO 总线, 复位脚 PH2.
 * phy_addr = 板级 strap, 实测 0 (见 GMAC_TEST_PHY_ADDR 注释).
 */
static MaePhy_t g_phy;

/* 测试 6: GPIO 位冲击总线 (MDC=PC1, MDIO=PA2, AF11), 字段在 GmacTest_Init 填充 */
static MdioGpio_t g_mdio_gpio;

/*
 * 公共初始化: 时钟 + GPIO + MDC 分频 + PHY 复位 + 参考时钟修复 (clk_init)
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

    mdio_cfg.gmac_mdc_div = GMAC_TEST_MDC_DIV;
    GmacMdioInit(ETH, &mdio_cfg);

    g_phy.gmac     = ETH;
    g_phy.phy_addr = GMAC_TEST_PHY_ADDR;
    g_phy.rst_port = GPIO_H;
    g_phy.rst_pin  = 2;

    /* 复位结论 (2026-09-07 修订): 引脚复位本身无害. 此前"复位后 MDI 零
     * 输出"的真因是参考时钟模式错误 (PHY 复位后默认振荡器模式 bit8=1,
     * 本板工作在晶振模式 bit8=0, PLL 锁不住 → AN 引擎无时钟), 与复位
     * 无关 —— 见 MaePhyClkInit 注释. Linux 同样不复位引脚 (设备树无
     * reset-gpios), 但 maxio 驱动在 probe 做软复位 + 时钟模式修复;
     * 裸机等效序列在此 (紧跟复位). */
    MaePhyReset(&g_phy);

    /* 时钟修复紧跟复位 (对应 Linux probe 时机): SWR 会把时钟模式重置回
     * 错误默认, 之后再软复位会重新破坏 —— Linux phylib link-down 后同样
     * 只重跑 config_aneg 不复跑 clk_init. 失败仅告警不中止: 探测类测试
     * 不依赖时钟模式, 链路测试的失败会在 WaitPhyLink 的 AN 诊断里体现. */
    if (MaePhyClkInit(&g_phy) != 0)
        PRINT("  [WARN] PHY clk init failed (AN will not work)\r\n");

    g_mdio_gpio.mdc_port  = GMAC_TEST_MDC_PORT;
    g_mdio_gpio.mdc_pin   = GMAC_TEST_MDC_PIN;
    g_mdio_gpio.mdio_port = GMAC_TEST_MDIO_PORT;
    g_mdio_gpio.mdio_pin  = GMAC_TEST_MDIO_PIN;
    g_mdio_gpio.mdc_af    = GMAC_TEST_MDIO_AF;
    g_mdio_gpio.mdio_af   = GMAC_TEST_MDIO_AF;
    return 1;
}


/* ================================================================== */
/*  子测试 1: MDIO 通信层验证 (读 ID + 栅格图 + 写读回环 + 换页)        */
/* ================================================================== */

/*
 * 测试 1: 一次性验证 MDIO 通信层四条路径, 固定地址 0 (实测 strap):
 *   1. 读 ID (reg2/3) — 读路径 + 首次读 REG2 0xFFFF 重试 (驱动内黑盒)
 *   2. 栅格图 reg0~18 — 寻址字段 (PB/RB) 组装, 有偏差呈"行间搬运"指纹
 *   3. INER(reg18) 写读回环 — 写路径与采样对齐; 判据只比对可写位 0x06BD
 *      (保留位写入不回显, 实测与手册位定义一致; INER 默认 0x0020, bit5=1)
 *   4. 换页 — 切 0xA43 读 PHY 专属寄存器 (PHYCR1/2, PHYSR, INSR),
 *      用完切回默认页 0xA42 并再读 ID 确认恢复
 * 区分两种失败: -1 = GB 超时 (MAC/MDC 侧); 最终值 0xFFFF = PHY 无应答.
 */
static void TestGmacMdioComm(void)
{
    static const struct { uint8_t reg; const char *hint; } anchor[] = {
        { 0,  "<BMCR  exp~1140>" },
        { 0,  "<BMCR  exp~1140>" },
        { 1,  "<BMSR  exp~79xx>" },
        { 2,  "<ID1   exp 7B74>" },
        { 3,  "<ID2   exp 4411>" },
        { 15, "<GBESR exp 2000>" },
        { 18, "<INER  def 0020 (bit5=1)>" },
    };
    static const uint16_t pattern[2] = { 0x55AA, 0xAA55 };
    static const uint32_t spd_tab[3] = { 10, 100, 1000 };
    char buf[16];
    int r, k, t;
    uint16_t id1, id2, v;
    uint16_t phycr1 = 0, phycr2 = 0, physr = 0, insr = 0;

    PRINT("\r\n--- MDIO Comm Layer ---\r\n");
    PRINT("  Read ID (reg2/3, first-read 0xFFFF retried in driver)\r\n");
    PRINT("  Grid reg0~18, INER write/read loopback, page switch 0xA43\r\n\r\n");

    if (!GmacTest_Init())
        return;

    /* ---- 1. 读 ID (含首次读重试, 驱动内黑盒) ---- */
    if (MaePhyGetId(&g_phy, &id1, &id2) != 0)
    {
        PRINT("  [STOP] GB timeout (MDIOAR=0x");
        PrintHex32(buf, ETH->MAC.MDIOAR);
        PRINT(buf);
        PRINT(")\r\n");
        return;
    }
    if (id1 == 0xFFFF || id2 == 0xFFFF)
    {
        PRINT("  [STOP] 0xFFFF after retries: PHY no ack\r\n");
        return;
    }
    PRINT("  ID 0x");
    PrintHex32(buf, id1);
    PRINT(buf);
    PRINT("/0x");
    PrintHex32(buf, id2);
    PRINT(buf);
    PRINT("  (MAE0621A)\r\n");

    /* ---- 2. 栅格图 reg0~18 (默认页 0xA42 可读部分) ---- */
    PRINT("  reg grid (page 0xA42, 0~18):\r\n");
    for (r = 0; r <= 18; r++)
    {
        if (MaePhyRegRead(&g_phy, r, &v) != 0)
            continue;

        PRINT("    reg ");
        if (r < 10)
            PRINT("0");
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

    /* ---- 3. INER 写读回环 (写路径 + 采样对齐) ---- */
    for (t = 0; t < 2; t++)
    {
        MaePhyRegWrite(&g_phy, GMAC_TEST_REG_INER, pattern[t]);
        MaePhyRegRead(&g_phy, GMAC_TEST_REG_INER, &v);

        PRINT("  INER write 0x");
        PrintHex32(buf, pattern[t]);
        PRINT(buf);
        PRINT(" -> read 0x");
        PrintHex32(buf, v);
        PRINT(buf);
        /* 判据只比对可写位: 保留位写入不回显 (与手册一致), 全值相等必 MISMATCH */
        if (((v ^ pattern[t]) & GMAC_TEST_INER_RW_MASK) == 0)
            PRINT("  [OK] (writable bits matched, reserved 1/6/8/11-15 dropped)\r\n");
        else
        {
            PRINT("  [MISMATCH] diff on writable bits=0x");
            PrintHex32(buf, (v ^ pattern[t]) & GMAC_TEST_INER_RW_MASK);
            PRINT(buf);
            PRINT("\r\n");
        }
    }
    MaePhyRegWrite(&g_phy, GMAC_TEST_REG_INER, 0);   /* 恢复: 中断全禁 */
    MaePhyRegRead(&g_phy, GMAC_TEST_REG_INER, &v);
    PRINT("  INER restore 0x");
    PrintHex32(buf, v);
    PRINT(buf);
    PRINT(v == 0 ? "  [OK] (all interrupts off)\r\n" : "  [FAIL]\r\n");

    /* ---- 4. 换页: 0xA43 读 PHY 专属寄存器, 用完切回默认页 ---- */
    MaePhyRegWrite(&g_phy, 31, 0x0A43);              /* PAGSR: 切到 0xA43 */
    MaePhyRegRead(&g_phy, 24, &phycr1);
    MaePhyRegRead(&g_phy, 25, &phycr2);
    MaePhyRegRead(&g_phy, 26, &physr);
    MaePhyRegRead(&g_phy, 29, &insr);
    MaePhyRegWrite(&g_phy, 31, 0x0A42);              /* 切回默认页 (IEEE 标准寄存器) */

    PRINT("  page 0xA43 regs:\r\n");
    PRINT("    PHYCR1=0x");
    PrintHex32(buf, phycr1);
    PRINT(buf);
    PRINT("\r\n    PHYCR2=0x");
    PrintHex32(buf, phycr2);
    PRINT(buf);
    PRINT("\r\n    PHYSR =0x");
    PrintHex32(buf, physr);
    PRINT(buf);

    /* PHYSR 解码: link 实时 / speed[5:4] / duplex[3] (仅 link 时有效) */
    {
        uint32_t link = (physr >> 2) & 1U;

        PRINT("  (link=");
        NumToStr(buf, (int)link);
        PRINT(buf);
        PRINT(link ? " up" : " down");
        if (link)
        {
            uint32_t spd_idx = (physr >> 4) & 3U;

            PRINT(", ");
            NumToStr(buf, (int)spd_tab[(spd_idx > 2u) ? 2u : spd_idx]);
            PRINT(buf);
            PRINT("Mbps ");
            PRINT((physr & (1u << 3)) ? "FDX" : "HDX");
        }
        PRINT(")\r\n    INSR  =0x");
    }
    PrintHex32(buf, insr);
    PRINT(buf);
    PRINT("\r\n");

    /* ---- 切回验证: 默认页读 ID 仍正常 ---- */
    if (MaePhyGetId(&g_phy, &id1, &id2) == 0
        && id1 != 0xFFFF && id2 != 0xFFFF)
        PRINT("  page restored, ID re-read OK\r\n");
    else
        PRINT("  [FAIL] page not restored\r\n");
}


/* ================================================================== */
/*  位冲击 MDIO 读 (菜单第 6 项) — 绕过 GMAC MDIO 控制器                */
/* ================================================================== */

/*
 * 测试 6: 全地址位冲击扫描, 只打印应答者; 全 0xFFFF 则定位硬件侧.
 * 直调 GPIO 模拟 MDIO 总线 (devices/mdio_gpio.c), 不经 PHY 驱动 —
 * MAC MDIO 或 PHY 驱动出问题时本测试不受污染, 用于定位故障层级.
 */
static void TestGmacMdioBitBang(void)
{
    char buf[16];
    int addr;
    int pass = 0;

    PRINT("\r\n--- MDIO Bit-Bang ---\r\n");
    PRINT("  Manual C22 read frame via GPIO (MDC=PC1, MDIO=PA2)\r\n");
    PRINT("  Bypasses GMAC MDIO controller; all-0xFFFF -> PHY/power/trace issue\r\n");

    if (!GmacTest_Init())
        return;

    for (addr = 0; addr < 32; addr++)
    {
        uint16_t id1 = 0xFFFF, id2 = 0xFFFF;
        int t;

        /* PHY 首次读 REG2 可能返回 0xFFFF, 重试×3 (同 phy_mae0621a 内处理) */
        for (t = 0; t < 3 && id1 == 0xFFFF; t++)
            id1 = MdioGpioRead(&g_mdio_gpio, addr, PHY_REG_ID1);
        for (t = 0; t < 3 && id2 == 0xFFFF; t++)
            id2 = MdioGpioRead(&g_mdio_gpio, addr, PHY_REG_ID2);

        if (id1 == 0xFFFF && id2 == 0xFFFF)
            continue;

        PRINT("  addr ");
        NumToStr(buf, addr);
        PRINT(buf);
        PRINT(": ID1=0x");
        PrintHex32(buf, id1);
        PRINT(buf);
        PRINT("  ID2=0x");
        PrintHex32(buf, id2);
        PRINT(buf);
        PRINT("  [responder]\r\n");
        pass = 1;
    }

    if (pass)
        PRINT("  => PHY alive on bit-bang path (ID 0x7B74/0x4411, MAE0621A)\r\n");
    else
        PRINT("  => all 32 addrs 0xFFFF even bypassing controller: PHY/power/trace hardware issue\r\n");
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

static const uint8_t g_mac_addr[6] = { 0x02, 0x12, 0x34, 0x56, 0x78, 0x90 };

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
 * 等自协商完成 + link up. 自协商状态经 PHY 驱动 (BMSR), 链路状态以 MAC 侧
 * ETH_MACPHYCSR 为准 (它读的是 RGMII 接口上 PHY 实际回送的状态信号).
 * 流程: 只读观测自主 AN (时钟修复已在 Init 完成) → 未完成则显式广告 +
 * 重启 AN (3 轮 × 5s) → MAC PHYCSR 轮询.
 *
 * @param spd_out/fdx_out 输出: 协商速率 Mbps / 全双工标志
 * @return 0 = link up 并解出速率双工; -1 超时无对端
 */
static int WaitPhyLink(uint32_t *spd_out, uint32_t *fdx_out)
{
    char buf[16];
    int an_done = 0;
    int i;

    PRINT("  AN phase: observe autonomous (read-only), else explicit-advert replica\r\n");
    /* ---- 只读轮询: 观测复位后自主 AN (手册 BMCR 注 3: 复位后 AN 自动重启) ----
     * 全程不写寄存器. 参考时钟修复已由 GmacTest_Init 内的 MaePhyClkInit
     * 完成 (紧跟复位, 对应 Linux probe 时机), 此阶段 AN 引擎已有正确
     * 时钟, 理论上可自主完成 —— 完成则跳过下方显式广告复刻; 否则落下
     * 复刻路径做对照. 注意 BMSR.5 非锁存, 实时反映协商状态. */
    {
        uint16_t bmsr = 0, anlpar = 0, aner = 0;
        uint16_t lb = 0xFFFF, lp = 0xFFFF, le = 0xFFFF;  /* 已打印值, 检测变化 */
        uint32_t ticks = 0;

        PRINT("  observe autonomous AN (read-only, up to 10s):\r\n");
        while (ticks < 40)
        {
            IwdgKickDog(IWDG2);
            if (MaePhyRegRead(&g_phy, PHY_REG_BMSR, &bmsr) != 0)
            {
                /* GB 超时 = 总线异常, 观测无意义, 落回复刻路径看诊断 */
                PRINT("  [WARN] MDIO fault during observe, fall back\r\n");
                break;
            }
            MaePhyRegRead(&g_phy, 5, &anlpar);           /* ANLPAR */
            MaePhyRegRead(&g_phy, 6, &aner);             /* ANER: bit0 LP_AN_ABLE 旁证 */

            /* 只在状态变化时打印 (首行基线 + 每个转折点): 逐 250ms 刷屏是
             * 排查时钟 bug 的产物, 成功路径已无必要; 静默失败态由完成
             * 超时行 + 复刻路径诊断覆盖 */
            if (bmsr != lb || anlpar != lp || aner != le || ticks == 0)
            {
                PRINT("  t=");
                NumToStr(buf, (int)(ticks * 250));
                PRINT(buf);
                PRINT("ms BMSR=0x");
                PrintHex32(buf, bmsr);
                PRINT(buf);
                PRINT(" ANLPAR=0x");
                PrintHex32(buf, anlpar);
                PRINT(buf);
                PRINT(" ANER=0x");
                PrintHex32(buf, aner);
                PRINT(buf);
                PRINT("\r\n");
                lb = bmsr;
                lp = anlpar;
                le = aner;
            }

            if (bmsr & PHY_BMSR_AN_COMPLETE)
            {
                an_done = 1;
                break;
            }
            BasicTimerDelayMs(TIM7, 250);
            ticks++;
        }
        if (an_done)
        {
            PRINT("  autonomous AN complete at ~");
            NumToStr(buf, (int)(ticks * 250));
            PRINT(buf);
            PRINT("ms -> skip replica (post-reset defaults advertise 1G FDX)\r\n");
        }
        else
            PRINT("  no autonomous AN -> run explicit-advert replica\r\n");
    }

    if (!an_done)
    {
        /* Linux 序列复刻后半段 (2026-09-07 修订): 前半段 clk_init 已前移到
         * GmacTest_Init (紧跟复位, 对应 Linux probe 时机; 根因背景见
         * MaePhyClkInit 注释). 此处复刻 phylib config_aneg: 显式写
         * ANAR/GBCR 再置 ANE|ANRESTART. 注意此处不再软复位 —— SWR 会把
         * 时钟模式重置回错误默认, 而修复只在复位后跑一次 (Linux phylib
         * link-down 后同样只重跑 config_aneg 不重跑 clk_init). */
        {
            uint16_t bmcr_v, pag_v, insr_v, clk_v;

            /* 诊断回读: 修复后的操作状态与时钟模式 (bit5=1 正常, bit8=1 振荡器) */
            MaePhyRegRead(&g_phy, PHY_REG_PAGSR, &pag_v);
            MaePhyRegWrite(&g_phy, PHY_REG_PAGSR, PHY_PAGE_A43);
            MaePhyRegRead(&g_phy, PHY_REG_INSR, &insr_v);
            MaePhyRegWrite(&g_phy, PHY_REG_PAGSR, PHY_PAGE_D92);
            MaePhyRegRead(&g_phy, PHY_REG_CLK_MODE, &clk_v);
            MaePhyRegWrite(&g_phy, PHY_REG_PAGSR, pag_v);
            PRINT("  INSR=0x");
            PrintHex32(buf, insr_v);
            PRINT(buf);
            PRINT(" CLK_MODE=0x");
            PrintHex32(buf, clk_v);
            PRINT(buf);
            PRINT("\r\n");

            /* 写探针: 上轮实验所有写入后的回读值都与默认相同, 无法区分"写入
             * 成功"与"被静默丢弃" (此 PHY 有静默异常前科). INER(reg18) bit2
             * 可写, 写 0x0004 回读验证写路径, 再恢复全禁. */
            MaePhyRegWrite(&g_phy, 18, 0x0004);
            MaePhyRegRead(&g_phy, 18, &bmcr_v);
            PRINT(bmcr_v & 0x0004 ? "  write probe OK\r\n" : "  [WARN] write dropped?!\r\n");
            MaePhyRegWrite(&g_phy, 18, 0x0000);

            MaePhyRegWrite(&g_phy, 4, 0x01E1);              /* ANAR: 10/100 广告 */
            MaePhyRegWrite(&g_phy, 9, 0x0200);              /* GBCR: 1000BASE-T FDX */
            MaePhyRegWrite(&g_phy, PHY_REG_BMCR, 0x1140 | 0x0200);  /* ANE=1 + Restart_AN */
        }
        /* AN 等待: 3 轮 × 5s, 轮间重启 AN (Linux restart→Link Up 实测 ~4.5s,
         * 单轮 5s 窗口太紧; 重启写 BMCR bit9, 自清零位不影响其余配置) */
        for (i = 0; i < 3; i++)
        {
            IwdgKickDog(IWDG2);          /* 3 轮跨 IWDG2 超时上限, 每轮前喂狗 */
            if (MaePhyWaitAnComplete(&g_phy, 5000) == 0)
            {
                an_done = 1;
                break;
            }
            PRINT("  AN wait round ");
            NumToStr(buf, i + 1);
            PRINT(buf);
            PRINT(" timeout\r\n");
            if (i < 2)
            {
                MaePhyRegWrite(&g_phy, PHY_REG_BMCR, 0x1140 | 0x0200);
                PRINT("  restart AN\r\n");
            }
        }
        if (!an_done)
        {
            uint16_t bmcr, anar, anlpar, aner, gbsr;

            /* 失败诊断: 判据 = ANER bit0 (LP_AN_ABLE) + ANLPAR. 全 0 且
             * LP_AN_ABLE=0 → PHY 从未检测到对端 AN 能力 (线/对端/PHY 接收
             * 路径问题); 对端被看到但 AN 未完成 → 时序/训练问题.
             * GBSR(reg10) 只对 1000T 有效: 主从解析与训练状态, 恒 0 说明
             * 1000T 训练从未开始. PAGE_RX(ANER bit1) 是读即清锁存位,
             * 轮询读到 0 只能作旁证; bit2 是本地 NP 能力恒 1. */
            MaePhyRegRead(&g_phy, PHY_REG_BMCR, &bmcr);
            MaePhyRegRead(&g_phy, 4, &anar);
            MaePhyRegRead(&g_phy, 5, &anlpar);
            MaePhyRegRead(&g_phy, 6, &aner);
            MaePhyRegRead(&g_phy, 10, &gbsr);
            PRINT("  [FAIL] AN timeout after 3 rounds. BMCR=0x");
            PrintHex32(buf, bmcr);
            PRINT(buf);
            PRINT(" ANAR=0x");
            PrintHex32(buf, anar);
            PRINT(buf);
            PRINT("\r\n");
            PRINT("    ANLPAR=0x");
            PrintHex32(buf, anlpar);
            PRINT(buf);
            PRINT(" ANER=0x");
            PrintHex32(buf, aner);
            PRINT(buf);
            PRINT(" GBSR=0x");
            PrintHex32(buf, gbsr);
            PRINT(buf);
            PRINT("\r\n");
            if (aner & 0x0001)
                PRINT("    LP_AN_ABLE=1 -> peer seen, AN incomplete: timing/training issue\r\n");
            else
                PRINT("    ANLPAR=0 + LP_AN_ABLE=0 -> peer never seen: cable/peer/PHY-hw issue\r\n");
            return -1;
        }
    }

    for (i = 0; i < 60; i++)
    {
        IwdgKickDog(IWDG2);                        /* 3s 轮询叠加 AN 等待可跨 IWDG2 上限 */
        if (ETH->MAC.PHYCSR & (1U << 19))          /* LNKSTS */
            break;
        BasicTimerDelayMs(TIM7, 50);
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

    return 0;
}

/*
 * 测试 2: 完整激活 — 验 PHY 身份 → 等自协商/link → SWR → MAC 配置 → DMA 启动
 * 通过后链路进入可用状态, 测试 3/4 在其之上收发.
 */
static void TestGmacActivate(void)
{
    volatile GmacDesc_t *d;
    char buf[16];
    int phy, i;
    uint32_t speed = 1000, fdx = 1;

    PRINT("\r\n--- Activate (find PHY -> AN/link -> SWR -> MAC/DMA) ---\r\n");
    PRINT("  Needs a peer on the cable: AN only completes against switch/router\r\n");

    GmacTest_Init();

    phy = MaePhyFind(&g_phy);
    if (phy < 0)
    {
        PRINT("  [STOP] no PHY responded (run test 1 for details)\r\n");
        return;
    }
    if (phy != GMAC_TEST_PHY_ADDR)
    {
        PRINT("  note: expected addr ");
        NumToStr(buf, GMAC_TEST_PHY_ADDR);
        PRINT(buf);
        PRINT(", found responder @ ");
        NumToStr(buf, phy);
        PRINT(buf);
        PRINT("\r\n");
    }
    PRINT("  PHY @addr ");
    NumToStr(buf, phy);
    PRINT(buf);
    PRINT(", ID OK\r\n");

    if (WaitPhyLink(&speed, &fdx) != 0)
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
    PRINT("\r\n  use menu 3 (RX monitor) to watch incoming broadcast,\r\n"
          "       menu 4 (TX broadcast) to send a frame\r\n");
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
 * 测试 3: 收包监控 ~12s. 理想素材 = 光猫的广播帧 (ARP/IPv6 RA 等).
 * 轮询通道 RI 而非走中断, 免去 EXTI/GIC 配置, 与现有测试框架同构.
 */
static void TestGmacRxMonitor(void)
{
    if (!g_activated)
    {
        PRINT("  run test 2 first\r\n");
        return;
    }

    PRINT("\r\n--- RX Monitor ---\r\n");
    PRINT("  Polls RI for ~12s, dumps frame len/status/head 16 bytes\r\n");
    PRINT("  Best fed by router broadcasts (ARP / IPv6 RA)\r\n");
    PRINT("  monitoring ~12s ...\r\n");
    {
        int t;
        for (t = 0; t < 240; t++)
        {
            IwdgKickDog(IWDG2);             /* ~12s 监控, 主循环被阻塞需自喂 */
            if (ETH->DMA.CH[0].SR & CHAN_SR_RI)
            {
                ETH->DMA.CH[0].SR |= CHAN_SR_RI;     /* rc_w1 */
                RxServiceRing();
            }
            BasicTimerDelayMs(TIM7, 50);
        }
    }
    PRINT("  monitor done\r\n");
}

static void BuildTestFrame(uint8_t *f);

/*
 * 测试 4: 发一帧最小以太网广播 (dst=FF*6, ethertype 0x88B5 本地实验用).
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
        PRINT("  run test 2 first\r\n");
        return;
    }

    PRINT("\r\n--- TX Broadcast ---\r\n");
    PRINT("  One 60-byte minimum broadcast (dst FF*6, ethertype 0x88B5)\r\n");
    PRINT("  CPC=00: MAC appends pad+CRC -> legal 64-byte frame on wire\r\n");

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
        BasicTimerDelayMs(TIM7, 5);
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
 * 测试 5: MAC 内部回环 (无需网线/PHY link).
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

    PRINT("\r\n--- MAC Internal Loopback ---\r\n");
    PRINT("  MACCR.LM loops TX into RX inside MAC, no cable/PHY link needed\r\n");
    PRINT("  Internal ETH_CLK as eth_rx_clk; TX vs RX compared byte-exact\r\n");

    g_activated = 0;                    /* 与真链路激活态互斥, 防误入测试3/4 */
    g_rx_cur = 0;                       /* 环重置后首帧必落槽 0, 清旧游标防错查 */

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
    BasicTimerDelayMs(TIM7, 2);

    /* ---- 提交一帧 (槽0), CPC=00 → MAC 会补 Pad+CRC 上线 64 字节 ---- */
    BuildTestFrame(&tx_buf[0][0]);
    txd = &tx_desc[0];
    txd->DES0 = (uint32_t)&tx_buf[0][0];
    txd->DES1 = 0;
    txd->DES2 = DESC_IOC | 60u;
    txd->DES3 = DESC_OWN | DESC_FD | DESC_LD;
    ETH->DMA.CH[0].TXDTPR = (uint32_t)&tx_desc[1];

    for (t = 0; t < 100 && (txd->DES3 & DESC_OWN); t++)   /* 发送闭环 ≤500ms */
        BasicTimerDelayMs(TIM7, 5);
    if (txd->DES3 & DESC_OWN)
    {
        PRINT("  [FAIL] tx never completed (check DMADSR.RPS)\r\n");
        goto cleanup;
    }

    /* ---- 等回环帧落到 Rx 描述符 (槽0 预期, 1s 上限) ---- */
    rxd = &rx_desc[g_rx_cur];           /* 此前无流量, 游标应在槽0 */
    for (t = 0; t < 200 && (rxd->DES3 & DESC_OWN); t++)
        BasicTimerDelayMs(TIM7, 5);
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
    { "1", "MDIO comm (id/grid/wr/page)", TestGmacMdioComm },
    { "2", "Activate (link+SWR+MAC)",      TestGmacActivate },
    { "3", "RX monitor (~12s)",            TestGmacRxMonitor },
    { "4", "TX broadcast frame",           TestGmacTxBroadcast },
    { "5", "MAC internal loopback",        TestGmacMacLoop },
    { "6", "MDIO bit-bang read",           TestGmacMdioBitBang },
};

void GmacTest(void)
{
    RunSubMenu("GMAC Ethernet Test",
               g_gmac_menu, sizeof(g_gmac_menu) / sizeof(g_gmac_menu[0]));
}
