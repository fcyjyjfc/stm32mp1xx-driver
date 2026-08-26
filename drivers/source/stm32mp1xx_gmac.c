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
 *
 *  Created on: 2026-8-26
 *      Author: gjsbr
 */

#include "stm32mp1xx_gmac.h"


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


/* MDC 分频系数 -> MACMDIOAR.CR 编码 (数组下标即编码, RM0436 Table 521) */
static const uint32_t gmac_mdc_div_tab[6] = {42, 62, 16, 26, 102, 124};


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
