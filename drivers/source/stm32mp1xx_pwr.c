#include "stm32mp1xx_pwr.h"

/** PWR 寄存器基地址指针 */
volatile PwrRegs_t *const PWR = (void *)0x50001000;

/* ======================== WKUP 引脚 ======================== */

/**
 * @brief  配置 WKUP 引脚极性及上拉
 * @param  pin       引脚编号 (1-6)
 * @param  polarity  0=上升沿唤醒, 1=下降沿唤醒
 * @param  pull      0=无上拉/下拉, 1=上拉, 2=下拉, 3=保留
 * @note   配置前会先禁用该引脚, 防止配置过程中误触发
 */
void PwrWakeupPinConfig(uint32_t pin, uint32_t polarity, uint32_t pull)
{
    uint32_t shift = pin * 4;       /* 每组 WKUP 占用 4 位 */

    if (pin == 0 || pin > PWR_WKUP_PIN_MAX)
        return;

    polarity = !!polarity;
    pull    &= 3U;

    /* 先清除该组配置并禁用, 再写入极性和上拉 */
    PWR->WKUPCR = (PWR->WKUPCR
        & ~((1U << (shift + 1)) | (3U << (shift + 2)))
        & ~(1U << shift))
        | (polarity << (shift + 1))
        | (pull     << (shift + 2));
}

/**
 * @brief  使能 WKUP 引脚唤醒功能
 * @param  pin  引脚编号 (1-6)
 */
void PwrWakeupPinEnable(uint32_t pin)
{
    if (pin == 0 || pin > PWR_WKUP_PIN_MAX)
        return;

    PWR->WKUPCR |= PWR_WKUPCR_WKUPEN(pin);
}

/**
 * @brief  禁用 WKUP 引脚唤醒功能
 * @param  pin  引脚编号 (1-6)
 */
void PwrWakeupPinDisable(uint32_t pin)
{
    if (pin == 0 || pin > PWR_WKUP_PIN_MAX)
        return;

    PWR->WKUPCR &= ~PWR_WKUPCR_WKUPEN(pin);
}

/**
 * @brief  读取 WKUP 引脚唤醒标志
 * @param  pin  引脚编号 (1-6)
 * @return 1=有唤醒事件, 0=无
 */
uint32_t PwrWakeupPinGetFlag(uint32_t pin)
{
    if (pin == 0 || pin > PWR_WKUP_PIN_MAX)
        return 0;

    return !!(PWR->WKUPFR & PWR_WKUPFR_WKUPF(pin));
}

/**
 * @brief  清除 WKUP 引脚唤醒标志
 * @param  pin  引脚编号 (1-6)
 * @note   在使能 WKUP 中断前应先清除标志, 防止误中断
 */
void PwrWakeupPinClearFlag(uint32_t pin)
{
    if (pin == 0 || pin > PWR_WKUP_PIN_MAX)
        return;

    PWR->WKUPFR = PWR_WKUPFR_WKUPF(pin);
}
