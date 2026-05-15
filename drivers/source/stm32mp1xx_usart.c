#include "stm32mp1xx_usart.h"


volatile UsartRegs_t *const USART1 = (void *)0x5C000000;
volatile UsartRegs_t *const USART6 = (void *)0x44003000;
volatile UsartRegs_t *const USART8 = (void *)0x40019000;
volatile UsartRegs_t *const USART7 = (void *)0x40018000;
volatile UsartRegs_t *const USART5 = (void *)0x40011000;
volatile UsartRegs_t *const USART4 = (void *)0x40010000;
volatile UsartRegs_t *const USART3 = (void *)0x4000F000;
volatile UsartRegs_t *const USART2 = (void *)0x4000E000;


void UsartCfg(volatile UsartRegs_t *const usart_reg, const UsartCfg_t *const cfg)
{
    usart_reg->CR1 &= ~1;

    // 超采样及波特率设置
    if (cfg->usart_sample_mode == 16)
    {
    	usart_reg->CR1 &= ~(1 << 15);
    }
    else
    {
    	usart_reg->CR1 |= 1 << 15;
    }
    usart_reg->BRR = 556;

    uint32_t word_len = cfg->usart_word_len;
    if (cfg->usart_parity != 0)
    {
    	word_len += 1;
    }
    // 字长设置 7~9bit
    if (word_len == 8)
    {
        usart_reg->CR1 &= ~(1 << 28);
        usart_reg->CR1 &= ~(1 << 12);
    }
    else if (word_len == 9)
    {
        usart_reg->CR1 &= ~(1 << 28);
        usart_reg->CR1 |= 1 << 12;
    }
    else
    {
        usart_reg->CR1 |= 1 << 28;
        usart_reg->CR1 &= ~(1 << 12);
    }

    // 校验设置
    if (cfg->usart_parity == 1)
    {
    	// 奇校验
        usart_reg->CR1 |= 1 << 10;
        usart_reg->CR1 |= 1 << 9;
    }
    else if (cfg->usart_parity == 2)
    {
    	// 偶校验
        usart_reg->CR1 |= 1 << 10;
        usart_reg->CR1 &= ~(1 << 9);
    }
    else
    {
    	// 无校验
        usart_reg->CR1 &= ~(1 << 10);
    }

    // 停止位
    usart_reg->CR2 &= ~(3 << 12);
    if (cfg->usart_stop_bit == 1)
    {
        usart_reg->CR2 &= ~(3 << 12);
    }
    else if (cfg->usart_stop_bit == 2)
    {
        usart_reg->CR2 |= 2 << 12;
    }
    else if (cfg->usart_stop_bit == 0)
    {
        usart_reg->CR2 |= 1 << 12;
    }
    else
    {
        usart_reg->CR2 |= 3 << 12;
    }

    // 采样bit设置
    if (cfg->usart_one_sample == 1)
    {
        usart_reg->CR3 |= 1 << 11;
    }
    else
    {
        usart_reg->CR3 &= ~(1 << 11);
    }

    // 硬件流控制
//    usart_reg->CR3 |= 1 << 10;
//    usart_reg->CR3 |= 1 << 9; // CTS
//    usart_reg->CR3 |= 1 << 8; // RTS

    // FIFO及中断设置
    if (cfg->usart_fifo_en == 1)
    {
        usart_reg->CR1 |= 1 << 29; // 使能FIFO

        usart_reg->CR3 |= cfg->usart_txff_tl << 29; // 发送FIFO阈值
        usart_reg->CR3 |= cfg->usart_txff_tl << 25; // 接收FIFO阈值

        usart_reg->CR1 |= 1 << 31; // rxff接收FF满中断使能
        usart_reg->CR1 |= 1 << 30; // txfe发送FF空中断使能
        usart_reg->CR1 |= 1 << 8;  // PE中断
        usart_reg->CR1 |= 1 << 7;  // 发送FF未满中断
        usart_reg->CR1 |= 1 << 6;  // TC发送完成中断
        usart_reg->CR1 |= 1 << 5;  // 接收FF非空中断
        usart_reg->CR1 |= 1 << 4;  // IDLE空闲中断
        usart_reg->CR3 |= 1 << 28; // 接收FIFO阈值中断
        usart_reg->CR3 |= 1 << 23; // 发送FIFO阈值中断
        usart_reg->CR3 |= 1 << 22; // 从低功耗唤醒中断
    }
    else
    {
        usart_reg->CR1 &= ~(1 << 29); // 禁止FIFO

        usart_reg->CR1 |= 1 << 8;  // PE中断
        usart_reg->CR1 |= 1 << 7;  // 发送寄存器空中断
        usart_reg->CR1 |= 1 << 6;  // TC发送完成中断
        usart_reg->CR1 |= 1 << 5;  // 接收寄存器非空中断
        usart_reg->CR1 |= 1 << 4;  // IDLE空闲中断
    }
    usart_reg->CR3 |= 1; // 错误中断使能

    usart_reg->CR1 |= 1 << 3; // 发送使能
    usart_reg->CR1 |= 1 << 2; // 接收使能
    usart_reg->CR1 |= 1; // 使能USART
}


// 阻塞CPU直至最后一个数写入发送寄存器
void UsartWrite(volatile UsartRegs_t *const usart_reg, const uint8_t *dat, const uint32_t len)
{
    int i;
    for (i = 0; i < len; i++)
    {
    	// 等待TXE发送寄存器为空（无FIFO）或TXFNF发送FIFO不满（FIFO模式）
        while ((usart_reg->ISR & (1 << 7)) == 0)
        {

        }
        usart_reg->TDR = dat[i];
    }
}


void UsartRead(volatile UsartRegs_t *const usart_reg, uint8_t *dat, const uint32_t len)
{
    int i;
    for (i = 0; i < len; i++)
    {
    	// 等待RXNE接收寄存器不为空（无FIFO）或RXFNE接收FIFO不为空（FIFO模式）
        while ((usart_reg->ISR & (1 << 5)) == 0)
        {

        }
        *dat++ = usart_reg->RDR;
    }
}


uint32_t UsartReadOne(volatile UsartRegs_t *const usart_reg, uint8_t *dat)
{

    if ((usart_reg->ISR & (1 << 5)) != 0)
    {
    	*dat = usart_reg->RDR;
    	return 1;
    }

    return 0;
}


uint8_t UsartReadAll(volatile UsartRegs_t *const usart_reg)
{
	uint8_t dat;
    while ((usart_reg->ISR & (1 << 5)) != 0)
    {
    	dat = usart_reg->RDR;
    }

    return dat;
}


