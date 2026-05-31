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
extern void Tim6IrqInit(void);                      /* TIM6 中断初始化 */


int main(void)
{
    UsartInit();
    I2cInit();
    IwdgInit();
    Tim6IrqInit();

    while (1)
    {
        IwdgKickDog(IWDG2);
        TestMenu_Run();
    }

    return 0;
}
