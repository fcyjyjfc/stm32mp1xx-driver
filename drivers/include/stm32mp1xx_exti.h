/*
 * stm32mp1xx_exti.h
 *
 *  Created on: 2025-5-31
 *      Author: gjsbr
 *
 *  EXTI register map
 *  RM0436 section 24.6, Table 129
 */

#ifndef STM32MP1XX_EXTI_H_
#define STM32MP1XX_EXTI_H_

#include <stdint.h>


#define EXTI_BASE               0x5000D000U


/* EXTI GPIO 端口选择 */
typedef enum {
    EXTI_GPIO_PA  = 0,
    EXTI_GPIO_PB  = 1,
    EXTI_GPIO_PC  = 2,
    EXTI_GPIO_PD  = 3,
    EXTI_GPIO_PE  = 4,
    EXTI_GPIO_PF  = 5,
    EXTI_GPIO_PG  = 6,
    EXTI_GPIO_PH  = 7,
    EXTI_GPIO_PI  = 8,
    EXTI_GPIO_PJ  = 9,
    EXTI_GPIO_PK  = 10,
    EXTI_GPIO_PZ  = 11,
} ExtiGpioPort_t;


/* 可配置事件寄存器块 (每组 0x20 字节) */
typedef struct {
    volatile uint32_t RTSR;
    volatile uint32_t FTSR;
    volatile uint32_t SWIER;
    volatile uint32_t RPR;
    volatile uint32_t FPR;
    volatile uint32_t TZENR;
    uint8_t           RSVD[0x020 - 0x018];
} ExtiCfgBlock_t;

/* IMR + EMR 寄存器块 (每组 0x10 字节) */
typedef struct {
    volatile uint32_t IMR;
    volatile uint32_t EMR;
    uint8_t           RSVD[0x010 - 0x008];
} ExtiImrEmrBlock_t;


typedef struct {
    ExtiCfgBlock_t    CFG[3];         /* 0x000, 0x020, 0x040 */

    /* 0x060 — EXTI mux GPIO 端口选择 */
    volatile uint32_t EXTICR[4];        /* EXTI0~15 端口选择     */
    uint8_t           RSVD3[0x080 - 0x070];

    /* 0x080 — CPU1 中断掩码 (EMR 保留) */
    ExtiImrEmrBlock_t C1IMR[3];          /* 0x080, 0x090, 0x0A0 */
    uint8_t           RSVD4[0x0C0 - 0x0B0];

    /* 0x0C0 — CPU2 中断/事件掩码 */
    ExtiImrEmrBlock_t C2IMR[3];          /* 0x0C0, 0x0D0, 0x0E0 */
    uint8_t           RSVD5[0x3C0 - 0x0F0];

    /* 0x3C0 — HW 配置寄存器 */
    volatile uint32_t HWCFGR13;            /* 0x3C0 */
    volatile uint32_t HWCFGR12;            /* 0x3C4 */
    volatile uint32_t HWCFGR11;            /* 0x3C8 */
    volatile uint32_t HWCFGR10;            /* 0x3CC */
    volatile uint32_t HWCFGR9;             /* 0x3D0 */
    volatile uint32_t HWCFGR8;             /* 0x3D4 */
    volatile uint32_t HWCFGR7;             /* 0x3D8 */
    volatile uint32_t HWCFGR6;             /* 0x3DC */
    volatile uint32_t HWCFGR5;             /* 0x3E0 */
    volatile uint32_t HWCFGR4;             /* 0x3E4 */
    volatile uint32_t HWCFGR3;             /* 0x3E8 */
    volatile uint32_t HWCFGR2;             /* 0x3EC */
    volatile uint32_t HWCFGR1;             /* 0x3F0 */
    volatile uint32_t VERR;                /* 0x3F4 IP 版本 */
    volatile uint32_t IPIDR;               /* 0x3F8 IP 标识 */
    volatile uint32_t SIDR;                /* 0x3FC 大小 ID */
} ExtiRegs_t;

extern volatile ExtiRegs_t *const EXTI;


/* edge: bit0=上升沿, bit1=下降沿. 取值 0=关, 1=上升沿, 2=下降沿, 3=双边沿 */
void ExtiSetTrig(uint32_t evt, uint8_t edge);
void ExtiSwInt(uint32_t evt);                           /* 软件触发一次上升沿事件 */
void ExtiClearRpr(uint32_t evt);                        /* 清除上升沿挂起标志 */
void ExtiClearFpr(uint32_t evt);                        /* 清除下降沿挂起标志 */
void ExtiSetGpio(uint32_t evt, ExtiGpioPort_t port);    /* evt=0~15, 指定EXTI线对应的GPIO端口 */
void ExtiEnableInt(uint32_t cpu, uint32_t evt);         /* cpu=1或2, 使能EXTI中断唤醒CPU */
void ExtiDisableInt(uint32_t cpu, uint32_t evt);        /* 禁能EXTI中断唤醒CPU */
void ExtiEnableEvt(uint32_t evt);                       /* 使能EXTI事件唤醒CPU2 (不产生中断) */
void ExtiDisableEvt(uint32_t evt);                      /* 禁能EXTI事件唤醒CPU2 */


#endif /* STM32MP1XX_EXTI_H_ */
