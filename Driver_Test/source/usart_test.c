#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_usart.h"

static UsartCfg_t usart4_cfg;

void UsartInit(void)
{
    GpioMode(GPIO_G, 11, GPIO_MODER_AF);
    GpioAf(GPIO_G, 11, 6);

    GpioMode(GPIO_B, 2, GPIO_MODER_AF);
    GpioAf(GPIO_B, 2, 8);

    usart4_cfg.usart_baud_rate = 115200;
    usart4_cfg.usart_word_len = 8;
    usart4_cfg.usart_stop_bit = 1;
    usart4_cfg.usart_parity = USART_PARITY_NONE;
    usart4_cfg.usart_fifo_en = 0;
    usart4_cfg.usart_sample_mode = 16;
    usart4_cfg.usart_one_sample = 0;

    UsartCfg(USART4, &usart4_cfg);
}
