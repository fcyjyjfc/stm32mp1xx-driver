/*
 * stm32mp1xx_btimer.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */


#include "stm32mp1xx_btimer.h"
#include "stm32mp1xx_rcc.h"


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


// 清 UIF: SR.UIF 是 rc_w0, 写 0 才清 (写 1 无效, 常见错误用法)
void BasicTimerClrUif(volatile BasicTimerRegs_t *const tim_reg)
{
    tim_reg->SR &= ~1U;
}


// 读 UIF 状态: 1 = 更新事件挂起, 0 = 无
uint32_t BasicTimerUifGet(volatile BasicTimerRegs_t *const tim_reg)
{
    return tim_reg->SR & 1U;
}


uint32_t BasicTimerCnt(volatile BasicTimerRegs_t *const tim_reg)
{
    uint32_t cnt;
    cnt = tim_reg->CNT;
    BasicTimerClrUif(tim_reg);
    return cnt;
}


void BasicTimerSetTrgo(volatile BasicTimerRegs_t *const tim_reg, BtimerTrgo_t mode)
{
    tim_reg->CR2 &= ~(7u << 4);
    tim_reg->CR2 |= (mode & 7u) << 4;
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


// 外设时钟使能 (TIM6=APB1 bit4, TIM7=APB1 bit5)
void BasicTimerClkEnable(volatile BasicTimerRegs_t *const tim_reg)
{
    if (tim_reg == TIM6)
        RCC->MP_APB1ENSETR |= (1u << 4);
    else if (tim_reg == TIM7)
        RCC->MP_APB1ENSETR |= (1u << 5);
}


/**
 * @brief   毫秒级忙等延时
 * @param   tim_reg  定时器 (TIM6/TIM7)
 * @param   ms       延时毫秒数
 * @note    自包含: 每次调用重新装载配置再启动, 幂等, 不依赖调用方先初始化.
 *          定时器时钟假设 64MHz (TIM6/7 挂 APB1 定时器时钟, 由系统配置决定):
 *          PSC=6399 → 10kHz 计数, ARR=9 → 每 10 计数一次更新 = 1ms.
 *          流程: Stop → UG 装载影子 PSC/ARR 并清 CNT → ClrUif → Start →
 *          轮询 UIF 计数. UIF 是 rc_w0, 必须写 0 清除.
 *          不用静态标志记录"已初始化": 启动代码不清 .bss, 静态标志复位后
 *          是 DDR 残留 (可能非 0), 靠它跳过配置会让时钟未使能而在延时里
 *          死循环 (历史回归根因), 故每次全量重配.
 */
void BasicTimerDelayMs(volatile BasicTimerRegs_t *const tim_reg, uint32_t ms)
{
    BasicTimerCfg_t cfg;

    cfg.tim_psc  = 6399;
    cfg.tim_arr  = 9;
    cfg.tim_arpe = 1;
    cfg.tim_opm  = 0;
    cfg.tim_urs  = 0;
    cfg.tim_udis = 0;
    cfg.tim_ude  = 0;
    cfg.tim_uie  = 0;

    BasicTimerClkEnable(tim_reg);           /* 时钟使能 (SETR 置位, 幂等) */
    BasicTimerCfg(tim_reg, &cfg);           /* 重配: 1ms/次更新 */
    BasicTimerStop(tim_reg);
    BasicTimerUg(tim_reg);                  /* 装载影子 PSC/ARR + CNT 归零 */
    BasicTimerClrUif(tim_reg);              /* 清残留 UIF (rc_w0, 写 0) */
    BasicTimerStart(tim_reg);

    while (ms > 0)
    {
        while (!BasicTimerUifGet(tim_reg))  /* 等本次 1ms 更新 */
            ;
        BasicTimerClrUif(tim_reg);
        ms--;
    }

    BasicTimerStop(tim_reg);                /* 停表, 不干扰后续使用 */
}
