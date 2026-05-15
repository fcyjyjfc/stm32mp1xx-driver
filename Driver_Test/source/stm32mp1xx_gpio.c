/*
 * stm32mp1x_gpio.c
 *
 *  Created on: 2025-4-25
 *      Author: gjsbr
 */

#include "stm32mp1xx_gpio.h"

volatile GpioRegs_t *const GPIO_A = (void *)0x50002000;
volatile GpioRegs_t *const GPIO_B = (void *)0x50003000;
volatile GpioRegs_t *const GPIO_C = (void *)0x50004000;
volatile GpioRegs_t *const GPIO_D = (void *)0x50005000;
volatile GpioRegs_t *const GPIO_E = (void *)0x50006000;
volatile GpioRegs_t *const GPIO_F = (void *)0x50007000;
volatile GpioRegs_t *const GPIO_G = (void *)0x50008000;
volatile GpioRegs_t *const GPIO_H = (void *)0x50009000;
volatile GpioRegs_t *const GPIO_I = (void *)0x5000A000;
volatile GpioRegs_t *const GPIO_J = (void *)0x5000B000;
volatile GpioRegs_t *const GPIO_K = (void *)0x5000C000;
volatile GpioRegs_t *const GPIO_Z = (void *)0x54004000;


void GpioMode(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioModer_t mode)
{
	gpio_reg->MODER &= ~(3 << (pin * 2));
	gpio_reg->MODER |= mode << (pin * 2);
}

void GpioOtype(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioOtype_t otype)
{
	gpio_reg->OTYPER &= ~(1 << pin);
	gpio_reg->OTYPER |= otype << pin;
}

void GpioOspeed(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioOspeed_t ospeed)
{
	gpio_reg->OSPEEDR &= ~(3 << (pin * 2));
	gpio_reg->OSPEEDR |= ospeed << (pin * 2);
}

void GpioPullUpDown(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const GpioPupd_t pupd)
{
	gpio_reg->PUPDR &= ~(3 << (pin * 2));
	gpio_reg->PUPDR |= pupd << (pin * 2);
}

uint32_t GpioInData(volatile GpioRegs_t *const gpio_reg, const uint32_t pin)
{
	return (gpio_reg->IDR & (1 << pin)) == (1 << pin);
}

void GpioOutHi(volatile GpioRegs_t *const gpio_reg, const uint32_t pin)
{
	gpio_reg->BSRR |= 1 << pin;
}

void GpioOutLow(volatile GpioRegs_t *const gpio_reg, const uint32_t pin)
{
	gpio_reg->BSRR |= 1 << (pin + 16);
}

void GpioToggle(volatile GpioRegs_t *const gpio_reg, const uint32_t pin)
{
	if (GpioInData(gpio_reg, pin) == 1)
	{
		gpio_reg->BSRR |= 1 << (pin + 16);
	}
	else
	{
		gpio_reg->BSRR |= 1 << pin;
	}
}

void GpioLockPortCfg(volatile GpioRegs_t *const gpio_reg, const uint16_t lck)
{
	gpio_reg->LCKR = (lck & (1 << 16));
	gpio_reg->LCKR = lck;
	gpio_reg->LCKR = (lck & (1 << 16));
}

void GpioAf(volatile GpioRegs_t *const gpio_reg, const uint32_t pin, const uint16_t af)
{
	if (pin < 8)
	{
		gpio_reg->AFRL &= ~(0xF << (pin * 4));
		gpio_reg->AFRL |= af << (pin * 4);
	}
	else
	{
		gpio_reg->AFRH &= ~(0xF << ((pin - 8) * 4));
		gpio_reg->AFRH |= af << ((pin - 8) * 4);
	}
}


