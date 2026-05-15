/*
 * stm32mp1xx_dma.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */


#include "stm32mp1xx_dma.h"


volatile DmaRegs_t *const DMA2 = (void *)0x48001000;
volatile DmaRegs_t *const DMA1 = (void *)0x48000000;


void DmaCfg(volatile DmaRegs_t *const dma, const DmaCfg_t *const cfg)
{
    uint8_t stream;
    stream = cfg->dma_stream_num;

    dma->STREAM[stream].CR &= ~1; // 关闭stream

    // 清除stream所有中断标志
    if (stream < 4)
    {
        dma->LIFCR |= 0x3D << ((stream % 4) * 6);
    }
    else
    {
        dma->HIFCR |= 0x3D << ((stream % 4) * 6);
    }

    // 源/目标地址
    dma->STREAM[stream].PAR = cfg->dma_per_addr;
    dma->STREAM[stream].M0AR = cfg->dma_mem0_addr;

    // 若使用双缓冲区则配置双缓冲模式及M1地址
    if (cfg->dma_double_buf == 1)
    {
        dma->STREAM[stream].CR |= 1 << 18;
        dma->STREAM[stream].M1AR = cfg->dma_mem1_addr;
    }

    // 传输的总字节数
    dma->STREAM[stream].NDTR = cfg->dma_ndtr;

    // TODO 通道路由

    // Flow控制
    if (cfg->dma_flow_ctrl == DMA_FLOW_CTRL_PER)
    {
        dma->STREAM[stream].CR |= 1 << 5; // 外设控制
    }
    else
    {
        dma->STREAM[stream].CR &= ~(1 << 5); // DMA控制
    }

    // stream优先级
    dma->STREAM[stream].CR &= ~(3 << 16);
    dma->STREAM[stream].CR |= cfg->dma_stream_pri << 16;

    // FIFO
    if (cfg->dma_dm_dis == 1)
    {
        dma->STREAM[stream].FCR |= 1 << 2; // FIFO使能
    }
    else
    {
        dma->STREAM[stream].FCR &= ~(1 << 2); // 直接模式
    }

    // FIFO阈值
    dma->STREAM[stream].FCR &= ~3;
    dma->STREAM[stream].FCR |= cfg->dma_fifo_thre;
    
    // 传输方向
    dma->STREAM[stream].CR &= ~3;
    dma->STREAM[stream].CR |= cfg->dma_dir;

    // 地址增长方式
    dma->STREAM[stream].CR &= ~(3 << 9);
    dma->STREAM[stream].CR |= cfg->dma_peraddr_incr << 10;
    dma->STREAM[stream].CR |= cfg->dma_memaddr_incr << 9;

    // 外设地址增长偏移
    if (cfg->dma_pincos_4 == 1)
    {
        dma->STREAM[stream].CR |= 1 << 15; // 固定为4字节
    }
    else
    {
        dma->STREAM[stream].CR &= ~(1 << 15); // PSIZE
    }

    // BURST设置
    dma->STREAM[stream].CR &= ~(0xF << 21);
    dma->STREAM[stream].CR |= cfg->dma_per_burst << 21;
    dma->STREAM[stream].CR |= cfg->dma_mem_burst << 23;

    // PSIZE MSIZE
    dma->STREAM[stream].CR &= ~(0xF << 11);
    dma->STREAM[stream].CR |= cfg->dma_psize;
    dma->STREAM[stream].CR |= cfg->dma_msize;

    // 环形缓冲
    if (cfg->dma_circual_buf == 1)
    {
        dma->STREAM[stream].CR |= 1 << 8; // 开启
    }
    else
    {
        dma->STREAM[stream].CR &= ~(1 << 8); // 关闭
    }

    // FIFO使能
    dma->STREAM[stream].CR &= ~(0xF << 1);
    dma->STREAM[stream].FCR &= ~(1 << 7);
    dma->STREAM[stream].CR |= cfg->dma_tcie << 4;
    dma->STREAM[stream].CR |= cfg->dma_htie << 3;
    dma->STREAM[stream].CR |= cfg->dma_teie << 2;
    dma->STREAM[stream].CR |= cfg->dma_dmeie << 1;
    dma->STREAM[stream].FCR |= cfg->dma_feie << 7;

    dma->STREAM[stream].CR |= 1; // 开启stream
}

