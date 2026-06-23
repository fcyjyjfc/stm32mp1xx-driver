/*
 * stm32mp1xx_crc.h
 *
 *  Created on: 2025-6-22
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_CRC_H_
#define STM32MP1XX_CRC_H_

#include <stdint.h>


/* CRC_CR 位定义 */
#define CRC_CR_REV_OUT        (1u << 7)
#define CRC_CR_REV_IN_BYTE    (1u << 5)
#define CRC_CR_REV_IN_HALF    (2u << 5)
#define CRC_CR_REV_IN_WORD    (3u << 5)
#define CRC_CR_POLYSIZE_32    (0u << 3)
#define CRC_CR_POLYSIZE_16    (1u << 3)
#define CRC_CR_POLYSIZE_8     (2u << 3)
#define CRC_CR_POLYSIZE_7     (3u << 3)
#define CRC_CR_RESET          (1u << 0)


/* 寄存器结构体 */
typedef struct {
    uint32_t DR;      // 0x000: 数据 (读=结果, 写=输入)
    uint32_t IDR;     // 0x004: 独立数据 (不受 RESET 影响)
    uint32_t CR;      // 0x008: 控制 (RESET, POLYSIZE, REV_IN, REV_OUT)
    uint8_t  RSVD0[0x10 - 0x0C];
    uint32_t INIT;    // 0x010: 初始值
    uint32_t POL;     // 0x014: 多项式
    uint8_t  RSVD1[0x3F0 - 0x018];
    uint32_t HWCFGR;  // 0x3F0: 硬件配置 (FULL_POLY)
    uint32_t VERR;    // 0x3F4: 版本
    uint32_t IPIDR;   // 0x3F8: 标识
    uint32_t SIDR;    // 0x3FC: 大小标识
} CrcRegs_t;


/* 配置结构体 */
typedef struct {
    uint32_t polynomial;     // CRC_POL 值 (默认 0x04C11DB7)
    uint32_t initial_value;  // CRC_INIT 值 (默认 0xFFFFFFFF)
    uint32_t polysize;       // POLYSIZE: 7/8/16/32
    uint32_t rev_in;         // REV_IN: 0=关闭 1=byte 2=half 3=word
    uint32_t rev_out;        // REV_OUT: 0=关闭 1=使能
} CrcCfg_t;


void CrcCfg(volatile CrcRegs_t *crc, const CrcCfg_t *cfg);
void CrcReset(volatile CrcRegs_t *crc);
void CrcFeed8(volatile CrcRegs_t *crc, const uint8_t *data, uint32_t len);
void CrcFeed16(volatile CrcRegs_t *crc, const uint16_t *data, uint32_t len);
void CrcFeed32(volatile CrcRegs_t *crc, const uint32_t *data, uint32_t len);
uint32_t CrcGet(volatile CrcRegs_t *crc);
uint32_t CrcCalculate(volatile CrcRegs_t *crc, const void *data, uint32_t byte_len);


#endif /* STM32MP1XX_CRC_H_ */
