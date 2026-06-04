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

#endif /* STM32MP1XX_ADC_H_ */
