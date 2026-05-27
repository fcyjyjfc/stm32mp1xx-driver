#ifndef __SI7006_H_
#define __SI7006_H_

#include <stdint.h>
#include "stm32mp1xx_i2c.h"


void SI7006_Init(volatile I2cRegs_t *const i2c);
int32_t SI7006_ReadTemp(void);
int32_t SI7006_ReadHumi(void);


#endif
