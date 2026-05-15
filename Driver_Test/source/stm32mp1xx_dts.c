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
   dts_reg->CFGR1 &= ~(0x7F << 24);
   dts_reg->CFGR1 |= cfg->dts_calib_div;

   dts_reg->CFGR1 &= ~(1 << 21);

   if (cfg->dts_refclk == DTS_REFCLK_LSE)
   {
       dts_reg->CFGR1 |= 1 << 20;
   }
   else
   {
       dts_reg->CFGR1 &= ~(1 << 20);
   }

   dts_reg->CFGR1 &= ~(0xF << 16);
   dts_reg->CFGR1 |= cfg->dts_smp_tim << 16;

   dts_reg->CFGR1 &= ~(0xF << 8);
   dts_reg->CFGR1 |= cfg->dts_trig_sel << 8;

   uint32_t threshold;
   threshold = cfg->dts_low_thre | (uint32_t)(cfg->dts_hi_thre) << 16;
   dts_reg->ITR1 = threshold;

   dts_reg->CFGR1 |= 1;
}


void DtsSoftTrig(volatile DtsRegs_t *const dts_reg)
{
   dts_reg->CFGR1 &= ~(0xF << 8);
   dts_reg->CFGR1 |= DST_TRIG_SOFTWARE << 8;

   // TS1_RDYΪ1
   if ((dts_reg->SR & (1 << 15)) == (1 << 15))
   {
       dts_reg->CFGR1 |= 1 << 4;
   }

   while ((dts_reg->SR & (1 << 0)) == 0)
   {
       ;
   }

   dts_reg->ICIFR |= 1;
}


uint32_t DtsTemperature(volatile DtsRegs_t *const dts_reg)
{
   uint32_t mfreq = dts_reg->DR;
   uint32_t ramp_coeff = dts_reg->RAMPVALR;
   uint32_t t0;
   uint32_t fmt0 = dts_reg->T0VALR1 & 0xFFFF;
   uint32_t smp_tim = (dts_reg->CFGR1 >> 16) & 0xF;
   uint32_t fpclk, flse;
   uint32_t temp;

   if ((dts_reg->T0VALR1 & (3 << 16)) == 0)
   {
       t0 = 30;
   }
   else
   {
       t0 = 130;
   }

   if ((dts_reg->CFGR1 & (1 << 20)) == 0)
   {
//	   temp = t0 + ((fpclk / mfreq) * smp_tim - 100 * fmt0) / ramp_coeff;
   }
   else
   {
//       temp = t0 + ((flse * mfreq / smp_tim) - (100 * fmt0)) / ramp_coeff;
   }

   return temp;
}

