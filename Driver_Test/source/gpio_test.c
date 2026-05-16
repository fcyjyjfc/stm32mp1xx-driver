#include <string.h>
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"

static void GpioInit(void)
{
    *(uint32_t *)(0x50000000 + 0x210) |= 1;

    // LED
    GpioMode(GPIO_Z, 5, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 5, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_Z, 5, GPIO_OSPEED_HI);

    // LED
    GpioMode(GPIO_Z, 6, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 6, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_Z, 6, GPIO_OSPEED_HI);

    // LED
    GpioMode(GPIO_Z, 7, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 7, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_Z, 7, GPIO_OSPEED_HI);

    // button
    GpioMode(GPIO_A, 0, GPIO_MODER_INPUT);
}

void GpioTest(void)
{
    const char msg[] = "GPIO: Z5=LED(toggle on A0 press), Z6/Z7=LED(blink), any key to exit\r\n";
    UsartWrite(USART4, (void *)msg, strlen(msg));

    GpioInit();

    int state = 0;
    int press = 0;

    char stat[] = "Key Pressed!\r\n";

    while (1)
    {
    	IwdgKickDog(IWDG2);
        // check button A0 -> toggle Z5
//        if (GpioInData(GPIO_A, 0) == 0)
//            GpioToggle(GPIO_Z, 5);

        // blink Z6 / Z7 alternately
        if (state)
        {
            GpioOutHi(GPIO_Z, 6);
            GpioOutLow(GPIO_Z, 7);
        }
        else
        {
            GpioOutLow(GPIO_Z, 6);
            GpioOutHi(GPIO_Z, 7);
        }
        state = !state;

        // check for keypress to exit
        char ch;
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 1)
            break;

        // delay
        int i;
        for (i = 0; i < 1000000 * 2; i++)
        {
            if (GpioInData(GPIO_A, 0) == 0)
            {
            	press = 1;
            	break;
            }
        }

        if (press == 1)
		{
			UsartWrite(USART4, (void *)stat, strlen(stat));
			GpioToggle(GPIO_Z, 5);
			press = 0;
		}

    }
}
