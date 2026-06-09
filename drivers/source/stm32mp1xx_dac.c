
#include "stm32mp1xx_dac.h"


volatile DacRegs_t *const DAC1 = (void *)0x40017000;


void DacCfg(volatile DacRegs_t *const dac, const DacCfg_t *const cfg)
{
    dac->CR |= cfg->dac_pclk_hi_80 << 15; // 高速时钟

    dac->MCR = 0;
    dac->MCR |=cfg->dac_ch2_mode << 16;
    dac->MCR |=cfg->dac_ch1_mode;

    // 通道2配置
    if (cfg->dac_ch2_en == 1)
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
    if (cfg->dac_ch1_en == 1)
    {
        dac->CR &= ~0x00007FFF;
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


void DacDualSoftTrig(volatile DacRegs_t *const dac, const uint16_t ch1_val, const uint16_t ch2_val)
{
    dac->DHR12RD = (ch2_val << 16) | ch1_val;
    dac->SWTRGR |= 3;      /* 同时触发 ch1 + ch2 */
}


void DacWriteDualDhr(volatile DacRegs_t *const dac, const uint16_t ch1_val, const uint16_t ch2_val)
{
    dac->DHR12RD = (ch2_val << 16) | ch1_val;
}


void DacWriteDhr(volatile DacRegs_t *const dac, const uint8_t ch, const uint16_t digital)
{
    if (ch == 1)
    {
        dac->DHR12R1 = digital;
        dac->DHR12RD = (dac->DHR12RD & ~0x0000FFFF) | digital;
    }
    else
    {
        dac->DHR12R2 = digital;
        dac->DHR12RD = (dac->DHR12RD & ~0xFFFF0000) | (digital << 16);
    }
}


void DacCalibrate(volatile DacRegs_t *const dac, const uint8_t ch)
{
    uint32_t otrim;
    uint32_t save_mode;
    uint32_t save_en;
    volatile uint32_t wait;

    if (ch == 1)
    {
        save_en   = dac->CR & (1 << 0);          // 保存 EN1
        save_mode = dac->MCR & 0x7;              // 保存 MODE1
        dac->MCR = (dac->MCR & ~0x7) | DAC_CH_NORMAL_EXT_PIN_BUFFER;
        dac->CR &= ~(1 << 0);                    // EN1 = 0
        for (wait = 0; wait < 10; wait++);
        dac->CR |= 1 << 14;                      // CEN1 = 1
        for (otrim = 0; otrim < 32; otrim++)
        {
            dac->CCR = (dac->CCR & ~0x1F) | otrim;
            for (wait = 0; wait < 100; wait++);
            if (dac->SR & (1 << 14))
                break;
        }
        dac->CR &= ~(1 << 14);                   // CEN1 = 0
        dac->CR |= save_en;                      // 恢复 EN1
        dac->MCR = (dac->MCR & ~0x7) | save_mode;
    }
    else
    {
        save_en   = dac->CR & (1 << 16);         // 保存 EN2
        save_mode = (dac->MCR >> 16) & 0x7;
        dac->MCR = (dac->MCR & ~(0x7 << 16)) | (DAC_CH_NORMAL_EXT_PIN_BUFFER << 16);
        dac->CR &= ~(1 << 16);                   // EN2 = 0
        for (wait = 0; wait < 10; wait++);
        dac->CR |= 1 << 30;                      // CEN2 = 1
        for (otrim = 0; otrim < 32; otrim++)
        {
            dac->CCR = (dac->CCR & ~(0x1F << 16)) | (otrim << 16);
            for (wait = 0; wait < 100; wait++);
            if (dac->SR & (1 << 30))
                break;
        }
        dac->CR &= ~(1 << 30);                   // CEN2 = 0
        dac->CR |= save_en;                      // 恢复 EN2
        dac->MCR = (dac->MCR & ~(0x7 << 16)) | (save_mode << 16);
    }
}


void DacSampleHoldCfg(volatile DacRegs_t *const dac, const uint8_t ch,
                      uint16_t sample_time, uint16_t hold_time, uint8_t refresh_time)
{
    if (ch == 1)
    {
        dac->SHSR1 = sample_time & 0x3FF;
        dac->SHHR = (dac->SHHR & ~0x3FF) | (hold_time & 0x3FF);
        dac->SHRR = (dac->SHRR & ~0xFF) | (refresh_time & 0xFF);
    }
    else
    {
        dac->SHSR2 = sample_time & 0x3FF;
        dac->SHHR = (dac->SHHR & ~(0x3FF << 16)) | ((hold_time & 0x3FF) << 16);
        dac->SHRR = (dac->SHRR & ~(0xFF << 16)) | ((refresh_time & 0xFF) << 16);
    }
}

