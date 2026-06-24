/*
 * stm32mp1xx_rng.c
 *
 *  Created on: 2025-6-22
 *      Author: gjsbr
 */


#include "stm32mp1xx_rng.h"


volatile RngRegs_t *RNG1 = (void *)0;


/**
 * @brief 配置并使能 RNG
 * @param rng  RNG 寄存器实例指针
 * @param cfg  配置参数（CED、IE）
 *
 * CED 只能在 RNGEN=0 时修改，此函数会先清除 CR 寄存器再写入新值并使能。
 */
void RngCfg(volatile RngRegs_t *const rng, const RngCfg_t *const cfg)
{
    rng->CR = 0;

    uint32_t cr = RNG_CR_RNGEN;

    if (cfg->clock_error_detection)
        cr |= RNG_CR_CED;
    if (cfg->interrupt_enable)
        cr |= RNG_CR_IE;

    rng->CR = cr;
}


void RngEnable(volatile RngRegs_t *const rng)
{
    rng->CR |= RNG_CR_RNGEN;
}


void RngDisable(volatile RngRegs_t *const rng)
{
    rng->CR &= ~RNG_CR_RNGEN;
}


/**
 * @brief 阻塞读取一个 32 位随机数
 * @param rng  RNG 寄存器实例指针
 * @return 32 位随机数；发生错误时返回 0
 *
 * 轮询 DRDY 等待数据就绪，同时监测 SEIS/CEIS 错误。
 * 若健康检查检测到异常（SEIS=1），RNG 停止生成，函数返回 0。
 * 调用者应通过 RngGetStatus / RngClearSeedError 处理错误后重试。
 */
uint32_t RngGetRandom(volatile RngRegs_t *const rng)
{
    for (;;)
    {
        uint32_t sr = rng->SR;

        if (sr & RNG_SR_DRDY)
        {
            if (sr & (RNG_SR_SEIS | RNG_SR_CEIS))
                return 0;               // 数据可能无效
            return rng->DR;
        }

        if (sr & RNG_SR_SEIS)
            return 0;                   // 种子错误，RNG 已停止
    }
}


/**
 * @brief 非阻塞读取一个 32 位随机数
 * @param rng   RNG 寄存器实例指针
 * @param data  输出缓冲指针
 * @return 1=读取成功, 0=数据未就绪或发生错误
 */
uint32_t RngRead(volatile RngRegs_t *const rng, uint32_t *const data)
{
    uint32_t sr = rng->SR;

    if (!(sr & RNG_SR_DRDY))
        return 0;
    if (sr & (RNG_SR_SEIS | RNG_SR_CEIS))
        return 0;

    *data = rng->DR;
    return 1;
}


/**
 * @brief 检查随机数据是否就绪
 * @param rng  RNG 寄存器实例指针
 * @return 1=就绪, 0=未就绪
 */
uint32_t RngIsReady(volatile RngRegs_t *const rng)
{
    return (rng->SR & RNG_SR_DRDY) ? 1 : 0;
}


uint32_t RngGetStatus(volatile RngRegs_t *const rng)
{
    return rng->SR;
}


void RngClearSeedError(volatile RngRegs_t *const rng)
{
    rng->SR &= ~RNG_SR_SEIS;
}


void RngClearClockError(volatile RngRegs_t *const rng)
{
    rng->SR &= ~RNG_SR_CEIS;
}


/**
 * @brief 种子错误完整恢复序列
 * @param rng  RNG 寄存器实例指针
 * @return 1=恢复成功, 0=恢复失败（错误持续存在）
 *
 * 按手册 37.3.7 节要求：
 *   1. 清除 SEIS
 *   2. 读取 12 个字并丢弃（清空流水线）
 *   3. 验证 SEIS 仍为 0
 */
uint32_t RngSeedErrorRecovery(volatile RngRegs_t *const rng)
{
    rng->SR &= ~RNG_SR_SEIS;

    for (int i = 0; i < 12; i++)
        (void)rng->DR;

    if (rng->SR & RNG_SR_SEIS)
        return 0;

    return 1;
}
