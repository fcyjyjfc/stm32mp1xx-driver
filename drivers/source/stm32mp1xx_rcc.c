#include "stm32mp1xx_rcc.h"

volatile RccRegs_t *const RCC = (void *)0x50000000;

/* ==================== HSE ==================== */

/**
 * @brief  初始化 HSE 时钟源
 * @param  mode  时钟源模式: CRYSTAL / BYPASS_ANALOG / BYPASS_DIGITAL
 * @return 0 成功, -1 关闭超时, -2 就绪超时
 */
int RccHseInit(RccHseMode_t mode)
{
    uint32_t timeout;

    /* 1. 先关闭 HSE */
    RCC->OCENCLRR = RCC_OCEN_HSEON;

    /* 2. 等待 HSERDY=0 */
    timeout = 100000;
    while ((RCC->OCRDYR & RCC_OCRDY_HSERDY) && --timeout);
    if (timeout == 0) return -1;

    /* 3. 设置 HSE 时钟源模式 */
    if (mode == RCC_HSE_MODE_CRYSTAL) {
        RCC->OCENCLRR = RCC_OCEN_HSEBYP;
    } else if (mode == RCC_HSE_MODE_BYPASS_ANALOG) {
        RCC->OCENCLRR = RCC_OCEN_DIGBYP;
        RCC->OCENSETR = RCC_OCEN_HSEBYP;
    } else {
        RCC->OCENSETR = RCC_OCEN_HSEBYP | RCC_OCEN_DIGBYP;
    }

    /* 4. 使能 HSE */
    RCC->OCENSETR = RCC_OCEN_HSEON;

    /* 5. 等待 HSERDY=1 (512 个 HSE 周期) */
    timeout = 100000;
    while (!(RCC->OCRDYR & RCC_OCRDY_HSERDY) && --timeout);
    if (timeout == 0) return -2;

    return 0;
}

/**
 * @brief  关闭 HSE 时钟源
 */
void RccHseDisable(void)
{
    RCC->OCENCLRR = RCC_OCEN_HSEON;
}

/* ==================== HSI ==================== */

/**
 * @brief  初始化 HSI (内部 64MHz 振荡器)
 * @return 0 成功, -1 关闭超时, -2 就绪超时
 */
int RccHsiInit(void)
{
    uint32_t timeout;

    /* 1. 先关闭 HSI */
    RCC->OCENCLRR = RCC_OCEN_HSION;

    /* 2. 等待 HSIRDY=0 */
    timeout = 100000;
    while ((RCC->OCRDYR & RCC_OCRDY_HSIRDY) && --timeout);
    if (timeout == 0) return -1;

    /* 3. 使能 HSI */
    RCC->OCENSETR = RCC_OCEN_HSION;

    /* 4. 等待 HSIRDY=1 */
    timeout = 100000;
    while (!(RCC->OCRDYR & RCC_OCRDY_HSIRDY) && --timeout);
    if (timeout == 0) return -2;

    return 0;
}

/**
 * @brief  关闭 HSI 时钟源
 */
void RccHsiDisable(void)
{
    RCC->OCENCLRR = RCC_OCEN_HSION;
}

/* ==================== CSI ==================== */

/**
 * @brief  初始化 CSI (内部 4MHz 振荡器)
 * @return 0 成功, -1 关闭超时, -2 就绪超时
 */
int RccCsiInit(void)
{
    uint32_t timeout;

    /* 1. 先关闭 CSI */
    RCC->OCENCLRR = RCC_OCEN_CSION;

    /* 2. 等待 CSIRDY=0 */
    timeout = 100000;
    while ((RCC->OCRDYR & RCC_OCRDY_CSIRDY) && --timeout);
    if (timeout == 0) return -1;

    /* 3. 使能 CSI */
    RCC->OCENSETR = RCC_OCEN_CSION;

    /* 4. 等待 CSIRDY=1 */
    timeout = 100000;
    while (!(RCC->OCRDYR & RCC_OCRDY_CSIRDY) && --timeout);
    if (timeout == 0) return -2;

    return 0;
}

/**
 * @brief  关闭 CSI 时钟源
 */
void RccCsiDisable(void)
{
    RCC->OCENCLRR = RCC_OCEN_CSION;
}

/* ==================== LSE ==================== */

/**
 * @brief  初始化 LSE 时钟源 (32.768kHz)
 * @param  bypass  0=晶振, 1=外部时钟旁路
 * @param  drv     驱动能力: LOW / MED / HIGH
 * @return 0 成功, -1 关闭超时, -2 就绪超时
 */
int RccLseInit(int bypass, RccLseDrv_t drv)
{
    uint32_t timeout;

    /* TODO: 写 BDCR 前需先置位 PWR_CR1.DBP */

    /* 1. 先关闭 LSE */
    RCC->BDCR &= ~RCC_BDCR_LSEON;

    /* 2. 等待 LSERDY=0 */
    timeout = 100000;
    while ((RCC->BDCR & RCC_BDCR_LSERDY) && --timeout);
    if (timeout == 0) return -1;

    /* 3. 设置驱动能力 */
    RCC->BDCR &= ~(RCC_BDCR_LSEDRV_0 | RCC_BDCR_LSEDRV_1);
    if (drv == RCC_LSE_DRV_LOW) {
        /* 00 = Low */
    } else if (drv == RCC_LSE_DRV_MED) {
        RCC->BDCR |= RCC_BDCR_LSEDRV_0;
    } else {
        RCC->BDCR |= RCC_BDCR_LSEDRV_1;
    }

    /* 4. 设置旁路模式 */
    if (bypass)
        RCC->BDCR |= RCC_BDCR_LSEBYP;
    else
        RCC->BDCR &= ~RCC_BDCR_LSEBYP;

    /* 5. 使能 LSE */
    RCC->BDCR |= RCC_BDCR_LSEON;

    /* 6. 等待 LSERDY=1 */
    timeout = 100000;
    while (!(RCC->BDCR & RCC_BDCR_LSERDY) && --timeout);
    if (timeout == 0) return -2;

    /* TODO: PWR_CR1.DBP=0 */
    return 0;
}

/**
 * @brief  关闭 LSE 时钟源
 */
void RccLseDisable(void)
{
    RCC->BDCR &= ~RCC_BDCR_LSEON;
}

/* ==================== LSI ==================== */

/**
 * @brief  初始化 LSI (内部 32kHz 振荡器)
 * @return 0 成功, -1 关闭超时, -2 就绪超时
 */
int RccLsiInit(void)
{
    uint32_t timeout;

    /* 1. 先关闭 LSI */
    RCC->BDCR &= ~RCC_BDCR_LSION;

    /* 2. 等待 LSIRDY=0 */
    timeout = 100000;
    while ((RCC->BDCR & RCC_BDCR_LSIRDY) && --timeout);
    if (timeout == 0) return -1;

    /* 3. 使能 LSI */
    RCC->BDCR |= RCC_BDCR_LSION;

    /* 4. 等待 LSIRDY=1 */
    timeout = 100000;
    while (!(RCC->BDCR & RCC_BDCR_LSIRDY) && --timeout);
    if (timeout == 0) return -2;

    return 0;
}

/**
 * @brief  关闭 LSI 时钟源
 */
void RccLsiDisable(void)
{
    RCC->BDCR &= ~RCC_BDCR_LSION;
}
