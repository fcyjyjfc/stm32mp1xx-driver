/*
 * stm32mp1xx_gic.h
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 *
 *  GIC v2 register map — GICD + GICC + GICH + GICV
 *  RM0436 section 23.5-23.8
 */

#ifndef STM32MP1XX_GIC_H_
#define STM32MP1XX_GIC_H_

#include <stdint.h>


#define GICD_BASE               0xA0021000U
#define GICC_BASE               0xA0022000U
#define GICH_BASE               0xA0024000U
#define GICV_BASE               0xA0026000U


/* GIC 中断 ID — Table 118, RM0436 Rev 7 pp.1249-1255 */
/* SGI: 0-15 */
/* PPI: 16-31 */
/* SPI: 32-287 */

/* PPI */
#define GIC_PPI_VIRT_MAINT      25U
#define GIC_PPI_HYP_TIMER       26U
#define GIC_PPI_VIRT_TIMER      27U
#define GIC_PPI_LEGACY_nFIQ     28U
#define GIC_PPI_SEC_PHY_TIMER   29U
#define GIC_PPI_NS_PHY_TIMER    30U
#define GIC_PPI_LEGACY_nIRQ     31U

/* SPI 32-54 */
#define GIC_WWDG1_IT            32U
#define GIC_PVD_AVD             33U
#define GIC_TAMP                34U
#define GIC_RTC_WKUP_ALARM      35U
#define GIC_TZC_IT              36U
#define GIC_RCC                 37U
#define GIC_EXTI0               38U
#define GIC_EXTI1               39U
#define GIC_EXTI2               40U
#define GIC_EXTI3               41U
#define GIC_EXTI4               42U
#define GIC_DMA1_STR0           43U
#define GIC_DMA1_STR1           44U
#define GIC_DMA1_STR2           45U
#define GIC_DMA1_STR3           46U
#define GIC_DMA1_STR4           47U
#define GIC_DMA1_STR5           48U
#define GIC_DMA1_STR6           49U
#define GIC_ADC1                50U
#define GIC_FDCAN1_IT0          51U
#define GIC_FDCAN2_IT0          52U
#define GIC_FDCAN1_IT1          53U
#define GIC_FDCAN2_IT1          54U

/* SPI 55-87 */
#define GIC_EXTI5               55U
#define GIC_TIM1_BRK            56U
#define GIC_TIM1_UP             57U
#define GIC_TIM1_TRG_COM        58U
#define GIC_TIM1_CC             59U
#define GIC_TIM2                60U
#define GIC_TIM3                61U
#define GIC_TIM4                62U
#define GIC_I2C1_EVT            63U
#define GIC_I2C1_ERR            64U
#define GIC_I2C2_EVT            65U
#define GIC_I2C2_ERR            66U
#define GIC_SPI1                67U
#define GIC_SPI2                68U
#define GIC_USART1              69U
#define GIC_USART2              70U
#define GIC_USART3              71U
#define GIC_EXTI10              72U
#define GIC_RTC_TS              73U
#define GIC_EXTI11              74U
#define GIC_TIM8_BRK            75U
#define GIC_TIM8_UP             76U
#define GIC_TIM8_TRG_COM        77U
#define GIC_TIM8_CC             78U
#define GIC_DMA1_STR7           79U
#define GIC_FMC                 80U
#define GIC_SDMMC1              81U
#define GIC_TIM5                82U
#define GIC_SPI3                83U
#define GIC_UART4               84U
#define GIC_UART5               85U
#define GIC_TIM6                86U
#define GIC_TIM7                87U

/* SPI 88-119 */
#define GIC_DMA2_STR0           88U
#define GIC_DMA2_STR1           89U
#define GIC_DMA2_STR2           90U
#define GIC_DMA2_STR3           91U
#define GIC_DMA2_STR4           92U
#define GIC_ETH1                93U
#define GIC_ETH1_WKUP           94U
#define GIC_FDCAN_CAL           95U
#define GIC_EXTI6               96U
#define GIC_EXTI7               97U
#define GIC_EXTI8               98U
#define GIC_EXTI9               99U
#define GIC_DMA2_STR5           100U
#define GIC_DMA2_STR6           101U
#define GIC_DMA2_STR7           102U
#define GIC_USART6              103U
#define GIC_I2C3_EVT            104U
#define GIC_I2C3_ERR            105U
#define GIC_USBH_OHCI           106U
#define GIC_USBH_EHCI           107U
#define GIC_EXTI12              108U
#define GIC_EXTI13              109U
#define GIC_DCMI                110U
#define GIC_CRYP1               111U
#define GIC_HASH1               112U
#define GIC_UART7               113U
#define GIC_UART8               114U
#define GIC_SPI4                115U
#define GIC_SPI5                116U
#define GIC_SPI6                117U
#define GIC_SAI1                118U
#define GIC_LTDC                119U

/* SPI 120-153 */
#define GIC_LTDC_ER             121U
#define GIC_ADC2                122U
#define GIC_SAI2                123U
#define GIC_QUADSPI             124U
#define GIC_LPTIM1              125U
#define GIC_CEC                 126U
#define GIC_I2C4_EVT            127U
#define GIC_I2C4_ERR            128U
#define GIC_SPDIFRX             129U
#define GIC_OTG                 130U
#define GIC_IPCC_RX0            132U
#define GIC_IPCC_TX0            133U
#define GIC_DMAMUX1_OVR_REQ     134U
#define GIC_IPCC_RX1            135U
#define GIC_IPCC_TX1            136U
#define GIC_CRYP2               137U
#define GIC_HASH2               138U
#define GIC_I2C5_EVT            139U
#define GIC_I2C5_ERR            140U
#define GIC_GPU_IT              141U
#define GIC_DFSDM1_FLT0         142U
#define GIC_DFSDM1_FLT1         143U
#define GIC_DFSDM1_FLT2         144U
#define GIC_DFSDM1_FLT3         145U
#define GIC_SAI3                146U
#define GIC_DFSDM1_FLT4         147U
#define GIC_TIM15               148U
#define GIC_TIM16               149U
#define GIC_TIM17               150U
#define GIC_TIM12               151U
#define GIC_MDIOS               152U
#define GIC_EXTI14              153U

/* SPI 154-183 */
#define GIC_MDMA                154U
#define GIC_DSI                 155U
#define GIC_SDMMC2              156U
#define GIC_HSEM_IT1            157U
#define GIC_DFSDM1_FLT5         158U
#define GIC_EXTI15              159U
#define GIC_MDMA_SEC_IT         160U
#define GIC_SYSRESETQ           161U
#define GIC_TIM13               162U
#define GIC_TIM14               163U
#define GIC_DAC                 164U
#define GIC_RNG1                165U
#define GIC_RNG2                166U
#define GIC_I2C6_EVT            167U
#define GIC_I2C6_ERR            168U
#define GIC_SDMMC3              169U
#define GIC_LPTIM2              170U
#define GIC_LPTIM3              171U
#define GIC_LPTIM4              172U
#define GIC_LPTIM5              173U
#define GIC_ETH1_LPI            174U
#define GIC_WWDG1_RST_IT        175U
#define GIC_MCU_SEV             176U
#define GIC_RCC_WAKEUP          177U
#define GIC_SAI4                178U
#define GIC_DTS                 179U
#define GIC_MPU_WAKEUP_PIN      181U
#define GIC_IWDG1_IT            182U
#define GIC_IWDG2_IT            183U

/* SPI 229-249 (secure/debug) */
#define GIC_TAMP_S              229U
#define GIC_RTC_WKUP_ALARM_S    230U
#define GIC_RTC_TS_S            231U
#define GIC_PMUIRQ0             232U
#define GIC_PMUIRQ1             233U
#define GIC_COMMRX0             236U
#define GIC_COMMRX1             237U
#define GIC_COMMTX0             240U
#define GIC_COMMTX1             241U
#define GIC_AXIERRIRQ           244U
#define GIC_DDRPERFM            245U
#define GIC_nCTIIRQ0            248U
#define GIC_nCTIIRQ1            249U


/* 中断处理函数类型 */
typedef void (*IrqHandler_t)(void);

/* GICD 驱动函数 */
void GicdEnableInt(uint32_t id);
void GicdDisableInt(uint32_t id);
void GicdSetPending(uint32_t id);
void GicdClearPending(uint32_t id);
void GicdSetPriority(uint32_t id, uint8_t pri);
void GicdSetTarget(uint32_t id, uint8_t cpu_mask);
void GicdSetTrigMode(uint32_t id, uint8_t edge);
void GicdSetGroup(uint32_t id);
void GicdInit(void);

/* GICC 驱动函数 */
void GiccInit(uint8_t pmr, uint8_t bpr);
uint32_t GiccAckInt(void);
void GiccEoiInt(uint32_t id);

/* 中断注册与默认分发 */
IrqHandler_t GicRegisterIrq(uint32_t id, IrqHandler_t handler);
void do_irq(void);


typedef struct {
    volatile uint32_t CTLR;                 /* 0x000 — 控制寄存器 */
    volatile uint32_t TYPER;                /* 0x004 — 类型寄存器 */
    volatile uint32_t IIDR;                 /* 0x008 — 实现者标识 */
    uint8_t           RSVD0[0x80 - 0x0C];
    volatile uint32_t IGROUPR[9];           /* 0x080 — Group 0/1 */
    uint8_t           RSVD1[0x100 - 0x0A4];
    volatile uint32_t ISENABLER[9];         /* 0x100 — 置使能 */
    uint8_t           RSVD2[0x180 - 0x124];
    volatile uint32_t ICENABLER[9];         /* 0x180 — 清使能 */
    uint8_t           RSVD3[0x200 - 0x1A4];
    volatile uint32_t ISPENDR[9];           /* 0x200 — 置挂起 */
    uint8_t           RSVD4[0x280 - 0x224];
    volatile uint32_t ICPENDR[9];           /* 0x280 — 清挂起 */
    uint8_t           RSVD5[0x300 - 0x2A4];
    volatile uint32_t ISACTIVER[9];         /* 0x300 — 置 Active */
    uint8_t           RSVD6[0x380 - 0x324];
    volatile uint32_t ICACTIVER[9];         /* 0x380 — 清 Active */
    uint8_t           RSVD7[0x400 - 0x3A4];
    volatile uint8_t  IPRIORITYR[288];      /* 0x400 — 优先级 */
    uint8_t           RSVD8[0x800 - 0x520];
    volatile uint8_t  ITARGETSR[288];       /* 0x800 — 目标处理器 */
    uint8_t           RSVD9[0xC00 - 0x920];
    volatile uint32_t ICFGR[18];            /* 0xC00 — 触发配置 */
    uint8_t           RSVD10[0xD00 - 0xC48];
    volatile uint32_t PPISR;                /* 0xD00 — PPI 状态 */
    volatile uint32_t SPISR[8];             /* 0xD04 — SPI 状态 */
    uint8_t           RSVD11[0xF00 - 0xD24];
    volatile uint32_t SGIR;                 /* 0xF00 — 软件中断 */
    uint8_t           RSVD12[0xF10 - 0xF04];
    volatile uint32_t CPENDSGIR[4];         /* 0xF10 — SGI清挂起 */
    volatile uint32_t SPENDSGIR[4];         /* 0xF20 — SGI置挂起 */
    uint8_t           RSVD13[0xFD0 - 0xF30];
    volatile uint32_t PIDR4;                /* 0xFD0 — 外设ID4 */
    volatile uint32_t PIDR5;                /* 0xFD4 — 外设ID5 */
    volatile uint32_t PIDR6;                /* 0xFD8 — 外设ID6 */
    volatile uint32_t PIDR7;                /* 0xFDC — 外设ID7 */
    volatile uint32_t PIDR0;                /* 0xFE0 — 外设ID0 */
    volatile uint32_t PIDR1;                /* 0xFE4 — 外设ID1 */
    volatile uint32_t PIDR2;                /* 0xFE8 — 外设ID2 */
    volatile uint32_t PIDR3;                /* 0xFEC — 外设ID3 */
    volatile uint32_t CIDR0;                /* 0xFF0 — 组件ID0 */
    volatile uint32_t CIDR1;                /* 0xFF4 — 组件ID1 */
    volatile uint32_t CIDR2;                /* 0xFF8 — 组件ID2 */
    volatile uint32_t CIDR3;                /* 0xFFC — 组件ID3 */
} GicdRegs_t;

extern volatile GicdRegs_t *const GICD;


typedef struct {
    volatile uint32_t CTLR_CTLRNS;          /* 0x000 — CPU接口控制 */
    volatile uint32_t PMR;                  /* 0x004 — 优先级屏蔽 */
    volatile uint32_t BPR_BPRNS;            /* 0x008 — 二进制点 */
    volatile uint32_t IAR;                  /* 0x00C — 中断确认 */
    volatile uint32_t EOIR;                 /* 0x010 — 中断结束 */
    volatile uint32_t RPR;                  /* 0x014 — 运行优先级 */
    volatile uint32_t HPPIR;                /* 0x018 — 最高待处理中断 */
    volatile uint32_t ABPR;                 /* 0x01C — 备用二进制点 */
    volatile uint32_t AIAR;                 /* 0x020 — 备用中断确认 */
    volatile uint32_t AEOIR;                /* 0x024 — 备用中断结束 */
    volatile uint32_t AHPPIR;               /* 0x028 — 备用最高待处理中断 */
    uint8_t           RSVD0[0x0D0 - 0x02C];
    volatile uint32_t APR0;                 /* 0x0D0 — 活动优先级 */
    uint8_t           RSVD1[0x0E0 - 0x0D4];
    volatile uint32_t NSAPR0;               /* 0x0E0 — 非安全活动优先级 */
    uint8_t           RSVD2[0x0FC - 0x0E4];
    volatile uint32_t IIDR;                 /* 0x0FC — 接口标识 */
    uint8_t           RSVD3[0x1000 - 0x100];
    volatile uint32_t DIR;                  /* 0x1000 — 去激活中断 */
} GiccRegs_t;

extern volatile GiccRegs_t *const GICC;


typedef struct {
    volatile uint32_t HCR;                  /* 0x000 — 超管控制寄存器 */
    volatile uint32_t VTR;                  /* 0x004 — VGIC类型寄存器 */
    volatile uint32_t VMCR;                 /* 0x008 — 虚拟机控制寄存器 */
    uint8_t           RSVD0[0x010 - 0x00C];
    volatile uint32_t MISR;                 /* 0x010 — 维护中断状态寄存器 */
    uint8_t           RSVD1[0x020 - 0x014];
    volatile uint32_t EISR0;                /* 0x020 — 中断结束状态寄存器0 */
    uint8_t           RSVD2[0x030 - 0x024];
    volatile uint32_t ELSR0;                /* 0x030 — 空列表状态寄存器0 */
    uint8_t           RSVD3[0x0F0 - 0x034];
    volatile uint32_t APR0;                 /* 0x0F0 — 活动优先级寄存器0 */
    uint8_t           RSVD4[0x100 - 0x0F4];
    volatile uint32_t LR[4];                /* 0x100 — 列表寄存器0~3 */
} GichRegs_t;

extern volatile GichRegs_t *const GICH;


typedef struct {
    volatile uint32_t CTLR;                 /* 0x000 — 虚拟机控制寄存器 */
    volatile uint32_t PMR;                  /* 0x004 — 优先级屏蔽 */
    volatile uint32_t BPR;                  /* 0x008 — 二进制点 */
    volatile uint32_t IAR;                  /* 0x00C — 中断确认 */
    volatile uint32_t EOIR;                 /* 0x010 — 中断结束 */
    volatile uint32_t RPR;                  /* 0x014 — 运行优先级 */
    volatile uint32_t HPPIR;                /* 0x018 — 最高待处理中断 */
    volatile uint32_t ABPR;                 /* 0x01C — 备用二进制点 */
    volatile uint32_t AIAR;                 /* 0x020 — 备用中断确认 */
    volatile uint32_t AEOIR;                /* 0x024 — 备用中断结束 */
    volatile uint32_t AHPPIR;               /* 0x028 — 备用最高待处理中断 */
    uint8_t           RSVD0[0x0D0 - 0x02C];
    volatile uint32_t APR0;                 /* 0x0D0 — 活动优先级 */
    uint8_t           RSVD1[0x0FC - 0x0D4];
    volatile uint32_t IIDR;                 /* 0x0FC — 接口标识 */
    uint8_t           RSVD2[0x1000 - 0x100];
    volatile uint32_t DIR;                  /* 0x1000 — 去激活中断 */
} GicvRegs_t;

extern volatile GicvRegs_t *const GICV;


#endif /* STM32MP1XX_GIC_H_ */
