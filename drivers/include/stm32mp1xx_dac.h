
#include <stdint.h>

typedef struct {
    uint32_t CR;
    uint32_t SWTRGR;
    uint32_t DHR12R1;
    uint32_t DHR12L1;
    uint32_t DHR8R1;
    uint32_t DHR12R2;
    uint32_t DHR12L2;
    uint32_t DHR8R2;
    uint32_t DHR12RD;
    uint32_t DHR12LD;
    uint32_t DHR8RD;
    uint32_t DOR1;
    uint32_t DOR2;
    uint32_t SR;
    uint32_t CCR;
    uint32_t MCR;
    uint32_t SHSR1;
    uint32_t SHSR2;
    uint32_t SHHR;
    uint32_t SHRR;
    uint8_t  RSVD0[0x3F0 - 0x4C - 4];
    uint32_t HWCFGR0;
    uint32_t VERR;
    uint32_t IPIDR;
    uint32_t SIDR;
} DacRegs_t;


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


typedef enum {
    DAC_WAVE_NO,
    DAC_WAVE_NOISE,
    DAC_WAVE_TRIANGLE,
} DacWave_t;


typedef struct {
    uint32_t dac_ch2_cen        : 1;
    uint32_t dac_ch2_dmaudrie   : 1;
    uint32_t dac_ch2_dma_en     : 1;
    uint32_t dac_ch2_mamp       : 4;
    uint32_t dac_ch2_wave       : 2;
    uint32_t dac_ch2_tsel       : 4;
    uint32_t dac_ch2_tr_en      : 1;
    uint32_t dac_ch2_en         : 1;
    uint32_t dac_ch1_cen        : 1;
    uint32_t dac_ch1_dmaudrie   : 1;
    uint32_t dac_ch1_dma_en     : 1;
    uint32_t dac_ch1_mamp       : 4;
    uint32_t dac_ch1_wave       : 2;
    uint32_t dac_ch1_tsel       : 4;
    uint32_t dac_ch1_tr_en      : 1;
    uint32_t dac_ch1_en         : 1;
    uint32_t dac_pclk_hi_80     : 1;
    uint32_t dac_ch2_mode       : 3;
    uint32_t dac_ch1_mode       : 3;
} DacCfg_t;


extern volatile DacRegs_t *const DAC1;