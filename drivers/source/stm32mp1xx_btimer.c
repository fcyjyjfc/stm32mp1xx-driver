/*
 * stm32mp1xx_btimer.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */


#include "stm32mp1xx_btimer.h"


volatile BasicTimerRegs_t *const TIM6 = (void *)0x40004000;
volatile BasicTimerRegs_t *const TIM7 = (void *)0x40005000;

uint32_t Tim6UifCnt = 0, Tim7UifCnt = 0;


void BasicTimerCfg(volatile BasicTimerRegs_t *const tim_reg, const BasicTimerCfg_t *const cfg)
{
    tim_reg->CR1 &= ~1; // 先停止计时再配置
    tim_reg->CR1 |= 1 << 11; // 打开UIF映射

    // ARR预加载
    tim_reg->CR1 &= ~(1 << 7);
    tim_reg->CR1 |= cfg->tim_arpe << 7;
    // 单脉冲模式
    tim_reg->CR1 &= ~(1 << 3);
    tim_reg->CR1 |= cfg->tim_opm << 3;
    // 可用于产生中断和DMA请求的更新事件源
    tim_reg->CR1 &= ~(1 << 2);
    tim_reg->CR1 |= cfg->tim_urs << 2;
    // 更新事件禁止
    tim_reg->CR1 &= ~(1 << 1);
    tim_reg->CR1 |= cfg->tim_udis << 1;

    // 更新事件DMA请求使能
    tim_reg->DIER &= ~(1 << 8);
    tim_reg->DIER |= cfg->tim_ude << 8;
    // 更新事件DMA中断使能
    tim_reg->DIER &= ~1;
    tim_reg->DIER |= cfg->tim_uie;

    // 计数器时钟分频
    tim_reg->PSC = cfg->tim_psc;
    // 重载值
    tim_reg->ARR = cfg->tim_arr;

    // tim_reg->CR1 |= 1; // 启动计时
}


void BasicTimerStart(volatile BasicTimerRegs_t *const tim_reg)
{
    tim_reg->CR1 |= 1;
}


void BasicTimerStop(volatile BasicTimerRegs_t *const tim_reg)
{
    tim_reg->CR1 &= ~1;
}


// 生成更新事件（需 CR1.URS=0 才生效，否则仅复位 CNT 不清 UIF）
void BasicTimerUg(volatile BasicTimerRegs_t *const tim_reg)
{
    tim_reg->EGR = 1;
}


uint32_t BasicTimerCnt(volatile BasicTimerRegs_t *const tim_reg)
{
    uint32_t cnt;
    cnt = tim_reg->CNT;
    tim_reg->SR |= 1;
    return cnt;
}


BasicTimerCfg_t Tim6Cfg;
void BasicTimerInit(void)
{
    Tim6Cfg.tim_psc = 15;
    Tim6Cfg.tim_arr = 10000;
    Tim6Cfg.tim_arpe = 1;
    Tim6Cfg.tim_opm = 0;
    Tim6Cfg.tim_urs = 0;
    Tim6Cfg.tim_udis = 0;
    Tim6Cfg.tim_ude = 0;
    Tim6Cfg.tim_uie = 0;

    BasicTimerCfg(TIM6, &Tim6Cfg);
}

