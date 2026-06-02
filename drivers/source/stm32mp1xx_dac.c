
#include "stm32mp1xx_dac.h"


volatile DacRegs_t *const DAC1 = (void *)0x40017000;


void DacCfg(volatile DacRegs_t *const dac, const DacCfg_t *const cfg)
{
    dac->CR |= cfg->dac_pclk_hi_80 << 15; // 高速时钟

    dac->MCR |=cfg->dac_ch2_mode << 16;
    dac->MCR |=cfg->dac_ch1_mode << 16;

    // 通道2配置
    if (cfg->dac_ch2_dma_en == 1)
    {
        dac->CR &= ~0xFFFF0000;
        dac->CR |= cfg->dac_ch2_tr_en << 17; // 触发使能
        dac->CR |= cfg->dac_ch2_tsel << 18; // 触发源
        dac->CR |= cfg->dac_ch2_wave << 22; // 波形发生
        dac->CR |= cfg->dac_ch2_mamp << 24; // 掩码/幅值
        dac->CR |= cfg->dac_ch2_dma_en << 28; // DMA请求
        dac->CR |= cfg->dac_ch2_dmaudrie << 29; // DMA下溢中断使能
        dac->CR |= 1 << 16;
    }
    else
    {
        dac->CR &= ~(1 << 16);
    }

    // 通道1配置
    if (cfg->dac_ch1_dma_en == 1)
    {
        dac->CR &= ~0x0000FFFF;
        dac->CR |= cfg->dac_ch1_tr_en << 1; // 触发使能
        dac->CR |= cfg->dac_ch1_tsel << 2; // 触发源
        dac->CR |= cfg->dac_ch1_wave << 6; // 波形发生
        dac->CR |= cfg->dac_ch1_mamp << 8; // 掩码/幅值
        dac->CR |= cfg->dac_ch1_dma_en << 12; // DMA请求
        dac->CR |= cfg->dac_ch1_dmaudrie << 13; // DMA下溢中断使能
        dac->CR |= 1 << 0;
    }
    else
    {
        dac->CR &= ~(1 << 0);
    }
}


void DacSoftTrig(volatile DacRegs_t *const dac, const uint8_t ch, const uint16_t digital)
{
    // Vout = digital / 4096 * Vref
    if (ch == 1)
    {
        dac->DHR12R1 = digital;
        dac->DHR12RD &= ~0x0000FFFF;
        dac->DHR12RD |= digital;
    }
    else
    {
        dac->DHR12R2 = digital;
        dac->DHR12RD &= ~0xFFFF0000;
        dac->DHR12RD |= digital << 16;
    }
    dac->SWTRGR |= 1 << (ch - 1);
}

