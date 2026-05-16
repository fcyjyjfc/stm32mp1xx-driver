#include "stm32mp1xx_spi.h"


volatile SpiRegs_t *const SPI6 = (void *)0x5C001000;
volatile SpiRegs_t *const SPI5 = (void *)0x44009000;
volatile SpiRegs_t *const SPI4 = (void *)0x44005000;
volatile SpiRegs_t *const SPI1 = (void *)0x44004000;
volatile SpiRegs_t *const SPI3 = (void *)0x4000C000;
volatile SpiRegs_t *const SPI2 = (void *)0x4000B000;


void SpiCfg(volatile SpiRegs_t *const spi_reg, const SpiCfg_t *const cfg)
{
    spi_reg->CR1 &= ~1; // 关闭SPI
//    spi_reg->IFCR |= 1 << 9;

    // SPI波特率
    spi_reg->CFG1 &= ~(7 << 28);
    spi_reg->CFG1 |= cfg->spi_baud_reate_div << 28;

    // bit25时钟极性：0 空闲为低  bit24相位：0相位延迟半个周期
    if (cfg->spi_clk_cfg == SPI_CLK_IDLE0_NO_DELAY)
    {
        spi_reg->CFG2 &= ~(1 << 25);
        spi_reg->CFG2 |= 1 << 24;
    }
    else if (cfg->spi_clk_cfg == SPI_CLK_IDLE0_DELAY)
    {
        spi_reg->CFG2 &= ~(1 << 25);
        spi_reg->CFG2 &= ~(1 << 24);
    }
    else if (cfg->spi_clk_cfg == SPI_CLK_IDLE1_NO_DELAY)
    {
        spi_reg->CFG2 |= 1 << 25;
        spi_reg->CFG2 |= 1 << 24;
    }
    else
    {
        spi_reg->CFG2 |= 1 << 25;
        spi_reg->CFG2 &= ~(1 << 24);
    }

    // 移位方式
    if (cfg->spi_shift == SPI_SHIFT_MSB_FIRST)
    {
        spi_reg->CFG2 &= ~(1 << 23); // MSB先
    }
    else
    {
        spi_reg->CFG2 |= 1 << 23; // LSB先
    }

    // 通信模式
    spi_reg->CFG2 &= ~(3 << 17);
    spi_reg->CFG2 |= cfg->spi_comm_mode;

    // SPI协议
    spi_reg->CFG2 &= ~(7 << 19);
    spi_reg->CFG2 |= cfg->spi_protocol;
    
    // 主从模式选择
    if (cfg->spi_master == SPI_MASTER)
    {
        spi_reg->CFG2 |= 1 << 22;
    }
    else
    {
        spi_reg->CFG2 &= ~(1 << 22);
    }
//    spi_reg->IFCR |= 1 << 9;
    spi_reg->CFG1 &= ~(1 << 22); // 关闭硬件CRC

    spi_reg->CFG1 &= ~0x1F;
    spi_reg->CFG1 |= cfg->spi_word_len; // 字长

    // 硬件SS，低有效
    spi_reg->CFG2 |= 1 << 29;
    spi_reg->CFG2 &= ~(1 << 26);
    spi_reg->CFG2 &= ~(1 << 28);
    spi_reg->CFG2 &= ~(1 << 30); // 多字节间SS有效

    spi_reg->CFG2 &= ~0xF;
    spi_reg->CFG2 |= 8; // 在SS有效至发送数据直接按插入8个spi clock
    spi_reg->CFG2 |= 10 << 4; // 两个数据之间插入10个spi clock



//    spi_reg->CR1 |= 1 << 10; // 传输挂起
//    spi_reg->CR1 |= 1 << 8; // 主机自动挂起（FIFO满时挂起主机避免溢出丢失数据）

    // TSER
//    spi_reg->CR2 &= ~(0xFF << 16);
//    spi_reg->CR2 |= 100 << 16;

//    spi_reg->CFG1 |= 16; // FTHLV
    spi_reg->CR1 |= 1; // 使能SPI
}


void SpiTxRx(volatile SpiRegs_t *const spi_reg, const uint8_t *wr_buf, uint8_t *rd_buf, const uint32_t len)
{
    spi_reg->CR1 &= ~1; // 使能SPI

    // TSIZE设置为待发送的数据个数
    spi_reg->CR2 &= ~0xFFFF;
    spi_reg->CR2 |= len;

    // FTHLV 4 一个packet包含4个数据帧
    spi_reg->CFG1 &= ~(0xF << 5);
//    spi_reg->CFG1 |= 3 << 5; // FTHLV

    // DSIZE 8 一个数据帧为8bit
    spi_reg->CFG1 &= ~0xF;
    spi_reg->CFG1 |= 7;

    spi_reg->CR1 |= 1; // 使能SPI
    spi_reg->CR1 |= 1 << 9; // 启动传输

    uint32_t write_val, read_val;
    uint32_t packet_len;
    uint32_t wr_index, rd_index;

    packet_len = len - (len % 4);
    for (wr_index = 0, rd_index = 0; wr_index < len/* || rd_index < len*/; )
    {
        if (wr_index < len)
        {
            if ((spi_reg->SR & (1 << 1)) != 0)
            {
                if (wr_buf != (void *)0)
                {
//                    write_val = (wr_buf[wr_index]) |
//                                (wr_buf[wr_index + 1] << 8)  |
//                                (wr_buf[wr_index + 2] << 16) |
//                                (wr_buf[wr_index + 3] << 24);
                    write_val = wr_buf[wr_index];
                }
                else
                {
                    write_val = 0;
                }

                spi_reg->TXDR = write_val;
                wr_index += 1;
            }
        }

//        if (rd_index < packet_len)
//        {
//            if ((spi_reg->SR & (1 << 0)) != 0)
//            {
//                read_val = spi_reg->RXDR;
//                if (rd_buf != (void *)0)
//                {
//                    rd_buf[rd_index]       = read_val & 0xFF;
//                    rd_buf[rd_index + 1]   = (read_val >> 8) & 0xFF;
//                    rd_buf[rd_index + 2]   = (read_val >> 16) & 0xFF;
//                    rd_buf[rd_index + 3]   = (read_val >> 24) & 0xFF;
//                }
//                rd_index += 4;
//            }
//
//            if (((spi_reg->SR & (1 << 15)) == 0) && ((spi_reg->SR & (3 << 13)) == (len % 4)))
//            {
//                read_val = spi_reg->RXDR;
//            }
//        }
//        else
//        {
//
//        }
    }

    spi_reg->IFCR = 0xff8;
    spi_reg->CR1 &= ~1;
}


