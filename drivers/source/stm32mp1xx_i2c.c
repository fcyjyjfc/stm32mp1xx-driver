/*
 * stm32mp1xx_i2c.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */


#include "stm32mp1xx_i2c.h"


volatile I2cRegs_t *const I2C6 = (void *)0x5C009000;
volatile I2cRegs_t *const I2C4 = (void *)0x5C002000;
volatile I2cRegs_t *const I2C5 = (void *)0x40015000;
volatile I2cRegs_t *const I2C3 = (void *)0x40014000;
volatile I2cRegs_t *const I2C2 = (void *)0x40013000;
volatile I2cRegs_t *const I2C1 = (void *)0x40012000;


void I2cCfg(volatile I2cRegs_t *const i2c)
{
    i2c->CR1 &= ~1; // 禁止I2C

    i2c->CR1 &= ~(1 << 12); // 开启模拟滤波
    i2c->CR1 &= ~(0xF << 8); // 关闭数字滤波

    i2c->TIMINGR = 0x10707dbc;

    i2c->CR1 |= 1; // 使能I2C
}


void I2cMstWrite(volatile I2cRegs_t *const i2c_reg, const uint16_t slave, const uint8_t *dat, const uint32_t len, const I2cBusStart_t start, const I2cBusStop_t stop)
{
    if (start == I2C_BUS_START)
    {
        if ((i2c_reg->ISR & (1 << 15)) == (1 << 15))
        {
            return; // 总线忙
        }
    }

    // 设置要通信的从机地址
    i2c_reg->CR2 &= ~0x1FF;
    i2c_reg->CR2 |= slave;

    i2c_reg->CR2 &= ~(1 << 10); // 方向：写

    uint32_t cur_wr_len = 0, total_wr_len = 0;
    uint32_t first = 1;
    uint32_t reload = 0;
    while (total_wr_len < len)
    {
        cur_wr_len = len - total_wr_len; // 剩余未发送的数据长度

        // 长度大于255时发送255个并设置RELOAD
        if (cur_wr_len > 255)
        {
            cur_wr_len = 255;
            i2c_reg->CR2 |= 1 << 24;
            reload = 1;
        }
        else
        {
            i2c_reg->CR2 &= ~(1 << 24); // 小于等于255个，不RELOAD
        }

        // NBYTES 发送字节数
        i2c_reg->CR2 &= ~(255 << 16);
        i2c_reg->CR2 |= cur_wr_len << 16;

        // 判断通信结束后是否生成STOP
        if (stop == I2C_BUS_STOP)
        {
            // 经过测试，发送字节数大于255时（即分为多次发送，前面的发送设置RELOAD），设置
            // AUTOEND不会生成STOP从而导致总线一直忙无法发起新的通信；只有发送字节<=255时
            // AUTOEND才生成STOP
            // 因此len<=255使用AUTOEND，len>255自动发送STOP
            if (len > 255)
            {
                i2c_reg->CR2 &= ~(1 << 25); // 不AUTOEND
            }
            else
            {
                i2c_reg->CR2 |= 1 << 25; // AUTOEND
            }
        }
        else
        {
            i2c_reg->CR2 &= ~(1 << 25); // 不AUTOEND
        }

        // 仅在发送开始时发送一个START
        if (first == 1)
        {
            i2c_reg->CR2 |= 1 << 13; // 发送START自动进入主机模式
            first = 0;
        }

        uint32_t index;
        for (index = 0; index < cur_wr_len; index++)
        {
            while ((i2c_reg->ISR & (1 << 1)) == 0) // 等待TXDR为空
            {
                ;
            }
            i2c_reg->TXDR = dat[total_wr_len + index]; // 写入发送数据
        }

        // 若设置了RELOAD，等待TCR标志被设置
        if (reload == 1)
        {
            while ((i2c_reg->ISR & (1 << 7)) == 0)
            {
                ;
            }
            reload = 0;
        }

        total_wr_len += cur_wr_len; // 更新已发送长度
    }

    // 若通信结束后要生成STOP则设置STOP等待总线产生STOP
    if (stop == I2C_BUS_STOP)
    {
        if (len > 255)
        {
            // 等待所有数据发送完毕
            while ((i2c_reg->ISR & (1 << 1)) == 0)
            {
                ;
            }
            i2c_reg->CR2 |= 1 << 14; // 手动发送STOP
        }
        else
        {
            // 等待直到检测到STOP
            while ((i2c_reg->ISR & (1 << 5)) == 0)
            {
                ;
            }
        }
    }
}


void I2cMstRead(volatile I2cRegs_t *const i2c_reg, const uint16_t slave, uint8_t *dat, const uint32_t len, const I2cBusStart_t start, const I2cBusStop_t stop)
{
    // 本次通信为start，则判断总线是否busy
    if (start == I2C_BUS_START)
    {
        if ((i2c_reg->ISR & (1 << 15)) == (1 << 15))
        {
            return; // 总线忙
        }
    }

    // 设置要通信的从机地址
    i2c_reg->CR2 &= ~0x1FF;
    i2c_reg->CR2 |= slave;

    i2c_reg->CR2 |= 1 << 10; // 方向：读

    uint32_t cur_rd_len = 0, total_rd_len = 0;
    uint32_t first = 1;
    uint32_t reload = 0;
    while (total_rd_len < len)
    {
        cur_rd_len = len - total_rd_len; // 剩余未发送的数据长度

        // 长度大于255时接收255个并设置RELOAD
        if (cur_rd_len > 255)
        {
            cur_rd_len = 255;
            i2c_reg->CR2 |= 1 << 24;
            reload = 1;
        }
        else
        {
            i2c_reg->CR2 &= ~(1 << 24); // 小于等于255个，不RELOAD
        }

        // NBYTES 发送字节数
        i2c_reg->CR2 &= ~(255 << 16);
        i2c_reg->CR2 |= cur_rd_len << 16;

        if (stop == I2C_BUS_STOP)
        {
            if (len > 255)
            {
                i2c_reg->CR2 &= ~(1 << 25); // AUTOEND
            }
            else
            {
                i2c_reg->CR2 |= 1 << 25; // AUTOEND
            }
        }

        // 仅在发送开始时发送一个START
        if (first == 1)
        {
            i2c_reg->CR2 |= 1 << 13; // 发送START自动进入主机模式
            first = 0;
        }

        uint32_t index;
        for (index = 0; index < cur_rd_len; index++)
        {
            while ((i2c_reg->ISR & (1 << 2)) == 0) // 等待RXDR不为空
            {
                ;
            }
            dat[index] = i2c_reg->RXDR; // 读取接收数据
        }

        // 若设置了RELOAD，等待TCR标志被设置
        if (reload == 1)
        {
            while ((i2c_reg->ISR & (1 << 7)) == 0)
            {
                ;
            }
            reload = 0;
        }

        total_rd_len += cur_rd_len; // 更新已发送长度
    }

    if (stop == I2C_BUS_STOP)
    {
        if (len > 255)
        {
            i2c_reg->CR2 |= 1 << 14; // 手动发送STOP
        }
        else
        {
            // 等待直到检测到STOP
            while ((i2c_reg->ISR & (1 << 5)) == 0)
            {
                ;
            }
        }
    }
}


void I2cWriteE2(volatile I2cRegs_t *const i2c_reg, const uint16_t slave, uint16_t addr, const uint8_t *dat, const uint32_t len)
{
    if ((i2c_reg->ISR & (1 << 15)) == (1 << 15))
    {
        return; // 总线忙
    }

    // 设置要通信的从机地址
    i2c_reg->CR2 &= ~0x1FF;
    i2c_reg->CR2 |= slave;

    i2c_reg->CR2 &= ~(1 << 10); // 方向：写

    uint32_t cur_wr_len = 0, total_wr_len = 0;
    uint32_t first = 1;
    uint32_t reload = 0;
    uint32_t dat_index = 0;
    while (total_wr_len < len)
    {
        cur_wr_len = len + 2 - total_wr_len; // 剩余未发送的数据长度

        // 长度大于255时发送255个并设置RELOAD
        if (cur_wr_len > 255)
        {
            cur_wr_len = 255;
            i2c_reg->CR2 |= 1 << 24;
            reload = 1;
        }
        else
        {
            i2c_reg->CR2 &= ~(1 << 24); // 小于等于255个，不RELOAD

        }

        // NBYTES 发送字节数
        i2c_reg->CR2 &= ~(255 << 16);
        i2c_reg->CR2 |= cur_wr_len << 16;

        // 经过测试，发送字节数大于255时（即分为多次发送，前面的发送设置RELOAD），设置
        // AUTOEND不会生成STOP从而导致总线一直忙无法发起新的通信；只有发送字节<=255时
        // AUTOEND才生成STOP
        // 因此len<=255使用AUTOEND，len>255自动发送STOP
        if (len > 255)
        {
            i2c_reg->CR2 &= ~(1 << 25); // AUTOEND
        }
        else
        {
            i2c_reg->CR2 |= 1 << 25; // AUTOEND
        }

        uint32_t index = 0;
        // 仅在发送开始时发送一个START
        if (first == 1)
        {
            i2c_reg->CR2 |= 1 << 13; // 发送START自动进入主机模式
            first = 0;

            // 先发送地址
            uint8_t *buf = (void *)&addr;
            for (index = 0; index < 2; index++)
            {
                while ((i2c_reg->ISR & (1 << 1)) == 0) // 等待TXDR为空
                {
                    ;
                }

                i2c_reg->TXDR = buf[1 - index]; // 地址以大端发送
            }
        }

        for (; index < cur_wr_len; index++, dat_index++)
        {
            while ((i2c_reg->ISR & (1 << 1)) == 0) // 等待TXDR为空
            {
                ;
            }
            i2c_reg->TXDR = dat[total_wr_len + dat_index]; // 写入发送数据
        }

        // 若设置了RELOAD，等待TCR标志被设置
        if (reload == 1)
        {
            while ((i2c_reg->ISR & (1 << 7)) == 0)
            {
                ;
            }
            reload = 0;
        }

        total_wr_len += cur_wr_len; // 更新已发送长度
    }

    if (len + 2 > 255)
    {
        // 等待所有数据发送完毕
        while ((i2c_reg->ISR & (1 << 1)) == 0)
        {
            ;
        }
        i2c_reg->CR2 |= 1 << 14; // 手动发送STOP
    }
    else
    {
        // 等待直到检测到STOP
        while ((i2c_reg->ISR & (1 << 5)) == 0)
        {
            ;
        }
    }
}


void I2cReadE2(volatile I2cRegs_t *const i2c_reg, const uint16_t slave, uint16_t addr, uint8_t *dat, const uint32_t len)
{
    if ((i2c_reg->ISR & (1 << 15)) == (1 << 15))
    {
        return; // 总线忙
    }

    // 设置要通信的从机地址
    i2c_reg->CR2 &= ~0x1FF;
    i2c_reg->CR2 |= slave;

    // 1、发起一个无STOP的写，将读取的数据地址发给E2
    i2c_reg->CR2 &= ~(1 << 10); // 写

    i2c_reg->CR2 &= ~(1 << 25); // 不自动结束

    // 发送字节数，E2数据地址长度
    i2c_reg->CR2 &= ~(255 << 16);
    i2c_reg->CR2 |= 2 << 16;

    i2c_reg->CR2 |= 1 << 13; // 发送START自动进入主机模式

    uint8_t *buf = (void *)&addr;
    uint32_t index;
    for (index = 0; index < 2; index++)
    {
        while ((i2c_reg->ISR & (1 << 1)) == 0) // 等待TXDR为空
        {
            ;
        }

        i2c_reg->TXDR = buf[1 - index];
    }

    while ((i2c_reg->ISR & (1 << 6)) == 0) // 等待传输完成TC
    {
        ;
    }

    // 2、RESTART一个STOP的读，读取E2数据
    i2c_reg->CR2 |= 1 << 10; // 方向：读

    uint32_t cur_rd_len = 0, total_rd_len = 0;
    uint32_t first = 1;
    uint32_t reload = 0;
    while (total_rd_len < len)
    {
        cur_rd_len = len - total_rd_len; // 剩余未发送的数据长度

        // 长度大于255时接收255个并设置RELOAD
        if (cur_rd_len > 255)
        {
            cur_rd_len = 255;
            i2c_reg->CR2 |= 1 << 24;
            reload = 1;
        }
        else
        {
            i2c_reg->CR2 &= ~(1 << 24); // 小于等于255个，不RELOAD
        }

        // NBYTES 发送字节数
        i2c_reg->CR2 &= ~(255 << 16);
        i2c_reg->CR2 |= cur_rd_len << 16;

//        i2c_reg->CR2 |= 1 << 25; // AUTOEND
        if (len > 255)
        {
            i2c_reg->CR2 &= ~(1 << 25); // AUTOEND
        }
        else
        {
            i2c_reg->CR2 |= 1 << 25; // AUTOEND
        }

        // 仅在发送开始时发送一个START
        if (first == 1)
        {
            i2c_reg->CR2 |= 1 << 13; // 发送START自动进入主机模式
            first = 0;
        }

        uint32_t index;
        for (index = 0; index < cur_rd_len; index++)
        {
            while ((i2c_reg->ISR & (1 << 2)) == 0) // 等待TXDR为空
            {
                ;
            }
            dat[index] = i2c_reg->RXDR; // 读取接收数据
        }

        // 若设置了RELOAD，等待TCR标志被设置
        if (reload == 1)
        {
            while ((i2c_reg->ISR & (1 << 7)) == 0)
            {
                ;
            }
            reload = 0;
        }

        total_rd_len += cur_rd_len; // 更新已发送长度
    }

    if (len > 255)
    {
        i2c_reg->CR2 |= 1 << 14; // 手动发送STOP
    }
    else
    {
        // 等待直到检测到STOP
        while ((i2c_reg->ISR & (1 << 5)) == 0)
        {
            ;
        }
    }
}


uint8_t I2cWrBuf[4096];


void I2cWriteEeprom(volatile I2cRegs_t *const i2c, uint8_t slave, uint16_t addr, uint8_t *dat, uint32_t len)
{
    uint8_t *buf = (uint8_t *)&addr;
    int i;
    for (i = 0; i < 2; i++)
    {
        I2cWrBuf[i] = buf[1 - i];
    }
    for (; i < len + 2; i++)
    {
        I2cWrBuf[i] = dat[i - 2];
    }
    // START一个写传输，结束后STOP
    I2cMstWrite(i2c, slave, I2cWrBuf, len + 2, I2C_BUS_START, I2C_BUS_STOP);
}


void I2cReadEeprom(volatile I2cRegs_t *const i2c, uint8_t slave, uint16_t addr, uint8_t *dat, uint32_t len)
{
    uint8_t *buf = (uint8_t *)&addr;
    int i;
    for (i = 0; i < 2; i++)
    {
        I2cWrBuf[i] = buf[1 - i];
    }
    // 1、START一个写通信（发送读取地址），不STOP
    I2cMstWrite(i2c, slave, I2cWrBuf, 2, I2C_BUS_START, I2C_BUS_NO_STOP);
    while ((i2c->ISR & (1 << 6)) == 0) // 等待传输完成TC
    {
        ;
    }
    // 2、RESTART一个读通信，结束后STOP
    I2cMstRead(i2c, slave, dat, len, I2C_BUS_RESTART, I2C_BUS_STOP);
}
