/*
 * stm32mp1x_gpio.h
 *
 *  Created on: 2025-4-25
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_GPIO_H_
#define STM32MP1XX_GPIO_H_

#include <stdint.h>

typedef struct {
	uint32_t MODER;
	uint32_t OTYPER;
	uint32_t OSPEEDR;
	uint32_t PUPDR;
	uint32_t IDR;
	uint32_t ODR;
	uint32_t BSRR;
	uint32_t LCKR;
//	uint64_t AFR;
	uint32_t AFRL;
	uint32_t AFRH;
	uint32_t BRR;
	uint32_t RSVD0;
	uint32_t SECCFGR;
	uint8_t  RSVD1[0x3C8 - 0x30 + 4];
	uint32_t HWCFGR[11];
	uint32_t VERR;
	uint32_t IPIDR;
	uint32_t SIDR;
} GpioRegs_t;


typedef enum {
	GPIO_MODER_INPUT,
	GPIO_MODER_OUTPUT,
	GPIO_MODER_AF,
	GPIO_MODER_ANALOG
} GpioModer_t;


typedef enum {
	GPIO_OTYPE_PUSH_PULL,
	GPIO_OTYPE_OD
} GpioOtype_t;


typedef enum {
	GPIO_OSPEED_LOW,
	GPIO_OSPEED_MEDIUM,
	GPIO_OSPEED_HI,
	GPIO_OSPEED_VERY_HI
} GpioOspeed_t;

typedef enum {
	GPIO_PUPDR_NO,
	GPIO_PUPDR_PULL_UP,
	GPIO_PUPDR_PULL_DOWN,
} GpioPupd_t;


extern volatile GpioRegs_t *const GPIO_A;
extern volatile GpioRegs_t *const GPIO_B;
extern volatile GpioRegs_t *const GPIO_C;
extern volatile GpioRegs_t *const GPIO_D;
extern volatile GpioRegs_t *const GPIO_E;
extern volatile GpioRegs_t *const GPIO_F;
extern volatile GpioRegs_t *const GPIO_G;
extern volatile GpioRegs_t *const GPIO_H;
extern volatile GpioRegs_t *const GPIO_I;
extern volatile GpioRegs_t *const GPIO_J;
extern volatile GpioRegs_t *const GPIO_K;
extern volatile GpioRegs_t *const GPIO_Z;


extern void GpioMode (volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioModer_t moder);

extern void GpioOtype(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioOtype_t otype);

extern void GpioOspeed(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioOspeed_t ospeed);

extern void GpioPullUpDown(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioPupd_t pupd);

extern uint32_t GpioInData(volatile GpioRegs_t *const gpio_reg, const uint32_t pin);

extern void GpioOutHi(volatile GpioRegs_t *const gpio_reg, const uint32_t pin);

extern void GpioOutLow(volatile GpioRegs_t *const gpio_reg, const uint32_t pin);

extern void GpioToggle(volatile GpioRegs_t *const gpio_reg, const uint32_t pin);

extern void GpioLockPortCfg(volatile GpioRegs_t *const gpio_reg, const uint16_t lck);

extern void GpioAf(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const uint16_t af);


#endif /* STM32MP1X_GPIO_H_ */
