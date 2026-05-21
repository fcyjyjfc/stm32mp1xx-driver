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

    // 通信模式 COMM[1:0] @ bit [18:17]
    spi_reg->CFG2 &= ~(3 << 17);
    spi_reg->CFG2 |= (cfg->spi_comm_mode & 3) << 17;

    // SPI协议 SP[2:0] @ bit [21:19]
    spi_reg->CFG2 &= ~(7 << 19);
    spi_reg->CFG2 |= (cfg->spi_protocol & 7) << 19;

//    spi_reg->IFCR |= 1 << 9;
    spi_reg->CFG1 &= ~(1 << 22); // 关闭硬件CRC

    spi_reg->CFG1 &= ~0x1F;
    spi_reg->CFG1 |= cfg->spi_word_len; // 字长

    // SS 输出使能，SSIOP=0（低有效），SSOM=0（帧间不脉冲）
    spi_reg->CFG2 |= 1 << 29;    // SSOE = 1
    spi_reg->CFG2 &= ~(1 << 28); // SSIOP = 0
    spi_reg->CFG2 &= ~(1 << 30); // SSOM = 0

    // SS 管理方式
    if (cfg->spi_ss_mgmt == SPI_SSM_SW)
    {
        spi_reg->CFG2 |= 1 << 26;   // SSM = 1，软件管理
        spi_reg->CR1  |= 1 << 12;    // SSI = 1，内部 NSS 无效（SSIOP=0 时≠即无效）
    }
    else
    {
        spi_reg->CFG2 &= ~(1 << 26); // SSM = 0，硬件管理
    }

    spi_reg->CFG2 &= ~0xF;
    spi_reg->CFG2 |= 8; // 在SS有效至发送数据直接按插入8个spi clock
    spi_reg->CFG2 |= 10 << 4; // 两个数据之间插入10个spi clock

    spi_reg->CFG2 &= ~(1 << 22);
    // 主从模式选择
    if (cfg->spi_master == SPI_MASTER)
    {
        spi_reg->CFG2 |= 1 << 22;
    }

    spi_reg->CR1 |= 1; // 使能SPI
}

uint8_t save_buf[300];
void SpiTxRx(volatile SpiRegs_t *const spi_reg, const uint8_t *wr_buf, uint8_t *rd_buf, const uint32_t len)
{
    uint32_t wr_index, rd_index;

    spi_reg->CR1 &= ~1; // 关闭SPI

    // TSIZE: 传输总帧数
    spi_reg->CR2 &= ~0xFFFF;
    spi_reg->CR2 |= len;

    // DSIZE = 8 (7 = 8-bit data)
    spi_reg->CFG1 &= ~0x1F;
    spi_reg->CFG1 |= 7;

    spi_reg->CR1 |= 1;      // 使能SPI
    spi_reg->CR1 |= 1 << 9; // 启动传输 (CSTART)

    wr_index = 0;
    rd_index = 0;

    while (wr_index < len || rd_index < len)
    {
        // 发送: TXP (SR bit 1) = TxFIFO有空位
        if (wr_index < len && (spi_reg->SR & (1 << 1)))
        {
            if (wr_buf != (void *)0)
                {spi_reg->TXDR = wr_buf[wr_index];save_buf[wr_index] = wr_buf[wr_index];}
            else
                spi_reg->TXDR = 0xFF; // 仅接收时发dummy字节产生时钟
            wr_index++;
        }

        // 接收: RXP (SR bit 0) = RxFIFO有数据
        if (rd_index < len && (spi_reg->SR & (1 << 0)))
        {
            uint32_t val = spi_reg->RXDR;
            if (rd_buf != (void *)0)
                rd_buf[rd_index] = val & 0xFF;
            rd_index++;
        }
    }

    // 等待传输完成 (EOT = SR bit 3)
    while ((spi_reg->SR & (1 << 3)) == 0);

    spi_reg->IFCR = 0xFF8;
    spi_reg->CR1 &= ~1;
}


