
/*
 * stm32mp1xx_dac.h
 *
 *  Created on: 2026-6-7
 *      Author: gjsbr
 *
 *  DAC 寄存器结构体定义
 *  RM0436 section 31
 */

#ifndef STM32MP1XX_DAC_H_
#define STM32MP1XX_DAC_H_

#include <stdint.h>


/**
 * DAC 寄存器映射
 * 基址: 0x40017000
 */
typedef struct {
    uint32_t CR;        // 0x00 控制寄存器
    uint32_t SWTRGR;    // 0x04 软件触发寄存器
    uint32_t DHR12R1;   // 0x08 通道1 12位右对齐数据保持
    uint32_t DHR12L1;   // 0x0C 通道1 12位左对齐数据保持
    uint32_t DHR8R1;    // 0x10 通道1 8位右对齐数据保持
    uint32_t DHR12R2;   // 0x14 通道2 12位右对齐数据保持
    uint32_t DHR12L2;   // 0x18 通道2 12位左对齐数据保持
    uint32_t DHR8R2;    // 0x1C 通道2 8位右对齐数据保持
    uint32_t DHR12RD;   // 0x20 双通道 12位右对齐数据保持
    uint32_t DHR12LD;   // 0x24 双通道 12位左对齐数据保持
    uint32_t DHR8RD;    // 0x28 双通道 8位右对齐数据保持
    uint32_t DOR1;      // 0x2C 通道1 数据输出 (只读)
    uint32_t DOR2;      // 0x30 通道2 数据输出 (只读)
    uint32_t SR;        // 0x34 状态寄存器
    uint32_t CCR;       // 0x38 校准控制寄存器
    uint32_t MCR;       // 0x3C 模式控制寄存器
    uint32_t SHSR1;     // 0x40 通道1 采样保持采样时间
    uint32_t SHSR2;     // 0x44 通道2 采样保持采样时间
    uint32_t SHHR;      // 0x48 采样保持保持时间
    uint32_t SHRR;      // 0x4C 采样保持刷新时间
    uint8_t  RSVD0[0x3F0 - 0x50];
    uint32_t HWCFGR0;   // 0x3F0 硬件配置
    uint32_t VERR;      // 0x3F4 版本寄存器
    uint32_t IPIDR;     // 0x3F8 外设ID
    uint32_t SIDR;      // 0x3FC 大小ID
} DacRegs_t;


/** DAC 通道模式: 正常/采样保持 + 缓冲/连接选择 */
typedef enum {
    DAC_CH_NORMAL_EXT_PIN_BUFFER            = 0,
    DAC_CH_NORMAL_EXT_PIN_PER_BUFFER        = 1,
    DAC_CH_NORMAL_EXT_PIN_NO_BUFFER         = 2,
    DAC_CH_NORMAL_PER_NO_BUFFER             = 3,
    DAC_CH_SAM_HOLD_EXT_PIN_BUFFER          = 4,
    DAC_CH_SAM_HOLD_EXT_PIN_PER_BUFFER      = 5,
    DAC_CH_SAM_HOLD_EXT_PIN_PER_NO_BUFFER   = 6,
    DAC_CH_SAM_HOLD_EXT_PER_NO_BUFFER       = 7
} DacChMod_t;


/** DAC 触发源选择, 对应 DAC_CR TSELx[3:0] */
typedef enum {
    DAC_CH_TRG_SW = 0,
    DAC_CH_TRG_TIM1_TRGO,
    DAC_CH_TRG_TIM2_TRGO,
    DAC_CH_TRG_TIM4_TRGO,
    DAC_CH_TRG_TIM5_TRGO,
    DAC_CH_TRG_TIM6_TRGO,
    DAC_CH_TRG_TIM7_TRGO,
    DAC_CH_TRG_TIM8_TRGO,
    DAC_CH_TRG_TIM15_TRGO,
    DAC_CH_TRG_9,
    DAC_CH_TRG_10,
    DAC_CH_TRG_LPTIM1_OUT,
    DAC_CH_TRG_LPTIM2_OUT,
    DAC_CH_TRG_EXTI9,
    DAC_CH_TRG_14,
    DAC_CH_TRG_15
} DacChTrgSrc_t;


/** LFSR 掩码/三角波幅值, 对应 MAMPx[3:0] */
typedef enum {
    UNMASK_BIT0_LFSR_AMP_1      = 0,
    UNMASK_BIT1_0_LFSR_AMP_3    = 1,
    UNMASK_BIT1_0_LFSR_AMP_7    = 2,
    UNMASK_BIT1_0_LFSR_AMP_15   = 3,
    UNMASK_BIT1_0_LFSR_AMP_31   = 4,
    UNMASK_BIT1_0_LFSR_AMP_63   = 5,
    UNMASK_BIT1_0_LFSR_AMP_127  = 6,
    UNMASK_BIT1_0_LFSR_AMP_255  = 7,
    UNMASK_BIT1_0_LFSR_AMP_511  = 8,
    UNMASK_BIT1_0_LFSR_AMP_1023 = 9,
    UNMASK_BIT1_0_LFSR_AMP_2047 = 10,
    UNMASK_BIT1_0_LFSR_AMP_4095 = 11
} DacMamp_t;


/** 波形类型 */
typedef enum {
    DAC_WAVE_NO,
    DAC_WAVE_NOISE,
    DAC_WAVE_TRIANGLE,
} DacWave_t;


/**
 * DAC 配置结构体
 * 各字段对应 DAC_CR/MCR 寄存器位
 */
typedef struct {
    uint32_t dac_ch2_cen        : 1; // CEN2 (校准使能)
    uint32_t dac_ch2_dmaudrie   : 1; // DMAUDRIE2 (DMA欠载中断使能)
    uint32_t dac_ch2_dma_en     : 1; // DMAEN2
    uint32_t dac_ch2_mamp       : 4; // MAMP2[3:0]
    uint32_t dac_ch2_wave       : 2; // WAVE2[1:0]
    uint32_t dac_ch2_tsel       : 4; // TSEL2[3:0]
    uint32_t dac_ch2_tr_en      : 1; // TEN2
    uint32_t dac_ch2_en         : 1; // EN2
    uint32_t dac_ch1_cen        : 1; // CEN1
    uint32_t dac_ch1_dmaudrie   : 1; // DMAUDRIE1
    uint32_t dac_ch1_dma_en     : 1; // DMAEN1
    uint32_t dac_ch1_mamp       : 4; // MAMP1[3:0]
    uint32_t dac_ch1_wave       : 2; // WAVE1[1:0]
    uint32_t dac_ch1_tsel       : 4; // TSEL1[3:0]
    uint32_t dac_ch1_tr_en      : 1; // TEN1
    uint32_t dac_ch1_en         : 1; // EN1
    uint32_t dac_pclk_hi_80     : 1; // HFSEL (dac_pclk > 80MHz)
    uint32_t dac_ch2_mode       : 3; // DAC_MCR MODE2[2:0]
    uint32_t dac_ch1_mode       : 3; // DAC_MCR MODE1[2:0]
} DacCfg_t;


extern volatile DacRegs_t *const DAC1;

void DacCfg(volatile DacRegs_t *const dac, const DacCfg_t *const cfg);
void DacSoftTrig(volatile DacRegs_t *const dac, const uint8_t ch, const uint16_t digital);
void DacDualSoftTrig(volatile DacRegs_t *const dac, const uint16_t ch1_val, const uint16_t ch2_val);
void DacWriteDualDhr(volatile DacRegs_t *const dac, const uint16_t ch1_val, const uint16_t ch2_val);
void DacWriteDhr(volatile DacRegs_t *const dac, const uint8_t ch, const uint16_t digital);
void DacCalibrate(volatile DacRegs_t *const dac, const uint8_t ch);
void DacSampleHoldCfg(volatile DacRegs_t *const dac, const uint8_t ch,
                      uint16_t sample_time, uint16_t hold_time, uint8_t refresh_time);

#endif /* STM32MP1XX_DAC_H_ */