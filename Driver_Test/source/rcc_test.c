#include "common_test.h"
#include "stm32mp1xx_rcc.h"

void PrintResetCause(void)
{
    uint32_t flags = RccResetCauseGet();
    RccResetCauseClear(flags);

    PRINT("\r\n=== Reset Cause ===\r\n");
    if (flags & RCC_RSTC_PORRSTF)    PRINT("  POR/PDR\r\n");
    if (flags & RCC_RSTC_BORRSTF)    PRINT("  BOR\r\n");
    if (flags & RCC_RSTC_PADRSTF)    PRINT("  NRST Pin\r\n");
    if (flags & RCC_RSTC_HCSSRSTF)   PRINT("  HSE CSS Fail\r\n");
    if (flags & RCC_RSTC_VCORERSTF)  PRINT("  VDDCORE\r\n");
    if (flags & RCC_RSTC_MPSYSRSTF)  PRINT("  MPU System Reset\r\n");
    if (flags & RCC_RSTC_MCSYSRSTF)  PRINT("  MCU System Reset\r\n");
    if (flags & RCC_RSTC_IWDG1RF)    PRINT("  IWDG1\r\n");
    if (flags & RCC_RSTC_IWDG2RF)    PRINT("  IWDG2\r\n");
    if (flags & RCC_RSTC_STDBYRSTF)  PRINT("  Standby Wakeup\r\n");
    if (flags & RCC_RSTC_CSTDBYRSTF) PRINT("  CStandby Wakeup\r\n");
    if (flags & RCC_RSTC_MPUP0RSTF)  PRINT("  MPU CPU0 Boot\r\n");
    if (flags & RCC_RSTC_MPUP1RSTF)  PRINT("  MPU CPU1 Boot\r\n");
    PRINT("===================\r\n");
}

void RccTest(void)
{
    RccClkTree_t t;
    RccClkDump(&t);

    PRINT("\r\n=== RCC Clock Tree Dump ===\r\n");

    PRINT("\r\n-- Oscillators --\r\n");
    PrintU32("  HSE", t.hse);
    PrintU32("  HSI", t.hsi);
    PrintU32("  CSI", t.csi);
    PrintU32("  LSE", t.lse);
    PrintU32("  LSI", t.lsi);

    PRINT("\r\n-- PLL1 --\r\n");
    PrintU32("  PLL1_P", t.pll1_p);
    PrintU32("  PLL1_Q", t.pll1_q);
    PrintU32("  PLL1_R", t.pll1_r);

    PRINT("\r\n-- PLL2 --\r\n");
    PrintU32("  PLL2_P", t.pll2_p);
    PrintU32("  PLL2_Q", t.pll2_q);
    PrintU32("  PLL2_R", t.pll2_r);

    PRINT("\r\n-- PLL3 --\r\n");
    PrintU32("  PLL3_P", t.pll3_p);
    PrintU32("  PLL3_Q", t.pll3_q);
    PrintU32("  PLL3_R", t.pll3_r);

    PRINT("\r\n-- PLL4 --\r\n");
    PrintU32("  PLL4_P", t.pll4_p);
    PrintU32("  PLL4_Q", t.pll4_q);
    PrintU32("  PLL4_R", t.pll4_r);

    PRINT("\r\n-- System Clocks --\r\n");
    PrintU32("  MPU", t.mpu);
    PrintU32("  AXI", t.axi);
    PrintU32("  MCU", t.mcu);

    PRINT("\r\n-- Bus Clocks (MCU Domain) --\r\n");
    PrintU32("  AHB2", t.ahb2);
    PrintU32("  AHB3", t.ahb3);
    PrintU32("  AHB4", t.ahb4);
    PrintU32("  APB1", t.apb1);
    PrintU32("  APB2", t.apb2);
    PrintU32("  APB3", t.apb3);

    PRINT("\r\n-- Bus Clocks (AXI Domain) --\r\n");
    PrintU32("  AHB5", t.ahb5);
    PrintU32("  AHB6", t.ahb6);
    PrintU32("  APB4", t.apb4);
    PrintU32("  APB5", t.apb5);

    PRINT("\r\n=== Dump Done ===\r\n");
}
