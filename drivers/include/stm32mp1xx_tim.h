/*
 * stm32mp1xx_tim.h
 *
 *  Created on: 2025-5-8
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_TIM_H_
#define STM32MP1XX_TIM_H_

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

typedef struct {
	uint32_t CR1;
	uint32_t CR2;
	uint32_t SMCR;
	uint32_t DIER;
	uint32_t SR;
	uint32_t EGR;
	uint32_t CCMR1;
	uint32_t CCMR2;
	uint32_t CCER;
	uint32_t CNT;
	uint32_t PSC;
	uint32_t ARR;
	uint32_t RCR;
	uint32_t CCR1;
	uint32_t CCR2;
	uint32_t CCR3;
	uint32_t CCR4;
	uint32_t BDTR;
	uint32_t DCR;
	uint32_t DMAR;
	uint32_t RSVD0;
	uint32_t CCMR3;
	uint32_t CCR5;
	uint32_t CCR6;
	uint32_t AF1;
	uint32_t AF2;
	uint32_t TISEL;
} TimRegs_t;


// 中心对齐模式选择
typedef enum {
	TIM_CMS_DIS,				// 边沿对齐
	TIM_CMS_COMP_FLAG_DOWN,		// 模式1：输出比较标志只在递减计数时设置
	TIM_CMS_COMP_FLAG_UP,		// 模式2：输出比较标志只在递增计数时设置
	TIM_CMS_COMP_FLAG_BOTH		// 模式3：输出比较标志在递减/增均设置
} TimCms_t;


// 边沿对齐，计数器方向
typedef enum {
	TIM_COUNT_UP,
	TIM_COUNT_DOWN,
} TimEdgeDir_t;


// TIM给ADC的同步信号发生源（见CR2寄存器）
typedef enum {
	TIM_ADC_SYNC_SRC_RESET				= 0b0000,
	TIM_ADC_SYNC_SRC_CNT_EN				= 0b0001,
	TIM_ADC_SYNC_SRC_UPDATE				= 0b0010,
	TIM_ADC_SYNC_SRC_COMP				= 0b0011,
	TIM_ADC_SYNC_SRC_OC1REFC			= 0b0100,
	TIM_ADC_SYNC_SRC_OC2REFC			= 0b0101,
	TIM_ADC_SYNC_SRC_OC3REFC			= 0b0110,
	TIM_ADC_SYNC_SRC_OC4REFC			= 0b0111,
	TIM_ADC_SYNC_SRC_OC5REFC			= 0b1000,
	TIM_ADC_SYNC_SRC_OC6REFC			= 0b1001,
	TIM_ADC_SYNC_SRC_OC4REFC_EDGE		= 0b1010,
	TIM_ADC_SYNC_SRC_OC6REFC_EDGE		= 0b1011,
	TIM_ADC_SYNC_SRC_OC4_6REFC_RIS		= 0b1100,
	TIM_ADC_SYNC_SRC_OC4RIS_6FALL		= 0b1101,
	TIM_ADC_SYNC_SRC_OC5_6REFC_RIS		= 0b1110,
	TIM_ADC_SYNC_SRC_OC5RIS_6FALL		= 0b1111
} Tim2AdcSync_t;


// TI1选择
typedef enum {
	TIM_TI_CH1,			// CH1连接至TI1输入
	TIM_TI1_CH123_XOR	// CH1~3通过XOR后连接到TI1
} TimT1Sel_t;


// TIM给其他Timer的同步信号发生源
typedef enum {
	TIM_SYNC_SRC_RESET				= 0b000,
	TIM_SYNC_SRC_CNT_EN				= 0b001,
	TIM_SYNC_SRC_UPDATE				= 0b010,
	TIM_SYNC_SRC_COMP				= 0b011,
	TIM_SYNC_SRC_OC1REFC			= 0b100,
	TIM_SYNC_SRC_OC2REFC			= 0b101,
	TIM_SYNC_SRC_OC3REFC			= 0b110,
	TIM_SYNC_SRC_OC4REFC			= 0b111
} Tim2AdcSync_t;


// CCxE\CCxNE\OCxM位预加载设置
typedef enum {
	TIM_CCPC_DIS,			// 禁止预加载
	TIM_CCUS_COMG,			// 仅在设置COMG位时更新
	TIM_CCUS_COMG_TRGI		// 在设置COMG位和TRGI的上升沿更新
} CcusPreLoad_t;


typedef struct {
	TimCms_t		tim_cms;			// 中心对齐模式选择
	TimEdgeDir_t	tim_edge_dir;		// 边沿对齐计数方向
	uint32_t		tim_opm;			// 单脉冲选择
	uint32_t		tim_urs;			// Update请求源（中断及DMA请求）
	uint32_t		tim_udis;			// Update禁止 1：仅UG位和来自从模块控制器的硬件复位产生更新事件；0：计数上/下溢也能产生
	uint32_t		tim_start;			// 开启计数
} TimerCfg_t;


#endif /* STM32MP1XX_TIM_H_ */
