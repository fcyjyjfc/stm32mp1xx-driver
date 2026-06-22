/*
 * stm32mp1xx_crc.c
 *
 *  Created on: 2025-6-22
 *      Author: gjsbr
 */


#include "stm32mp1xx_crc.h"


/**
 * @brief 配置 CRC（多项式、初始值、反转、宽度）并复位
 * @param crc  CRC 寄存器实例指针
 * @param cfg  配置参数
 */
void CrcCfg(volatile CrcRegs_t *const crc, const CrcCfg_t *const cfg)
{
    uint32_t cr = 0;

    switch (cfg->polysize)
    {
        case 7:  cr |= CRC_CR_POLYSIZE_7;  break;
        case 8:  cr |= CRC_CR_POLYSIZE_8;  break;
        case 16: cr |= CRC_CR_POLYSIZE_16; break;
        default: cr |= CRC_CR_POLYSIZE_32; break;
    }

    switch (cfg->rev_in)
    {
        case 1:  cr |= CRC_CR_REV_IN_BYTE; break;
        case 2:  cr |= CRC_CR_REV_IN_HALF; break;
        case 3:  cr |= CRC_CR_REV_IN_WORD; break;
        default: break;
    }

    if (cfg->rev_out)
        cr |= CRC_CR_REV_OUT;

    crc->POL  = cfg->polynomial;
    crc->INIT = cfg->initial_value;
    crc->CR   = cr;
}


/**
 * @brief 复位 CRC 计算单元
 *
 * CR.RESET 写 1 → CRC_DR 重新载入 CRC_INIT 值。硬件自动清 RESET 位。
 */
void CrcReset(volatile CrcRegs_t *const crc)
{
    crc->CR |= CRC_CR_RESET;
}


/**
 * @brief 以 8 位宽度输入数据
 * @param crc   CRC 寄存器实例指针
 * @param data  数据缓冲
 * @param len   字节数
 */
void CrcFeed8(volatile CrcRegs_t *const crc, const uint8_t *const data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        *(volatile uint8_t *)&crc->DR = data[i];
}


/**
 * @brief 以 16 位宽度输入数据
 * @param crc   CRC 寄存器实例指针
 * @param data  数据缓冲
 * @param len   半字数
 */
void CrcFeed16(volatile CrcRegs_t *const crc, const uint16_t *const data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        *(volatile uint16_t *)&crc->DR = data[i];
}


/**
 * @brief 以 32 位宽度输入数据
 * @param crc   CRC 寄存器实例指针
 * @param data  数据缓冲
 * @param len   字数
 */
void CrcFeed32(volatile CrcRegs_t *const crc, const uint32_t *const data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        crc->DR = data[i];
}


/**
 * @brief 读取当前 CRC 计算结果
 * @param crc  CRC 寄存器实例指针
 * @return 32 位 CRC 结果
 */
uint32_t CrcGet(volatile CrcRegs_t *const crc)
{
    return crc->DR;
}


/**
 * @brief 便捷函数：重置后一次性计算字节数据的 CRC
 * @param crc       CRC 寄存器实例指针
 * @param data      输入数据
 * @param byte_len  字节长度
 * @return 32 位 CRC 结果
 *
 * 优先按 32 位字处理，剩余字节按 8 位处理。
 */
uint32_t CrcCalculate(volatile CrcRegs_t *const crc, const void *data, uint32_t byte_len)
{
    CrcReset(crc);

    const uint8_t *bytes = (const uint8_t *)data;

    /* 按 32 位字处理完整字部分 */
    uint32_t word_count = byte_len >> 2;
    if (word_count > 0)
    {
        const uint32_t *words = (const uint32_t *)bytes;
        for (uint32_t i = 0; i < word_count; i++)
            crc->DR = words[i];
        bytes += word_count << 2;
    }

    /* 剩余字节按 8 位处理 */
    uint32_t remain = byte_len & 3;
    for (uint32_t i = 0; i < remain; i++)
        *(volatile uint8_t *)&crc->DR = bytes[i];

    return crc->DR;
}
