#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_btimer.h"
#include "stm32mp1xx_exti.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_iwdg.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

/*
 * 串口读一行，'S' 作为结束符
 */
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

/*
 * ========== 子菜单函数声明 ==========
 */
static void IrqTest_ExtiFalling(void);
static void IrqTest_ExtiBoth(void);             /* TODO */
static void IrqTest_Preempt(void);              /* TODO */


/*
 * ========== 共用 ISR ==========
 */

static volatile uint32_t tim6_irq_cnt = 0;

static void tim6_isr(void)
{
    TIM6->SR &= ~1;
    tim6_irq_cnt++;
    GpioToggle(GPIO_Z, 6);
}


/*
 * ========== 子菜单 ==========
 */

void IrqTest(void)
{

    while (1)
    {
        IwdgKickDog(IWDG2);
        PRINT("\r\n===== IRQ Test Menu =====\r\n");
        PRINT("1. TIM6 + EXTI0 falling edge\r\n");
        PRINT("2. EXTI0 both edge\r\n");
        PRINT("3. Preempt\r\n");
        PRINT("0. Back\r\n");
        PRINT("===========================\r\n");
        PRINT("Select: ");

        /* 读一行，'S' 结束 */
        char buf[8];
        ReadLine(buf, sizeof(buf));

        if (strcmp(buf, "1") == 0)
            IrqTest_ExtiFalling();
        else if (strcmp(buf, "2") == 0)
            IrqTest_ExtiBoth();
        else if (strcmp(buf, "3") == 0)
            IrqTest_Preempt();
        else if (strcmp(buf, "0") == 0)
        {
            PRINT("Back to main menu.\r\n");
            return;
        }
        else
            PRINT("Invalid.\r\n");
    }
}


/*
 * ========== 1. TIM6 + EXTI0 下降沿 ==========
 */

static volatile uint32_t exti0_falling_cnt = 0;

static void exti0_falling_isr(void)
{
    ExtiClearFpr(0);
    exti0_falling_cnt++;
    GpioToggle(GPIO_Z, 7);
    PRINT("PA0 Pressed!\r\n");
}

static void IrqTest_ExtiFalling(void)
{
    /* TIM6 时钟 */
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

    GicdSetGroup(GIC_TIM6);
    GicdSetPriority(GIC_TIM6, 5);
    GicdSetTarget(GIC_TIM6, 1);
    GicdSetTrigMode(GIC_TIM6, 0);
    GicRegisterIrq(GIC_TIM6, tim6_isr);
    GicdEnableInt(GIC_TIM6);

    /* EXTI0 */
    RCC->MP_AHB4ENSETR |= 1 << 0;

    GpioMode(GPIO_A, 0, GPIO_MODER_INPUT);
    GpioPullUpDown(GPIO_A, 0, GPIO_PUPDR_PULL_UP);

    ExtiSetGpio(0, EXTI_GPIO_PA);
    ExtiSetTrig(0, 2);
    ExtiEnableInt(1, 0);

    GicdSetGroup(GIC_EXTI0);
    GicdSetPriority(GIC_EXTI0, 5);
    GicdSetTarget(GIC_EXTI0, 1);
    GicdSetTrigMode(GIC_EXTI0, 0);
    GicRegisterIrq(GIC_EXTI0, exti0_falling_isr);
    GicdEnableInt(GIC_EXTI0);

    /* GIC + CPU 使能 */
    GicdInit();
    GiccInit(10, 2);

    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :
        :
        : "r0"
    );

    GpioMode(GPIO_Z, 6, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 6, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(GPIO_Z, 6);

    GpioMode(GPIO_Z, 7, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 7, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(GPIO_Z, 7);

    BasicTimerStart(TIM6);

    PRINT("\r\nTIM6 toggles Z6, PA0 (falling) toggles Z7 (press 'S' to stop)...\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;
        if (UsartReadOne(USART4, &ch))
            break;
    }

    BasicTimerStop(TIM6);
    GicdDisableInt(GIC_TIM6);
    GicdDisableInt(GIC_EXTI0);
    ExtiDisableInt(1, 0);
    GpioOutLow(GPIO_Z, 6);
    GpioOutLow(GPIO_Z, 7);

    PRINT("Test stopped.\r\n");
}


/*
 * ========== 2. EXTI0 双边沿 ==========
 */

static void exti0_both_isr(void)
{
    if (EXTI->CFG[0].FPR & (1 << 0))
    {
        ExtiClearFpr(0);
        GpioOutHi(GPIO_Z, 7);
        PRINT("PA0 Pressed!\r\n");
    }

    if (EXTI->CFG[0].RPR & (1 << 0))
    {
        ExtiClearRpr(0);
        GpioOutLow(GPIO_Z, 7);
        PRINT("PA0 Released!\r\n");
    }
}

static void IrqTest_ExtiBoth(void)
{
    /* EXTI0 */
    RCC->MP_AHB4ENSETR |= 1 << 0;

    GpioMode(GPIO_A, 0, GPIO_MODER_INPUT);
    GpioPullUpDown(GPIO_A, 0, GPIO_PUPDR_PULL_UP);

    ExtiSetGpio(0, EXTI_GPIO_PA);
    ExtiSetTrig(0, 3);                              /* 双边沿 */
    ExtiEnableInt(1, 0);

    GicdSetGroup(GIC_EXTI0);
    GicdSetPriority(GIC_EXTI0, 5);
    GicdSetTarget(GIC_EXTI0, 1);
    GicdSetTrigMode(GIC_EXTI0, 0);
    GicRegisterIrq(GIC_EXTI0, exti0_both_isr);
    GicdEnableInt(GIC_EXTI0);

    /* GIC + CPU 使能 */
    GicdInit();
    GiccInit(10, 2);

    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :
        :
        : "r0"
    );

    GpioMode(GPIO_Z, 7, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 7, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(GPIO_Z, 7);

    PRINT("\r\nPA0 both edge: press/release toggles Z7 LED (press 'S' to stop)...\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;
        if (UsartReadOne(USART4, &ch))
            break;
    }

    GicdDisableInt(GIC_EXTI0);
    ExtiDisableInt(1, 0);
    GpioOutLow(GPIO_Z, 7);

    PRINT("Test stopped.\r\n");
}


/*
 * ========== 3. 中断抢占 ==========
 *
 * TIM6(优先级 10) 与 TIM7(优先级 1) 相同周期 ~1s。
 * TIM7 延迟几周期启动，略晚于 TIM6 溢出。
 * TIM6 ISR 忙等期间，TIM7 必定触发 → 验证高优先级能否抢占。
 */

static void tim6_low_isr(void)
{
    TIM6->SR &= ~1;
    GpioOutHi(GPIO_Z, 6);
    PRINT("[");                                     /* 进入低优先级 ISR */

    /* 忙等 > 1 个 TIM7 周期, 等 TIM7 触发 */
    for (volatile uint32_t i = 0; i < 10000000; i++);

    GpioOutLow(GPIO_Z, 6);
    PRINT("]");                                     /* 退出低优先级 ISR */
}


static void tim7_high_isr(void)
{
    TIM7->SR &= ~1;
    GpioToggle(GPIO_Z, 7);
    PRINT("!");                                     /* 高优先级抢断 */
}


static void IrqTest_Preempt(void)
{
    /* TIM6 + TIM7 时钟 */
    RCC->MP_APB1ENSETR |= (1 << 4) | (1 << 5);

    /* 相同配置, 相同周期 ~1s */
    BasicTimerCfg_t cfg;
    cfg.tim_psc = 15999;
    cfg.tim_arr = 7999;                             /* 4000 * 250us = 1s */
    cfg.tim_arpe = 1;
    cfg.tim_opm = 0;
    cfg.tim_urs = 0;
    cfg.tim_udis = 0;
    cfg.tim_ude = 0;
    cfg.tim_uie = 1;

    /* TIM6 (低优先级 10) */
    BasicTimerCfg(TIM6, &cfg);

    GicdSetGroup(GIC_TIM6);
    GicdSetPriority(GIC_TIM6, 10);
    GicdSetTarget(GIC_TIM6, 1);
    GicdSetTrigMode(GIC_TIM6, 0);
    GicRegisterIrq(GIC_TIM6, tim6_low_isr);
    GicdEnableInt(GIC_TIM6);

    /* TIM7 (高优先级 1) */
    BasicTimerCfg(TIM7, &cfg);

    GicdSetGroup(GIC_TIM7);
    GicdSetPriority(GIC_TIM7, 1);
    GicdSetTarget(GIC_TIM7, 1);
    GicdSetTrigMode(GIC_TIM7, 0);
    GicRegisterIrq(GIC_TIM7, tim7_high_isr);
    GicdEnableInt(GIC_TIM7);

    /* GIC + CPU 使能 */
    GicdInit();
    GiccInit(31, 2);

    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :
        :
        : "r0"
    );

    GpioMode(GPIO_Z, 6, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 6, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(GPIO_Z, 6);

    GpioMode(GPIO_Z, 7, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_Z, 7, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(GPIO_Z, 7);

    /* 先启动 TIM6, 延迟几周期再启动 TIM7, 使二者失步 */
    BasicTimerStart(TIM6);
    for (volatile uint32_t i = 0; i < 100000; i++);    /* ~几 us 延迟 */
    BasicTimerStart(TIM7);

    PRINT("Preempt test: TIM6(pri=10)~1s, TIM7(pri=1)~1s\r\n");
    PRINT("  []    = TIM6 ISR entry/exit\r\n");
    PRINT("  !     = TIM7 preempts during []\r\n");
    PRINT("Press 'S' to stop...\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;
        if (UsartReadOne(USART4, &ch))
            break;
    }

    BasicTimerStop(TIM6);
    BasicTimerStop(TIM7);
    GicdDisableInt(GIC_TIM6);
    GicdDisableInt(GIC_TIM7);
    GpioOutLow(GPIO_Z, 6);
    GpioOutLow(GPIO_Z, 7);

    PRINT("\r\nTest stopped.\r\n");
}
