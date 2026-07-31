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
    RCC->RDLSICR &= ~RCC_RDLSICR_LSION;

    /* 2. 等待 LSIRDY=0 */
    timeout = 100000;
    while ((RCC->RDLSICR & RCC_RDLSICR_LSIRDY) && --timeout);
    if (timeout == 0) return -1;

    /* 3. 使能 LSI */
    RCC->RDLSICR |= RCC_RDLSICR_LSION;

    /* 4. 等待 LSIRDY=1 */
    timeout = 100000;
    while (!(RCC->RDLSICR & RCC_RDLSICR_LSIRDY) && --timeout);
    if (timeout == 0) return -2;

    return 0;
}

/**
 * @brief  关闭 LSI 时钟源
 */
void RccLsiDisable(void)
{
    RCC->RDLSICR &= ~RCC_RDLSICR_LSION;
}

/* ==================== PLL 参考时钟 ==================== */

/**
 * @brief  选择 PLL1/PLL2 参考时钟源
 * @param  src  PLLSRC_HSI / PLLSRC_HSE
 * @return 0 成功, -1 切换超时
 */
int RccPll12SrcSelect(RccPllSrc_t src)
{
    uint32_t timeout;

    RCC->RCK12SELR = (RCC->RCK12SELR & ~RCC_RCK_SRC_MASK) | src;

    timeout = 100000;
    while (!(RCC->RCK12SELR & RCC_RCK_SRCRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  选择 PLL3 参考时钟源
 * @param  src  PLLSRC_HSI / PLLSRC_HSE / PLLSRC_CSI / PLLSRC_OFF
 * @return 0 成功, -1 切换超时
 */
int RccPll3SrcSelect(RccPllSrc_t src)
{
    uint32_t timeout;

    RCC->RCK3SELR = (RCC->RCK3SELR & ~RCC_RCK_SRC_MASK) | src;

    timeout = 100000;
    while (!(RCC->RCK3SELR & RCC_RCK_SRCRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  选择 PLL4 参考时钟源
 * @param  src  PLLSRC_HSI / PLLSRC_HSE / PLLSRC_CSI / PLLSRC_I2S_CKIN
 * @return 0 成功, -1 切换超时
 */
int RccPll4SrcSelect(RccPllSrc_t src)
{
    uint32_t timeout;

    RCC->RCK4SELR = (RCC->RCK4SELR & ~RCC_RCK_SRC_MASK) | src;

    timeout = 100000;
    while (!(RCC->RCK4SELR & RCC_RCK_SRCRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/* ==================== PLL1 ==================== */

/**
 * @brief  初始化 PLL1
 * @param  cfg  分频参数配置 (fracv=0 为整数模式, >0 为小数模式)
 * @return 0 成功, -1 DIVN 超范围, -3 DIVP 为奇数, -4 关闭超时, -5 锁定超时
 * @note   整数: Fpll_y = Fref × (DIVN+1) / (DIVy+1)
 *         小数: Fpll_y = Fref × ((DIVN+1) + FRACV/8192) / (DIVy+1)
 *         Fref = Fin / (DIVM+1), FVCO 约束 [800,1600] MHz
 */
int RccPll1Init(RccPll12Cfg_t *cfg)
{
    uint32_t cr, timeout;

    if (cfg->divn < 25 || cfg->divn > 100) return -1;
    if (cfg->divp != 0 && (cfg->divp & 1)) return -3;

    /* 1. 关 PLL (先关所有输出) */
    RCC->PLL1CR &= ~(RCC_PLLCR_DIVPEN | RCC_PLLCR_DIVQEN | RCC_PLLCR_DIVREN);
    RCC->PLL1CR &= ~RCC_PLLCR_PLLON;

    /* 2. 等 PLLRDY=0 */
    timeout = 100000;
    while ((RCC->PLL1CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -4;

    /* 3. 配置 CFGR1 / CFGR2 */
    RCC->PLL1CFGR1 = ((cfg->divm << RCC_PLLCFGR1_DIVM_Pos) & RCC_PLLCFGR1_DIVM_Msk)
                   | ((cfg->divn << RCC_PLLCFGR1_DIVN_Pos) & RCC_PLLCFGR1_DIVN_Msk);
    RCC->PLL1CFGR2 = ((cfg->divp << RCC_PLLCFGR2_DIVP_Pos) & RCC_PLLCFGR2_DIVP_Msk)
                   | ((cfg->divq << RCC_PLLCFGR2_DIVQ_Pos) & RCC_PLLCFGR2_DIVQ_Msk)
                   | ((cfg->divr << RCC_PLLCFGR2_DIVR_Pos) & RCC_PLLCFGR2_DIVR_Msk);

    /* 4. 小数分频 (FRACLE=0→写 FRACV→FRACLE=1 锁存) */
    if (cfg->fracv) {
        RCC->PLL1FRACR = (cfg->fracv << RCC_PLLFRACR_FRACV_Pos);
        RCC->PLL1FRACR |= RCC_PLLFRACR_FRACLE;
    }

    /* 5. 使能 PLL */
    cr = RCC_PLLCR_PLLON;
    if (cfg->en_p) cr |= RCC_PLLCR_DIVPEN;
    if (cfg->en_q) cr |= RCC_PLLCR_DIVQEN;
    if (cfg->en_r) cr |= RCC_PLLCR_DIVREN;
    RCC->PLL1CR |= cr;

    /* 6. 等 PLLRDY=1 */
    timeout = 100000;
    while (!(RCC->PLL1CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -5;

    return 0;
}

/**
 * @brief  关闭 PLL1
 */
void RccPll1Disable(void)
{
    RCC->PLL1CR &= ~(RCC_PLLCR_DIVPEN | RCC_PLLCR_DIVQEN | RCC_PLLCR_DIVREN);
    RCC->PLL1CR &= ~RCC_PLLCR_PLLON;
}

/* ==================== PLL2 ==================== */

/**
 * @brief  初始化 PLL2
 * @param  cfg  分频参数配置 (fracv=0 为整数模式, >0 为小数模式)
 * @return 0 成功, -1 DIVN 超范围, -3 DIVP 为奇数, -4 关闭超时, -5 锁定超时
 * @note   公式同 PLL1
 */
int RccPll2Init(RccPll12Cfg_t *cfg)
{
    uint32_t cr, timeout;

    if (cfg->divn < 25 || cfg->divn > 100) return -1;
    if (cfg->divp != 0 && (cfg->divp & 1)) return -3;

    RCC->PLL2CR &= ~(RCC_PLLCR_DIVPEN | RCC_PLLCR_DIVQEN | RCC_PLLCR_DIVREN);
    RCC->PLL2CR &= ~RCC_PLLCR_PLLON;

    timeout = 100000;
    while ((RCC->PLL2CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -4;

    RCC->PLL2CFGR1 = ((cfg->divm << RCC_PLLCFGR1_DIVM_Pos) & RCC_PLLCFGR1_DIVM_Msk)
                   | ((cfg->divn << RCC_PLLCFGR1_DIVN_Pos) & RCC_PLLCFGR1_DIVN_Msk);
    RCC->PLL2CFGR2 = ((cfg->divp << RCC_PLLCFGR2_DIVP_Pos) & RCC_PLLCFGR2_DIVP_Msk)
                   | ((cfg->divq << RCC_PLLCFGR2_DIVQ_Pos) & RCC_PLLCFGR2_DIVQ_Msk)
                   | ((cfg->divr << RCC_PLLCFGR2_DIVR_Pos) & RCC_PLLCFGR2_DIVR_Msk);

    if (cfg->fracv) {
        RCC->PLL2FRACR = (cfg->fracv << RCC_PLLFRACR_FRACV_Pos);
        RCC->PLL2FRACR |= RCC_PLLFRACR_FRACLE;
    }

    cr = RCC_PLLCR_PLLON;
    if (cfg->en_p) cr |= RCC_PLLCR_DIVPEN;
    if (cfg->en_q) cr |= RCC_PLLCR_DIVQEN;
    if (cfg->en_r) cr |= RCC_PLLCR_DIVREN;
    RCC->PLL2CR |= cr;

    timeout = 100000;
    while (!(RCC->PLL2CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -5;

    return 0;
}

/**
 * @brief  关闭 PLL2
 */
void RccPll2Disable(void)
{
    RCC->PLL2CR &= ~(RCC_PLLCR_DIVPEN | RCC_PLLCR_DIVQEN | RCC_PLLCR_DIVREN);
    RCC->PLL2CR &= ~RCC_PLLCR_PLLON;
}

/* ==================== PLL3 ==================== */

/**
 * @brief  初始化 PLL3
 * @param  cfg  分频参数配置 (fracv=0 为整数模式, >0 为小数模式)
 * @return 0 成功, -1 DIVN 超范围, -4 关闭超时, -5 锁定超时
 * @note   整数: Fpll_y = Fref × (DIVN+1) / (DIVy+1)
 *         小数: Fpll_y = Fref × ((DIVN+1) + FRACV/8192) / (DIVy+1)
 *         Fref = Fin / (DIVM+1), FVCO 约束 [400,800] MHz
 */
int RccPll3Init(RccPll34Cfg_t *cfg)
{
    uint32_t cr, timeout;

    if (cfg->divn < 25 || cfg->divn > 200) return -1;

    RCC->PLL3CR &= ~(RCC_PLLCR_DIVPEN | RCC_PLLCR_DIVQEN | RCC_PLLCR_DIVREN);
    RCC->PLL3CR &= ~RCC_PLLCR_PLLON;

    timeout = 100000;
    while ((RCC->PLL3CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -4;

    RCC->PLL3CFGR1 = ((cfg->ifrge << RCC_PLLCFGR1_IFRGE_Pos))
                   | ((cfg->divm  << RCC_PLLCFGR1_DIVM_Pos)  & RCC_PLLCFGR1_DIVM_Msk)
                   | ((cfg->divn  << RCC_PLLCFGR1_DIVN_Pos)  & RCC_PLLCFGR1_DIVN_Msk);
    RCC->PLL3CFGR2 = ((cfg->divp << RCC_PLLCFGR2_DIVP_Pos) & RCC_PLLCFGR2_DIVP_Msk)
                   | ((cfg->divq << RCC_PLLCFGR2_DIVQ_Pos) & RCC_PLLCFGR2_DIVQ_Msk)
                   | ((cfg->divr << RCC_PLLCFGR2_DIVR_Pos) & RCC_PLLCFGR2_DIVR_Msk);

    if (cfg->fracv) {
        RCC->PLL3FRACR = (cfg->fracv << RCC_PLLFRACR_FRACV_Pos);
        RCC->PLL3FRACR |= RCC_PLLFRACR_FRACLE;
    }

    cr = RCC_PLLCR_PLLON;
    if (cfg->en_p) cr |= RCC_PLLCR_DIVPEN;
    if (cfg->en_q) cr |= RCC_PLLCR_DIVQEN;
    if (cfg->en_r) cr |= RCC_PLLCR_DIVREN;
    RCC->PLL3CR |= cr;

    timeout = 100000;
    while (!(RCC->PLL3CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -5;

    return 0;
}

/**
 * @brief  关闭 PLL3
 */
void RccPll3Disable(void)
{
    RCC->PLL3CR &= ~(RCC_PLLCR_DIVPEN | RCC_PLLCR_DIVQEN | RCC_PLLCR_DIVREN);
    RCC->PLL3CR &= ~RCC_PLLCR_PLLON;
}

/* ==================== PLL4 ==================== */

/**
 * @brief  初始化 PLL4
 * @param  cfg  分频参数配置 (fracv=0 为整数模式, >0 为小数模式)
 * @return 0 成功, -1 DIVN 超范围, -4 关闭超时, -5 锁定超时
 * @note   公式同 PLL3
 */
int RccPll4Init(RccPll34Cfg_t *cfg)
{
    uint32_t cr, timeout;

    if (cfg->divn < 25 || cfg->divn > 200) return -1;

    RCC->PLL4CR = 0;

    timeout = 100000;
    while ((RCC->PLL4CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -4;

    RCC->PLL4CFGR1 = ((cfg->ifrge << RCC_PLLCFGR1_IFRGE_Pos))
                   | ((cfg->divm  << RCC_PLLCFGR1_DIVM_Pos)  & RCC_PLLCFGR1_DIVM_Msk)
                   | ((cfg->divn  << RCC_PLLCFGR1_DIVN_Pos)  & RCC_PLLCFGR1_DIVN_Msk);
    RCC->PLL4CFGR2 = ((cfg->divp << RCC_PLLCFGR2_DIVP_Pos) & RCC_PLLCFGR2_DIVP_Msk)
                   | ((cfg->divq << RCC_PLLCFGR2_DIVQ_Pos) & RCC_PLLCFGR2_DIVQ_Msk)
                   | ((cfg->divr << RCC_PLLCFGR2_DIVR_Pos) & RCC_PLLCFGR2_DIVR_Msk);

    if (cfg->fracv) {
        RCC->PLL4FRACR = (cfg->fracv << RCC_PLLFRACR_FRACV_Pos);
        RCC->PLL4FRACR |= RCC_PLLFRACR_FRACLE;
    }

    cr = RCC_PLLCR_PLLON;
    if (cfg->en_p) cr |= RCC_PLLCR_DIVPEN;
    if (cfg->en_q) cr |= RCC_PLLCR_DIVQEN;
    if (cfg->en_r) cr |= RCC_PLLCR_DIVREN;
    RCC->PLL4CR |= cr;

    timeout = 100000;
    while (!(RCC->PLL4CR & RCC_PLLCR_PLLRDY) && --timeout);
    if (timeout == 0) return -5;

    return 0;
}

/**
 * @brief  关闭 PLL4
 */
void RccPll4Disable(void)
{
    RCC->PLL4CR &= ~(RCC_PLLCR_DIVPEN | RCC_PLLCR_DIVQEN | RCC_PLLCR_DIVREN);
    RCC->PLL4CR &= ~RCC_PLLCR_PLLON;
}

/* ==================== 子系统时钟 ==================== */

/**
 * @brief  配置 MPU 子系统时钟
 * @param  src  时钟源, 含分频: PLL1_DIV2=/2, PLL1_DIV4=/4, PLL1_DIV8=/8, PLL1_DIV16=/16
 * @return 0 成功, -1 分频更新超时, -2 源切换超时
 */
int RccMpuClkCfg(RccMpuSrc_t src)
{
    uint32_t timeout;

    if (src >= RCC_MPUSRC_PLL1_DIV2) {
        /* 设置分频, 然后以 PLL1_DIV 源选择 */
        uint32_t div = src - RCC_MPUSRC_PLL1_DIV2 + 1;
        RCC->MPCKDIVR = (RCC->MPCKDIVR & ~RCC_DIVR_DIV_Msk3)
                      | ((div << RCC_MPCKDIVR_MPUDIV_Pos) & RCC_DIVR_DIV_Msk3);

        timeout = 100000;
        while (!(RCC->MPCKDIVR & RCC_MPCKDIVR_MPUDIVRDY) && --timeout);
        if (timeout == 0) return -1;

        RCC->MPCKSELR = (RCC->MPCKSELR & ~RCC_MPCK_SRC_MASK) | 3;
    } else {
        RCC->MPCKSELR = (RCC->MPCKSELR & ~RCC_MPCK_SRC_MASK) | src;
    }

    timeout = 100000;
    while (!(RCC->MPCKSELR & RCC_MPCK_SRCRDY) && --timeout);
    if (timeout == 0) return -2;

    return 0;
}

/**
 * @brief  选择 AXI 子系统时钟源
 * @param  src  AXISRC_HSI / AXISRC_HSE / AXISRC_PLL2
 * @return 0 成功, -1 切换超时
 */
int RccAxiSrcSelect(RccAxiSrc_t src)
{
    uint32_t timeout;

    RCC->ASSCKSELR = (RCC->ASSCKSELR & ~RCC_AXISS_SRC_MASK) | src;

    timeout = 100000;
    while (!(RCC->ASSCKSELR & RCC_AXISS_SRCRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  设置 AXI 分频比 (同步影响 APB4/5, AHB5/6)
 * @param  div  0=/1, 1=/2, 2=/3, >=3=/4
 * @return 0 成功, -1 更新超时
 */
int RccAxiDivSet(uint32_t div)
{
    uint32_t timeout;

    RCC->AXIDIVR = (RCC->AXIDIVR & ~RCC_DIVR_DIV_Msk3)
                 | ((div << RCC_DIVR_DIV_Pos) & RCC_DIVR_DIV_Msk3);

    timeout = 100000;
    while (!(RCC->AXIDIVR & RCC_DIVR_DIVRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  选择 MCU 子系统时钟源
 * @param  src  MCUSRC_HSI / MCUSRC_HSE / MCUSRC_CSI / MCUSRC_PLL3
 * @return 0 成功, -1 切换超时
 */
int RccMcuSrcSelect(RccMcuSrc_t src)
{
    uint32_t timeout;

    RCC->MSSCKSELR = (RCC->MSSCKSELR & ~RCC_MPCK_SRC_MASK) | src;

    timeout = 100000;
    while (!(RCC->MSSCKSELR & RCC_MPCK_SRCRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  设置 MCU 分频比 (同步影响所有总线矩阵时钟)
 * @param  div  0=/1, 1=/2, 2=/4, 3=/8, 4=/16, ..., 8=/256, >=9=/512
 * @return 0 成功, -1 更新超时
 */
int RccMcuDivSet(uint32_t div)
{
    uint32_t timeout;

    RCC->MCUDIVR = (RCC->MCUDIVR & ~RCC_DIVR_DIV_Msk4)
                 | ((div << RCC_DIVR_DIV_Pos) & RCC_DIVR_DIV_Msk4);

    timeout = 100000;
    while (!(RCC->MCUDIVR & RCC_DIVR_DIVRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  设置 APB4 分频比
 * @param  div  0=/1, 1=/2, 2=/4, 3=/8, >=4=/16
 * @return 0 成功, -1 更新超时
 */
int RccApb4DivSet(uint32_t div)
{
    uint32_t timeout;

    RCC->APB4DIVR = (RCC->APB4DIVR & ~RCC_DIVR_DIV_Msk3)
                  | ((div << RCC_DIVR_DIV_Pos) & RCC_DIVR_DIV_Msk3);

    timeout = 100000;
    while (!(RCC->APB4DIVR & RCC_DIVR_DIVRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  设置 APB5 分频比
 * @param  div  0=/1, 1=/2, 2=/4, 3=/8, >=4=/16
 * @return 0 成功, -1 更新超时
 */
int RccApb5DivSet(uint32_t div)
{
    uint32_t timeout;

    RCC->APB5DIVR = (RCC->APB5DIVR & ~RCC_DIVR_DIV_Msk3)
                  | ((div << RCC_DIVR_DIV_Pos) & RCC_DIVR_DIV_Msk3);

    timeout = 100000;
    while (!(RCC->APB5DIVR & RCC_DIVR_DIVRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  设置 APB1 分频比 (时钟源: mlhclk)
 * @param  div  0=/1, 1=/2, 2=/4, 3=/8, 4=/16
 * @return 0 成功, -1 更新超时
 */
int RccApb1DivSet(uint32_t div)
{
    uint32_t timeout;

    RCC->APB1DIVR = (RCC->APB1DIVR & ~RCC_DIVR_DIV_Msk3)
                  | ((div << RCC_DIVR_DIV_Pos) & RCC_DIVR_DIV_Msk3);

    timeout = 100000;
    while (!(RCC->APB1DIVR & RCC_DIVR_DIVRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  设置 APB2 分频比 (时钟源: mlhclk)
 * @param  div  0=/1, 1=/2, 2=/4, 3=/8, 4=/16
 * @return 0 成功, -1 更新超时
 */
int RccApb2DivSet(uint32_t div)
{
    uint32_t timeout;

    RCC->APB2DIVR = (RCC->APB2DIVR & ~RCC_DIVR_DIV_Msk3)
                  | ((div << RCC_DIVR_DIV_Pos) & RCC_DIVR_DIV_Msk3);

    timeout = 100000;
    while (!(RCC->APB2DIVR & RCC_DIVR_DIVRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/**
 * @brief  设置 APB3 分频比 (时钟源: hclk)
 * @param  div  0=/1, 1=/2, 2=/4, 3=/8, >=4=/16
 * @return 0 成功, -1 更新超时
 */
int RccApb3DivSet(uint32_t div)
{
    uint32_t timeout;

    RCC->APB3DIVR = (RCC->APB3DIVR & ~RCC_DIVR_DIV_Msk3)
                  | ((div << RCC_DIVR_DIV_Pos) & RCC_DIVR_DIV_Msk3);

    timeout = 100000;
    while (!(RCC->APB3DIVR & RCC_DIVR_DIVRDY) && --timeout);
    if (timeout == 0) return -1;

    return 0;
}

/* ==================== 时钟树回读 ==================== */

#define HSE_FREQ  24000000u  /* 板载 HSE 晶振频率, 按实际修改 */
#define HSI_FREQ  64000000u
#define CSI_FREQ  4000000u
#define LSE_FREQ  32768u
#define LSI_FREQ  32000u

/** @brief 通用分频器解码: 寄存器值→实际除数 */
static uint32_t div_decode(uint32_t reg_div)
{
    if (reg_div == 0) return 1;
    if (reg_div == 1) return 2;
    if (reg_div == 2) return 4;
    if (reg_div == 3) return 8;
    return 16;
}

/** @brief MCUDIV 分频器解码: 0=/1 ... 8=/256, ≥9=/512 */
static uint32_t mcudiv_decode(uint32_t reg_div)
{
    if (reg_div == 0) return 1;
    if (reg_div == 1) return 2;
    if (reg_div == 2) return 4;
    if (reg_div == 3) return 8;
    if (reg_div == 4) return 16;
    if (reg_div == 5) return 32;
    if (reg_div == 6) return 64;
    if (reg_div == 7) return 128;
    if (reg_div == 8) return 256;
    return 512;
}

/** @brief MPUDIV 分频器解码: 0=/1, 1=/2, 2=/4, 3=/8, ≥4=/16 */
static uint32_t mpudiv_decode(uint32_t reg_div)
{
    if (reg_div == 0) return 1;
    if (reg_div == 1) return 2;
    if (reg_div == 2) return 4;
    if (reg_div == 3) return 8;
    return 16;
}

/** @brief 计算单个 PLL 输出频率
 *  @note  整数: Fout = Fref × (DIVN+1) / (DIVy+1)
 *         小数: Fout = Fref × ((DIVN+1) + FRACV/8192) / (DIVy+1)
 *         PLL1/2 与 PLL3/4 公式相同 (×2/÷2 抵消)
 */
static uint32_t pll_out_calc(uint32_t fref, uint32_t divn, uint32_t fracv,
                              uint32_t divy, int frac_en)
{
    uint64_t num;

    if (frac_en && fracv) {
        num = (uint64_t)fref * ((divn + 1) * 8192 + fracv);
        return (uint32_t)(num / (8192ull * (divy + 1)));
    }
    num = (uint64_t)fref * (divn + 1);
    return (uint32_t)(num / (divy + 1));
}

/**
 * @brief  回读完整时钟树, 填充所有节点频率
 * @param  tree  输出: 各时钟节点当前频率 (Hz), 未就绪为 0
 */
void RccClkDump(RccClkTree_t *tree)
{
    uint32_t reg, src, divn, divm, divy, fracv;
    uint32_t pll12_src, pll3_src, pll4_src;
    uint32_t fref;          /* PLL 输入 = 源 / (DIVM+1) */
    uint32_t pp, pq, pr;    /* 各 PLL 三路输出 */
    uint32_t mpu_src, axi_src, mcu_src;
    uint32_t axiss_ck, mcuss_ck;
    int      frac_en;

    /* ── 振荡器 ── */
    reg = RCC->OCRDYR;
    tree->hse = (reg & RCC_OCRDY_HSERDY) ? HSE_FREQ : 0;
    tree->hsi = (reg & RCC_OCRDY_HSIRDY) ? HSI_FREQ : 0;
    tree->csi = (reg & RCC_OCRDY_CSIRDY) ? CSI_FREQ : 0;

    reg = RCC->BDCR;
    tree->lse = (reg & RCC_BDCR_LSERDY) ? LSE_FREQ : 0;

    reg = RCC->RDLSICR;
    tree->lsi = (reg & RCC_RDLSICR_LSIRDY) ? LSI_FREQ : 0;

    /* ── PLL 参考源频率 ── */
    reg = RCC->RCK12SELR & RCC_RCK_SRC_MASK;
    pll12_src = (reg == RCC_PLLSRC_HSE) ? HSE_FREQ : HSI_FREQ;

    reg = RCC->RCK3SELR & RCC_RCK_SRC_MASK;
    if (reg == RCC_PLLSRC_HSE)      pll3_src = HSE_FREQ;
    else if (reg == RCC_PLLSRC_CSI) pll3_src = CSI_FREQ;
    else                            pll3_src = HSI_FREQ;

    reg = RCC->RCK4SELR & RCC_RCK_SRC_MASK;
    if (reg == RCC_PLLSRC_HSE)          pll4_src = HSE_FREQ;
    else if (reg == RCC_PLLSRC_CSI)     pll4_src = CSI_FREQ;
    else if (reg == RCC_PLLSRC_I2S_CKIN) pll4_src = 0;   /* I2S_CKIN 频率未知 */
    else                                pll4_src = HSI_FREQ;

    /* ── PLL1 ── */
    if (RCC->PLL1CR & RCC_PLLCR_PLLRDY) {
        divm = (RCC->PLL1CFGR1 & RCC_PLLCFGR1_DIVM_Msk) >> RCC_PLLCFGR1_DIVM_Pos;
        divn = (RCC->PLL1CFGR1 & RCC_PLLCFGR1_DIVN_Msk) >> RCC_PLLCFGR1_DIVN_Pos;
        fref = pll12_src / (divm + 1);
        fracv = (RCC->PLL1FRACR >> RCC_PLLFRACR_FRACV_Pos) & 0x1FFF;
        frac_en = (RCC->PLL1FRACR & RCC_PLLFRACR_FRACLE) != 0;

        divy = (RCC->PLL1CFGR2 & RCC_PLLCFGR2_DIVP_Msk) >> RCC_PLLCFGR2_DIVP_Pos;
        pp = (RCC->PLL1CR & RCC_PLLCR_DIVPEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL1CFGR2 & RCC_PLLCFGR2_DIVQ_Msk) >> RCC_PLLCFGR2_DIVQ_Pos;
        pq = (RCC->PLL1CR & RCC_PLLCR_DIVQEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL1CFGR2 & RCC_PLLCFGR2_DIVR_Msk) >> RCC_PLLCFGR2_DIVR_Pos;
        pr = (RCC->PLL1CR & RCC_PLLCR_DIVREN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
    } else {
        pp = pq = pr = 0;
    }
    tree->pll1_p = pp;
    tree->pll1_q = pq;
    tree->pll1_r = pr;

    /* ── PLL2 ── */
    if (RCC->PLL2CR & RCC_PLLCR_PLLRDY) {
        divm = (RCC->PLL2CFGR1 & RCC_PLLCFGR1_DIVM_Msk) >> RCC_PLLCFGR1_DIVM_Pos;
        divn = (RCC->PLL2CFGR1 & RCC_PLLCFGR1_DIVN_Msk) >> RCC_PLLCFGR1_DIVN_Pos;
        fref = pll12_src / (divm + 1);
        fracv = (RCC->PLL2FRACR >> RCC_PLLFRACR_FRACV_Pos) & 0x1FFF;
        frac_en = (RCC->PLL2FRACR & RCC_PLLFRACR_FRACLE) != 0;

        divy = (RCC->PLL2CFGR2 & RCC_PLLCFGR2_DIVP_Msk) >> RCC_PLLCFGR2_DIVP_Pos;
        pp = (RCC->PLL2CR & RCC_PLLCR_DIVPEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL2CFGR2 & RCC_PLLCFGR2_DIVQ_Msk) >> RCC_PLLCFGR2_DIVQ_Pos;
        pq = (RCC->PLL2CR & RCC_PLLCR_DIVQEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL2CFGR2 & RCC_PLLCFGR2_DIVR_Msk) >> RCC_PLLCFGR2_DIVR_Pos;
        pr = (RCC->PLL2CR & RCC_PLLCR_DIVREN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
    } else {
        pp = pq = pr = 0;
    }
    tree->pll2_p = pp;
    tree->pll2_q = pq;
    tree->pll2_r = pr;

    /* ── PLL3 ── */
    if (RCC->PLL3CR & RCC_PLLCR_PLLRDY) {
        divm = (RCC->PLL3CFGR1 & RCC_PLLCFGR1_DIVM_Msk) >> RCC_PLLCFGR1_DIVM_Pos;
        divn = (RCC->PLL3CFGR1 & RCC_PLLCFGR1_DIVN_Msk) >> RCC_PLLCFGR1_DIVN_Pos;
        fref = pll3_src / (divm + 1);
        fracv = (RCC->PLL3FRACR >> RCC_PLLFRACR_FRACV_Pos) & 0x1FFF;
        frac_en = (RCC->PLL3FRACR & RCC_PLLFRACR_FRACLE) != 0;

        divy = (RCC->PLL3CFGR2 & RCC_PLLCFGR2_DIVP_Msk) >> RCC_PLLCFGR2_DIVP_Pos;
        pp = (RCC->PLL3CR & RCC_PLLCR_DIVPEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL3CFGR2 & RCC_PLLCFGR2_DIVQ_Msk) >> RCC_PLLCFGR2_DIVQ_Pos;
        pq = (RCC->PLL3CR & RCC_PLLCR_DIVQEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL3CFGR2 & RCC_PLLCFGR2_DIVR_Msk) >> RCC_PLLCFGR2_DIVR_Pos;
        pr = (RCC->PLL3CR & RCC_PLLCR_DIVREN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
    } else {
        pp = pq = pr = 0;
    }
    tree->pll3_p = pp;
    tree->pll3_q = pq;
    tree->pll3_r = pr;

    /* ── PLL4 ── */
    if (RCC->PLL4CR & RCC_PLLCR_PLLRDY) {
        divm = (RCC->PLL4CFGR1 & RCC_PLLCFGR1_DIVM_Msk) >> RCC_PLLCFGR1_DIVM_Pos;
        divn = (RCC->PLL4CFGR1 & RCC_PLLCFGR1_DIVN_Msk) >> RCC_PLLCFGR1_DIVN_Pos;
        fref = pll4_src / (divm + 1);
        fracv = (RCC->PLL4FRACR >> RCC_PLLFRACR_FRACV_Pos) & 0x1FFF;
        frac_en = (RCC->PLL4FRACR & RCC_PLLFRACR_FRACLE) != 0;

        divy = (RCC->PLL4CFGR2 & RCC_PLLCFGR2_DIVP_Msk) >> RCC_PLLCFGR2_DIVP_Pos;
        pp = (RCC->PLL4CR & RCC_PLLCR_DIVPEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL4CFGR2 & RCC_PLLCFGR2_DIVQ_Msk) >> RCC_PLLCFGR2_DIVQ_Pos;
        pq = (RCC->PLL4CR & RCC_PLLCR_DIVQEN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
        divy = (RCC->PLL4CFGR2 & RCC_PLLCFGR2_DIVR_Msk) >> RCC_PLLCFGR2_DIVR_Pos;
        pr = (RCC->PLL4CR & RCC_PLLCR_DIVREN) ? pll_out_calc(fref, divn, fracv, divy, frac_en) : 0;
    } else {
        pp = pq = pr = 0;
    }
    tree->pll4_p = pp;
    tree->pll4_q = pq;
    tree->pll4_r = pr;

    /* ── MPU 时钟 ── */
    src = RCC->MPCKSELR & RCC_MPCK_SRC_MASK;
    if (src == RCC_MPUSRC_HSI)  mpu_src = HSI_FREQ;
    else if (src == RCC_MPUSRC_HSE)  mpu_src = HSE_FREQ;
    else                        mpu_src = tree->pll1_p;  /* PLL1 或 PLL1_DIV */

    if (src >= RCC_MPUSRC_PLL1_DIV2) {
        reg = (RCC->MPCKDIVR & RCC_MPCKDIVR_MPUDIV_Msk) >> RCC_MPCKDIVR_MPUDIV_Pos;
        tree->mpu = mpu_src / mpudiv_decode(reg);
    } else {
        tree->mpu = mpu_src;
    }

    /* ── AXI 子系统 ── */
    src = RCC->ASSCKSELR & RCC_AXISS_SRC_MASK;
    if (src == RCC_AXISRC_HSE)      axi_src = HSE_FREQ;
    else if (src == RCC_AXISRC_PLL2) axi_src = tree->pll2_p;
    else                            axi_src = HSI_FREQ;

    reg = (RCC->AXIDIVR & RCC_DIVR_DIV_Msk3) >> RCC_DIVR_DIV_Pos;
    axiss_ck = axi_src / div_decode(reg);
    tree->axi   = axiss_ck;
    tree->ahb5  = axiss_ck;
    tree->ahb6  = axiss_ck;
    tree->apb4  = axiss_ck / div_decode((RCC->APB4DIVR & RCC_DIVR_DIV_Msk3) >> RCC_DIVR_DIV_Pos);
    tree->apb5  = axiss_ck / div_decode((RCC->APB5DIVR & RCC_DIVR_DIV_Msk3) >> RCC_DIVR_DIV_Pos);

    /* ── MCU 子系统 ── */
    src = RCC->MSSCKSELR & RCC_MPCK_SRC_MASK;
    if (src == RCC_MCUSRC_HSE)      mcu_src = HSE_FREQ;
    else if (src == RCC_MCUSRC_CSI) mcu_src = CSI_FREQ;
    else if (src == RCC_MCUSRC_PLL3) mcu_src = tree->pll3_p;
    else                            mcu_src = HSI_FREQ;

    reg = (RCC->MCUDIVR & RCC_DIVR_DIV_Msk4) >> RCC_DIVR_DIV_Pos;
    mcuss_ck = mcu_src / mcudiv_decode(reg);
    tree->mcu   = mcuss_ck;
    tree->ahb2  = mcuss_ck;
    tree->ahb3  = mcuss_ck;
    tree->ahb4  = mcuss_ck;
    tree->apb1  = mcuss_ck / div_decode((RCC->APB1DIVR & RCC_DIVR_DIV_Msk3) >> RCC_DIVR_DIV_Pos);
    tree->apb2  = mcuss_ck / div_decode((RCC->APB2DIVR & RCC_DIVR_DIV_Msk3) >> RCC_DIVR_DIV_Pos);
    tree->apb3  = mcuss_ck / div_decode((RCC->APB3DIVR & RCC_DIVR_DIV_Msk3) >> RCC_DIVR_DIV_Pos);
}

/* ==================== 复位原因 ==================== */

/**
 * @brief  读取 MPU 复位原因
 * @return 复位标志位组合 (RCC_RSTC_PORRSTF | RCC_RSTC_BORRSTF | ...)
 * @note   读完后需调用 RccResetCauseClear 清除, 否则下次读仍包含旧标志
 */
uint32_t RccResetCauseGet(void)
{
    return RCC->MP_RSTSCLRR;
}

/**
 * @brief  清除指定复位标志
 * @param  mask  要清除的标志位组合 (写1清0)
 * @note   通常调用方式: RccResetCauseClear(RccResetCauseGet())
 */
void RccResetCauseClear(uint32_t mask)
{
    RCC->MP_RSTSCLRR = mask;
}
