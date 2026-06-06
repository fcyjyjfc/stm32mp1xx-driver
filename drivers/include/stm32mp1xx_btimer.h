/*
 * stm32mp1xx_btimer.h
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_BTIMER_H_
#define STM32MP1XX_BTIMER_H_

#include <stdint.h>


typedef struct {
    uint32_t CR1;
    uint32_t CR2;
    uint32_t RSVD0;
    uint32_t DIER;
    uint32_t SR;
    uint32_t EGR;
    uint8_t  RSVD1[0x24 - 0x14 - 4];
    uint32_t CNT;
    uint32_t PSC;
    uint32_t ARR;
} BasicTimerRegs_t;


// t = (arr + 1) * (psc + 1) / clk
typedef struct {
    uint32_t tim_psc    : 16; // 计数时钟分频
    uint32_t tim_arr    : 16; // 计数重载值
    uint32_t tim_arpe   : 1;  // ARR预加载 0 ARR写入立刻生效，1 ARR写入在下个更新事件生效
    uint32_t tim_opm    : 1;  // 单脉冲模式
    uint32_t tim_urs    : 1;  // Update的中断及DMA源 0 所有事件 1 仅上/下溢事件
    uint32_t tim_udis   : 1;  // update禁止 0 使能 1 禁止
    uint32_t tim_ude    : 1;  // update DMA请求使能
    uint32_t tim_uie    : 1;  // update 中断使能
} BasicTimerCfg_t;


extern volatile BasicTimerRegs_t *const TIM6;
extern volatile BasicTimerRegs_t *const TIM7;

extern uint32_t Tim6UifCnt;
extern uint32_t Tim7UifCnt;

extern void BasicTimerCfg(volatile BasicTimerRegs_t *const tim_reg, const BasicTimerCfg_t *const cfg);
extern void BasicTimerStart(volatile BasicTimerRegs_t *const tim_reg);
extern void BasicTimerStop(volatile BasicTimerRegs_t *const tim_reg);
extern void BasicTimerUg(volatile BasicTimerRegs_t *const tim_reg);
extern uint32_t BasicTimerCnt(volatile BasicTimerRegs_t *const tim_reg);
extern void BasicTimerInit(void);

/* TRGO 主模式选择（CR2.MMS[2:0]）*/
typedef enum {
    BTIM_TRGO_RESET    = 0,  /* UG 位作为 TRGO */
    BTIM_TRGO_ENABLE   = 1,  /* CNT_EN 作为 TRGO */
    BTIM_TRGO_UPDATE   = 2,  /* 更新事件作为 TRGO */
} BtimerTrgo_t;

extern void BasicTimerSetTrgo(volatile BasicTimerRegs_t *const tim_reg, BtimerTrgo_t mode);


#endif /* STM32MP1XX_BTIMER_H_ */
