/*
 * stm32mp1xx_stgen.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */


#include "stm32mp1xx_stgen.h"
#include "stm32mp1xx_rcc.h"


//static volatile StgencRegs_t *const STGENC = (void *)0x5C008000;
//static volatile StgenrRegs_t *const STGENR = (void *)0x5A005000;

StgenRegs_t STGEN = {(void *)0x5C008000, (void *)0x5A005000};


void StgenCfg(StgenRegs_t *const stgen_reg)
{
    // enable clocks and release reset for both STGEN interfaces
    RCC->MP_APB4ENSETR |= 1 << 20;   // STGENROEN: 读接口时钟
    RCC->MP_APB5ENSETR |= 1 << 20;   // STGENEN:   控制接口时钟
    RCC->APB5RSTCLRR |= 1 << 20;     // STGENRST:  释放控制接口复位

    stgen_reg->stgenc->CNTCR |= 1 << 1; // 仿真挂起（计数保持静止）

    stgen_reg->stgenc->CNTFID0 = 64000000; // 计数频率，设置为实际计数频率

    stgen_reg->stgenc->CNTCR |= 1; // 计数使能
}


int32_t StgenIsHalt(StgenRegs_t *const stgen_reg)
{
    return (stgen_reg->stgenc->CNTSR & (1 << 1)) >> 1;
}


uint64_t StgenTim(StgenRegs_t *const stgen_reg)
{
    uint32_t lo, hi, hi2;
    do {
        hi  = stgen_reg->stgenr->CNTCVU;
        lo  = stgen_reg->stgenr->CNTCVL;
        hi2 = stgen_reg->stgenr->CNTCVU;
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}
