/*
 * stm32mp1xx_etzpc.c
 */

#include "stm32mp1xx_etzpc.h"

volatile EtzpcRegs_t *const ETZPC = (void *)0x5C007000;


/* 索引 0-95 → DECPROTx 寄存器 (每 16 个索引一组) + 2-bit 位域 */
void EtzpcDecprotSet(volatile EtzpcRegs_t *const etzpc, uint32_t index, uint32_t val)
{
    volatile uint32_t *reg  = &etzpc->DECPROT0 + (index >> 4);
    uint32_t shift = (index & 0xF) * 2;

    *reg = (*reg & ~(0x3U << shift)) | ((val & 0x3U) << shift);
}

uint32_t EtzpcDecprotGet(volatile EtzpcRegs_t *const etzpc, uint32_t index)
{
    const volatile uint32_t *reg = &etzpc->DECPROT0 + (index >> 4);
    uint32_t shift = (index & 0xF) * 2;

    return ((*reg) >> shift) & 0x3U;
}


/* 索引 0-95 → DECPROT_LOCKx 寄存器 (每 32 个索引一组) */
void EtzpcDecprotLock(volatile EtzpcRegs_t *const etzpc, uint32_t index)
{
    volatile uint32_t *reg = &etzpc->DECPROT_LOCK0 + (index >> 5);

    *reg |= (1U << (index & 31));
}

uint32_t EtzpcDecprotIsLocked(volatile EtzpcRegs_t *const etzpc, uint32_t index)
{
    const volatile uint32_t *reg = &etzpc->DECPROT_LOCK0 + (index >> 5);

    return ((*reg) >> (index & 31)) & 1U;
}
