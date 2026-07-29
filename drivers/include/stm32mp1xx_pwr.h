#ifndef STM32MP1XX_PWR_H_
#define STM32MP1XX_PWR_H_

#include <stdint.h>

/* ======================== 寄存器结构体 ======================== */

typedef struct {
    // 0x000
    volatile uint32_t CR1;          // 控制寄存器 1
    // 0x004
    volatile uint32_t CSR1;         // 控制状态寄存器 1
    // 0x008
    volatile uint32_t CR2;          // 控制寄存器 2
    // 0x00C
    volatile uint32_t CR3;          // 控制寄存器 3
    // 0x010
    volatile uint32_t MPUCR;        // MPU 控制寄存器
    // 0x014
    volatile uint32_t MCUCR;        // MCU 控制寄存器
    // 0x018 - 0x01F
    volatile uint8_t  RSVD0[0x020 - 0x018];
    // 0x020
    volatile uint32_t WKUPCR;       // 唤醒控制寄存器
    // 0x024
    volatile uint32_t WKUPFR;       // 唤醒标志寄存器
    // 0x028
    volatile uint32_t MPUWKUPENR;   // MPU 唤醒使能寄存器
    // 0x02C
    volatile uint32_t MCUWKUPENR;   // MCU 唤醒使能寄存器
    // 0x030 - 0x3F3
    volatile uint8_t  RSVD1[0x3F4 - 0x030];
    // 0x3F4
    volatile uint32_t VER;          // IP 版本寄存器
    // 0x3F8
    volatile uint32_t ID;           // IP 标识寄存器
    // 0x3FC
    volatile uint32_t SID;          // 大小 ID 寄存器
} PwrRegs_t;

/* ======================== 位宏定义 ======================== */

/* CR1 — 控制寄存器 1 */
#define PWR_CR1_LPDS            (1U << 0)   /* 低功耗 Deep Sleep (LP-Stop) */
#define PWR_CR1_LPCFG           (1U << 1)   /* PWR_ON 引脚配置: 0=仅Standby拉低, 1=LP/LPLV也拉低 */
#define PWR_CR1_LVDS            (1U << 2)   /* 低电压 Deep Sleep (LPLV-Stop) */
#define PWR_CR1_PVDEN           (1U << 4)   /* PVD 使能 */
#define PWR_CR1_PLS_POS         5           /* PVD 阈值选择位偏移 */
#define PWR_CR1_PLS_MASK        (7U << 5)
#define PWR_CR1_PLS_0           (0U << 5)   /* PVD_IN 外部输入 */
#define PWR_CR1_PLS_1           (1U << 5)
#define PWR_CR1_PLS_2           (2U << 5)
#define PWR_CR1_PLS_3           (3U << 5)
#define PWR_CR1_PLS_4           (4U << 5)
#define PWR_CR1_PLS_5           (5U << 5)
#define PWR_CR1_PLS_6           (6U << 5)
#define PWR_CR1_PLS_7           (7U << 5)
#define PWR_CR1_DBP             (1U << 8)   /* 备份域写保护禁用 (访问RTC/BDCR前需置位) */
#define PWR_CR1_AVDEN           (1U << 18)  /* AVD 使能 */
#define PWR_CR1_ALS_POS         19          /* AVD 阈值选择位偏移 */
#define PWR_CR1_ALS_MASK        (3U << 19)
#define PWR_CR1_ALS_0           (0U << 19)
#define PWR_CR1_ALS_1           (1U << 19)
#define PWR_CR1_ALS_2           (2U << 19)
#define PWR_CR1_ALS_3           (3U << 19)

/* CSR1 — 控制状态寄存器 1 */
#define PWR_CSR1_PVDO           (1U << 0)   /* PVD 输出标志 (只读) */
#define PWR_CSR1_AVDO           (1U << 1)   /* AVD 输出标志 (只读) */

/* CR2 — 控制寄存器 2 */
#define PWR_CR2_MONEN           (1U << 0)   /* VBAT/温度监控使能 (使能后会增加功耗) */
#define PWR_CR2_TEMPL           (1U << 1)   /* 低温标志: 1=低于低温阈值 */
#define PWR_CR2_TEMPH           (1U << 2)   /* 高温标志: 1=高于高温阈值 */
#define PWR_CR2_BREN            (1U << 8)   /* 备份稳压器使能 (Standby下保持备份RAM) */
#define PWR_CR2_BRRDY           (1U << 9)   /* 备份稳压器就绪 (只读) */
#define PWR_CR2_RREN            (1U << 10)  /* 保留稳压器使能 (Standby下保持Retention RAM) */
#define PWR_CR2_RRRDY           (1U << 11)  /* 保留稳压器就绪 (只读) */

/* CR3 — 控制寄存器 3 */
#define PWR_CR3_REG18EN         (1U << 0)   /* 1.8V 稳压器使能 */
#define PWR_CR3_REG18RDY        (1U << 1)   /* 1.8V 稳压器就绪 (只读) */
#define PWR_CR3_REG11EN         (1U << 2)   /* 1.1V 稳压器使能 */
#define PWR_CR3_REG11RDY        (1U << 3)   /* 1.1V 稳压器就绪 (只读) */
#define PWR_CR3_VBRS            (1U << 4)   /* VBAT 充电电阻: 0=5kΩ, 1=1.5kΩ */
#define PWR_CR3_VBE             (1U << 5)   /* VBAT 充电使能 (进VBAT模式自动关闭) */
#define PWR_CR3_USB33RDY        (1U << 6)   /* USB 3.3V 检测就绪 (只读) */
#define PWR_CR3_DDRRETEN        (1U << 8)   /* DDR 保留使能 (LPLV-Stop 下保持DDR自刷新) */
#define PWR_CR3_POPL_POS        17          /* PWR_ON 最小低电平脉冲宽度 */
#define PWR_CR3_POPL_MASK       (0x1FU << 17)

/* MPUCR — MPU 控制寄存器 */
#define PWR_MPUCR_PDDS          (1U << 0)   /* MPU 掉电 Deep Sleep: 0=Stop, 1=Standby */
#define PWR_MPUCR_CSTBYDIS      (1U << 1)   /* CStandby 禁用: 1=MPU在CStop即可进Standby */
#define PWR_MPUCR_CSSF          (1U << 8)   /* 写1清除 SBFMPU/STOPF */
#define PWR_MPUCR_SBFMPU        (1U << 12)  /* CStandby 唤醒标志 (BOOTROM专用,应用勿用) */
#define PWR_MPUCR_STOPF         (1U << 13)  /* Stop/LP-Stop/LPLV-Stop 唤醒标志 */

/* MCUCR — MCU 控制寄存器 */
#define PWR_MCUCR_PDDS          (1U << 0)   /* MCU 掉电 Deep Sleep: 0=Stop, 1=Standby */
#define PWR_MCUCR_CSSF          (1U << 8)   /* 写1清除 SBF/STOPF */
#define PWR_MCUCR_SBF           (1U << 12)  /* Standby 唤醒标志 (BOOTROM专用,应用勿用) */
#define PWR_MCUCR_STOPF         (1U << 13)  /* Stop/LP-Stop/LPLV-Stop 唤醒标志 */

/* WKUPCR — 唤醒控制 (6组, 每组偏移4位) */
#define PWR_WKUPCR_WKUPEN(n)    (1U << ((n) * 4))       /* 使能 */
#define PWR_WKUPCR_WKUPP(n)     (1U << (((n) * 4) + 1)) /* 极性: 0=上升沿, 1=下降沿 */
#define PWR_WKUPCR_WKUPPUPD(n)  (3U << (((n) * 4) + 2)) /* 上拉/下拉: 00=无, 01=上拉, 10=下拉, 11=保留 */
#define PWR_WKUPCR_WKUPC(n)     (1U << (((n) * 4) + 0)) /* 写1清除对应WKUPF */

/* WKUPFR — 唤醒标志寄存器 */
#define PWR_WKUPFR_WKUPF(n)     (1U << (n))             /* WKUPn 标志 */

/* MPUWKUPENR — MPU 唤醒使能 (6组位对, 每组2位) */
#define PWR_MPUWKUPENR_EN(n)    (3U << ((n) * 2))       /* 位对掩码 */
#define PWR_MPUWKUPENR_EN_0(n)  (1U << ((n) * 2))       /* 01=安全唤醒 */
#define PWR_MPUWKUPENR_EN_1(n)  (2U << ((n) * 2))       /* 10=非安全唤醒 */

/* MCUWKUPENR — MCU 唤醒使能 */
#define PWR_MCUWKUPENR_EN(n)    (3U << ((n) * 2))
#define PWR_MCUWKUPENR_EN_0(n)  (1U << ((n) * 2))
#define PWR_MCUWKUPENR_EN_1(n)  (2U << ((n) * 2))

/* WKUP 引脚范围 */
#define PWR_WKUP_PIN_MAX        6

/* ======================== 函数声明 ======================== */

/**
 * @brief  使能备份域写访问
 * @note   访问 RTC/备份寄存器/RCC_BDCR 前必须先调用此函数
 */
static inline void PwrBackupDomainEnable(void)
{
    PWR->CR1 |= PWR_CR1_DBP;
}

/**
 * @brief  禁止备份域写访问
 * @note   操作完成后调用以防止意外改写
 */
static inline void PwrBackupDomainDisable(void)
{
    PWR->CR1 &= ~PWR_CR1_DBP;
}

/**
 * @brief  选择 Stop 模式 (LPDS=0, LVDS=0)
 * @note   稳压器保持 Main 模式, 唤醒延迟最小
 */
static inline void PwrStopModeSelect(void)
{
    PWR->CR1 &= ~(PWR_CR1_LPDS | PWR_CR1_LVDS);
}

/**
 * @brief  选择 LP-Stop 模式 (LPDS=1, LVDS=0)
 * @note   允许外部稳压器进入低功耗模式
 */
static inline void PwrLPStopModeSelect(void)
{
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LVDS) | PWR_CR1_LPDS;
}

/**
 * @brief  选择 LPLV-Stop 模式 (LPDS=1, LVDS=1)
 * @note   允许外部稳压器降低 VDDCORE 电压
 */
static inline void PwrLPLVStopModeSelect(void)
{
    PWR->CR1 |= PWR_CR1_LPDS | PWR_CR1_LVDS;
}

/**
 * @brief  配置 PWR_ON 引脚低功耗行为
 * @param  lpcfg  0=仅Standby拉低, 1=LP-Stop/LPLV-Stop也拉低
 */
static inline void PwrSetLpCfg(uint32_t lpcfg)
{
    if (lpcfg)
        PWR->CR1 |= PWR_CR1_LPCFG;
    else
        PWR->CR1 &= ~PWR_CR1_LPCFG;
}

/* ---- 稳压器控制 ---- */

/**
 * @brief  使能备份稳压器 (Standby/VBAT下保持备份RAM内容)
 * @note   等待 PwrBackupRegulatorIsReady 后再进 Standby
 */
static inline void PwrBackupRegulatorEnable(void)
{
    PWR->CR2 |= PWR_CR2_BREN;
}

static inline void PwrBackupRegulatorDisable(void)
{
    PWR->CR2 &= ~PWR_CR2_BREN;
}

/**
 * @brief  查询备份稳压器就绪状态
 * @return 1=就绪, 0=未就绪
 */
static inline uint32_t PwrBackupRegulatorIsReady(void)
{
    return !!(PWR->CR2 & PWR_CR2_BRRDY);
}

/**
 * @brief  使能保留稳压器 (Standby/VBAT下保持Retention RAM内容)
 */
static inline void PwrRetentionRegulatorEnable(void)
{
    PWR->CR2 |= PWR_CR2_RREN;
}

static inline void PwrRetentionRegulatorDisable(void)
{
    PWR->CR2 &= ~PWR_CR2_RREN;
}

/**
 * @return 1=就绪, 0=未就绪
 */
static inline uint32_t PwrRetentionRegulatorIsReady(void)
{
    return !!(PWR->CR2 & PWR_CR2_RRRDY);
}

/**
 * @brief  使能 1.8V 稳压器 (为 USB HS PHY 供电)
 */
static inline void Pwr1V8RegulatorEnable(void)
{
    PWR->CR3 |= PWR_CR3_REG18EN;
}

static inline void Pwr1V8RegulatorDisable(void)
{
    PWR->CR3 &= ~PWR_CR3_REG18EN;
}

static inline uint32_t Pwr1V8RegulatorIsReady(void)
{
    return !!(PWR->CR3 & PWR_CR3_REG18RDY);
}

/**
 * @brief  使能 1.1V 稳压器 (为 USB HS PHY 供电)
 */
static inline void Pwr1V1RegulatorEnable(void)
{
    PWR->CR3 |= PWR_CR3_REG11EN;
}

static inline void Pwr1V1RegulatorDisable(void)
{
    PWR->CR3 &= ~PWR_CR3_REG11EN;
}

static inline uint32_t Pwr1V1RegulatorIsReady(void)
{
    return !!(PWR->CR3 & PWR_CR3_REG11RDY);
}

/* ---- VBAT 充电 ---- */

static inline void PwrVBATChargeResistor5k(void)
{
    PWR->CR3 &= ~PWR_CR3_VBRS;   /* VBRS=0 → 5kΩ */
}

static inline void PwrVBATChargeResistor1k5(void)
{
    PWR->CR3 |= PWR_CR3_VBRS;    /* VBRS=1 → 1.5kΩ */
}

static inline void PwrVBATChargeEnable(void)
{
    PWR->CR3 |= PWR_CR3_VBE;
}

static inline void PwrVBATChargeDisable(void)
{
    PWR->CR3 &= ~PWR_CR3_VBE;
}

/* ---- WKUP 引脚 (实现在 .c) ---- */

/**
 * @brief  配置 WKUP 引脚极性及上拉
 * @param  pin       引脚编号 (1-6)
 * @param  polarity  0=上升沿, 1=下降沿
 * @param  pull      0=无, 1=上拉, 2=下拉
 */
void PwrWakeupPinConfig(uint32_t pin, uint32_t polarity, uint32_t pull);

/**
 * @brief  使能 WKUP 引脚唤醒功能
 * @param  pin  引脚编号 (1-6)
 */
void PwrWakeupPinEnable(uint32_t pin);

/**
 * @brief  禁用 WKUP 引脚唤醒功能
 * @param  pin  引脚编号 (1-6)
 */
void PwrWakeupPinDisable(uint32_t pin);

/**
 * @brief  读取 WKUP 引脚唤醒标志
 * @param  pin  引脚编号 (1-6)
 * @return 1=有唤醒事件, 0=无
 */
uint32_t PwrWakeupPinGetFlag(uint32_t pin);

/**
 * @brief  清除 WKUP 引脚唤醒标志
 * @param  pin  引脚编号 (1-6)
 */
void PwrWakeupPinClearFlag(uint32_t pin);

/* ---- MPU/MCU 唤醒使能 ---- */

/**
 * @brief  使能指定 WKUP 引脚向 MPU 发送唤醒信号
 * @param  pin  引脚编号 (1-6)
 */
static inline void PwrMPUWakeupEnable(uint32_t pin)
{
    PWR->MPUWKUPENR |= (3U << ((pin) * 2));
}

static inline void PwrMPUWakeupDisable(uint32_t pin)
{
    PWR->MPUWKUPENR &= ~(3U << ((pin) * 2));
}

/**
 * @brief  使能指定 WKUP 引脚向 MCU 发送唤醒信号
 * @param  pin  引脚编号 (1-6)
 */
static inline void PwrMCUWakeupEnable(uint32_t pin)
{
    PWR->MCUWKUPENR |= (3U << ((pin) * 2));
}

static inline void PwrMCUWakeupDisable(uint32_t pin)
{
    PWR->MCUWKUPENR &= ~(3U << ((pin) * 2));
}

/* ---- 唤醒标志清除 ---- */

static inline void PwrMPUClearFlags(void)
{
    PWR->MPUCR |= PWR_MPUCR_CSSF;
}

static inline void PwrMCUClearFlags(void)
{
    PWR->MCUCR |= PWR_MCUCR_CSSF;
}

/* ---- PVD 可编程电压检测器 ---- */

static inline void PwrPvdEnable(void)
{
    PWR->CR1 |= PWR_CR1_PVDEN;
}

static inline void PwrPvdDisable(void)
{
    PWR->CR1 &= ~PWR_CR1_PVDEN;
}

/**
 * @brief  设置 PVD 阈值
 * @param  pls  阈值编号 (PWR_CR1_PLS_0..7)
 */
static inline void PwrPvdSetThreshold(uint32_t pls)
{
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_PLS_MASK) | (pls << PWR_CR1_PLS_POS);
}

/* ---- AVD 模拟电压检测器 ---- */

static inline void PwrAvdEnable(void)
{
    PWR->CR1 |= PWR_CR1_AVDEN;
}

static inline void PwrAvdDisable(void)
{
    PWR->CR1 &= ~PWR_CR1_AVDEN;
}

/**
 * @brief  设置 AVD 阈值
 * @param  als  阈值编号 (PWR_CR1_ALS_0..3)
 */
static inline void PwrAvdSetThreshold(uint32_t als)
{
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_ALS_MASK) | (als << PWR_CR1_ALS_POS);
}

/* ---- VBAT/温度监控 ---- */

static inline void PwrMonitorEnable(void)
{
    PWR->CR2 |= PWR_CR2_MONEN;
}

static inline void PwrMonitorDisable(void)
{
    PWR->CR2 &= ~PWR_CR2_MONEN;
}

#endif /* STM32MP1XX_PWR_H_ */
