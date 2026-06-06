#include <string.h>
#include "test_menu.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"

typedef struct {
    const char *key;
    const char *desc;
    void (*func)(void);
} MenuEntry_t;

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
};

#define MENU_CNT  (sizeof(g_menu) / sizeof(g_menu[0]))

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

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

static int ReadLine(char *buf, int max_len)
{
    int pos = 0;
    char ch;

    while (pos < max_len - 1)
    {
    	IwdgKickDog(IWDG2);
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 0)
            continue;

        if (ch == 'S' || ch == 's')
        {
            UsartWrite(USART4, (void *)"\r\n", 2);
            break;
        }

        UsartWrite(USART4, &ch, 1);
        buf[pos++] = ch;
    }

    buf[pos] = '\0';
    return pos;
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
