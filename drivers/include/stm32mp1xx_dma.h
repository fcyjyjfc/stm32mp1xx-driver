/*
 * stm32mp1xx_dma.h
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_DMA_H_
#define STM32MP1XX_DMA_H_

#include <stdint.h>


/* Stream 寄存器块, 每 Stream 间距 0x18, 基偏移 0x010 + 0x18*x */
typedef struct {
    uint32_t CR;    // 配置: EN/DIR/CIRC/PINC/MINC/PSIZE/MSIZE/PL/DBM/PBURST/MBURST/PFCTRL
    uint32_t NDTR;  // 传输数量 0~65535, EN=0 才可写, 循环模式自动重载
    uint32_t PAR;   // 外设地址, EN=0 才可写
    uint32_t M0AR;  // 内存0地址, EN=0 才可写 (DBM: CT=1 时也可写)
    uint32_t M1AR;  // 内存1地址, EN=0 才可写 (DBM: CT=0 时也可写)
    uint32_t FCR;   // FIFO 控制: DMDIS/FTH/FS(只读)/FEIE
} DmaStreamRegs_t;


/* DMA 控制器寄存器, 必须 32 位字访问 */
typedef struct {
    uint32_t        LISR;       // 低中断状态 Stream 0~3 (只读)
    uint32_t        HISR;       // 高中断状态 Stream 4~7 (只读)
    uint32_t        LIFCR;      // 低中断清标志 (写1清)
    uint32_t        HIFCR;      // 高中断清标志 (写1清)
    DmaStreamRegs_t STREAM[8];  // 8 个 Stream 寄存器块
    uint8_t         RSVD0[0x3EC - 0x0CC - 4];
    uint32_t        HWCFGR2;    // FIFO_SIZE=4word, CHSEL_WIDTH=0 (无 CHSEL, 用 DMAMUX)
    uint32_t        HWCFGR1;    // 各 Stream 类型
    uint32_t        VERR;       // 版本 1.4
    uint32_t        IPIDR;      // 外设 ID
    uint32_t        SIDR;       // 大小 ID
} DmaRegs_t;


/* DMAMUX 寄存器 */
typedef struct {
    uint32_t CR[16];    // CxCR: 通道 0~15 配置 (DMAREQ_ID/SOIE/EGE/SE/SPOL/NBREQ/SYNC_ID)
    uint8_t  RSVD0[0x40];
    uint32_t CSR;       // 通道状态, SOFx 同步溢出标志 (只读)
    uint32_t CFR;       // 通道清标志, CSOFx (写1清)
    uint8_t  RSVD1[0x100 - 0x84 - 4];
    uint32_t RGCR[8];   // RGxCR: 请求发生器通道 0~7 (SIG_ID/OIE/GE/GPOL/GNBREQ)
    uint8_t  RSVD2[0x20];
    uint32_t RGSR;      // 发生器状态, OFx 触发溢出标志 (只读)
    uint32_t RGCFR;     // 发生器清标志, COFx (写1清)
    uint8_t  RSVD3[0x3EC - 0x144 - 4];
    uint32_t HWCFGR2;
    uint32_t HWCFGR1;
    uint32_t VERR;
    uint32_t IPIDR;
    uint32_t SIDR;
} DmaMuxRegs_t;


/* Burst 拍数, CR bit[24:23] MBURST / bit[22:21] PBURST, 直接模式下硬件强制 SINGLE */
typedef enum {
    DMA_BURST_DIS       = 0, // SINGLE
    DMA_BURST_INCR4     = 1, // 4 beats
    DMA_BURST_INCR8     = 2, // 8 beats
    DMA_BURST_INCR16    = 3, // 16 beats
} DmaBurstCfg_t;


/* Stream 优先级, CR bit[17:16] PL, EN=0 才可写 */
typedef enum {
    DMA_STREAM_PRIOR_LOW        = 0,
    DMA_STREAM_PRIOR_MED        = 1,
    DMA_STREAM_PRIOR_HI         = 2,
    DMA_STREAM_PRIOR_VERY_HI    = 3
} DmaStreamPrior_t;


/* 数据宽度, CR bit[14:13] MSIZE / bit[12:11] PSIZE, EN=0 才可写 */
typedef enum {
    DMA_DATA_SIZE_BIT8  = 0,
    DMA_DATA_SIZE_BIT16 = 1,
    DMA_DATA_SIZE_BIT32 = 2
} DmaDataSize_t;


/* 地址递增, CR bit[10] MINC / bit[9] PINC, EN=0 才可写 */
typedef enum {
    DMA_ADDR_INCR_MODE_FIXED = 0,
    DMA_ADDR_INCR_MODE_INCR  = 1
} DmaAddrIncr_t;


/* 传输方向, CR bit[7:6] DIR, EN=0 才可写 */
typedef enum {
    DMA_DIR_PER_2_MEM = 0, // 00: P→M
    DMA_DIR_MEM_2_PER = 1, // 01: M→P
    DMA_DIR_MEM_2_MEM = 2  // 10: M→M, 禁止直接模式+循环模式
} DmaDir_t;


/* FIFO 阈值, FCR bit[1:0] FTH, EN=0 才可写 */
typedef enum {
    DMA_FIFO_THRE_1_4   = 0,
    DMA_FIFO_THRE_1_2   = 1,
    DMA_FIFO_THRE_3_4   = 2,
    DMA_FIFO_THRE_FULL  = 3
} DmaFth_t;


/* 流控制器, CR bit[5] PFCTRL, EN=0 才可写 */
typedef enum {
    DMA_FLOW_CTRL_DMA, // DMA 控制, NDTR 递减到 0 结束
    DMA_FLOW_CTRL_PER  // 外设控制, NDTR 强制 0xFFFF, 禁止循环模式
} DmaFlowCtrl_t;


/* DMA Stream 配置参数 */
typedef struct {
    uint32_t dma_mem0_addr;         // M0AR
    uint32_t dma_mem1_addr;         // M1AR (仅 DBM)
    uint32_t dma_per_addr;          // PAR
    uint32_t dma_ndtr           : 16; // 传输数量
    uint32_t dma_stream_num     : 3;  // Stream 编号 0~7
    uint32_t dma_mem_burst      : 3;  // MBURST
    uint32_t dma_per_burst      : 3;  // PBURST
    uint32_t dma_dir            : 2;  // DIR
    uint32_t dma_stream_pri     : 2;  // PL
    uint32_t dma_msize          : 2;  // MSIZE, 直接模式下硬件强制=PSIZE
    uint32_t dma_psize          : 2;  // PSIZE
    uint32_t dma_memaddr_incr   : 1;  // MINC
    uint32_t dma_peraddr_incr   : 1;  // PINC
    uint32_t dma_fifo_thre      : 2;  // FTH
    uint32_t dma_circual_buf    : 1;  // CIRC, DBM 时硬件强制 1
    uint32_t dma_double_buf     : 1;  // DBM, 禁止 M→M
    uint32_t dma_flow_ctrl      : 1;  // PFCTRL
    uint32_t dma_dm_dis         : 1;  // DMDIS, 0=直接模式 1=FIFO 模式
    uint32_t dma_pincos_4       : 1;  // PINCOS, 1=外设地址固定+4
    uint32_t dma_tcie           : 1;  // TCIE 传输完成中断
    uint32_t dma_htie           : 1;  // HTIE 半传输中断
    uint32_t dma_teie           : 1;  // TEIE 传输错误中断
    uint32_t dma_dmeie          : 1;  // DMEIE 直接模式错误中断
    uint32_t dma_feie           : 1;  // FEIE FIFO 错误中断 (在 FCR 中)
} DmaCfg_t;


/* 同步极性, CxCR bit[18:17] SPOL */
typedef enum {
    DMA_MUX_SYNC_POL_NO         = 0, // 不使用
    DMA_MUX_SYNC_POL_RIS_EDGE   = 1,
    DMA_MUX_SYNC_POL_FAL_EDGE   = 2,
    DMA_MUX_SYNC_POL_BOTH_EDGE  = 3
} DmaMuxSyncPol_t;


/* 触发极性, RGxCR bit[18:17] GPOL */
typedef enum {
    DMA_MUX_TRI_POL_NO         = 0, // 不使用
    DMA_MUX_TRI_POL_RIS_EDGE   = 1,
    DMA_MUX_TRI_POL_FAL_EDGE   = 2,
    DMA_MUX_TRI_POL_BOTH_EDGE  = 3
} DmaMuxTriPol_t;


/* DMAMUX 配置参数 (多路复用器通道 + 请求发生器通道) */
typedef struct {
    /* --- 多路复用器通道 CxCR --- */
    uint32_t dma_mux_ch         : 4;  // 通道编号 0~15 (0~7→DMA1, 8~15→DMA2)
    uint32_t dma_mux_sync_en    : 1;  // SE 同步使能
    uint32_t dma_mux_sync_id    : 3;  // SYNC_ID 同步输入选择 (Table 113)
    uint32_t dma_mux_sync_nbreq : 5;  // NBREQ 同步后放行 NBREQ+1 个请求, SE=0且EGE=0才可写
    uint32_t dma_mux_sync_pol   : 2;  // SPOL 同步极性
    uint32_t dma_mux_event_en   : 1;  // EGE 事件发生使能
    uint32_t dma_mux_soie       : 1;  // SOIE 同步溢出中断使能
    uint32_t dma_mux_req        : 7;  // DMAREQ_ID 请求输入选择 (Table 111), 0=无
    /* --- 请求发生器通道 RGxCR --- */
    uint32_t dma_mux_req_gen_ch : 3;  // 发生器通道编号 0~7
    uint32_t dma_mux_gnbreq     : 5;  // GNBREQ 每次触发产生 GNBREQ+1 个请求, GE=0才可写
    uint32_t dma_mux_tri_pol    : 2;  // GPOL 触发极性
    uint32_t dma_mux_ge         : 1;  // GE 发生器使能
    uint32_t dma_mux_oie        : 1;  // OIE 触发溢出中断使能
    uint32_t dma_mux_sig_id     : 3;  // SIG_ID 触发输入选择 (Table 112)
} DmaMuxCfg_t;


extern volatile DmaRegs_t *const DMA2;
extern volatile DmaRegs_t *const DMA1;
extern volatile DmaMuxRegs_t *const DMAMUX1;
extern void DmaMuxCfg(volatile DmaMuxRegs_t *const dma_mux, const uint32_t ch, const DmaMuxCfg_t *const cfg);

#endif /* STM32MP1XX_DMA_H_ */
