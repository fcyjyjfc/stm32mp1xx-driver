/*
 * stm32mp1xx_stgen.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */


#include "stm32mp1xx_stgen.h"


//static volatile StgencRegs_t *const STGENC = (void *)0x5C008000;
//static volatile StgenrRegs_t *const STGENR = (void *)0x5A005000;

const StgenRegs_t STGEN = {(void *)0x5C008000, (void *)0x5A005000};


void StgenCfg(StgenRegs_t *const stgen_reg)
{
    stgen_reg->stgenc->CNTCR |= 1 << 1; // 仿真挂起（计数保持静止）

    stgen_reg->stgenc->CNTFID0 = 10000000; // 计数频率，设置为实际计数频率

    stgen_reg->stgenc->CNTCR |= 1; // 计数使能
}


int32_t StgenIsHalt(StgenRegs_t *const stgen_reg)
{
    return (stgen_reg->stgenc->CNTSR & (1 << 1)) >> 1;
}


uint64_t StgenTim(StgenRegs_t *const stgen_reg)
{
    uint64_t tim_l, tim_hi;
    tim_l = stgen_reg->stgenr->CNTCVL;
    tim_hi = stgen_reg->stgenr->CNTCVU;

    return (tim_hi << 32) | tim_l;
}
