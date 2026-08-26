/*
 * stm32mp1xx_syscfg.c — SYSCFG 系统配置控制器驱动
 *
 *   BOOTR     — BOOT 引脚电平/下拉
 *   PMCSETR   — ETH 接口/时钟选择, ANA 连接, 模拟开关, Booster, I2C FM+
 *   IOCTRLSETR— _h/_e pad 高速低电压模式
 *   ICNR      — AXI master DDR 端口选择
 *   CMPCR     — IO 补偿单元
 *   CBR       — PVD/M4 LOCKUP 到定时器 break
 *   VERR/IPIDR/SIDR — 版本标识
 *
 *  Created on: 2026-8-26
 *      Author: gjsbr
 */

#include "stm32mp1xx_syscfg.h"


/* ================================================================== */
/*  基地址变量定义                                                       */
/* ================================================================== */

volatile SyscfgRegs_t *const SYSCFG = (void *)0x50020000;


/* ================================================================== */
/*  BOOTR                                                              */
/* ================================================================== */

/**
 * @brief  BOOTn 引脚电平 (pin 0~2)
 * @param  pin BOOT 引脚号 [0, 2]
 * @return 引脚电平 (0/1), 参数非法返回 0
 */
uint32_t SyscfgBootPinValue(uint32_t pin)
{
    if (pin > 2)
        return 0;
    return (SYSCFG->BOOTR >> pin) & 1U;
}


/**
 * @brief  BOOTn 下拉控制
 * @param  pin      BOOT 引脚号 [0, 2]
 * @param  disable  1 = 禁用下拉 (BOOT 接 VDD 时省电), 0 = 使能下拉
 *
 *   下拉禁用后引脚不能悬空. 参数非法时无操作.
 */
void SyscfgBootPulldown(uint32_t pin, int disable)
{
    uint32_t bit;

    if (pin > 2)
        return;
    bit = 1U << (pin + 4);  /* BOOT0_PD~BOOT2_PD = bit4~6 */
    if (disable)
        SYSCFG->BOOTR |= bit;
    else
        SYSCFG->BOOTR &= ~bit;
}


/* ================================================================== */
/*  PMCSETR/PMCCLRR — 外设模式配置 (set 写 1 置位, clear 写 1 清零)     */
/* ================================================================== */

/**
 * @brief  ETH PHY 接口选择
 * @param  mode SYSCFG_ETH_GMII/MII/RGMII/RMII
 * @return 1 = 成功, 0 = 非法参数
 *
 *   ETH_SEL[2:0] 须在 ETH 处于复位期间、使能 ETH 时钟之前配置.
 *   内部先 PMCCLRR 清字段再 PMCSETR 置位.
 */
int SyscfgEthModeSet(SyscfgEthMode_t mode)
{
    uint32_t sel;
    uint32_t miibit;

    switch (mode)
    {
        case SYSCFG_ETH_GMII:
            sel = 0; miibit = 0; break;
        case SYSCFG_ETH_MII:
            sel = 0; miibit = 1; break;
        case SYSCFG_ETH_RGMII:
            sel = 1; miibit = 0; break;
        case SYSCFG_ETH_RMII:
            sel = 4; miibit = 0; break;
        default:
            return 0;
    }

    SYSCFG->PMCCLRR = SYSCFG_PMCSETR_ETH_SEL_Msk | SYSCFG_PMCSETR_ETH_SELMII;
    SYSCFG->PMCSETR = (sel << SYSCFG_PMCSETR_ETH_SEL_Pos)
                    | (miibit ? SYSCFG_PMCSETR_ETH_SELMII : 0);
    return 1;
}


/**
 * @brief  125MHz 时钟源选择 (GMII/RGMII)
 * @param  internal 1 = 内部 ETH_CLK1 (忽略 AFMux), 0 = 外部 PG5
 * @return 1
 */
int SyscfgEthClkSelSet(int internal)
{
    if (internal)
        SYSCFG->PMCSETR = SYSCFG_PMCSETR_ETH_CLK_SEL;
    else
        SYSCFG->PMCCLRR = SYSCFG_PMCSETR_ETH_CLK_SEL;
    return 1;
}


/**
 * @brief  50MHz RMII 时钟源选择
 * @param  internal 1 = 内部 ETH_CLK1 (忽略 AFMux, 仅 RMII PHY), 0 = 外部 PA1
 * @return 1
 */
int SyscfgEthRefClkSelSet(int internal)
{
    if (internal)
        SYSCFG->PMCSETR = SYSCFG_PMCSETR_ETH_REF_CLK_SEL;
    else
        SYSCFG->PMCCLRR = SYSCFG_PMCSETR_ETH_REF_CLK_SEL;
    return 1;
}


/**
 * @brief  ANA0/ANA1 内部连接 PA0/PA1
 * @param  ana 0 = ANA0, 1 = ANA1
 * @param  en  1 = 连接, 0 = 断开
 *
 *   仅无 ANA 引脚的封装使用; 有 ANA 引脚时用 ANA 输入可获得最佳 ADC 性能.
 */
void SyscfgAnaSel(uint32_t ana, int en)
{
    uint32_t bit;

    if (ana > 1)
        return;
    bit = SYSCFG_PMCSETR_ANA0_SEL << ana;
    if (en)
        SYSCFG->PMCSETR = bit;
    else
        SYSCFG->PMCCLRR = bit;
}


/**
 * @brief  GPIO 模拟开关供电选择
 * @param  en 1 = VDD 供电 (需同时清 EN_BOOSTER), 0 = VDDA/Booster
 */
void SyscfgAnalogSwVdd(int en)
{
    if (en)
        SYSCFG->PMCSETR = SYSCFG_PMCSETR_ANASWVDD;
    else
        SYSCFG->PMCCLRR = SYSCFG_PMCSETR_ANASWVDD;
}


/**
 * @brief  Pad Booster 使能
 * @param  en 1 = 使能 (增加 VDD 电流), 0 = 禁用
 *
 *   仅 VDDA/VDD < 2.7V 时需要, 保证模拟开关 AC 性能.
 */
void SyscfgBoosterEnable(int en)
{
    if (en)
        SYSCFG->PMCSETR = SYSCFG_PMCSETR_EN_BOOSTER;
    else
        SYSCFG->PMCCLRR = SYSCFG_PMCSETR_EN_BOOSTER;
}


/**
 * @brief  I2C Fast Mode Plus 使能
 * @param  i2c I2C 序号 [1, 6]
 * @param  en  1 = FM+ (SCL/SDA 最高 1MHz), 0 = 最高 400kHz
 */
void SyscfgI2cFmp(uint32_t i2c, int en)
{
    uint32_t bit;

    if (i2c < 1 || i2c > 6)
        return;
    bit = 1U << (i2c - 1);  /* I2C1_FMP~I2C6_FMP = bit0~5 */
    if (en)
        SYSCFG->PMCSETR = bit;
    else
        SYSCFG->PMCCLRR = bit;
}


/* ================================================================== */
/*  IOCTRLSETR/CLRR — IO 控制                                          */
/* ================================================================== */

/**
 * @brief  _h/_e pad 高速低电压模式使能
 * @param  periph SYSCFG_HSLVE_SPI/SDMMC/ETH/QUADSPI/TRACE
 * @param  en     1 = 使能, 0 = 禁用
 * @return 1 = 成功, 0 = 非法参数
 *
 *   危险: OTP product_below_2V5 = 0 (默认) 时该位无效;
 *   VDD > 2.7V 时使能会损坏芯片, 严禁开启!
 */
int SyscfgHslveEnable(SyscfgHslve_t periph, int en)
{
    uint32_t bit;

    switch (periph)
    {
        case SYSCFG_HSLVE_SPI:     bit = SYSCFG_IOCTRLSETR_HSLVEN_SPI;     break;
        case SYSCFG_HSLVE_SDMMC:   bit = SYSCFG_IOCTRLSETR_HSLVEN_SDMMC;   break;
        case SYSCFG_HSLVE_ETH:     bit = SYSCFG_IOCTRLSETR_HSLVEN_ETH;     break;
        case SYSCFG_HSLVE_QUADSPI: bit = SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI; break;
        case SYSCFG_HSLVE_TRACE:   bit = SYSCFG_IOCTRLSETR_HSLVEN_TRACE;   break;
        default: return 0;
    }

    if (en)
        SYSCFG->IOCTRLSETR = bit;
    else
        SYSCFG->IOCTRLCLRR = bit;
    return 1;
}


/* ================================================================== */
/*  ICNR — 互连控制                                                     */
/* ================================================================== */

/**
 * @brief  AXI master 访问 DDR 的 slave 端口选择
 * @param  master AXI master 号 [0, 10], 无 M4 (bit4 保留)
 * @param  port   0 = S0, 1 = S1
 * @return 1 = 成功, 0 = 非法参数
 *
 *   用于性能/延迟调优.
 */
int SyscfgDdrPort(uint32_t master, uint32_t port)
{
    uint32_t bit;

    if (master > 10 || master == 4 || port > 1)
        return 0;
    bit = 1U << master;
    if (port == 0)
        SYSCFG->ICNR &= ~bit;
    else
        SYSCFG->ICNR |= bit;
    return 1;
}


/* ================================================================== */
/*  CMPCR/CMPENSETR/ENCLRR — IO 补偿单元                                */
/* ================================================================== */

/**
 * @brief  IO 补偿单元使能
 * @param  en 1 = 使能 (MPU+MCU 两侧), 0 = 掉电
 *
 *   使能前必须先使能 CSI 振荡器 (RCC); 只有两侧都关闭才能关 CSI.
 *   仅影响 OSPEEDR = 0b10/0b11 的 IO.
 */
void SyscfgCompEnable(int en)
{
    if (en)
        SYSCFG->CMPENSETR = SYSCFG_CMPENSETR_MCU_EN | SYSCFG_CMPENSETR_MPU_EN;
    else
        SYSCFG->CMPENCLRR = SYSCFG_CMPENSETR_MCU_EN | SYSCFG_CMPENSETR_MPU_EN;
}


/**
 * @brief  补偿单元就绪标志
 * @return 1 = 就绪 (APSRC/ANSRC 有效), 0 = 未就绪
 */
uint32_t SyscfgCompReady(void)
{
    return (SYSCFG->CMPCR & SYSCFG_CMPCR_READY) ? 1 : 0;
}


/**
 * @brief  补偿软件控制
 * @param  sw 0 = 自动补偿 (用 APSRC/ANSRC), 1 = 用 RAPSRC/RANSRC 保持值
 *
 *   sw=0 在 READY=1 前不生效; 补偿单元掉电时恒用保持值.
 */
void SyscfgCompSwCtrl(int sw)
{
    if (sw)
        SYSCFG->CMPCR |= SYSCFG_CMPCR_SW_CTRL;
    else
        SYSCFG->CMPCR &= ~SYSCFG_CMPCR_SW_CTRL;
}


/* ================================================================== */
/*  CBR — 定时器 break 控制                                             */
/* ================================================================== */

/**
 * @brief  PVD 连接 TIM1/8/15/16/17 break 输入
 *
 *   只能置位, 由系统复位清除. 置位后 PVDE/PLS[2:0] 变为只读.
 */
void SyscfgPvdBreakEnable(void)
{
    SYSCFG->CBR |= SYSCFG_CBR_PVDL;
}


/**
 * @brief  Cortex-M4 LOCKUP (Hardfault) 连接 TIM break 输入
 *
 *   只能置位, 由系统复位清除.
 */
void SyscfgM4LockupBreakEnable(void)
{
    SYSCFG->CBR |= SYSCFG_CBR_CLL;
}


/* ================================================================== */
/*  VERR/IPIDR/SIDR — 版本标识                                          */
/* ================================================================== */

/**
 * @brief  获取 SYSCFG 版本 (VERR: MAJREV[7:4], MINREV[3:0])
 * @return 版本值
 */
uint32_t SyscfgVersionGet(void)
{
    return SYSCFG->VERR;
}


/**
 * @brief  获取 SYSCFG 标识 (IPIDR)
 * @return 标识值
 */
uint32_t SyscfgIdGet(void)
{
    return SYSCFG->IPIDR;
}


/**
 * @brief  获取 SYSCFG 尺寸标识 (SIDR)
 * @return 尺寸标识值
 */
uint32_t SyscfgSizeIdGet(void)
{
    return SYSCFG->SIDR;
}
