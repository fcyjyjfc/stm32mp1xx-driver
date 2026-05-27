#include "si7006.h"

static volatile I2cRegs_t *SI7006_I2C;


void SI7006_Init(volatile I2cRegs_t *const i2c)
{
    SI7006_I2C = i2c;
}


int32_t SI7006_ReadTemp(void)
{
    uint8_t cmd = 0xE3;
    uint8_t buf[2];

    I2cMstWrite(SI7006_I2C, 0x80, &cmd, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
    I2cMstRead(SI7006_I2C, 0x80, buf, 2, I2C_BUS_RESTART, I2C_BUS_STOP);

    uint16_t raw = (buf[0] << 8) | buf[1];
    return 17572 * (int32_t)raw / 65536 - 4685;
}


int32_t SI7006_ReadHumi(void)
{
    uint8_t cmd = 0xE5;
    uint8_t buf[2];

    I2cMstWrite(SI7006_I2C, 0x80, &cmd, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
    I2cMstRead(SI7006_I2C, 0x80, buf, 2, I2C_BUS_RESTART, I2C_BUS_STOP);

    uint16_t raw = (buf[0] << 8) | buf[1];
    return 12500 * (int32_t)raw / 65536 - 600;
}
