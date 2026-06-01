/*
 * stm32mp1xx_gic.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 *
 *  GICD Distributor + GICC CPU Interface 驱动
 *  RM0436 section 23.5-23.6
 */

#include "stm32mp1xx_gic.h"


#define GIC_MAX_ID          288U


volatile GicdRegs_t *const GICD = (void *)GICD_BASE;
volatile GiccRegs_t *const GICC = (void *)GICC_BASE;

static IrqHandler_t irq_table[288];             /* 中断处理函数表 */


void GicdEnableInt(uint32_t id)
{
    if (id >= GIC_MAX_ID)
        return;

    GICD->ISENABLER[id >> 5] = (1U << (id & 0x1F));
}


void GicdDisableInt(uint32_t id)
{
    if (id >= GIC_MAX_ID)
        return;

    GICD->ICENABLER[id >> 5] = (1U << (id & 0x1F));
}


void GicdSetPending(uint32_t id)
{
    if (id >= GIC_MAX_ID)
        return;

    GICD->ISPENDR[id >> 5] = (1U << (id & 0x1F));
}


void GicdClearPending(uint32_t id)
{
    if (id >= GIC_MAX_ID)
        return;

    GICD->ICPENDR[id >> 5] = (1U << (id & 0x1F));
}


void GicdSetPriority(uint32_t id, uint8_t pri)
{
    if (id >= GIC_MAX_ID)
        return;

    GICD->IPRIORITYR[id] = pri << 3;
}


void GicdSetTarget(uint32_t id, uint8_t cpu_mask)
{
    if (id >= GIC_MAX_ID)
        return;

    GICD->ITARGETSR[id] = cpu_mask;
}


void GicdSetTrigMode(uint32_t id, uint8_t edge)
{
    uint32_t idx, pos;

    if (id >= GIC_MAX_ID)
        return;

    idx = id >> 4;
    pos = (id & 0xFU) << 1;

    GICD->ICFGR[idx] &= ~(3U << pos);
    GICD->ICFGR[idx] |= (edge & 3U) << pos;
}


void GicdSetGroup(uint32_t id)
{
    if (id >= GIC_MAX_ID)
        return;

    GICD->IGROUPR[id >> 5] |= (1U << (id & 0x1F));  /* 1 = Group 1 (非安全) */
}


void GicdInit(void)
{
    /* 使能 Group 0 + Group 1 中断转发到 CPU 接口 */
    GICD->CTLR |= 3;
}


void GiccInit(uint8_t pmr, uint8_t bpr)
{
    GICC->PMR = pmr << 3;
    GICC->BPR_BPRNS = bpr;
    GICC->CTLR_CTLRNS |= 1;     /* ENABLEGRP1 */
}


uint32_t GiccAckInt(void)
{
    return GICC->IAR & 0x7FF;   /* 取低 10 位中断 ID */
}


void GiccEoiInt(uint32_t id)
{
    GICC->EOIR = id;
}


IrqHandler_t GicRegisterIrq(uint32_t id, IrqHandler_t handler)
{
    IrqHandler_t old;

    if (id >= GIC_MAX_ID)
        return (void *)0;

    old = irq_table[id];                            /* 记录旧处理函数 */
    irq_table[id] = handler;                        /* 注册新处理函数 */
    return old;                                     /* 返回旧函数，可恢复 */
}


void do_irq(void)
{
    uint32_t id = GiccAckInt();
    uint32_t spsr;

    if (id < GIC_MAX_ID && irq_table[id])
    {
        /* 保存 SPSR_irq (嵌套中断会覆盖它) */
        __asm__ volatile("mrs %0, spsr" : "=r"(spsr));

        __asm__ volatile(                               /* 开 IRQ 允许抢占 */
            "mrs r0, cpsr\n\t"
            "bic r0, r0, #0x80\n\t"
            "msr cpsr, r0\n\t"
            "isb\n\t"                                   /* 保证 IRQ 立即被响应 */
            :
            :
            : "r0"
        );

        irq_table[id]();

        __asm__ volatile(                               /* 关 IRQ 准备 EOIR */
            "mrs r0, cpsr\n\t"
            "orr r0, r0, #0x80\n\t"
            "msr cpsr, r0\n\t"
            "isb\n\t"                                   /* 保证关中断立即生效 */
            :
            :
            : "r0"
        );

        /* 恢复 SPSR_irq, 保证 first-level 返回 ^ 拿到正确 CPSR */
        __asm__ volatile("msr spsr, %0" : : "r"(spsr));
    }

    GiccEoiInt(id);
}
