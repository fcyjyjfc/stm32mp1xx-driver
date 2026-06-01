/*
 * stm32mp1xx_exti.c
 *
 *  Created on: 2025-6-1
 *      Author: gjsbr
 *
 *  EXTI 驱动
 *  RM0436 section 24.6
 */

#include "stm32mp1xx_exti.h"


#define EXTI_CFG_EVT_MAX        (3U * 32U)          /* 可配置事件 0-95 */
#define EXTI_GPIO_EVT_MAX       16U                  /* EXTICR 仅 EXTI0-15 */


volatile ExtiRegs_t *const EXTI = (void *)EXTI_BASE;


void ExtiSetTrig(uint32_t evt, uint8_t edge)
{
    uint32_t idx = evt >> 5;            /* CFG 块索引 (evt/32) */
    uint32_t bit = 1U << (evt & 0x1F);  /* 块内位偏移 (evt%32) */

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    /* edge bit0 → RTSR: 使能上升沿触发 */
    if (edge & 1)
        EXTI->CFG[idx].RTSR |= bit;
    else
        EXTI->CFG[idx].RTSR &= ~bit;

    /* edge bit1 → FTSR: 使能下降沿触发 */
    if (edge & 2)
        EXTI->CFG[idx].FTSR |= bit;
    else
        EXTI->CFG[idx].FTSR &= ~bit;
}


void ExtiSwInt(uint32_t evt)
{
    uint32_t idx = evt >> 5;
    uint32_t bit = 1U << (evt & 0x1F);

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    /* SWIER 写 1 触发一次上升沿事件, 对应 RPR 位会置 1 */
    EXTI->CFG[idx].SWIER |= bit;
}


void ExtiClearRpr(uint32_t evt)
{
    uint32_t idx = evt >> 5;
    uint32_t bit = 1U << (evt & 0x1F);

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    /* !!! RPR 写 1 清 0, 写 0 无效 !!! */
    EXTI->CFG[idx].RPR |= bit;
}


void ExtiClearFpr(uint32_t evt)
{
    uint32_t idx = evt >> 5;
    uint32_t bit = 1U << (evt & 0x1F);

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    /* !!! FPR 写 1 清 0, 写 0 无效 !!! */
    EXTI->CFG[idx].FPR |= bit;
}


void ExtiSetGpio(uint32_t evt, ExtiGpioPort_t port)
{
    uint32_t idx, shift;

    /* EXTICR 仅支持 EXTI0~15 */
    if (evt >= EXTI_GPIO_EVT_MAX)
        return;

    idx = evt >> 2;                     /* EXTICR[0~3] */
    shift = (evt & 3) << 3;             /* 每个域 8 位 */

    EXTI->EXTICR[idx] &= ~(0xFFU << shift);
    EXTI->EXTICR[idx] |= (uint32_t)port << shift;
}


void ExtiEnableInt(uint32_t cpu, uint32_t evt)
{
    uint32_t idx = evt >> 5;
    uint32_t bit = 1U << (evt & 0x1F);
    ExtiImrEmrBlock_t *blk;

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    /* cpu=1 → C1IMR, cpu=2 → C2IMR */
    blk = (cpu == 2) ? EXTI->C2IMR : EXTI->C1IMR;
    blk[idx].IMR |= bit;
}


void ExtiDisableInt(uint32_t cpu, uint32_t evt)
{
    uint32_t idx = evt >> 5;
    uint32_t bit = 1U << (evt & 0x1F);
    ExtiImrEmrBlock_t *blk;

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    blk = (cpu == 2) ? EXTI->C2IMR : EXTI->C1IMR;
    blk[idx].IMR &= ~bit;
}


void ExtiEnableEvt(uint32_t evt)
{
    uint32_t idx = evt >> 5;
    uint32_t bit = 1U << (evt & 0x1F);

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    /* C2EMR: 使能事件唤醒 CPU2 (不产生中断, 只唤醒) */
    EXTI->C2IMR[idx].EMR |= bit;
}


void ExtiDisableEvt(uint32_t evt)
{
    uint32_t idx = evt >> 5;
    uint32_t bit = 1U << (evt & 0x1F);

    if (evt >= EXTI_CFG_EVT_MAX)
        return;

    EXTI->C2IMR[idx].EMR &= ~bit;
}
