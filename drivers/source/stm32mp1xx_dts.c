/*
* stm32mp1xx_dts.c
*
*  Created on: 2025-5-14
*      Author: gjsbr
*/


#include "stm32mp1xx_dts.h"


volatile DtsRegs_t *const DTS = (void *)0x50028000;


void DtsCfg(volatile DtsRegs_t *const dts_reg, const DtsCfg_t *const cfg)
{
   // HSREF_CLK_DIV[6:0] @ bit [30:24]
   dts_reg->CFGR1 &= ~(0x7F << 24);
   dts_reg->CFGR1 |= (cfg->dts_calib_div & 0x7F) << 24;

   // Q_MEAS_OPT @ bit 21
   if (cfg->dts_q_meas_opt)
       dts_reg->CFGR1 |= 1 << 21;
   else
       dts_reg->CFGR1 &= ~(1 << 21);

   // REFCLK_SEL @ bit 20
   if (cfg->dts_refclk == DTS_REFCLK_LSE)
       dts_reg->CFGR1 |= 1 << 20;
   else
       dts_reg->CFGR1 &= ~(1 << 20);

   // TS1_SMP_TIME[3:0] @ bit [19:16]
   dts_reg->CFGR1 &= ~(0xF << 16);
   dts_reg->CFGR1 |= (cfg->dts_smp_tim & 0xF) << 16;

   // TS1_INTRIG_SEL[3:0] @ bit [11:8]
   dts_reg->CFGR1 &= ~(0xF << 8);
   dts_reg->CFGR1 |= (cfg->dts_trig_sel & 0xF) << 8;

   // threshold: ITR1 = HI_THD[31:16] | LO_THD[15:0]
   dts_reg->ITR1 = (uint32_t)cfg->dts_hi_thre << 16 | cfg->dts_low_thre;

   // TS1_EN @ bit 0
   dts_reg->CFGR1 |= 1;
}


void DtsSoftTrig(volatile DtsRegs_t *const dts_reg)
{
   // wait until sensor is ready
   while (!((dts_reg->SR >> 15) & 1));

   dts_reg->CFGR1 |= 1 << 4;  // TS1_START = 1

   // wait for measurement complete (TS1_RDY = 1 again)
   while (!((dts_reg->SR >> 15) & 1));

   // clear end-of-measure flag if enabled
   dts_reg->ICIFR |= 1;
}


int32_t DtsTemperature(volatile DtsRegs_t *const dts_reg, uint32_t fpclk, uint32_t flse)
{
   uint16_t mfreq   = dts_reg->DR & 0xFFFF;
   uint16_t fmt0    = dts_reg->T0VALR1 & 0xFFFF;
   uint16_t ramp    = dts_reg->RAMPVALR & 0xFFFF;
   uint32_t smp_tim = (dts_reg->CFGR1 >> 16) & 0xF;
   uint32_t t0;
   int32_t  temp;

   // T0 temperature: 30°C or 130°C
   if ((dts_reg->T0VALR1 & (3 << 16)) == 0)
       t0 = 30;
   else
       t0 = 130;

   if ((dts_reg->CFGR1 & (1 << 20)) == 0)
   {
       // PCLK mode
       temp = t0 + ((int32_t)(fpclk / mfreq) * (int32_t)smp_tim - 100 * (int32_t)fmt0) / (int32_t)ramp;
   }
   else
   {
       // LSE mode
       temp = t0 + ((int32_t)(flse * mfreq / smp_tim) - 100 * (int32_t)fmt0) / (int32_t)ramp;
   }

   return temp;
}
