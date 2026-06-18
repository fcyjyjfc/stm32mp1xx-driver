#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_btimer.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_rcc.h"
#include "test_common.h"

static volatile uint32_t tim6_irq_cnt = 0;          /* TIM6 中断计数 */


static void tim6_isr(void)
{
    TIM6->SR &= ~1;                                 /* 清 UIF */
    tim6_irq_cnt++;                                 /* 计数 +1 */
    GpioToggle(GPIO_Z, 6);                          /* 翻转 LED */
}


void Tim6IrqInit(void)
{
    RCC->MP_APB1ENSETR |= 1 << 4;                   /* TIM6 时钟使能 */

    BasicTimerCfg_t cfg;
    cfg.tim_psc = 15999;                            /* 分频 16000 */
    cfg.tim_arr = 4999;                             /* 计数 5000 → ~0.8Hz */
    cfg.tim_arpe = 1;
    cfg.tim_opm = 0;
    cfg.tim_urs = 0;
    cfg.tim_udis = 0;
    cfg.tim_ude = 0;
    cfg.tim_uie = 1;                                /* 开 UIF 中断 */

    BasicTimerCfg(TIM6, &cfg);

    /* GIC 初始化 */
    GicdInit();                                     /* CTLR ENABLEGRP0+1 */
    GicdSetGroup(GIC_TIM6);                         /* IGROUPR → Group 1 */
    GicdSetPriority(GIC_TIM6, 1);                   /* 高优先级 */
    GicdSetTarget(GIC_TIM6, 1);                     /* CPU0 */
    GicdSetTrigMode(GIC_TIM6, 0);                   /* 电平触发 */

    GicRegisterIrq(GIC_TIM6, tim6_isr);             /* 注册 ISR */

    GicdEnableInt(GIC_TIM6);                        /* ISENABLER 使能 */

    GiccInit(10, 2);                                /* GICC PMR/BPR/使能 */

    __asm__ volatile(                               /* CPSR.I 清 0, 开 IRQ */
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :
        :
        : "r0"
    );

    BasicTimerStart(TIM6);                          /* 启动 TIM6 */

    GpioMode(GPIO_Z, 6, GPIO_MODER_OUTPUT);         /* GPIOZ6 推挽输出 */
    GpioOtype(GPIO_Z, 6, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(GPIO_Z, 6);
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
            out[pos] = '\0';
            PRINT(out);

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
			out[pos] = '\0';
			PRINT(out);

			// 翻转 LED
			GpioToggle(GPIO_Z, 7);
		}

        // 检查串口是否有输入
        if (FramePoll(0))
        {
            PRINT("\r\n");
            break;
        }
    }

    BasicTimerStop(TIM6);
    PRINT("BTIMER stopped.\r\n");
}
