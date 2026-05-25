#ifndef __M74HC595_H_
#define __M74HC595_H_

#include <stdint.h>
#include "stm32mp1xx_spi.h"

void Led_Init(volatile SpiRegs_t *const spi);

/**
 * @brief 指定一个数码管显示数字
 * @param digit  数码管编号 1~4
 * @param num    显示的数字 0~15（0x0~0xF）
 */
void Led_DisplayDigit(uint8_t digit, uint8_t num);

/**
 * @brief 4 个数码管同时显示同一个数字
 * @param num    显示的数字 0~15（0x0~0xF）
 */
void Led_DisplayAll(uint8_t num);

void Led_Clear(void);

#endif
