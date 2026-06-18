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
extern void TestCommonInit(void);


int main(void)
{
    UsartInit();
    TestCommonInit();
    I2cInit();
    IwdgInit();

    while (1)
    {
        IwdgKickDog(IWDG2);
        TestMenu_Run();
    }

    return 0;
}
