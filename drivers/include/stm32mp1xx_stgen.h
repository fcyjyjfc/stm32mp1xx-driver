/*
 * stm32mp1xx_stgen.h
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_STGEN_H_
#define STM32MP1XX_STGEN_H_

#include <stdint.h>

typedef struct {
    uint32_t CNTCR;
    uint32_t CNTSR;
    uint32_t CNTCVL;
    uint32_t CNTCVU;
    uint8_t  RSVD0[0x20 - 0xC - 4];
    uint32_t CNTFID0;
    uint8_t  RSVD1[0xFD0 - 0x20 - 4];
    uint32_t PIDR4;
    uint32_t PIDR5;
    uint32_t PIDR6;
    uint32_t PIDR7;
    uint32_t PIDR0;
    uint32_t PIDR1;
    uint32_t PIDR2;
    uint32_t PIDR3;
    uint32_t CIDR0;
    uint32_t CIDR1;
    uint32_t CIDR2;
    uint32_t CIDR3;
} StgencRegs_t;


typedef struct {
    uint32_t CNTCVL;
    uint32_t CNTCVU;
    uint8_t  RSVD0[0xFD0 - 0x4 - 4];
    uint32_t PIDR4;
    uint32_t PIDR5;
    uint32_t PIDR6;
    uint32_t PIDR7;
    uint32_t PIDR0;
    uint32_t PIDR1;
    uint32_t PIDR2;
    uint32_t PIDR3;
    uint32_t CIDR0;
    uint32_t CIDR1;
    uint32_t CIDR2;
    uint32_t CIDR3;
} StgenrRegs_t;


typedef struct {
    volatile StgencRegs_t *stgenc;
    volatile StgenrRegs_t *stgenr;
} StgenRegs_t;


// extern volatile StgencRegs_t *const STGENC;
// extern volatile StgenrRegs_t *const STGENR;
extern const StgenRegs_t STGEN;

#endif /* STM32MP1XX_STGEN_H_ */
