/*
 * stm32mp1xx_i2c.h
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_I2C_H_
#define STM32MP1XX_I2C_H_

#include <stdint.h>


typedef struct {
    uint32_t CR1;
    uint32_t CR2;
    uint32_t OAR1;
    uint32_t OAR2;
    uint32_t TIMINGR;
    uint32_t TIMOUTR;
    uint32_t ISR;
    uint32_t ICR;
    uint32_t PECR;
    uint32_t RXDR;
    uint32_t TXDR;
    uint8_t  RSVD0[0x3F0 - 0x28 - 4];
    uint32_t HWCFGR;
    uint32_t VERR;
    uint32_t IPIDR;
    uint32_t SIDR;
} I2cRegs_t;


typedef enum {
	I2C_BUS_STOP,
	I2C_BUS_NO_STOP
} I2cBusStop_t;


typedef enum {
	I2C_BUS_START,	// 本次通信为start
	I2C_BUS_RESTART // 本次通信为restart
} I2cBusStart_t;


extern volatile I2cRegs_t *const I2C6;
extern volatile I2cRegs_t *const I2C4;
extern volatile I2cRegs_t *const I2C5;
extern volatile I2cRegs_t *const I2C3;
extern volatile I2cRegs_t *const I2C2;
extern volatile I2cRegs_t *const I2C1;


extern void I2cCfg(volatile I2cRegs_t *const i2c);
extern void I2cMstWrite(volatile I2cRegs_t *const, const uint16_t, const uint8_t *, const uint32_t, const I2cBusStart_t start, const I2cBusStop_t);
extern void I2cMstRead(volatile I2cRegs_t *const, const uint16_t, uint8_t *, const uint32_t, const I2cBusStart_t start, const I2cBusStop_t);
extern void I2cReadE2(volatile I2cRegs_t *const i2c_reg, const uint16_t slave, uint16_t addr, uint8_t *dat, const uint32_t len);
extern void I2cWriteE2(volatile I2cRegs_t *const i2c_reg, const uint16_t slave, uint16_t addr, const uint8_t *dat, const uint32_t len);
extern void I2cWriteEeprom(volatile I2cRegs_t *const i2c, uint8_t slave, uint16_t addr, uint8_t *dat, uint32_t len);
extern void I2cReadEeprom(volatile I2cRegs_t *const i2c, uint8_t slave, uint16_t addr, uint8_t *dat, uint32_t len);


#endif /* STM32MP1XX_I2C_H_ */
