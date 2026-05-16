/*
 * stm32mp1xx_iwdg.c
 *
 *  Created on: 2025-5-9
 *      Author: gjsbr
 */

#include "stm32mp1xx_iwdg.h"

volatile IwdgRegs_t *const IWDG1 = (void *)0x5C003000;
volatile IwdgRegs_t *const IWDG2 = (void *)0x5A002000;


void IwdgCfg(volatile IwdgRegs_t *const iwdg_reg, const IwdgCfg_t *const cfg)
{
    iwdg_reg->KR = 0xCCCC; // 使能看门狗（计数值从4095开始递减）

    iwdg_reg->KR = 0x5555; // 允许访问写保护寄存器

    iwdg_reg->PR = cfg->iwdg_div; // 计数器时钟分频

    iwdg_reg->RLR = cfg->iwdg_rl; // 刷新时重载值

    // 使能提前唤醒中断
    if (cfg->iwdg_ew_en == 1)
    {
        iwdg_reg->EWCR |= 1 << 15; // 使能
        iwdg_reg->EWCR &= ~0xFFF;
        iwdg_reg->EWCR |= cfg->iwdg_ew_comp; // 设置提前中断的比较值
    }

    if (cfg->iwdg_win_en == 1)
    {
        iwdg_reg->WINR = cfg->iwdg_window; // 喂狗窗口
    }

    iwdg_reg->KR = 0xAAAA; // 禁止访问写保护寄存器（RLR的值被载入并开始递减 ）

//    while (iwdg_reg->SR != 0)
//    {
//        ;
//    }
}


// 看门狗喂狗
void IwdgKickDog(volatile IwdgRegs_t *const iwdg_reg)
{
    iwdg_reg->KR = 0xAAAA; // 复位计数
}
