#include "at24cxx.h"

static volatile I2cRegs_t *AT24C_I2C;
static uint8_t AT24C_Slave;

void AT24C_Init(volatile I2cRegs_t *const i2c, uint8_t slave)
{
    AT24C_I2C   = i2c;
    AT24C_Slave = slave;
}

void AT24C_Write(uint16_t addr, const uint8_t *dat, uint32_t len)
{
    uint8_t buf[258];
    uint32_t i;

    buf[0] = (addr >> 8) & 0xFF;
    buf[1] = addr & 0xFF;
    for (i = 0; i < len; i++)
        buf[i + 2] = dat[i];

    I2cMstWrite(AT24C_I2C, AT24C_Slave, buf, len + 2, I2C_BUS_START, I2C_BUS_STOP);
}

void AT24C_Read(uint16_t addr, uint8_t *dat, uint32_t len)
{
    uint8_t buf[2];

    buf[0] = (addr >> 8) & 0xFF;
    buf[1] = addr & 0xFF;

    I2cMstWrite(AT24C_I2C, AT24C_Slave, buf, 2, I2C_BUS_START, I2C_BUS_NO_STOP);
    while ((AT24C_I2C->ISR & (1 << 6)) == 0);
    I2cMstRead(AT24C_I2C, AT24C_Slave, dat, len, I2C_BUS_RESTART, I2C_BUS_STOP);
}
