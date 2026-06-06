/*
 * stm32mp1xx_adc.h
 *
 *  Created on: 2026-6-4
 *      Author: gjsbr
 *
 *  ADC1/2 寄存器结构体定义
 *  RM0436 section 29
 */

#ifndef STM32MP1XX_ADC_H_
#define STM32MP1XX_ADC_H_

#include <stdint.h>


// ADC 寄存器结构体（每个 ADC 独立，offset 0x000 ~ 0x0FF）
typedef struct {
    uint32_t ISR;           // 0x000 ADC interrupt and status register
    uint32_t IER;           // 0x004 ADC interrupt enable register
    uint32_t CR;            // 0x008 ADC control register
    uint32_t CFGR;          // 0x00C ADC configuration register 1
    uint32_t CFGR2;         // 0x010 ADC configuration register 2
    uint32_t SMPR1;         // 0x014 ADC sampling time register 1
    uint32_t SMPR2;         // 0x018 ADC sampling time register 2
    uint32_t PCSEL;         // 0x01C ADC channel preselection register
    uint32_t LTR1;          // 0x020 ADC watchdog lower threshold register 1
    uint32_t HTR1;          // 0x024 ADC watchdog higher threshold register 1
    uint32_t RSVD1[2];      // 0x028-0x02C
    uint32_t SQR1;          // 0x030 ADC regular sequence register 1
    uint32_t SQR2;          // 0x034 ADC regular sequence register 2
    uint32_t SQR3;          // 0x038 ADC regular sequence register 3
    uint32_t SQR4;          // 0x03C ADC regular sequence register 4
    uint32_t DR;            // 0x040 ADC regular data register
    uint32_t RSVD2[2];      // 0x044-0x048
    uint32_t JSQR;          // 0x04C ADC injected sequence register
    uint32_t RSVD3[4];      // 0x050-0x05C
    uint32_t OFR1;          // 0x060 ADC offset register 1
    uint32_t OFR2;          // 0x064 ADC offset register 2
    uint32_t OFR3;          // 0x068 ADC offset register 3
    uint32_t OFR4;          // 0x06C ADC offset register 4
    uint32_t RSVD4[4];      // 0x070-0x07C
    uint32_t JDR1;          // 0x080 ADC injected data register 1
    uint32_t JDR2;          // 0x084 ADC injected data register 2
    uint32_t JDR3;          // 0x088 ADC injected data register 3
    uint32_t JDR4;          // 0x08C ADC injected data register 4
    uint32_t RSVD5[4];      // 0x090-0x09C
    uint32_t AWD2CR;        // 0x0A0 ADC analog watchdog 2 config
    uint32_t AWD3CR;        // 0x0A4 ADC analog watchdog 3 config
    uint32_t RSVD6[2];      // 0x0A8-0x0AC
    uint32_t LTR2;          // 0x0B0 ADC watchdog lower threshold reg 2
    uint32_t HTR2;          // 0x0B4 ADC watchdog higher threshold reg 2
    uint32_t LTR3;          // 0x0B8 ADC watchdog lower threshold reg 3
    uint32_t HTR3;          // 0x0BC ADC watchdog higher threshold reg 3
    uint32_t DIFSEL;        // 0x0C0 ADC differential mode selection
    uint32_t CALFACT;       // 0x0C4 ADC calibration factor reg
    uint32_t CALFACT2;      // 0x0C8 ADC calibration factor reg 2
    uint32_t RSVD7;         // 0x0CC
    uint32_t OR;            // 0x0D0 ADC option register (ADC2_OR)
    uint32_t RSVD8[11];      // 0x0D4-0x0FF
} AdcRegsPrv_t;

// 公共寄存器（offset 0x300）
typedef struct {
    uint32_t CSR;           // 0x300 ADC common status register
    uint32_t RSVD0;         // 0x304
    uint32_t CCR;           // 0x308 ADC common config register
    uint32_t CDR;           // 0x30C ADC common regular data reg (dual mode)
    uint32_t CDR2;          // 0x310 ADC common regular data reg 2 (dual mode)
    uint8_t  RSVD1[0xF0 - 0x14];
    uint32_t HWCFGR0;       // 0x0F0 ADC hardware configuration reg 0
    uint32_t VERR;          // 0x0F4 ADC version register
    uint32_t IPDR;          // 0x0F8 ADC IP identification register
    uint32_t SIDR;          // 0x0FC ADC size identification register
} AdcCommonRegs_t;

typedef struct {
    AdcRegsPrv_t    ADC[2];
    uint8_t         RSVD0[0x100];
    AdcCommonRegs_t COMM;
} AdcRegs_t;

#define ADC_BASE       0x48003000U


/* ADC 索引 */
typedef enum {
    ADC_IDX1 = 0,
    ADC_IDX2 = 1,
} AdcIdx_t;

/* ADC 分辨率 */
typedef enum {
    ADC_RES_16BIT = 0,
    ADC_RES_14BIT = 1,
    ADC_RES_12BIT = 2,
    ADC_RES_10BIT = 3,
    ADC_RES_8BIT  = 4,
} AdcRes_t;

/* ADC 连续模式 */
typedef enum {
    ADC_SINGLE    = 0,
    ADC_CONTINUOUS = 1,
} AdcContMode_t;

/* ADC 溢出行为 */
typedef enum {
    ADC_OVR_PRESERVE  = 0,
    ADC_OVR_OVERWRITE = 1,
} AdcOvrMode_t;

/* ADC DMA 模式 */
typedef enum {
    ADC_DMA_DISABLE  = 0,
    ADC_DMA_ONESHOT  = 1,
    ADC_DMA_DFSDM    = 2,
    ADC_DMA_CIRCULAR = 3,
} AdcDmaMode_t;

/* ADC 时钟模式 */
typedef enum {
    ADC_CK_ASYNC = 0,
    ADC_CK_HCLK1 = 1,
    ADC_CK_HCLK2 = 2,
    ADC_CK_HCLK4 = 3,
} AdcCkMode_t;

/* ADC 触发极性 */
typedef enum {
    ADC_TRIG_SOFTWARE = 0,
    ADC_TRIG_RISING   = 1,
    ADC_TRIG_FALLING  = 2,
    ADC_TRIG_BOTH     = 3,
} AdcTrigEn_t;

/* ADC 采样时间（ADC 时钟周期数） */
typedef enum {
    ADC_SMP_1P5   = 0,
    ADC_SMP_2P5   = 1,
    ADC_SMP_8P5   = 2,
    ADC_SMP_16P5  = 3,
    ADC_SMP_32P5  = 4,
    ADC_SMP_64P5  = 5,
    ADC_SMP_128P5 = 6,
    ADC_SMP_810P5 = 7,  /* 快速通道(VINP0~5)=387.5, 慢速通道(VINP6~19)=810.5 */
} AdcSmp_t;


/* 全局寄存器指针（定义在 .c） */
extern volatile AdcRegs_t *const ADC;


/* ===== ISR/IER 通用位掩码 =====
 *
 * ISR（中断状态寄存器）和 IER（中断使能寄存器）位布局完全相同。
 * 这组宏两用：
 *   读/清 ISR：AdcGetFlag(adc, ADC_FLAG_EOC) / AdcClearFlag(adc, ADC_FLAG_EOC)
 *   使能中断：  adc->IER |= ADC_FLAG_EOC
 */

#define ADC_FLAG_ADRDY  (1u << 0)
#define ADC_FLAG_EOSMP  (1u << 1)
#define ADC_FLAG_EOC    (1u << 2)
#define ADC_FLAG_EOS    (1u << 3)
#define ADC_FLAG_OVR    (1u << 4)
#define ADC_FLAG_JEOC   (1u << 5)
#define ADC_FLAG_JEOS   (1u << 6)
#define ADC_FLAG_AWD1   (1u << 7)
#define ADC_FLAG_AWD2   (1u << 8)
#define ADC_FLAG_AWD3   (1u << 9)
#define ADC_FLAG_JQOVF  (1u << 10)
#define ADC_FLAG_LDORDY (1u << 12)


/* ===== CR 控制位掩码 ===== */

#define ADC_CR_ADEN         (1u << 0)
#define ADC_CR_ADDIS        (1u << 1)
#define ADC_CR_ADSTART      (1u << 2)
#define ADC_CR_JADSTART     (1u << 3)
#define ADC_CR_ADSTP        (1u << 4)
#define ADC_CR_JADSTP       (1u << 5)
#define ADC_CR_BOOST        (1u << 8)
#define ADC_CR_ADCALLIN     (1u << 16)
#define ADC_CR_LINCALRDYW1  (1u << 22)
#define ADC_CR_LINCALRDYW2  (1u << 23)
#define ADC_CR_LINCALRDYW3  (1u << 24)
#define ADC_CR_LINCALRDYW4  (1u << 25)
#define ADC_CR_LINCALRDYW5  (1u << 26)
#define ADC_CR_LINCALRDYW6  (1u << 27)
#define ADC_CR_ADVREGEN     (1u << 28)
#define ADC_CR_DEEPPWD      (1u << 29)
#define ADC_CR_ADCALDIF     (1u << 30)
#define ADC_CR_ADCAL        (1u << 31)


/* ===== 校准结果结构体 ===== */

typedef struct {
    uint32_t calfact_s;        // CALFACT_S[10:0]  单端偏移系数
    uint32_t calfact_d;        // CALFACT_D[10:0]  差分偏移系数
    uint32_t lincalfact[6];    // 线性系数 160 位，分 6 段：
                               //   [0]: W6  bits[159:150] 存于 CALFACT2[9:0]
                               //   [1]: W5  bits[149:120] 存于 CALFACT2[29:0]
                               //   [2]: W4  bits[119:90]
                               //   [3]: W3  bits[89:60]
                               //   [4]: W2  bits[59:30]
                               //   [5]: W1  bits[29:0]
} AdcCalibResult_t;


/* ===== ADC_CCR 内部通道使能位 ===== */

#define ADC_CCR_VBATEN   (1u << 24)
#define ADC_CCR_TSEN     (1u << 23)
#define ADC_CCR_VREFEN   (1u << 22)

/* ===== ADC2_OR 位 ===== */

#define ADC2_OR_VDDCOREEN (1u << 0)


/* ===== 内部通道号（用于 SQR/JSQR 序列）=====
 *
 * ADC1 内部通道映射（图 Figure 181）：
 *   通道 16: VSENSE      温度传感器（需 CCR.TSEN=1）
 *   通道 17: VREFINT     内部参考电压（需 CCR.VREFEN=1）
 *   通道 18: VBAT/4      VBAT 监测（需 CCR.VBATEN=1）
 *
 * ADC2 内部通道映射（图 Figure 182, Table 187）：
 *   通道 12: VSENSE      温度传感器（需 CCR.TSEN=1）
 *   通道 13: VREFINT     内部参考电压（需 CCR.VREFEN=1）
 *   通道 14: VDDCORE     内核电压监测（需 ADC2_OR.VDDCOREEN=1）
 *   通道 15: VBAT/4      VBAT 监测（需 CCR.VBATEN=1）
 *   通道 16: DAC_OUT1    DAC1 通道 1 输出
 *   通道 17: DAC_OUT2    DAC1 通道 2 输出
 *
 * 注：ADC1 的通道 16~18 与外部引脚复用，使能内部信号后外部引脚自动断开。
 *     ADC2 的通道 12/13/15 同样与外部引脚复用，14/16/17 为专用内部通道。
 */

#define ADC_CH_VSENSE    16      /* ADC1 only */
#define ADC_CH_VREFINT   17      /* ADC1 only */
#define ADC_CH_VBAT      18      /* ADC1 only */

#define ADC2_CH_VSENSE   12
#define ADC2_CH_VREFINT  13
#define ADC2_CH_VDDCORE  14
#define ADC2_CH_VBAT     15
#define ADC2_CH_DAC1     16
#define ADC2_CH_DAC2     17


/* ===== 函数声明 ===== */

// 内联：标志读写
static inline void AdcClearFlag(AdcIdx_t idx, uint32_t mask)
{
    ADC->ADC[idx].ISR = mask;
}

static inline uint32_t AdcGetFlag(AdcIdx_t idx, uint32_t mask)
{
    return !!(ADC->ADC[idx].ISR & mask);
}

static inline uint32_t AdcWaitFlagSet(AdcIdx_t idx, uint32_t mask, uint32_t tout)
{
    while (tout--)
    {
        if (ADC->ADC[idx].ISR & mask)
            return 1;
    }
    return 0;
}

static inline uint32_t AdcWaitFlagClr(AdcIdx_t idx, uint32_t mask, uint32_t tout)
{
    while (tout--)
    {
        if (!(ADC->ADC[idx].ISR & mask))
            return 1;
    }
    return 0;
}

static inline uint32_t AdcWaitEoc(AdcIdx_t idx, uint32_t tout)
{
    return AdcWaitFlagSet(idx, ADC_FLAG_EOC, tout);
}

static inline void AdcClearEoc(AdcIdx_t idx)
{
    AdcClearFlag(idx, ADC_FLAG_EOC);
}

static inline uint32_t AdcWaitEos(AdcIdx_t idx, uint32_t tout)
{
    return AdcWaitFlagSet(idx, ADC_FLAG_EOS, tout);
}

static inline void AdcClearEos(AdcIdx_t idx)
{
    AdcClearFlag(idx, ADC_FLAG_EOS);
}

void     AdcPowerUp(AdcIdx_t idx);
void     AdcPowerDown(AdcIdx_t idx);
uint32_t AdcCalibrate(AdcIdx_t idx, uint32_t adcaldif, uint32_t adcallin);
uint32_t AdcRestoreCalib(AdcIdx_t idx, const AdcCalibResult_t *calib);
uint32_t AdcEnable(AdcIdx_t idx);
void     AdcDisable(AdcIdx_t idx);

void     AdcStart(AdcIdx_t idx);
void     AdcStartInjected(AdcIdx_t idx);
void     AdcStop(AdcIdx_t idx);
void     AdcStopInjected(AdcIdx_t idx);

uint32_t AdcRead(AdcIdx_t idx);
uint32_t AdcReadInjected(AdcIdx_t idx, uint32_t ch);

void     AdcSetResolution(AdcIdx_t idx, AdcRes_t res);
void     AdcSetContMode(AdcIdx_t idx, AdcContMode_t cont);
void     AdcSetOvrMode(AdcIdx_t idx, AdcOvrMode_t mode);
void     AdcSetAutoDelay(AdcIdx_t idx, uint32_t enable);
void     AdcSetAutoInject(AdcIdx_t idx, uint32_t enable);
void     AdcSetDmaMode(AdcIdx_t idx, AdcDmaMode_t dmngt);
void     AdcSetDiscMode(AdcIdx_t idx, uint32_t discnum);

void     AdcSetRegularSeq(AdcIdx_t idx, uint32_t len, const uint32_t *channels);

void     AdcSetExtTrig(AdcIdx_t idx, uint32_t extsel, AdcTrigEn_t exten);
void     AdcSetInjectedSeq(AdcIdx_t idx, uint32_t len, const uint32_t *channels,
                           uint32_t jextsel, AdcTrigEn_t jexten);
void     AdcSetJqConfig(AdcIdx_t idx, uint32_t disable, uint32_t mode, uint32_t jdiscen);

void     AdcSetSampleTime(AdcIdx_t idx, uint32_t ch, AdcSmp_t smp);

void     AdcSetChanPreselect(AdcIdx_t idx, uint32_t mask);
void     AdcSetDiffMode(AdcIdx_t idx, uint32_t mask);

void     AdcSetOverSample(AdcIdx_t idx, uint32_t ratio, uint32_t shift,
                          uint32_t enable_reg, uint32_t enable_inj);
void     AdcSetOverSampleMode(AdcIdx_t idx, uint32_t rovs_mode, uint32_t trovs);
void     AdcSetLeftShift(AdcIdx_t idx, uint32_t shift);

void     AdcSetPrescaler(uint32_t presc);
void     AdcSetCkMode(AdcCkMode_t ckmode);

void     AdcSetVrefint(uint32_t enable);
void     AdcSetTempSensor(uint32_t enable);
void     AdcSetVbat(uint32_t enable);
void     Adc2SetVddcore(uint32_t enable);


#endif /* STM32MP1XX_ADC_H_ */
