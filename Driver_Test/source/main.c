/*
 * main.c
 *
 *  Created on: 2025-4-24
 *      Author: gjsbr
 */

#include "stm32mp1xx_iwdg.h"
#include "test_menu.h"
#include "stm32mp1xx_usart.h"
#include <string.h>

extern void UsartInit(void);
extern void I2cInit(void);
extern void IwdgInit(void);

static void FormatVal(char *buf, int32_t val)
{
    if (val < 0)
    {
        *buf++ = '-';
        val = -val;
    }
    if (val == 0)
    {
        *buf++ = '0';
        *buf = '\0';
        return;
    }
    char *p = buf;
    while (val > 0)
    {
        *p++ = '0' + (val % 10);
        val /= 10;
    }
    *p = '\0';
    // reverse
    while (p > buf)
    {
        p--;
        char t = *buf;
        *buf++ = *p;
        *p = t;
    }
}


int main(void)
{
    UsartInit();
    I2cInit();
    IwdgInit();

    while (1)
    {
        IwdgKickDog(IWDG2);
        TestMenu_Run();
    }

    return 0;
}
