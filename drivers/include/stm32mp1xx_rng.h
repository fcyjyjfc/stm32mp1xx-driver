/*
 * stm32mp1xx_rng.h
 *
 *  Created on: 2025-6-22
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_RNG_H_
#define STM32MP1XX_RNG_H_

#include <stdint.h>


/* RNG_CR 位定义 */
#define RNG_CR_CED            (1u << 5)
#define RNG_CR_IE             (1u << 3)
#define RNG_CR_RNGEN          (1u << 2)

/* RNG_SR 位定义 */
#define RNG_SR_SEIS           (1u << 6)
#define RNG_SR_CEIS           (1u << 5)
#define RNG_SR_SECS           (1u << 2)
#define RNG_SR_CECS           (1u << 1)
#define RNG_SR_DRDY           (1u << 0)


/* 寄存器结构体 */
typedef struct {
    uint32_t CR;      // 0x000: 控制 (RNGEN, IE, CED)
    uint32_t SR;      // 0x004: 状态 (DRDY, CECS, SECS, CEIS, SEIS)
    uint32_t DR;      // 0x008: 数据 (RNDATA[31:0])
    uint8_t  RSVD0[0x3F0 - 0x00C];  // 0x00C - 0x3EF: 保留
    uint32_t HWCFGR;  // 0x3F0: 硬件配置 (保留)
    uint32_t VERR;    // 0x3F4: 版本 (MAJREV, MINREV)
    uint32_t IPIDR;   // 0x3F8: 标识
    uint32_t SIDR;    // 0x3FC: 大小标识
} RngRegs_t;


/* 配置结构体 */
typedef struct {
    uint32_t clock_error_detection;   // CED: 0=使能, 1=关闭
    uint32_t interrupt_enable;        // IE: 0=关闭, 1=使能
} RngCfg_t;


void RngCfg(volatile RngRegs_t *rng, const RngCfg_t *cfg);
void RngEnable(volatile RngRegs_t *rng);
void RngDisable(volatile RngRegs_t *rng);

uint32_t RngGetRandom(volatile RngRegs_t *rng);
uint32_t RngRead(volatile RngRegs_t *rng, uint32_t *data);
uint32_t RngIsReady(volatile RngRegs_t *rng);

uint32_t RngGetStatus(volatile RngRegs_t *rng);
void     RngClearSeedError(volatile RngRegs_t *rng);
void     RngClearClockError(volatile RngRegs_t *rng);
uint32_t RngSeedErrorRecovery(volatile RngRegs_t *rng);


#endif /* STM32MP1XX_RNG_H_ */
