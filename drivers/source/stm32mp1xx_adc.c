/*
 * stm32mp1xx_adc.c
 *
 *  Created on: 2026-6-4
 *      Author: gjsbr
 *
 *  ADC 寄存器地址指针定义
 *  RM0436 section 29
 */

#include "stm32mp1xx_adc.h"


volatile AdcRegs_t *const ADC = (void *)ADC_BASE;
