/*
 * stm32mp1xx_iwdg.h
 *
 *  Created on: 2025-5-9
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_IWDG_H_
#define STM32MP1XX_IWDG_H_

#include <stdint.h>

typedef struct {
	uint32_t KR;
	uint32_t PR;
	uint32_t RLR;
	uint32_t SR;
	uint32_t WINR;
	uint32_t EWCR;
	uint8_t  RSVD0[0x3F0 - 0x14 - 4];
	uint32_t HWCFGR;
	uint32_t VERR;
	uint32_t IDR;
	uint32_t SIDR;
} IwdgRegs_t;


// 计数时钟分频
typedef enum {
	IWDG_DIV_4		= 0,
	IWDG_DIV_8		= 1,
	IWDG_DIV_16		= 2,
	IWDG_DIV_32		= 3,
	IWDG_DIV_64		= 4,
	IWDG_DIV_128	= 5,
	IWDG_DIV_256	= 6,
	IWDG_DIV_512	= 7,
	IWDG_DIV_1024	= 8
} IwdgDiv_t;


// 看门狗配置
// 超时时间计算 t_timout = (iwdg_rl + 1) / (32000 / (2 ^ (iwdg_div + 2)))
//			   t_timout = (iwdg_rl + 1) * (2 ^ (iwdg_div + 2)) / 32000
// 单位：s
typedef struct {
	uint32_t 	iwdg_rl			: 12;	// 喂狗后重载的计数值
	uint32_t 	iwdg_window		: 12;	// 窗口（计数大于该值喂狗将复位）
	uint32_t 	iwdg_ew_comp	: 12;	// 提前中断比较值（计数低于该值生成提前中断）
	uint32_t 	iwdg_ew_en		: 1;	// 提前中断使能
	uint32_t 	iwdg_win_en		: 1;	// 启用窗口
	IwdgDiv_t	iwdg_div;				// 计数分频
} IwdgCfg_t;


extern volatile IwdgRegs_t *const IWDG1;
extern volatile IwdgRegs_t *const IWDG2;


extern void IwdgCfg(volatile IwdgRegs_t *const iwdg_reg, const IwdgCfg_t *const cfg);
extern void IwdgKickDog(volatile IwdgRegs_t *const iwdg_reg);


#endif /* STM32MP1XX_IWDG_H_ */
