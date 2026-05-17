/*
 * stm32mp1xx_dts.h
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_DTS_H_
#define STM32MP1XX_DTS_H_

#include <stdint.h>


typedef struct {
    uint32_t CFGR1;
    uint32_t RSVD0;
    uint32_t T0VALR1;
    uint32_t RSVD1;
    uint32_t RAMPVALR;
    uint32_t ITR1;
    uint32_t RSVD2;
    uint32_t DR;
    uint32_t SR;
    uint32_t ITENR;
    uint32_t ICIFR;
    uint32_t OR;
} DtsRegs_t;


typedef enum {
    DTS_REFCLK_PCLK = 0,
    DTS_REFCLK_LSE  = 1
} DtsRefclk_t;


typedef enum {
    DTS_TRIG_SOFTWARE   = 0,
    DTS_TRIG_LPTIM1_OUT = 1,
    DTS_TRIG_LPTIM2_OUT = 2,
    DTS_TRIG_LPTIM3_OUT = 3,
    DTS_TRIG_EXTI13     = 4
} DtsTrigSel_t;


typedef struct {
    uint32_t dts_calib_div : 7;
    uint32_t dts_q_meas_opt : 1;
    uint32_t dts_refclk : 1;
    uint32_t dts_smp_tim : 4;
    uint32_t dts_trig_sel : 4;
    uint16_t dts_low_thre;
    uint16_t dts_hi_thre;
} DtsCfg_t;


extern volatile DtsRegs_t *const DTS;

extern void DtsCfg(volatile DtsRegs_t *const dts_reg, const DtsCfg_t *const cfg);
extern void DtsSoftTrig(volatile DtsRegs_t *const dts_reg);
extern int32_t DtsTemperature(volatile DtsRegs_t *const dts_reg, uint32_t fpclk, uint32_t flse);


#endif /* STM32MP1XX_DTS_H_ */
