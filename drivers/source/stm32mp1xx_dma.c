/*
 * stm32mp1xx_dma.c
 *
 *  Created on: 2025-5-14
 *      Author: gjsbr
 */


#include "stm32mp1xx_dma.h"


volatile DmaRegs_t *const DMA2 = (void *)0x48001000;
volatile DmaRegs_t *const DMA1 = (void *)0x48000000;
volatile DmaMuxRegs_t *const DMAMUX1 = (void *)0x48002000;

const DmaCfg_t DMA_CFG_DEFAULT = {
    .dma_dir          = DMA_DIR_MEM_2_PER,
    .dma_psize        = DMA_DATA_SIZE_BIT32,
    .dma_msize        = DMA_DATA_SIZE_BIT32,
    .dma_memaddr_incr = 1,
};


void DmaCfg(volatile DmaRegs_t *const dma, const DmaCfg_t *const cfg)
{
    uint8_t stream = cfg->dma_stream_num;

    dma->STREAM[stream].CR &= ~1u;
    while (dma->STREAM[stream].CR & 1u);

    // 清除中断标志
    uint8_t s = stream % 4;
    uint32_t ifcr_mask = 0x3Du << ((s / 2) * 16 + (s % 2) * 6);
    if (stream < 4)
        dma->LIFCR = ifcr_mask;
    else
        dma->HIFCR = ifcr_mask;

    // 读取参数
    uint32_t dir     = cfg->dma_dir;
    uint32_t dm_dis  = cfg->dma_dm_dis;
    uint32_t mburst  = cfg->dma_mem_burst;
    uint32_t pburst  = cfg->dma_per_burst;
    uint32_t msize   = cfg->dma_msize;
    uint32_t psize   = cfg->dma_psize;
    uint32_t circ    = cfg->dma_circual_buf;
    uint32_t dbm     = cfg->dma_double_buf;
    uint32_t pfctrl  = cfg->dma_flow_ctrl;
    uint32_t pincos  = cfg->dma_pincos_4;

    // 约束: M→M 强制 FIFO, 禁止外设流控/双缓冲/循环
    if (dir == DMA_DIR_MEM_2_MEM)
    {
        dm_dis = 1;
        pfctrl = 0;
        dbm    = 0;
        circ   = 0;
    }

    // 约束: 直接模式强制单次传输, MSIZE=PSIZE, PINCOS=0
    if (dm_dis == 0)
    {
        mburst = 0;
        pburst = 0;
        msize  = psize;
        pincos = 0;
    }

    // 约束: PBURST!=0 时 PINCOS 强制 0
    if (pburst != 0)
        pincos = 0;

    // 约束: 外设流控禁止循环
    if (pfctrl == 1)
        circ = 0;

    // 约束: 双缓冲强制循环
    if (dbm == 1)
        circ = 1;

    // 地址与传输数量
    dma->STREAM[stream].PAR  = cfg->dma_per_addr;
    dma->STREAM[stream].M0AR = cfg->dma_mem0_addr;
    if (dbm)
        dma->STREAM[stream].M1AR = cfg->dma_mem1_addr;
    dma->STREAM[stream].NDTR = cfg->dma_ndtr;

    // 构建 CR (不含 EN)
    uint32_t cr = (cfg->dma_dmeie << 1)
               | (cfg->dma_teie << 2)
               | (cfg->dma_htie << 3)
               | (cfg->dma_tcie << 4)
               | (pfctrl << 5)
               | (dir << 6)
               | (circ << 8)
               | (cfg->dma_peraddr_incr << 9)
               | (cfg->dma_memaddr_incr << 10)
               | (psize << 11)
               | (msize << 13)
               | (pincos << 15)
               | (cfg->dma_stream_pri << 16)
               | (dbm << 18)
               | (pburst << 21)
               | (mburst << 23);

    // 构建 FCR
    uint32_t fcr = cfg->dma_fifo_thre
                 | (dm_dis << 2)
                 | (cfg->dma_feie << 7);

    dma->STREAM[stream].FCR = fcr;
    dma->STREAM[stream].CR  = cr;
    dma->STREAM[stream].CR  = cr | 1u;
}


void DmaMuxSyncDisable(volatile DmaMuxRegs_t *mux, uint32_t ch)
{
    mux->CR[ch] &= ~((1u << 16) | (1u << 9));  /* 清 SE 和 EGE */
}


void DmaMuxSyncEnable(volatile DmaMuxRegs_t *mux, uint32_t ch,
                      uint32_t sync_id, DmaMuxSyncPol_t pol,
                      uint32_t nbreq, uint32_t ege)
{
    uint32_t cr = mux->CR[ch];

    /* 先关 SE 和 EGE, NBREQ 才可写 */
    mux->CR[ch] = cr & ~((1u << 16) | (1u << 9));

    /* 清同步相关位, 保留 DMAREQ_ID 和 SOIE */
    cr &= ~((0x7u << 24) | (0x1Fu << 19) | (0x3u << 17)
          | (1u << 16) | (1u << 9));

    cr |= ((sync_id & 0x7u) << 24)
        | ((nbreq & 0x1Fu) << 19)
        | ((pol & 0x3u) << 17)
        | ((ege & 1u) << 9)
        | (1u << 16);  /* SE=1 */

    mux->CR[ch] = cr;
}


void DmaMuxReqGenDisable(volatile DmaMuxRegs_t *mux, uint32_t gen_ch)
{
    mux->RGCR[gen_ch] &= ~(1u << 16);  /* 清 GE */
}


void DmaMuxReqGenEnable(volatile DmaMuxRegs_t *mux, uint32_t gen_ch,
                        uint32_t sig_id, DmaMuxTriPol_t pol,
                        uint32_t gnbreq)
{
    uint32_t rg = mux->RGCR[gen_ch];

    /* 先关 GE, GNBREQ 才可写 */
    mux->RGCR[gen_ch] = rg & ~(1u << 16);

    /* 清配置位, 保留 OIE */
    rg &= ~((0x1Fu << 19) | (0x3u << 17) | (1u << 16) | 0x7u);

    rg |= ((gnbreq & 0x1Fu) << 19)
        | ((pol & 0x3u) << 17)
        | (1u << 16)  /* GE=1 */
        | (sig_id & 0x7u);

    mux->RGCR[gen_ch] = rg;
}


void DmaMuxRoute(volatile DmaMuxRegs_t *mux, uint32_t ch, uint32_t req_id)
{
    uint32_t cr = mux->CR[ch];
    cr &= ~0x7Fu;
    cr |= req_id & 0x7Fu;
    mux->CR[ch] = cr;
}


void DmaMuxSoieEnable(volatile DmaMuxRegs_t *mux, uint32_t ch)
{
    mux->CR[ch] |= (1u << 8);
}

void DmaMuxSoieDisable(volatile DmaMuxRegs_t *mux, uint32_t ch)
{
    mux->CR[ch] &= ~(1u << 8);
}

uint32_t DmaMuxSyncOvfGet(volatile DmaMuxRegs_t *mux, uint32_t ch)
{
    return (mux->CSR >> ch) & 1u;
}

void DmaMuxSyncOvfClear(volatile DmaMuxRegs_t *mux, uint32_t ch)
{
    mux->CFR = (1u << ch);
}


void DmaMuxOieEnable(volatile DmaMuxRegs_t *mux, uint32_t gen_ch)
{
    mux->RGCR[gen_ch] |= (1u << 8);
}

void DmaMuxOieDisable(volatile DmaMuxRegs_t *mux, uint32_t gen_ch)
{
    mux->RGCR[gen_ch] &= ~(1u << 8);
}

uint32_t DmaMuxReqGenOvfGet(volatile DmaMuxRegs_t *mux, uint32_t gen_ch)
{
    return (mux->RGSR >> gen_ch) & 1u;
}

void DmaMuxReqGenOvfClear(volatile DmaMuxRegs_t *mux, uint32_t gen_ch)
{
    mux->RGCFR = (1u << gen_ch);
}

