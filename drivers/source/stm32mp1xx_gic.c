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
    uint32_t id = GiccAckInt();                     /* 读 IAR 获取中断 ID */

    if (id < GIC_MAX_ID && irq_table[id])
        irq_table[id]();                            /* 分发到注册的处理函数 */

    GiccEoiInt(id);                                 /* 写 EOIR 结束中断 */
}
