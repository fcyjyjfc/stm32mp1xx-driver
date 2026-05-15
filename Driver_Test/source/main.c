/*
 * main.c
 *
 *  Created on: 2025-4-24
 *      Author: gjsbr
 */

#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_spi.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_dma.h"
#include "stm32mp1xx_i2c.h"

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef char int8_t;

//#define GPIO_TEST
//#define UART_TEST
//#define SPI_TEST
//#define I2C_TEST
#define STGEN_TEST

#ifdef I2C_TEST
#define I2C_EEPROM
//#define I2C_SENSOR
#endif

uint8_t Usart4RcvBuf[256];

void UsartInit();
void SpiInit();
void IwdgInit();

int main(void)
{
#ifdef GPIO_TEST
	*(unsigned int *)(0x50000000 + 0x210) |= 1;

	// LED
	GpioMode(GPIO_Z, 5, GPIO_MODER_OUTPUT);
	GpioOtype(GPIO_Z, 5, GPIO_OTYPE_PUSH_PULL);
	GpioOspeed(GPIO_Z, 5, GPIO_OSPEED_HI);

	// LED
	GpioMode(GPIO_Z, 6, GPIO_MODER_OUTPUT);
	GpioOtype(GPIO_Z, 6, GPIO_OTYPE_PUSH_PULL);
	GpioOspeed(GPIO_Z, 6, GPIO_OSPEED_HI);

	// LED
	GpioMode(GPIO_Z, 7, GPIO_MODER_OUTPUT);
	GpioOtype(GPIO_Z, 7, GPIO_OTYPE_PUSH_PULL);
	GpioOspeed(GPIO_Z, 7, GPIO_OSPEED_HI);

	// 输入按键
	GpioMode(GPIO_A, 0, GPIO_MODER_INPUT);
#endif

#ifdef UART_TEST
	UsartInit();
	UsartReadAll(USART4);

	char say_hello[] = "Hello Arm World!\n\r";
	UsartWrite(USART4, (void *)say_hello, 18);
#endif

#ifdef SPI_TEST
	SpiInit();
#endif

	extern void I2cInit();
	I2cInit();

	IwdgInit();

	while (1)
	{
		IwdgKickDog(IWDG2); // 喂狗

#ifdef UART_TEST
		char ch;
		// 收到串口的数据回显给串口
		if (UsartReadOne(USART4, (void *)&ch) == 1)
		{
			UsartWrite(USART4, (void *)&ch, 1);
		}
#endif

		int i;
#ifdef GPIO_TEST

		// 每按下一次GPA0，将GPZ5输出翻转
		if (GpioInData(GPIO_A, 0) == 0)
		{
			GpioToggle(GPIO_Z, 5);
		}

		for (i = 0; i < 1000000 * 2; i++)
		{
			// 每按下一次GPA0，将GPZ5输出翻转
			if (GpioInData(GPIO_A, 0) == 0)
			{
				GpioToggle(GPIO_Z, 5);
			}
		}
		GpioOutHi(GPIO_Z, 6);
		GpioOutLow(GPIO_Z, 7);

		for (i = 0; i < 1000000 * 2; i++)
		{
			// 每按下一次GPA0，将GPZ5输出翻转
			if (GpioInData(GPIO_A, 0) == 0)
			{
				GpioToggle(GPIO_Z, 5);
			}
		}
		GpioOutLow(GPIO_Z, 6);
		GpioOutHi(GPIO_Z, 7);
#endif

#ifdef SPI_TEST

//		for (i = 0; i < 1000000 * 2; i++)
//		{
//		}
//		GpioOutHi(GPIO_E, 11);
//		GpioOutHi(GPIO_E, 12);
//		GpioOutHi(GPIO_E, 13);
//		GpioOutHi(GPIO_E, 14);
//
//		for (i = 0; i < 1000000 * 2; i++)
//		{
//		}
//		GpioOutLow(GPIO_E, 11);
//		GpioOutLow(GPIO_E, 12);
//		GpioOutLow(GPIO_E, 13);
//		GpioOutLow(GPIO_E, 14);

//		uint8_t hex_char_map[16];
//		hex_char_map[0] = 0x3F;
//		hex_char_map[1] = 0x06;
//		hex_char_map[2] = 0x5B;
//		hex_char_map[3] = 0x4F;
//		hex_char_map[4] = 0x66;
//		hex_char_map[5] = 0x6D;
//		hex_char_map[6] = 0x7D;
//		hex_char_map[7] = 0x07;
//		hex_char_map[8] = 0x7F;
//		hex_char_map[9] = 0x6F;
//		hex_char_map[10] = 0x77;
//		hex_char_map[11] = 0x7C;
//		hex_char_map[12] = 0x39;
//		hex_char_map[13] = 0x5E;
//		hex_char_map[14] = 0x79;
//		hex_char_map[15] = 0x71;

		uint8_t ctrl_cmd[2];
		uint8_t spi_rd_buf[2];

		// 关闭4个数码管
		ctrl_cmd[0] = 1 | 2 | 4 | 8;
		ctrl_cmd[1] = 0;
		SpiTxRx(SPI4, ctrl_cmd, spi_rd_buf, 2);
//
//		int j, k;
//		for (j = 0; j < 4; j++)
//		{
//			ctrl_cmd[0] = 1 << j;
//			for (k = 0; k < 8; k++)
//			{
//				ctrl_cmd[1] = 1 << k;
//				SpiTxRx(SPI4, ctrl_cmd, (void *)0, 2);
//
//				for (i = 0; i < 50000; i++);
//			}
//		}
		for (i = 0; i < 1000000 * 2; i++)
		{
		}

//		uint8_t buf[256];
//		for (i = 0; i < 256; i++)
//		{
//			buf[i] = i + 1;
//		}
//		SpiTxRx(SPI4, buf, (void *)0, 4);

//		for (j = 0; j < 4; j++)
//		{
//			ctrl_cmd[0] = j + 1;
//			for (k = 0; k < 16; k++)
//			{
//				ctrl_cmd[1] = hex_char_map[k];
//				SpiTxRx(SPI4, ctrl_cmd, (void *)0, 2);
//
//				for (i = 0; i < 50000; i++);
//			}
//		}
//
#endif

#ifdef I2C_TEST
		static int16_t temp_buf[4096];
		static uint16_t index = 0;

#ifdef I2C_EEPROM
		uint8_t wr_buf[550];

		for (i = 0; i < 550; i++)
		{
			wr_buf[i] = i & 0xFF;
		}

		// 测试EEPROM读写
		I2cWriteE2(I2C1, 0XA0, 0, &wr_buf, 32);
		for (i = 0; i < 1000000 * 2; i++)
		{
		}

		I2cReadE2(I2C1, 0XA0, 0, temp_buf, 550);
		for (i = 0; i < 1000000 * 2; i++)
		{
		}

		for (i = 0; i < 32; i++)
		{
			wr_buf[i] = 0xAA;
		}
		// 向32-63写入数据0xAA
		I2cWriteEeprom(I2C1, 0XA0, 32, wr_buf, 32);
		// 延迟一段时间等待E2写数据
		for (i = 0; i < 1000000 * 2; i++)
		{
		}

		// 读取32-63处的数据
		I2cReadEeprom(I2C1, 0XA0, 32, temp_buf, 32);
		for (i = 0; i < 1000000 * 2; i++)
		{
		}
#else
		// 测试温湿度传感器读写
		int32_t temperature = 0, humidity = 0;
//		uint8_t rd_buf[2];
		uint16_t val;
		uint8_t temp_addr = 0xE3, hum_addr = 0xE5;

		I2cMstRead(I2C1, 0X80, rd_buf, 2);
		val = rd_buf[1] | (rd_buf[0] << 8);
		temperature = 17572 * val / 65536 - 4685;

		temp_buf[index++] = temperature;
		index &= 4095;

		for (i = 0; i < 1000000 * 2; i++)
		{
		}

		I2cMstWrite(I2C1, 0X80, &hum_addr, 1);
		I2cMstRead(I2C1, 0X80, rd_buf, 2);
		val = rd_buf[1] | (rd_buf[0] << 8);
		humidity = 125 * val / 65536 - 6;
#endif
#endif

#ifdef STGEN_TEST

#endif
	}

	for (;;);
	return 0;
}

UsartCfg_t usart4_cfg;

void UsartInit()
{
	GpioMode(GPIO_G, 11, GPIO_MODER_AF);
	GpioAf(GPIO_G, 11, 6);

	GpioMode(GPIO_B, 2, GPIO_MODER_AF);
	GpioAf(GPIO_B, 2, 8);

	usart4_cfg.usart_baud_rate = 115200;
	usart4_cfg.usart_word_len = 8;
	usart4_cfg.usart_stop_bit = 1;
	usart4_cfg.usart_parity = 1;
	usart4_cfg.usart_fifo_en = 0;
	usart4_cfg.usart_sample_mode = 16;
	usart4_cfg.usart_one_sample = 0;

	UsartCfg(USART4, &usart4_cfg);
}


SpiCfg_t Spi4Cfg;

void SpiInit()
{
	*(uint32_t *)(0X50000000 + 0XA28) |= 0x7f;

	GpioMode(GPIO_E, 11, GPIO_MODER_AF);
	GpioMode(GPIO_E, 12, GPIO_MODER_AF);
	GpioMode(GPIO_E, 13, GPIO_MODER_AF);
	GpioMode(GPIO_E, 14, GPIO_MODER_AF);
	GpioAf(GPIO_E, 11, 5);
	GpioAf(GPIO_E, 12, 5);
	GpioAf(GPIO_E, 13, 5);
	GpioAf(GPIO_E, 14, 5);
	GpioOtype(GPIO_E, 11, GPIO_OTYPE_PUSH_PULL);
	GpioOtype(GPIO_E, 12, GPIO_OTYPE_PUSH_PULL);
	GpioOtype(GPIO_E, 13, GPIO_OTYPE_PUSH_PULL);
	GpioOtype(GPIO_E, 14, GPIO_OTYPE_PUSH_PULL);
	GpioOspeed(GPIO_E, 11, GPIO_OSPEED_MEDIUM);
	GpioOspeed(GPIO_E, 12, GPIO_OSPEED_MEDIUM);
	GpioOspeed(GPIO_E, 13, GPIO_OSPEED_MEDIUM);
	GpioOspeed(GPIO_E, 14, GPIO_OSPEED_MEDIUM);
	GpioPullUpDown(GPIO_E, 11, GPIO_PUPDR_NO);
	GpioPullUpDown(GPIO_E, 12, GPIO_PUPDR_NO);
	GpioPullUpDown(GPIO_E, 13, GPIO_PUPDR_NO);
	GpioPullUpDown(GPIO_E, 14, GPIO_PUPDR_NO);

//	GpioMode(GPIO_E, 11, GPIO_MODER_OUTPUT);
//	GpioOtype(GPIO_E, 11, GPIO_OTYPE_PUSH_PULL);
//	GpioOspeed(GPIO_E, 11, GPIO_OSPEED_HI);
//	GpioMode(GPIO_E, 12, GPIO_MODER_OUTPUT);
//	GpioOtype(GPIO_E, 12, GPIO_OTYPE_PUSH_PULL);
//	GpioOspeed(GPIO_E, 12, GPIO_OSPEED_HI);
//	GpioMode(GPIO_E, 13, GPIO_MODER_OUTPUT);
//	GpioOtype(GPIO_E, 13, GPIO_OTYPE_PUSH_PULL);
//	GpioOspeed(GPIO_E, 13, GPIO_OSPEED_HI);
//	GpioMode(GPIO_E, 14, GPIO_MODER_OUTPUT);
//	GpioOtype(GPIO_E, 14, GPIO_OTYPE_PUSH_PULL);
//	GpioOspeed(GPIO_E, 14, GPIO_OSPEED_HI);



	Spi4Cfg.spi_baud_reate_div	= 1;
	Spi4Cfg.spi_clk_cfg	 		= SPI_CLK_IDLE0_DELAY;
	Spi4Cfg.spi_comm_mode 		= SPI_COMM_FULL_DUPLEX;
	Spi4Cfg.spi_master 			= SPI_MASTER;
	Spi4Cfg.spi_protocol 		= SPI_PROTOCOL_MOTOROLA;
	Spi4Cfg.spi_shift 			= SPI_SHIFT_MSB_FIRST;
	Spi4Cfg.spi_word_len 		= 8;

	*(uint32_t *)(0X50000000 + 0XA08) |= 1 << 9;
	SpiCfg(SPI4, &Spi4Cfg);
}


IwdgCfg_t Iwdg2Cfg;

void IwdgInit()
{
	Iwdg2Cfg.iwdg_div = IWDG_DIV_4;
	Iwdg2Cfg.iwdg_rl = 4095;
	Iwdg2Cfg.iwdg_ew_en = 0;
	Iwdg2Cfg.iwdg_win_en = 0;

	// 超时时间 4096 * 4 / 32000 = 512ms
	IwdgCfg(IWDG2, &Iwdg2Cfg);
}

uint8_t dma_src_dat[4096];
uint8_t dma_dst_dat[4096];

DmaCfg_t Uart4DmaCfg;

void DmaInit()
{
#if 0
	Uart4DmaCfg.dma_per_addr = (uint32_t)&USART4->RDR;
	Uart4DmaCfg.dma_mem0_addr = (uint32_t)Usart4RcvBuf;
	Uart4DmaCfg.dma_mem1_addr = (uint32_t)Usart4RcvBuf + 128;
	Uart4DmaCfg.dma_ndtr = 128;
	Uart4DmaCfg.dma_stream_num = 3;
	Uart4DmaCfg.dma_mem_burst = DMA_BURST_DIS;
	Uart4DmaCfg.dma_per_burst = DMA_BURST_DIS;
	Uart4DmaCfg.dma_dir = DMA_DIR_PER_2_MEM;
	Uart4DmaCfg.dma_stream_pri = DMA_STREAM_PRIOR_HI;
	Uart4DmaCfg.dma_msize = DMA_DATA_SIZE_BIT8;
	Uart4DmaCfg.dma_psize = DMA_DATA_SIZE_BIT8;
	Uart4DmaCfg.dma_memaddr_incr = DMA_ADDR_INCR_MODE_INCR;
	Uart4DmaCfg.dma_peraddr_incr = DMA_ADDR_INCR_MODE_FIXED;
	Uart4DmaCfg.dma_double_buf = 1;
	Uart4DmaCfg.dma_flow_ctrl = DMA_FLOW_CTRL_DMA;
	Uart4DmaCfg.dma_dm_dis = 0;
#else
	Uart4DmaCfg.dma_per_addr = (uint32_t)&USART4->RDR;
	Uart4DmaCfg.dma_mem0_addr = (uint32_t)Usart4RcvBuf;
	Uart4DmaCfg.dma_mem1_addr = (uint32_t)Usart4RcvBuf + 128;
	Uart4DmaCfg.dma_ndtr = 128;
	Uart4DmaCfg.dma_stream_num = 3;
	Uart4DmaCfg.dma_mem_burst = DMA_BURST_DIS;
	Uart4DmaCfg.dma_per_burst = DMA_BURST_DIS;
	Uart4DmaCfg.dma_dir = DMA_DIR_PER_2_MEM;
	Uart4DmaCfg.dma_stream_pri = DMA_STREAM_PRIOR_HI;
	Uart4DmaCfg.dma_msize = DMA_DATA_SIZE_BIT8;
	Uart4DmaCfg.dma_psize = DMA_DATA_SIZE_BIT8;
	Uart4DmaCfg.dma_memaddr_incr = DMA_ADDR_INCR_MODE_INCR;
	Uart4DmaCfg.dma_peraddr_incr = DMA_ADDR_INCR_MODE_FIXED;
	Uart4DmaCfg.dma_double_buf = 1;
	Uart4DmaCfg.dma_flow_ctrl = DMA_FLOW_CTRL_DMA;
	Uart4DmaCfg.dma_dm_dis = 0;


#endif
}


void I2cInit()
{
	*(uint32_t *)(0X50000000 + 0XA28) |= 1 << 5;
	*(uint32_t *)(0X50000000 + 0XA00) |= 1 << 21;

	GpioMode(GPIO_F, 15, GPIO_MODER_AF);
	GpioMode(GPIO_F, 14, GPIO_MODER_AF);
	GpioAf(GPIO_F, 15, 5);
	GpioAf(GPIO_F, 14, 5);
	GpioOtype(GPIO_F, 15, GPIO_OTYPE_OD);
	GpioOtype(GPIO_F, 14, GPIO_OTYPE_OD);
	GpioOspeed(GPIO_F, 15, GPIO_OSPEED_LOW);
	GpioOspeed(GPIO_F, 14, GPIO_OSPEED_LOW);

	I2cCfg(I2C1);
}




