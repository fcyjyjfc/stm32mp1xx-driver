#include <string.h>
#include "stm32mp1xx_dts.h"
#include "test_common.h"

void DtsTest(void)
{
    char buf[16];
    uint32_t fpclk = 100000000;  // FIXME: read from RCC when clock API is ready

    PRINT("\r\n===== DTS Test =====\r\n");

    // read factory calibration values
    uint16_t fmt0  = DTS->T0VALR1 & 0xFFFF;
    uint16_t ramp  = DTS->RAMPVALR & 0xFFFF;
    uint32_t t0raw = (DTS->T0VALR1 >> 16) & 3;

    PRINT("  FMT0=");
    PrintDec(buf, fmt0);
    PRINT(buf);
    PRINT("  RAMP=");
    PrintDec(buf, ramp);
    PRINT(buf);
    PRINT("  T0=");
    PrintDec(buf, t0raw == 0 ? 30 : 130);
    PRINT(buf);
    PRINT("\r\n");

    // configure DTS
    DtsCfg_t cfg;
    cfg.dts_calib_div  = 100;    // PCLK/100 to get < 1MHz for calibration
    cfg.dts_q_meas_opt = 0;      // normal measurement with calibration
    cfg.dts_refclk     = DTS_REFCLK_PCLK;
    cfg.dts_smp_tim    = 8;      // 1 cycle
    cfg.dts_trig_sel   = DTS_TRIG_SOFTWARE;
    cfg.dts_low_thre   = 0;
    cfg.dts_hi_thre    = 0;
    DtsCfg(DTS, &cfg);

    // trigger and read
    int i, j;
    for (i = 0; i < 5; i++)
    {
        IwdgKickDog(IWDG2);

        DtsSoftTrig(DTS);
        int32_t temp = DtsTemperature(DTS, fpclk, 32768);

        PRINT("  MFREQ=");
        PrintDec(buf, DTS->DR & 0xFFFF);
        PRINT(buf);
        PRINT("  TEMP=");
        PrintDec(buf, temp);
        PRINT(buf);
        PRINT(" C\r\n");

        char ch;
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 1)
        {
            PRINT("aborted.\r\n");
            break;
        }

        for (j = 0; j < 5000000; j++) ;
    }

    // === LSE mode test ===
    PRINT("\r\n--- LSE mode ---\r\n");
    cfg.dts_refclk     = DTS_REFCLK_LSE;
    cfg.dts_q_meas_opt = 0;      // quick measurement, no calibration needed
    cfg.dts_smp_tim    = 15;      // 1 LSE cycle
    DtsCfg(DTS, &cfg);

    for (i = 0; i < 5; i++)
    {
        IwdgKickDog(IWDG2);

        DtsSoftTrig(DTS);
        int32_t temp = DtsTemperature(DTS, fpclk, 32768);

        PRINT("  MFREQ=");
        PrintDec(buf, DTS->DR & 0xFFFF);
        PRINT(buf);
        PRINT("  TEMP=");
        PrintDec(buf, temp);
        PRINT(buf);
        PRINT(" C\r\n");

        char ch;
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 1)
        {
            PRINT("aborted.\r\n");
            break;
        }

        for (j = 0; j < 5000000; j++) ;
    }

    PRINT("DTS test done.\r\n");
}
