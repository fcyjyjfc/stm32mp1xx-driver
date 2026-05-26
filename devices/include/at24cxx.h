#ifndef __AT24CXX_H_
#define __AT24CXX_H_

#include <stdint.h>
#include "stm32mp1xx_i2c.h"

void AT24C_Init(volatile I2cRegs_t *const i2c, uint8_t slave);

void AT24C_Write(uint16_t addr, const uint8_t *dat, uint32_t len);
void AT24C_Read(uint16_t addr, uint8_t *dat, uint32_t len);

#endif
