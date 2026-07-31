#include <string.h>
#include "menu_test.h"
#include "common_test.h"

extern void GpioTest(void);
extern void SpiTest(void);
extern void I2cTest(void);
extern void DmaTest(void);
extern void StgenTest(void);
extern void IwdgTest(void);
extern void DtsTest(void);
extern void BtimerTest(void);
extern void IrqTest(void);
extern void AdcTest(void);
extern void DacTest(void);
extern void MdmaTest(void);
extern void SdTest(void);
extern void RngTest(void);
extern void CrcTest(void);
extern void EmmcTest(void);
extern void SdioTest(void);
extern void FdcanTest(void);
extern void SaiTest(void);
extern void RccTest(void);
// extern void DisplayTest(void);
extern void EtzpcTest(void);

static const MenuEntry_t g_menu[] = {
    { "1", "GPIO",   GpioTest },
    { "2", "SPI",    SpiTest  },
    { "3", "I2C",    I2cTest  },
    { "4", "DMA",    DmaTest  },
    { "5", "STGEN",  StgenTest },
    { "6", "IWDG",   IwdgTest },
    { "7", "DTS",    DtsTest },
    { "8", "BTIMER", BtimerTest },
    { "9", "IRQ",    IrqTest },
    { "10", "ADC",    AdcTest },
    { "11", "DAC",    DacTest },
    { "12", "MDMA",   MdmaTest },
    { "13", "SD",     SdTest },
    { "14", "eMMC",   EmmcTest },
    { "15", "SDIO",   SdioTest },
    { "16", "RNG",    RngTest },
    { "17", "CRC",    CrcTest },
    { "18", "FDCAN",  FdcanTest },
    { "19", "SAI",    SaiTest },
    { "20", "RCC",    RccTest },
    // { "21", "DISPLAY", DisplayTest },
    { "21", "ETZPC",   EtzpcTest },
};

#define MENU_CNT  (sizeof(g_menu) / sizeof(g_menu[0]))

static void PrintMenu(void)
{
    PRINT("\r\n===== Driver Test Menu =====\r\n");
    int i;
    for (i = 0; i < MENU_CNT; i++)
    {
        PRINT(g_menu[i].key);
        PRINT(". ");
        PRINT(g_menu[i].desc);
        PRINT("\r\n");
    }
    PRINT("0. Exit\r\n");
    PRINT("============================\r\n");
    PRINT("Select: ");
}

void TestMenu_Run(void)
{
    char buf[16];

    while (1)
    {
    	IwdgKickDog(IWDG2);
        PrintMenu();

        ReadLine(buf, sizeof(buf));

        if (strcmp(buf, "0") == 0)
        {
            PRINT("Exit.\r\n");
            break;
        }

        int i;
        for (i = 0; i < MENU_CNT; i++)
        {
            if (strcmp(buf, g_menu[i].key) == 0)
            {
                PRINT("\r\n");
                PRINT(g_menu[i].desc);
                PRINT(" test start...\r\n");

                if (g_menu[i].func)
                    g_menu[i].func();

                PRINT(g_menu[i].desc);
                PRINT(" test done.\r\n");
                break;
            }
        }

        if (i == MENU_CNT)
            PRINT("Invalid selection.\r\n");
    }
}
