/*
 * stm32mp1xx_dma.h
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_DMA_H_
#define STM32MP1XX_DMA_H_

#include <stdint.h>


typedef struct {
    uint32_t CR;
    uint32_t NDTR;
    uint32_t PAR;
    uint32_t M0AR;
    uint32_t M1AR;
    uint32_t FCR;
} DmaStreamRegs_t;


typedef struct {
    uint32_t        LISR;
    uint32_t        HISR;
    uint32_t        LIFCR;
    uint32_t        HIFCR;
    DmaStreamRegs_t STREAM[8];
    uint8_t         RSVD0[0x3EC - 0x0CC - 4];
    uint32_t        HWCFGR2;
    uint32_t        HWCFGR1;
    uint32_t        VERR;
    uint32_t        IPIDR;
    uint32_t        SIDR;
} DmaRegs_t;


typedef enum {
    DMA_BURST_DIS       = 0, // SINGLE
    DMA_BURST_INCR4     = 1, // 4 beats
    DMA_BURST_INCR8     = 2, // 8 beats
    DMA_BURST_INCR16    = 3, // 16 beats
} DmaBurstCfg_t;


// STREAM优先级
typedef enum {
    DMA_STREAM_PRIOR_LOW        = 0,
    DMA_STREAM_PRIOR_MED        = 1,
    DMA_STREAM_PRIOR_HI         = 2,
    DMA_STREAM_PRIOR_VERY_HI    = 3
} DmaStreamPrior_t;


// MSIZE PSIZE
typedef enum {
    DMA_DATA_SIZE_BIT8  = 0,
    DMA_DATA_SIZE_BIT16 = 1,
    DMA_DATA_SIZE_BIT32 = 2
} DmaDataSize_t;


// 地址增长方式
typedef enum {
    DMA_ADDR_INCR_MODE_FIXED = 0, // 固定
    DMA_ADDR_INCR_MODE_INCR  = 1  // 增长
} DmaAddrIncr_t;


// 传输方向
typedef enum {
    DMA_DIR_PER_2_MEM = 0,
    DMA_DIR_MEM_2_PER = 1,
    DMA_DIR_MEM_2_MEM = 2
} DmaDir_t;


// FIFO阈值
typedef enum {
    DMA_FIFO_THRE_1_4   = 0, // 1/4
    DMA_FIFO_THRE_1_2   = 1, // 1/2
    DMA_FIFO_THRE_3_4   = 2, // 3/4
    DMA_FIFO_THRE_FULL  = 3  // 1
} DmaFth_t;


// flow 控制器
typedef enum {
    DMA_FLOW_CTRL_DMA, // DMA控制
    DMA_FLOW_CTRL_PER  // 外设控制
} DmaFlowCtrl_t;


typedef struct {
    uint32_t dma_mem0_addr;
    uint32_t dma_mem1_addr;
    uint32_t dma_per_addr;
    uint32_t dma_ndtr           : 16;
    uint32_t dma_stream_num     : 3;
    uint32_t dma_mem_burst      : 3;
    uint32_t dma_per_burst      : 3;
    uint32_t dma_dir            : 2;
    uint32_t dma_stream_pri     : 2;
    uint32_t dma_msize          : 2;
    uint32_t dma_psize          : 2;
    uint32_t dma_memaddr_incr   : 1;
    uint32_t dma_peraddr_incr   : 1;
    uint32_t dma_fifo_thre      : 2;
    uint32_t dma_circual_buf    : 1;
    uint32_t dma_double_buf     : 1;
    uint32_t dma_flow_ctrl      : 1;
    uint32_t dma_dm_dis         : 1;
    uint32_t dma_pincos_4       : 1;
    uint32_t dma_tcie           : 1;
    uint32_t dma_htie           : 1;
    uint32_t dma_teie           : 1;
    uint32_t dma_dmeie          : 1;
    uint32_t dma_feie           : 1;
} DmaCfg_t;


extern volatile DmaRegs_t *const DMA2;
extern volatile DmaRegs_t *const DMA1;

#endif /* STM32MP1XX_DMA_H_ */
