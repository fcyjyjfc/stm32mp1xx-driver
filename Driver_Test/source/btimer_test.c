#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_btimer.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_rcc.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

static volatile uint32_t tim6_irq_cnt = 0;


static void tim6_isr(void)
{
    TIM6->SR &= ~1;
    tim6_irq_cnt++;
    GpioToggle(GPIO_Z, 6);
}


void Tim6IrqInit(void)
{
    RCC->MP_APB1ENSETR |= 1 << 4;

    BasicTimerCfg_t cfg;
    cfg.tim_psc = 15999;
    cfg.tim_arr = 4999;
    cfg.tim_arpe = 1;
    cfg.tim_opm = 0;
    cfg.tim_urs = 0;
    cfg.tim_udis = 0;
    cfg.tim_ude = 0;
    cfg.tim_uie = 1;

    BasicTimerCfg(TIM6, &cfg);

    GicdInit();
    GicdSetGroup(GIC_TIM6);
    GicdSetPriority(GIC_TIM6, 1);
    GicdSetTarget(GIC_TIM6, 1);
    GicdSetTrigMode(GIC_TIM6, 0);

    GicRegisterIrq(GIC_TIM6, tim6_isr);  /* 先注册，再使能 */

    GicdEnableInt(GIC_TIM6);

    GiccInit(10, 2);

    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :
        :
        : "r0"
    );

    BasicTimerStart(TIM6);

    GpioMode(GPIO_Z, 6, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 6, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(GPIO_Z, 6);
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


static void NumToStr(char *buf, uint32_t val)
{
    char rev[12];
    int i = 0;

    do
    {
        rev[i++] = '0' + val % 10;
        val /= 10;
    }
    while (val);

    while (i > 0)
    {
        *buf++ = rev[--i];
    }
    *buf = '\0';
}


void BtimerTest(void)
{
    BasicTimerCfg_t tim6_cfg, tim7_cfg;
    char out[32];
    uint32_t cnt = 0, cnt2 = 0;

    // 使能 TIM6 时钟（RCC MP_APB1ENSETR bit4）
    RCC->MP_APB1ENSETR |= 1 << 4;
    RCC->MP_APB1ENSETR |= 1 << 5;

    // 配置 GPIOZ6 为输出（LED）
    GpioMode(GPIO_Z, 6, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 6, GPIO_OTYPE_PUSH_PULL);

    GpioMode(GPIO_Z, 7, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 7, GPIO_OTYPE_PUSH_PULL);


    tim6_cfg.tim_psc = 15999;   // 分频 16000
    tim6_cfg.tim_arr = 4999;    // 计数 5000 -> ~0.8Hz @64MHz APB1
    tim6_cfg.tim_arpe = 1;
    tim6_cfg.tim_opm = 0;
    tim6_cfg.tim_urs = 0;
    tim6_cfg.tim_udis = 0;
    tim6_cfg.tim_ude = 0;
    tim6_cfg.tim_uie = 0;

    tim7_cfg.tim_psc = 15999;   // 分频 16000
    tim7_cfg.tim_arr = 9999;    // 计数 5000 -> ~0.8Hz @64MHz APB1
    tim7_cfg.tim_arpe = 1;
    tim7_cfg.tim_opm = 0;
    tim7_cfg.tim_urs = 0;
    tim7_cfg.tim_udis = 0;
    tim7_cfg.tim_ude = 0;
    tim7_cfg.tim_uie = 0;

    BasicTimerCfg(TIM6, &tim6_cfg);
    BasicTimerCfg(TIM7, &tim7_cfg);

    GpioOutLow(GPIO_Z, 6);
    GpioOutLow(GPIO_Z, 7);

    BasicTimerStart(TIM6);
    BasicTimerStart(TIM7);

    PRINT("\r\n--- BTIMER Test ---\r\n");
    PRINT("Overflow (press 'S' to stop):\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        // 检查 UIF（SR bit0）
        if (TIM6->SR & 1)
        {
            TIM6->SR = 0;   // 清 UIF
            cnt++;

            // 打印计数
            int pos = 0;
            out[pos++] = ' ';
            out[pos++] = ' ';
            PRINT("TIM6: ");
            NumToStr(out + pos, cnt);

            while (out[pos])
            {
                pos++;
            }
            out[pos++] = '\r';
            out[pos++] = '\n';
            UsartWrite(USART4, (void *)out, pos);

            // 翻转 LED
            GpioToggle(GPIO_Z, 6);
        }

        if (TIM7->SR & 1)
		{
			TIM7->SR = 0;   // 清 UIF
			cnt2++;

			// 打印计数
			int pos = 0;
			out[pos++] = ' ';
			out[pos++] = ' ';
			PRINT("TIM7: ");
			NumToStr(out + pos, cnt2);

			while (out[pos])
			{
				pos++;
			}
			out[pos++] = '\r';
			out[pos++] = '\n';
			UsartWrite(USART4, (void *)out, pos);

			// 翻转 LED
			GpioToggle(GPIO_Z, 7);
		}

        // 检查串口是否有输入
        if (UsartReadOne(USART4, &ch))
        {
            PRINT("\r\n");
            break;
        }
    }

    BasicTimerStop(TIM6);
    PRINT("BTIMER stopped.\r\n");
}
