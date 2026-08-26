#ifndef STM32MP1XX_SYSCFG_H_
#define STM32MP1XX_SYSCFG_H_

#include <stdint.h>

typedef struct {
    // 0x000 - 0x003
    volatile uint32_t BOOTR;        // boot pins control
    // 0x004 - 0x007
    volatile uint32_t PMCSETR;      // peripheral mode configuration set
    // 0x008 - 0x017
    volatile uint8_t  RSVD0[0x018 - 0x008];
    // 0x018 - 0x01B
    volatile uint32_t IOCTRLSETR;   // IO control set
    // 0x01C - 0x01F
    volatile uint32_t ICNR;         // interconnect control
    // 0x020 - 0x023
    volatile uint32_t CMPCR;        // compensation cell control
    // 0x024 - 0x027
    volatile uint32_t CMPENSETR;    // compensation cell enable set
    // 0x028 - 0x02B
    volatile uint32_t CMPENCLRR;    // compensation cell enable clear
    // 0x02C - 0x02F
    volatile uint32_t CBR;          // control timer break
    // 0x030 - 0x043
    volatile uint8_t  RSVD1[0x044 - 0x030];
    // 0x044 - 0x047
    volatile uint32_t PMCCLRR;      // peripheral mode configuration clear
    // 0x048 - 0x057
    volatile uint8_t  RSVD2[0x058 - 0x048];
    // 0x058 - 0x05B
    volatile uint32_t IOCTRLCLRR;   // IO control clear
    // 0x05C - 0x3F3
    volatile uint8_t  RSVD3[0x3F4 - 0x05C];
    // 0x3F4 - 0x3F7
    volatile uint32_t VERR;         // version
    // 0x3F8 - 0x3FB
    volatile uint32_t IPIDR;        // identification
    // 0x3FC - 0x3FF
    volatile uint32_t SIDR;         // size identification
} SyscfgRegs_t;

extern volatile SyscfgRegs_t *const SYSCFG;


/* ================================================================== */
/*  位定义 (RM0436 14.3)                                               */
/* ================================================================== */

/* ---- BOOTR (0x000) ---- */
#define SYSCFG_BOOTR_BOOT0_PD     (1U << 4)   /* BOOT0 下拉禁用 */
#define SYSCFG_BOOTR_BOOT1_PD     (1U << 5)   /* BOOT1 下拉禁用 */
#define SYSCFG_BOOTR_BOOT2_PD     (1U << 6)   /* BOOT2 下拉禁用 */

/* ---- PMCSETR (0x004) ---- */
#define SYSCFG_PMCSETR_ANA1_SEL        (1U << 25)  /* ANA1 连 PA1 */
#define SYSCFG_PMCSETR_ANA0_SEL        (1U << 24)  /* ANA0 连 PA0 */
#define SYSCFG_PMCSETR_ETH_SEL_Pos     21
#define SYSCFG_PMCSETR_ETH_SEL_Msk     (7U << 21)  /* 000=GMII/MII, 001=RGMII, 100=RMII */
#define SYSCFG_PMCSETR_ETH_SELMII      (1U << 20)  /* ETH_SEL=000 时: 1=MII, 0=GMII */
#define SYSCFG_PMCSETR_ETH_REF_CLK_SEL (1U << 17)  /* 0=外部 PA1, 1=内部 ETH_CLK1 (仅 RMII) */
#define SYSCFG_PMCSETR_ETH_CLK_SEL     (1U << 16)  /* 0=外部 PG5, 1=内部 ETH_CLK1 (GMII/RGMII) */
#define SYSCFG_PMCSETR_ANASWVDD        (1U << 9)   /* 1: 模拟开关用 VDD 供电 */
#define SYSCFG_PMCSETR_EN_BOOSTER      (1U << 8)   /* 1: 使能 Booster (VDDA/VDD<2.7V) */
#define SYSCFG_PMCSETR_I2C1_FMP        (1U << 0)
#define SYSCFG_PMCSETR_I2C2_FMP        (1U << 1)
#define SYSCFG_PMCSETR_I2C3_FMP        (1U << 2)
#define SYSCFG_PMCSETR_I2C4_FMP        (1U << 3)
#define SYSCFG_PMCSETR_I2C5_FMP        (1U << 4)
#define SYSCFG_PMCSETR_I2C6_FMP        (1U << 5)

/* ---- IOCTRLSETR (0x018) ---- */
#define SYSCFG_IOCTRLSETR_HSLVEN_SPI     (1U << 4)   /* SPI _h/_e pad 高速低电压模式 */
#define SYSCFG_IOCTRLSETR_HSLVEN_SDMMC   (1U << 3)   /* SDMMC 同上 */
#define SYSCFG_IOCTRLSETR_HSLVEN_ETH     (1U << 2)   /* ETH 同上 */
#define SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI (1U << 1)   /* QUADSPI 同上 */
#define SYSCFG_IOCTRLSETR_HSLVEN_TRACE   (1U << 0)   /* TRACE 同上 */

/* ---- CMPCR (0x020) ---- */
#define SYSCFG_CMPCR_READY    (1U << 8)   /* 补偿单元就绪 */
#define SYSCFG_CMPCR_SW_CTRL  (1U << 1)   /* 1: 用 RAPSRC/RANSRC 保持值 */

/* ---- CMPENSETR (0x024) ---- */
#define SYSCFG_CMPENSETR_MCU_EN (1U << 1)   /* M4 侧补偿单元使能 */
#define SYSCFG_CMPENSETR_MPU_EN (1U << 0)   /* A7 侧补偿单元使能 */

/* ---- CBR (0x02C) ---- */
#define SYSCFG_CBR_PVDL (1U << 2)   /* PVD 连 TIM1/8/15/16/17 break (只能置位) */
#define SYSCFG_CBR_CLL  (1U << 0)   /* M4 LOCKUP 连 TIM break (只能置位) */


/* ================================================================== */
/*  类型定义                                                           */
/* ================================================================== */

typedef enum {
    SYSCFG_ETH_GMII,    /* ETH_SEL=000, ETH_SELMII=0 */
    SYSCFG_ETH_MII,     /* ETH_SEL=000, ETH_SELMII=1 */
    SYSCFG_ETH_RGMII,   /* ETH_SEL=001 */
    SYSCFG_ETH_RMII,    /* ETH_SEL=100 */
} SyscfgEthMode_t;

typedef enum {
    SYSCFG_HSLVE_SPI,
    SYSCFG_HSLVE_SDMMC,
    SYSCFG_HSLVE_ETH,
    SYSCFG_HSLVE_QUADSPI,
    SYSCFG_HSLVE_TRACE,
} SyscfgHslve_t;


/* ================================================================== */
/*  函数声明 (BOOTR)                                                   */
/* ================================================================== */

/* BOOTn 引脚电平 (pin 0~2) */
uint32_t SyscfgBootPinValue(uint32_t pin);

/* BOOTn 下拉控制: disable=1 禁用下拉 (BOOT 接 VDD 时省电) */
void SyscfgBootPulldown(uint32_t pin, int disable);


/* ================================================================== */
/*  函数声明 (PMCSETR/PMCCLRR)                                         */
/* ================================================================== */

/* ETH PHY 接口选择 (须在 ETH 复位期间配置, 见 rcc_clock_tree.md) */
int SyscfgEthModeSet(SyscfgEthMode_t mode);

/* 125MHz 时钟源: internal=1 用内部 ETH_CLK1, 0 用外部 PG5 (GMII/RGMII) */
int SyscfgEthClkSelSet(int internal);

/* 50MHz RMII 时钟源: internal=1 用内部 ETH_CLK1, 0 用外部 PA1 */
int SyscfgEthRefClkSelSet(int internal);

/* ANA0/ANA1 内部连接到 PA0/PA1 (ana 0~1, 无 ANA 引脚封装用) */
void SyscfgAnaSel(uint32_t ana, int en);

/* 模拟开关供电: en=1 用 VDD, 0 用 VDDA/Booster */
void SyscfgAnalogSwVdd(int en);

/* Booster 使能 (仅 VDDA/VDD < 2.7V 时需要) */
void SyscfgBoosterEnable(int en);

/* I2C1~6 Fast Mode Plus (i2c 1~6): en=1 允许最高 1MHz */
void SyscfgI2cFmp(uint32_t i2c, int en);


/* ================================================================== */
/*  函数声明 (IOCTRLSETR/CLRR)                                         */
/* ================================================================== */

/*
 * _h/_e pad 高速低电压模式.
 * 危险: OTP product_below_2V5=0 时无效; VDD > 2.7V 时开启会损坏芯片!
 */
int SyscfgHslveEnable(SyscfgHslve_t periph, int en);


/* ================================================================== */
/*  函数声明 (ICNR)                                                    */
/* ================================================================== */

/* AXI master (0~10, 无 M4) 访问 DDR 端口选择: port 0=S0, 1=S1 */
int SyscfgDdrPort(uint32_t master, uint32_t port);


/* ================================================================== */
/*  函数声明 (CMPCR/CMPENSETR/ENCLRR)                                  */
/* ================================================================== */

/* 补偿单元使能 (须先使能 CSI 振荡器) */
void SyscfgCompEnable(int en);

/* 补偿单元就绪标志 (1 = 可自动补偿) */
uint32_t SyscfgCompReady(void);

/* 补偿软件控制: sw=0 自动补偿, 1 用 RAPSRC/RANSRC 保持值 */
void SyscfgCompSwCtrl(int sw);


/* ================================================================== */
/*  函数声明 (CBR)                                                     */
/* ================================================================== */

/* PVD 连接 TIM1/8/15/16/17 break 输入 (只能置位, 系统复位清除) */
void SyscfgPvdBreakEnable(void);

/* Cortex-M4 LOCKUP 连接 TIM break 输入 (只能置位, 系统复位清除) */
void SyscfgM4LockupBreakEnable(void);


/* ================================================================== */
/*  函数声明 (VERR/IPIDR/SIDR)                                         */
/* ================================================================== */

uint32_t SyscfgVersionGet(void);    /* VERR */
uint32_t SyscfgIdGet(void);         /* IPIDR */
uint32_t SyscfgSizeIdGet(void);     /* SIDR */

#endif /* STM32MP1XX_SYSCFG_H_ */
