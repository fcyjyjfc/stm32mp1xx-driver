/*
 * stm32mp1xx_gmac.c — GMAC 底层外设驱动
 *
 *   GmacSoftReset   — 软件复位 (MAC+DMA+MTL, 等 SWR 自清零)
 *   GmacMacInit     — MAC 配置 (地址/过滤/MACCR/MTL 队列)
 *   GmacMacStart    — 启动 MAC 收发器 (置 RE/TE, DMA 激活后调用)
 *   GmacMacDeinit   — 关闭 MAC 收发器 (清 RE/TE)
 *   GmacMdioInit    — MDC 分频配置
 *   GmacMdioRead    — C22 读 PHY 寄存器 (轮询)
 *   GmacMdioWrite   — C22 写 PHY 寄存器 (轮询)
 *   GmacEthLinkUp   — 完整链路激活 (MDIO → PHY → MAC, 见下)
 *   GmacLinkStatus  — 读 RGMII 链路状态 (PHYCSR 解码)
 *
 *  Created on: 2026-8-26
 *      Author: gjsbr
 */

#include "stm32mp1xx_gmac.h"
#include "stm32mp1xx_btimer.h"   /* BasicTimerDelayMs (链路轮询延时) */
#include "stm32mp1xx_iwdg.h"     /* IwdgKickDog (AN 3 轮跨看门狗窗口) */
#include "phy_mae0621a.h"        /* PHY 黑盒 (复位/时钟修复/找地址/等 AN) */


/* ================================================================== */
/*  基地址变量定义                                                       */
/* ================================================================== */

volatile GmacRegs_t *const ETH = (void *)0x5800A000U;


/* ================================================================== */
/*  寄存器位定义 (RM0436 63.11)                                         */
/* ================================================================== */

/* ---- MACCR (0x0000) ---- */
#define GMAC_MACCR_RE   (1U << 0)   /* 接收使能 */
#define GMAC_MACCR_TE   (1U << 1)   /* 发送使能 */
#define GMAC_MACCR_LM   (1U << 12)  /* 回环模式 (自回环验证用) */
#define GMAC_MACCR_DM   (1U << 13)  /* 双工模式: 1 = 全双工 */
#define GMAC_MACCR_FES  (1U << 14)  /* 10/100M 速度: 1 = 100M, 0 = 10M (仅 PS=1) */
#define GMAC_MACCR_PS   (1U << 15)  /* 端口选择: 0 = 1000M, 1 = 10/100M */
#define GMAC_MACCR_JE   (1U << 16)  /* 巨型帧使能 */
#define GMAC_MACCR_ACS  (1U << 20)  /* 自动剥离 Pad/CRC (仅 <1536 字节包) */

/* ---- MACPFR (0x0008) ---- */
#define GMAC_MACPFR_PR  (1U << 0)   /* 混杂模式, 全部通过 */

/* ---- MACRXQC0R (0x00A0) ---- */
#define GMAC_RXQC0R_RXQ0EN (2U << 0) /* 队列 0 使能 10 (默认 00, 必须写) */

/* ---- MTL TXQOMR (0x0D00) / RXQOMR (0x0D30) ---- */
#define GMAC_TXQOMR_TXQEN (2U << 2)  /* 队列使能 10 */
#define GMAC_TXQOMR_TSF   (1U << 1)  /* 发送存储转发 */
#define GMAC_RXQOMR_RSF   (1U << 5)  /* 接收存储转发 */

/* ---- MACMDIOAR (0x0200) ---- */
#define GMAC_MDIOAR_GB     (1U << 0)   /* 忙/启动位 */
#define GMAC_MDIOAR_GOC_WR (1U << 2)   /* 操作命令: 01 = 写 */
#define GMAC_MDIOAR_GOC_RD (3U << 2)   /* 操作命令: 11 = 读 */
#define GMAC_MDIOAR_CR     (0xFU << 8) /* MDC 分频 */
#define GMAC_MDIOAR_RDA    (0x1FU << 16) /* C22: PHY 内寄存器地址 */
#define GMAC_MDIOAR_PA     (0x1FU << 21) /* PHY 地址 */

/* ---- DMAMR (0x1000) ---- */
#define GMAC_DMAMR_SWR    (1U << 0)   /* 软件复位 */

/* ---- ETH_MACPHYCSR (0x00F8) ---- */
#define GMAC_PHYCSR_LNKSTS   (1U << 19)   /* RGMII link up */
#define GMAC_PHYCSR_LNKSPEED (3U << 17)   /* [18:17]: 00=10M, 01=100M, 10=1000M */
#define GMAC_PHYCSR_LNKMOD   (1U << 16)   /* 1 = 全双工 */


/* MDC 分频系数 -> MACMDIOAR.CR 编码 (数组下标即编码, RM0436 Table 521) */
static const uint32_t gmac_mdc_div_tab[6] = {42, 62, 16, 26, 102, 124};

/* PHYCSR LNKSPEED 索引 -> Mbps (数组下标 = [18:17] 编码) */
static const uint32_t gmac_lnkspeed_tab[3] = {10, 100, 1000};


/* ================================================================== */
/*  复位                                                                 */
/* ================================================================== */

/**
 * @brief  软件复位 — 复位 MAC+DMA+MTL 全部逻辑和内部寄存器, 等 SWR 自清零
 * @param  gmac ETH
 * @return 1 = 成功, 0 = 超时
 *
 *   注意: 复位完成需要 PHY 输入时钟在场 (RGMII/ETH_CLK 未配置时可能超时).
 */
int GmacSoftReset(volatile GmacRegs_t *gmac)
{
    uint32_t timeout = 1000000;

    /* SWR=1 启动复位, 硬件完成后自动清 0 */
    gmac->DMA.DMAMR |= GMAC_DMAMR_SWR;
    while ((gmac->DMA.DMAMR & GMAC_DMAMR_SWR) != 0)
    {
        if (--timeout == 0)
            return 0;
    }
    return 1;
}


/* ================================================================== */
/*  MAC 配置                                                            */
/* ================================================================== */

/**
 * @brief  MAC 配置 — 地址/过滤/MACCR/MTL 队列
 * @param  gmac ETH
 * @param  cfg  配置 (MAC 地址/速率/双工/巨型帧/CRC 剥离)
 *
 *   流程对应 63.9.3 步骤 1/2/5 + 63.9.2 MTL 配置.
 *   不使能 RE/TE, 等 DMA 激活后由 GmacMacStart 启动 (63.9.3 最后一步),
 *   否则收到的帧会灌满 Rx FIFO 溢出.
 */
void GmacMacInit(volatile GmacRegs_t *gmac, const GmacMacCfg_t *cfg)
{
    uint32_t cr;
    uint32_t addr = 0;
    int i;

    /* ---- 0. 配置前确保 RE/TE=0 ---- */
    gmac->MAC.CR &= ~(GMAC_MACCR_RE | GMAC_MACCR_TE);

    /* ---- 1. MAC 地址 (63.9.3 步骤 1, cfg 全 0 = 不设置) ---- */
    for (i = 0; i < 6; i++)
        addr |= cfg->gmac_mac_addr[i];
    if (addr != 0)
    {
        /* ADDRLO 低 4 字节, ADDRHI 高 2 字节 + AE 恒 1 */
        gmac->MAC.ADDR[0].LR = (uint32_t)cfg->gmac_mac_addr[3] << 24
                             | (uint32_t)cfg->gmac_mac_addr[2] << 16
                             | (uint32_t)cfg->gmac_mac_addr[1] << 8
                             | (uint32_t)cfg->gmac_mac_addr[0];
        gmac->MAC.ADDR[0].HR = (1U << 31)                      /* AE */
                             | (uint32_t)cfg->gmac_mac_addr[5] << 8
                             | (uint32_t)cfg->gmac_mac_addr[4];
    }

    /* ---- 2. MACCR (63.9.3 步骤 5): 速率/双工/巨型帧/CRC 剥离 ---- */
    /* 速率: PS=0 千兆, PS=1 时 FES 选 10/100M */
    cr = gmac->MAC.CR & ~(GMAC_MACCR_PS | GMAC_MACCR_FES | GMAC_MACCR_DM);
    if (cfg->gmac_speed == 10)
        cr |= GMAC_MACCR_PS;
    else if (cfg->gmac_speed == 100)
        cr |= GMAC_MACCR_PS | GMAC_MACCR_FES;
    if (cfg->gmac_duplex)
        cr |= GMAC_MACCR_DM;
    if (cfg->gmac_jumbo)
        cr |= GMAC_MACCR_JE;
    if (cfg->gmac_strip_crc)
        cr |= GMAC_MACCR_ACS;
    gmac->MAC.CR = cr;

    /* ---- 3. 接收过滤 (63.9.3 步骤 2): 混杂模式全收 ---- */
    gmac->MAC.PFR = GMAC_MACPFR_PR;

    /* ---- 4. 接收队列 0 使能 (默认全禁, 不使能则收包静默丢弃) ---- */
    gmac->MAC.RXQC0R |= GMAC_RXQC0R_RXQ0EN;

    /* ---- 5. MTL 队列 0 (63.9.2): 存储转发 ---- */
    gmac->MTL.QUEUE[0].TXQOMR = GMAC_TXQOMR_TXQEN | GMAC_TXQOMR_TSF;
    gmac->MTL.QUEUE[0].RXQOMR = GMAC_RXQOMR_RSF;
}


/**
 * @brief  启动 MAC 收发器 (置 RE/TE)
 * @param  gmac ETH
 *
 *   63.9.3 步骤 6: 必须在 DMA 激活 (描述符环就绪) 后调用.
 */
void GmacMacStart(volatile GmacRegs_t *gmac)
{
    gmac->MAC.CR |= GMAC_MACCR_RE | GMAC_MACCR_TE;
}


/**
 * @brief  关闭 MAC 收发器 (清 RE/TE)
 * @param  gmac ETH
 */
void GmacMacDeinit(volatile GmacRegs_t *gmac)
{
    gmac->MAC.CR &= ~(GMAC_MACCR_RE | GMAC_MACCR_TE);
}


/* ================================================================== */
/*  MDIO 接口 (63.6.1 SMA)                                              */
/* ================================================================== */

/**
 * @brief  配置 MDC 分频
 * @param  gmac ETH
 * @param  cfg  分频配置 (gmac_mdc_div = HCLK / MDC)
 *
 *   分频值不在表 (42/62/16/26/102/124) 中时不修改寄存器.
 */
void GmacMdioInit(volatile GmacRegs_t *gmac, const GmacMdioCfg_t *cfg)
{
    int i;

    for (i = 0; i < 6; i++)
    {
        if (gmac_mdc_div_tab[i] == cfg->gmac_mdc_div)
            break;
    }
    if (i == 6)
        return;

    gmac->MAC.MDIOAR = (gmac->MAC.MDIOAR & ~GMAC_MDIOAR_CR) | ((uint32_t)i << 8);
}


/**
 * @brief  C22 读 PHY 寄存器
 * @param  gmac      ETH
 * @param  phy_addr  PHY 地址 [0, 31]
 * @param  reg_addr  PHY 内寄存器地址 [0, 31]
 * @param  val       输出: 读回的 16 位数据
 * @return 1 = 成功, 0 = 超时
 *
 *   写 RDA/PA (GOC=读) 置 GB 启动, 等 GB 清零后取 MDIODR 低 16 位.
 */
int GmacMdioRead(volatile GmacRegs_t *gmac, uint32_t phy_addr, uint32_t reg_addr, uint16_t *val)
{
    uint32_t timeout = 10000;

    /* ---- 1. 等上一次操作结束 (GB=1 期间寄存器不可写) ---- */
    while ((gmac->MAC.MDIOAR & GMAC_MDIOAR_GB) != 0)
    {
        if (--timeout == 0)
            return 0;
    }

    /* ---- 2. 发起读: 地址 + 读命令 + GB ---- */
    gmac->MAC.MDIOAR = ((reg_addr << 16) & GMAC_MDIOAR_RDA)
                     | ((phy_addr << 21) & GMAC_MDIOAR_PA)
                     | (gmac->MAC.MDIOAR & GMAC_MDIOAR_CR)
                     | GMAC_MDIOAR_GOC_RD
                     | GMAC_MDIOAR_GB;

    /* ---- 3. 等完成 (GB 由硬件清 0) ---- */
    timeout = 10000;
    while ((gmac->MAC.MDIOAR & GMAC_MDIOAR_GB) != 0)
    {
        if (--timeout == 0)
            return 0;
    }
    *val = (uint16_t)(gmac->MAC.MDIODR & 0xFFFFU);
    return 1;
}


/**
 * @brief  C22 写 PHY 寄存器
 * @param  gmac      ETH
 * @param  phy_addr  PHY 地址 [0, 31]
 * @param  reg_addr  PHY 内寄存器地址 [0, 31]
 * @param  val       要写入的 16 位数据
 * @return 1 = 成功, 0 = 超时
 *
 *   先写数据 MDIODR, 再写地址 (GOC=写) + GB 启动.
 */
int GmacMdioWrite(volatile GmacRegs_t *gmac, uint32_t phy_addr, uint32_t reg_addr, uint16_t val)
{
    uint32_t timeout = 10000;

    /* ---- 1. 等上一次操作结束 (GB=1 期间寄存器不可写) ---- */
    while ((gmac->MAC.MDIOAR & GMAC_MDIOAR_GB) != 0)
    {
        if (--timeout == 0)
            return 0;
    }

    /* ---- 2. 写数据, 再发起写: 地址 + 写命令 + GB ---- */
    gmac->MAC.MDIODR = val;
    gmac->MAC.MDIOAR = ((reg_addr << 16) & GMAC_MDIOAR_RDA)
                     | ((phy_addr << 21) & GMAC_MDIOAR_PA)
                     | (gmac->MAC.MDIOAR & GMAC_MDIOAR_CR)
                     | GMAC_MDIOAR_GOC_WR
                     | GMAC_MDIOAR_GB;

    /* ---- 3. 等完成 (GB 由硬件清 0) ---- */
    timeout = 10000;
    while ((gmac->MAC.MDIOAR & GMAC_MDIOAR_GB) != 0)
    {
        if (--timeout == 0)
            return 0;
    }
    return 1;
}


/* ================================================================== */
/*  完整链路激活 (MDIO → PHY → MAC)                                      */
/* ================================================================== */

/**
 * @brief  等 RGMII link up, 解出协商速率/双工
 * @param  gmac   ETH
 * @param  speed  输出: Mbps (10/100/1000)
 * @param  fdx    输出: 1 = 全双工
 * @return 1 = link up (输出有效); 0 = 超时无 link 信号
 * @note   ETH_MACPHYCSR 是 RGMII 接口上 PHY 回送状态信号的解码, 属 MAC
 *         侧寄存器 (PHY 驱动不碰). 必须在 GmacSoftReset 之前调用:
 *         SWR 复位 MAC 全部寄存器, 含本寄存器.
 *         轮询 3s (60×50ms), 期间自喂 IWDG2 (主循环被本函数阻塞).
 */
int GmacLinkStatus(volatile GmacRegs_t *gmac, uint32_t *speed, uint32_t *fdx)
{
    int i;

    for (i = 0; i < 60; i++)
    {
        IwdgKickDog(IWDG2);                    /* 3s 轮询, 主循环被阻塞需自喂 */
        if (gmac->MAC.PHYCSR & GMAC_PHYCSR_LNKSTS)
            break;
        BasicTimerDelayMs(TIM7, 50);
    }
    if (i >= 60)
        return 0;

    {
        uint32_t spd_idx = (gmac->MAC.PHYCSR & GMAC_PHYCSR_LNKSPEED) >> 17;

        *fdx   = (gmac->MAC.PHYCSR & GMAC_PHYCSR_LNKMOD) ? 1u : 0u;
        *speed = gmac_lnkspeed_tab[(spd_idx > 2u) ? 2u : spd_idx];
    }
    return 1;
}


/**
 * @brief  完整链路激活: MDIO 分频 → PHY 复位 → 时钟修复 → 找地址 →
 *         等自协商 → 读链路 → 软复位 → MAC 按协商结果配置 → LED 显示配置
 * @param  gmac   ETH (时钟/引脚/GPIO 由上层就绪)
 * @param  cfg    板级参数 (分频/PHY 地址/复位脚/MAC 地址/AN 超时)
 * @param  speed  输出: 协商速率 Mbps
 * @param  fdx    输出: 1 = 全双工
 * @return GMAC_ETH_LINKUP_OK 或负值错误码 (见 stm32mp1xx_gmac.h);
 *         失败无打印, 诊断由上层按错误码展开 (驱动静默)
 * @note   对应手册激活序列 + PHY 驱动时序, 顺序不可乱:
 *         - 软复位需 RX_CLK 在场 (link 之后), 无 link 时提前调用必超时
 *         - SWR 会清掉 MDC 分频与 MAC 配置, 之后必须重配
 *         - AN: 等 SWR 触发的自主协商 (2 片 × an_timeout_ms) 优先,
 *           超时才显式广告 + RESTART_AN 兜底 (1 片); 片间喂狗跨 IWDG2 上限
 *         - 参考时钟修复在 PHY 复位后执行 (见 MaePhyClkInit), 与
 *           GmacSoftReset 无关 (SWR 是 MAC 侧, 不影响 PHY 时钟模式)
 */
int GmacEthLinkUp(volatile GmacRegs_t *gmac, const GmacEthCfg_t *cfg,
                  uint32_t *speed, uint32_t *fdx)
{
    MaePhy_t phy;
    GmacMdioCfg_t mdio_cfg;
    GmacMacCfg_t mac_cfg;
    int i;

    /* ---- 1. MDC 分频 (PHY 复位前配好, 复位后即可访问总线) ---- */
    mdio_cfg.gmac_mdc_div = cfg->gmac_mdc_div;
    GmacMdioInit(gmac, &mdio_cfg);

    /* ---- 2. PHY 复位 + 参考时钟模式修复 (紧跟复位, 见 MaePhyClkInit) ---- */
    phy.gmac     = gmac;
    phy.phy_addr = cfg->phy_addr;
    phy.rst_port = cfg->phy_rst_port;
    phy.rst_pin  = cfg->phy_rst_pin;
    MaePhyReset(&phy);
    if (MaePhyClkInit(&phy) != 0)
        return GMAC_ETH_LINKUP_ERR_MDIO;

    /* ---- 3. 找应答的 PHY (期望地址优先, 失败扫 0~31) ---- */
    if (MaePhyFind(&phy) < 0)
        return GMAC_ETH_LINKUP_ERR_NOPHY;

    /* ---- 4. AN 完成: 先等自主协商, 超时才显式重启兜底 ----
     * 手册注 3: 硬件/软件复位后 AN 自动重启, 且复位默认广告含 1G FDX ——
     * 本 PHY 上 clk_init 的 SWR 触发自主协商实测完成 (~2-4s), 而"显式写
     * ANAR/GBCR + RESTART_AN"路径历史从未成功 (复刻 3×5s 恒超时), 故不
     * 干扰正在运行的自主协商, 直接等待. 10s 跨 IWDG2 超时上限, 分两片
     * 各 an_timeout_ms, 片间喂狗. */
    IwdgKickDog(IWDG2);
    if (MaePhyWaitAnComplete(&phy, cfg->an_timeout_ms) != 0)
    {
        IwdgKickDog(IWDG2);
        if (MaePhyWaitAnComplete(&phy, cfg->an_timeout_ms) != 0)
        {
            /* 兜底: 显式广告 + RESTART_AN (手册注 1/2: 写广告需重启才生效) */
            MaePhyRegWrite(&phy, PHY_REG_ANAR, MAE_PHY_ANAR_ADV);
            MaePhyRegWrite(&phy, PHY_REG_GBCR, PHY_GBCR_1000T_FDX);
            MaePhyRegWrite(&phy, PHY_REG_BMCR,
                           MAE_PHY_BMCR_RESET_VAL | PHY_BMCR_RESTART_AN);
            IwdgKickDog(IWDG2);
            if (MaePhyWaitAnComplete(&phy, cfg->an_timeout_ms) != 0)
                return GMAC_ETH_LINKUP_ERR_ANTMO;
        }
    }

    /* ---- 5. 读 MAC 侧链路 (软复位前! SWR 复位 PHYCSR) ---- */
    if (!GmacLinkStatus(gmac, speed, fdx))
        return GMAC_ETH_LINKUP_ERR_NOLINK;

    /* ---- 6. 软复位: 此刻 RX_CLK 在场, 应当干净通过 ---- */
    if (!GmacSoftReset(gmac))
        return GMAC_ETH_LINKUP_ERR_SWR;

    /* ---- 7. SWR 清掉 MDC 分频与 MAC 配置: 重配 + 按协商结果配置 ---- */
    GmacMdioInit(gmac, &mdio_cfg);
    for (i = 0; i < 6; i++)
        mac_cfg.gmac_mac_addr[i] = cfg->mac_addr[i];
    mac_cfg.gmac_speed     = *speed;
    mac_cfg.gmac_duplex    = *fdx;
    mac_cfg.gmac_jumbo     = 0;
    mac_cfg.gmac_strip_crc = 0;
    GmacMacInit(gmac, &mac_cfg);

    /* ---- 8. LED 显示配置 (PHY 内, 通用千兆双灯): PHY 复位已全部结束
     * (ClkInit 内 SWR 会清 LCR 回默认), 此刻改写生效. 纯显示, 失败不阻断. */
    MaePhyLedConfig(&phy);

    return GMAC_ETH_LINKUP_OK;
}


/* ================================================================== */
/*  DMA 通道与描述符环 (RM0436 63.9.4, 63.10)                           */
/* ================================================================== */

#define GMAC_TXCR_ST     (1U << 0)   /* 发送启动 */
#define GMAC_RXCR_SR     (1U << 0)   /* 接收启动 */


/*
 * 停止两个 DMA 引擎 (整体复位用 GmacSoftReset, 需 link 后调用).
 * 清 ST/SR 会记住各自停在哪一格, 重启后从原处继续.
 */
void GmacDmaInit(volatile GmacRegs_t *gmac)
{
    gmac->DMA.CH[0].TXCR &= ~GMAC_TXCR_ST;
    gmac->DMA.CH[0].RXCR &= ~GMAC_RXCR_SR;
}


/*
 * 启动接收通道
 * @param ring      已预填的描述符数组 (地址+OWN|BUF1V|IOC 由调用者写),
 *                  双字对齐; 本函数不改描述符内容
 * @param count     环内描述符个数 [2..1024]
 * @param buf_size  单缓冲字节大小, 必须为 8 的倍数 (RBSZ[14:1], 低 3 位硬件忽略)
 *
 * 序列 = 手册接收初始化: RXDLAR 仅停止态可写且启动命令前必须先写 →
 * RXRLR = count-1 ("写 0x9 = 10 个") → RBSZ|SR=1 → 尾指针.
 * 初始尾指针指向最后一个描述符: 有效窗口为 D0..D(count-2),
 * 尾部留一格应用所有, 收包回收时由软件推进尾指针扩展窗口.
 */
void GmacDmaRxStart(volatile GmacRegs_t *gmac, GmacDesc_t *ring,
                    uint32_t count, uint32_t buf_size)
{
    volatile EthDmaChanRegs_t *ch = &gmac->DMA.CH[0];

    ch->RXCR  &= ~GMAC_RXCR_SR;
    ch->RXDLAR = (uint32_t)ring;
    ch->RXRLR  = ((count - 1u) & 0x3FFU);
    ch->RXCR   = (ch->RXCR & ~(0x3FFFU << 1))
               | (((buf_size << 1) & (0x3FFFU << 1)) | GMAC_RXCR_SR);
    ch->RXDTPR = (uint32_t)&ring[count - 1u];
}


/*
 * Rx 描述符预填/重灌: 绑定缓冲地址 + OWN|IOC|BUF1V.
 * 重灌必须重放地址: 收帧写回已把 DES0 复用为 VLAN tag (RDES0 写回格式,
 * 仅 RS0V 时有效), 地址不再由硬件保留 —— 每次回收都要由本函数重建绑定,
 * 否则依赖"硬件保留旧地址"这个手册未承诺的行为, 且全 0 地址的缓冲会被跳过.
 */
void GmacDmaRxArm(GmacDesc_t *d, void *buf)
{
    d->DES0 = (uint32_t)buf;
    d->DES3 = GMAC_DESC_OWN | GMAC_DESC_IOC | GMAC_DESC_BUF1V;
}


/*
 * Rx 回收: 重灌 + 推进尾指针 (RXDTPR = 回收格下一格 = "最后一个有效描述符
 * 的下一个位置", 手册 63.10.2), 两步必须成对 —— 只重灌不推尾指针, 该格
 * 不会回到硬件可见窗口, 环逐渐饿死.
 */
void GmacDmaRxRecycle(volatile GmacRegs_t *gmac, GmacDesc_t *ring,
                      uint32_t count, uint32_t idx, void *buf)
{
    GmacDmaRxArm(&ring[idx], buf);
    gmac->DMA.CH[0].RXDTPR = (uint32_t)&ring[(idx + 1u) % count];
}


/*
 * 启动发送通道
 * 序列同 Rx: TXDLAR(停止态) → TXRLR = count-1 → TXPBL=8 拍 | ST=1.
 * 包提交时机: 软件填好描述符置 OWN 后写 TXDTPR (&提交格 + 16 字节),
 * 硬件取到 current==tail 即 Suspend 待新提交.
 */
void GmacDmaTxStart(volatile GmacRegs_t *gmac, GmacDesc_t *ring, uint32_t count)
{
    volatile EthDmaChanRegs_t *ch = &gmac->DMA.CH[0];

    ch->TXCR  &= ~GMAC_TXCR_ST;
    ch->TXDLAR = (uint32_t)ring;
    ch->TXRLR  = ((count - 1u) & 0x3FFU);
    ch->TXCR   = (ch->TXCR & ~(0x3FU << 16)) | (8U << 16) | GMAC_TXCR_ST;
}


/*
 * Tx 提交: 填包缓冲 (BUF1AP + B1L) 并置 OWN|FD|LD, 尾指针推到提交格下一格
 * 交给硬件 (TXDTPR = 最后有效描述符的下一个位置). CPC=00 = MAC 自动补
 * Pad+CRC; 单缓冲, BUF2 置 0.
 */
void GmacDmaTxSubmit(volatile GmacRegs_t *gmac, GmacDesc_t *ring,
                     uint32_t count, uint32_t idx, void *buf, uint32_t len)
{
    ring[idx].DES0 = (uint32_t)buf;
    ring[idx].DES1 = 0;
    ring[idx].DES2 = GMAC_DESC_IOC | (len & 0x3FFFU);
    ring[idx].DES3 = GMAC_DESC_OWN | GMAC_DESC_FD | GMAC_DESC_LD;
    gmac->DMA.CH[0].TXDTPR = (uint32_t)&ring[(idx + 1u) % count];
}
