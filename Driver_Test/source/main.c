/*
 * main.c
 *
 *  Created on: 2025-4-24
 *      Author: gjsbr
 */

#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_rcc.h"
#include "menu_test.h"
#include "stm32mp1xx_usart.h"
#include <string.h>

extern void UsartInit(void);
extern void I2cInit(void);
extern void IwdgInit(void);
extern void TestCommonInit(void);
extern void PrintResetCause(void);


int main(void)
{
    UsartInit();
    TestCommonInit();
    PrintResetCause();
    I2cInit();
    IwdgInit();

    while (1)
    {
        IwdgKickDog(IWDG2);
        TestMenu_Run();
    }

    return 0;
}
