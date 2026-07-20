/*
 * stm32mp1xx_fdcan.c — FDCAN 底层外设驱动
 *
 *   FdcanInit         — 初始化 (bit timing + 消息 RAM + 回环/正常模式)
 *   FdcanSend         — 发送一帧 (非阻塞, 写 Tx buffer + TXBAR)
 *   FdcanRead         — 从 Rx FIFO 0 读一帧 (轮询)
 *
 *  Created on: 2026-7-13
 *      Author: gjsbr
 */

#include <string.h>
#include "stm32mp1xx_fdcan.h"


/* ================================================================== */
/*  基地址变量定义                                                       */
/* ================================================================== */

volatile FdcanRegs_t *const FDCAN1  = (void *)0x4400E000U;
volatile FdcanRegs_t *const FDCAN2  = (void *)0x4400F000U;
volatile FdcanCcuRegs_t *const FDCAN_CCU = (void *)0x44010000U;
volatile uint32_t *const FDCAN_SRAM = (void *)0x44011000U;


/* ================================================================== */
/*  位时序计算 — 标称相位 (仲裁)                                        */
/* ================================================================== */

/**
 * @brief 自动计算 NBTP, 使实际 bitrate ≤ target_bitrate, 采样点接近 sp%
 *        全部使用 32-bit 运算, 不依赖 libgcc 64-bit 支持.
 * @return NBTP 寄存器值, 0 = 参数不可达
 */
static uint32_t Fdcan_CalcNBTP(uint32_t ker_ck_hz, uint32_t target_bitrate,
                                uint32_t sample_point_pct)
{
    /*
     * bit_time = Sync(1) + TSEG1 + TSEG2
     *          = 1 + (NTSEG1+1) + (NTSEG2+1) = NTSEG1 + NTSEG2 + 3 [tq]
     * tq = (NBRP+1) / ker_ck   →   total_tq = ker_ck / (bitrate × (NBRP+1))
     * total_tq 范围 [4, 81] (标称相位)
     */
    for (uint32_t nbrp = 0; nbrp <= 511; nbrp++) /* NBRP 9-bit, 0~511 */
    {
        uint32_t divisor = target_bitrate * (nbrp + 1);
        if (divisor == 0) continue;               /* 溢出保护 */

        uint32_t total_tq = ker_ck_hz / divisor;  /* 整数除, 向下取整 */
        if (total_tq < 4 || total_tq > 81)        /* 标称相位: 4~81 tq */
            continue;

        /* 余数四舍五入: rem × 2 ≥ divisor 则向上取整, 提高精度 */
        uint32_t rem = ker_ck_hz - total_tq * divisor;
        if (rem * 2 >= divisor)
            total_tq++;

        if (total_tq < 4 || total_tq > 81)
            continue;

        /* NTSEG1 + NTSEG2 = total_tq - 3 (去掉 1 Sync + 2 个隐含+1) */
        uint32_t seg_sum = total_tq - 3;

        /* NTSEG2 = 采样点之后部分 ≈ (1-sp%) × seg_sum, +50 四舍五入 */
        uint32_t ntseg2 = (seg_sum * (100 - sample_point_pct) + 50) / 100;
        if (ntseg2 < 1) ntseg2 = 1;               /* 至少 1 tq */
        if (ntseg2 > 127) ntseg2 = 127;           /* 7-bit 上限 */

        uint32_t ntseg1 = seg_sum - ntseg2;       /* 剩余部分 = TSEG1 - 1 */
        if (ntseg1 > 255 || ntseg1 < 1)           /* 8-bit 上限 */
            continue;

        /* SJW ≤ TSEG2, 直接取 ntseg2 (7-bit 上限) */
        uint32_t nsjw = ntseg2;
        if (nsjw > 127) nsjw = 127;

        return (nsjw  << FDCAN_NBTP_NSJW_Pos)  |
               (nbrp  << FDCAN_NBTP_NBRP_Pos)   |
               (ntseg1 << FDCAN_NBTP_NTSEG1_Pos) |
               (ntseg2 << FDCAN_NBTP_NTSEG2_Pos);
    }
    return 0;  /* 无解 */
}


/* ================================================================== */
/*  消息 RAM 辅助                                                       */
/* ================================================================== */

/*
 * DsizeDataWords[DSIZE] = 数据 word 数 (不含 2 头字)
 *   每元素总 word = 2 + DsizeDataWords[DSIZE]
 *   DSIZE 0(8B)→2  1(12B)→3  2(16B)→4  3(20B)→5
 *         4(24B)→6  5(32B)→8  6(48B)→12 7(64B)→16
 */
static const uint8_t DsizeDataWords[] = { 2, 3, 4, 5, 6, 8, 12, 16 };

/**
 * @brief  DSIZE → 每元素总 word 数 (2 头字 + 数据 word)
 * @param  dsize FDCAN_DSIZE 编码 (0~7)
 * @return word 数 [4, 18]
 */
static uint32_t FdcanElemWords(uint32_t dsize)
{
    return 2 + DsizeDataWords[dsize & 7];
}


/**
 * @brief FDCAN 初始化 — 位时序 + 消息 RAM 分配
 * @param fdcan      FDCAN1 或 FDCAN2
 * @param ker_ck_hz  内核时钟频率 (Hz)
 * @param bitrate    仲裁相位比特率 (bit/s)
 * @param data_bitrate 数据相位比特率 (CAN FD, 经典 CAN 传 0)
 * @param cfg        初始化配置 (RAM + 过滤)
 * @param mode       经典 CAN / CAN FD
 */
void FdcanInit(volatile FdcanRegs_t *fdcan, uint32_t ker_ck_hz,
               uint32_t bitrate, uint32_t data_bitrate,
               const FdcanCfg_t *cfg, FdcanMode_t mode)
{
    /* ---- 1. 进入配置模式 ---- */
    /* INIT=1: 停止 CAN 通信, 等待硬件确认 */
    fdcan->CCCR |= FDCAN_CCCR_INIT;
    while (!(fdcan->CCCR & FDCAN_CCCR_INIT));

    /* ---- 2. 解锁写保护 ---- */
    /* CCE=1: 允许写入 NBTP/DBTP/TXBC/RXF0C 等受保护寄存器 */
    fdcan->CCCR |= FDCAN_CCCR_CCE;

    /* ---- 3. 位时序 ---- */

    /* 标称相位: 采样点 87.5% (经典 CAN 推荐值, 兼顾抗干扰和采样裕量) */
    uint32_t nbtp = Fdcan_CalcNBTP(ker_ck_hz, bitrate, 875);
    if (nbtp == 0) {
        /* 算不出来时用保守值: NBRP=0, 15+3=18tq, 能跑通但波特率可能不准 */
        nbtp = (2U << FDCAN_NBTP_NSJW_Pos)   |
               (0U << FDCAN_NBTP_NBRP_Pos)   |
               (12U << FDCAN_NBTP_NTSEG1_Pos) |
               (2U << FDCAN_NBTP_NTSEG2_Pos);
    }
    fdcan->NBTP = nbtp;  /* 写入标称位时序 */

    /* 数据相位: 仅 CAN FD 需要, 采样点 80% (ISO 11898-1 推荐) */
    if (mode == FDCAN_MODE_FD) {
        uint32_t dbrp, dtseg1, dtseg2, dsjw;

        /* 复用 Fdcan_CalcNBTP 算数据相位参数 (算法相同, 只是字段位宽不同) */
        uint32_t dbtp = Fdcan_CalcNBTP(ker_ck_hz, data_bitrate, 800);

        if (dbtp) {
            /* 从 NBTP 格式中提取各字段值 */
            dbrp   = (dbtp & FDCAN_NBTP_NBRP_Msk)   >> FDCAN_NBTP_NBRP_Pos;
            dtseg1 = (dbtp & FDCAN_NBTP_NTSEG1_Msk) >> FDCAN_NBTP_NTSEG1_Pos;
            dtseg2 = (dbtp & FDCAN_NBTP_NTSEG2_Msk) >> FDCAN_NBTP_NTSEG2_Pos;
            dsjw   = (dbtp & FDCAN_NBTP_NSJW_Msk)   >> FDCAN_NBTP_NSJW_Pos;
        } else {
            /* 算不出来就用仲裁相位的值 (至少能跑) */
            uint32_t nbtp_val = nbtp;
            dbrp   = (nbtp_val & FDCAN_NBTP_NBRP_Msk)   >> FDCAN_NBTP_NBRP_Pos;
            dtseg1 = (nbtp_val & FDCAN_NBTP_NTSEG1_Msk) >> FDCAN_NBTP_NTSEG1_Pos;
            dtseg2 = (nbtp_val & FDCAN_NBTP_NTSEG2_Msk) >> FDCAN_NBTP_NTSEG2_Pos;
            dsjw   = (nbtp_val & FDCAN_NBTP_NSJW_Msk)   >> FDCAN_NBTP_NSJW_Pos;
        }

        /* DBTP: TDC=0(关闭) | DBRP | DTSEG1 | DTSEG2 | DSJW */
        fdcan->DBTP = (0U << 23)                      |
                      (dbrp  << FDCAN_DBTP_DBRP_Pos)   |
                      (dtseg1 << FDCAN_DBTP_DTSEG1_Pos) |
                      (dtseg2 << FDCAN_DBTP_DTSEG2_Pos) |
                      (dsjw  << FDCAN_DBTP_DSJW_Pos);
    }

    /* ---- 4. 消息 RAM 自动分配 (从 cfg->start_word 开始, 顺序排) ---- */
    uint32_t w = cfg->start_word;  /* 当前分配指针 (word 偏移) */

    /* -- Rx FIFO 0 -- */
    uint32_t rxf0_size = cfg->rxf0_elements;
    uint32_t rxf0_sa   = w;          /* 起始 SA */
    if (rxf0_size)
        w += rxf0_size * FdcanElemWords(cfg->rxf0_dsize); /* 推进指针 */

    /* RXF0C: F0OM(阻塞/覆盖) | F0WM(水位) | F0S(元素数) | F0SA(起始偏移) */
    fdcan->RXF0C = ((cfg->rxf0_mode ? 1U : 0U) << 31)     |
                   (cfg->rxf0_wm << FDCAN_RXF0C_F0WM_Pos)   |
                   (rxf0_size    << FDCAN_RXF0C_F0S_Pos)    |
                   (rxf0_sa      << FDCAN_RXF0C_F0SA_Pos);

    /* -- Rx FIFO 1 (同 FIFO 0) -- */
    uint32_t rxf1_size = cfg->rxf1_elements;
    uint32_t rxf1_sa   = w;
    if (rxf1_size)
        w += rxf1_size * FdcanElemWords(cfg->rxf1_dsize);

    fdcan->RXF1C = ((cfg->rxf1_mode ? 1U : 0U) << 31)     |
                   (cfg->rxf1_wm << FDCAN_RXF1C_F1WM_Pos)   |
                   (rxf1_size    << FDCAN_RXF1C_F1S_Pos)    |
                   (rxf1_sa      << FDCAN_RXF1C_F1SA_Pos);

    /* -- Rx Buffers (NDAT 机制, 不归 FIFO 管辖) -- */
    uint32_t rx_buf_cnt = cfg->rx_buffers;
    uint32_t rx_buf_sa  = w;
    if (rx_buf_cnt)
        w += rx_buf_cnt * FdcanElemWords(cfg->rx_dsize);

    fdcan->RXBC = (rx_buf_sa << FDCAN_RXBC_RBSA_Pos); /* 只有起始地址, 无独立计数 */

    /* -- Rx 元素大小: FIFO0 / FIFO1 / RxBuffer 三区独立 -- */
    fdcan->RXESC = (cfg->rxf0_dsize << FDCAN_RXESC_F0DS_Pos) |
                   (cfg->rxf1_dsize << FDCAN_RXESC_F1DS_Pos) |
                   (cfg->rx_dsize   << FDCAN_RXESC_RBDS_Pos);

    /* -- Tx: FIFO/Queue (tx_fifo) + dedicated (tx_dedicated) -- */
    uint32_t tx_total = cfg->tx_fifo_elements + cfg->tx_dedicated;
    uint32_t tx_sa    = w;
    if (tx_total)
        w += tx_total * FdcanElemWords(cfg->tx_dsize);

    /* TXBC: TFQM(FIFO/Queue) | TFQS(FIFO数) | NDTB(专用数) | TBSA(起始偏移) */
    fdcan->TXBC = ((cfg->tx_mode ? 1U : 0U) << 30)            |
                  (cfg->tx_fifo_elements << FDCAN_TXBC_TFQS_Pos) |
                  (cfg->tx_dedicated    << FDCAN_TXBC_NDTB_Pos)  |
                  (tx_sa                << FDCAN_TXBC_TBSA_Pos);

    fdcan->TXESC = cfg->tx_dsize;       /* Tx 元素数据大小 */

    /* -- 标准 ID 过滤列表: 每元素 1 word -- */
    uint32_t sid_sa  = w;
    uint32_t sid_cnt = cfg->std_filters;
    w += sid_cnt * 1;                   /* 1 word/element */

    /* SIDFC: LSS(元素数) | FLSSA(起始偏移) */
    fdcan->SIDFC = (sid_cnt << FDCAN_SIDFC_LSS_Pos) |
                   (sid_sa  << FDCAN_SIDFC_FLSSA_Pos);

    /* -- 扩展 ID 过滤列表: 每元素 2 words -- */
    uint32_t xid_sa  = w;
    uint32_t xid_cnt = cfg->ext_filters;
    w += xid_cnt * 2;                   /* 2 words/element */

    /* XIDFC: LSE(元素数) | FLESA(起始偏移) */
    fdcan->XIDFC = (xid_cnt << FDCAN_XIDFC_LSE_Pos) |
                   (xid_sa  << FDCAN_XIDFC_FLESA_Pos);

    /* -- Tx Event FIFO: 每元素固定 2 words -- */
    uint32_t tx_evt_cnt = cfg->tx_evt_elements;
    uint32_t tx_evt_sa  = w;
    if (tx_evt_cnt)
        w += tx_evt_cnt * 2;

    fdcan->TXEFC = (cfg->tx_evt_wm << FDCAN_TXEFC_EFWM_Pos) |
                   (tx_evt_cnt     << FDCAN_TXEFC_EFS_Pos)  |
                   (tx_evt_sa      << FDCAN_TXEFC_EFSA_Pos);

    /* -- 全局过滤: 未匹配帧 → Rx FIFO 0, 可选拒绝远程帧 -- */
    fdcan->GFC = (FDCAN_GFC_ACCEPT_F0 << FDCAN_GFC_ANFS_Pos) |
                 (FDCAN_GFC_ACCEPT_F0 << FDCAN_GFC_ANFE_Pos) |
                 (cfg->reject_remote ? (FDCAN_GFC_RRFS | FDCAN_GFC_RRFE) : 0U);

    /* XIDAM: 扩展 ID 掩码, 全 1 (0x1FFFFFFF) = 不屏蔽任何位 */
    fdcan->XIDAM = cfg->xidam;

    /* ---- 5. 工作模式 ---- */
    /* 经典 CAN 清 FDOE/BRSE/DAR, CAN FD 置 FDOE+BRSE */
    fdcan->CCCR &= ~(FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE | FDCAN_CCCR_DAR);
    if (mode == FDCAN_MODE_FD)
        fdcan->CCCR |= FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE;
    if (cfg->disable_auto_retrans)
        fdcan->CCCR |= FDCAN_CCCR_DAR;

    /* ---- 6. 退出配置模式, 硬件同步到 CAN 总线 ---- */
    fdcan->CCCR &= ~FDCAN_CCCR_INIT;       /* 清 INIT */
    while (fdcan->CCCR & FDCAN_CCCR_INIT); /* 等硬件确认: 同步完成后 INIT 回 0 */
}


/**
 * @brief  写 Tx 元素到 SRAM + 触发 TXBAR (FdcanSend/FdcanSendBuf 共用)
 * @param  fdcan  FDCAN 实例
 * @param  idx    绝对 buffer 索引
 * @param  id     CAN ID
 * @param  xtd    0=标准帧, 1=扩展帧
 * @param  data   数据缓冲区
 * @param  len    DLC
 * @param  fd     0=经典 CAN, 1=CAN FD (同时置 FDF + BRS)
 *
 *   Tx 元素格式 (per element), 参考 RM0436 Table 462:
 *     T0: ESI[31] | XTD[30] | RTR[29] | ID[28:0]
 *     T1: MM[31:24] | EFC[22] | FDF[21] | BRS[20] | DLC[19:16]
 *     数据从 T2 开始, 小端
 */
static void FdcanWriteTxElement(volatile FdcanRegs_t *fdcan, uint32_t idx,
                                uint32_t id, int xtd,
                                const uint8_t *data, uint32_t len,
                                int fd)
{
    /* Tx 元素在 SRAM 中的 word 偏移 = TBSA + idx × 每元素 word 数 */
    uint32_t tx_dsize  = fdcan->TXESC & FDCAN_TXESC_TBDS_Msk;
    uint32_t words_per = 2 + DsizeDataWords[tx_dsize & 7];

    uint32_t base = ((fdcan->TXBC & FDCAN_TXBC_TBSA_Msk) >> FDCAN_TXBC_TBSA_Pos)
                    + idx * words_per;

    volatile uint32_t *ram = FDCAN_SRAM;

    /* T0: XTD 在 bit[30], ID 在 bit[28:0]; ESI/RTR 保持 0 */
    uint32_t t0 = (xtd ? (1U << 30) : 0) | (id & 0x1FFFFFFFU);

    /* T1: MM=0|EFC=0|FDF|BRS|DLC=len */
    uint32_t t1 = (len & 0xFU) << 16;
    if (fd)  t1 |= (1U << 21) | (1U << 20);

    ram[base + 0] = t0;
    ram[base + 1] = t1;

    /*
     * 数据从 T2 (word[2]) 开始写入.
     * len = DLC (0~15), 实际字节数按 ISO 11898-1 CAN FD:
     *   ≤8 → len; 9→12; 10→16; 11→20; 12→24; 13→32; 14→48; 15→64
     */
    if (data && len) {
        uint32_t nbytes;
        if (len < 9)           nbytes = len;
        else if (len < 13)     nbytes = 8 + (len - 8) * 4;
        else if (len == 13)    nbytes = 32;
        else if (len == 14)    nbytes = 48;
        else                   nbytes = 64;

        memcpy((void *)&ram[base + 2], data, nbytes);
    }

    /* TXBAR[n]=1 → 请求发送 buffer n */
    fdcan->TXBAR = (1U << idx);
}


/**
 * @brief  Tx FIFO/Queue 发送 — 硬件管理 index, 取 TFQPI
 * @param  fdcan FDCAN 实例
 * @param  id    CAN ID
 * @param  xtd   0=标准帧, 1=扩展帧
 * @param  data  数据缓冲区
 * @param  len   DLC
 * @param  fdf   0=经典 CAN, 1=CAN FD 帧
 * @param  brs   0=不切换速率, 1=数据段切换 (仅 FDF=1 时有效)
 * @return 0=成功, -1=FIFO/Queue 满
 */
int FdcanSend(volatile FdcanRegs_t *fdcan, uint32_t id, int xtd,
              const uint8_t *data, uint32_t len, int fd)
{
    /* TFQF: Tx FIFO/Queue Full — 硬件置 1 表示满 */
    uint32_t txfqs = fdcan->TXFQS;
    if (txfqs & FDCAN_TXFQS_TFQF)
        return -1;

    /* TFQPI: 硬件维护的 put index, 0-based; 绝对 index = NDTB + TFQPI */
    uint32_t ndtb    = (fdcan->TXBC & FDCAN_TXBC_NDTB_Msk) >> FDCAN_TXBC_NDTB_Pos;
    uint32_t put_idx = (txfqs & FDCAN_TXFQS_TFQPI_Msk) >> FDCAN_TXFQS_TFQPI_Pos;
    FdcanWriteTxElement(fdcan, ndtb + put_idx, id, xtd, data, len, fd);
    return 0;
}


/**
 * @brief  专用 Tx Buffer 发送 — 调用方指定 buffer 索引
 * @param  fdcan   FDCAN 实例
 * @param  buf_idx 专用 buffer 索引 [0, NDTB)
 * @param  id      CAN ID
 * @param  xtd     0=标准帧, 1=扩展帧
 * @param  data    数据缓冲区
 * @param  len     DLC
 * @param  fdf     0=经典 CAN, 1=CAN FD 帧
 * @param  brs     0=不切换速率, 1=数据段切换 (仅 FDF=1 时有效)
 * @return 0=成功, -1=该 buffer 忙或越界
 */
int FdcanSendBuf(volatile FdcanRegs_t *fdcan, uint32_t buf_idx,
                 uint32_t id, int xtd, const uint8_t *data, uint32_t len,
                 int fd)
{
    /* NDTB: 专用 buffer 数量 */
    uint32_t ndtb = (fdcan->TXBC & FDCAN_TXBC_NDTB_Msk) >> FDCAN_TXBC_NDTB_Pos;
    if (buf_idx >= ndtb)
        return -1;

    /* TXBRP[n]=1 表示该 buffer 还在等待发送 */
    if (fdcan->TXBRP & (1U << buf_idx))
        return -1;

    /* 专用 buffer 就是绝对 index [0, NDTB) */
    FdcanWriteTxElement(fdcan, buf_idx, id, xtd, data, len, fd);
    return 0;
}


/**
 * @brief  读 Rx 元素头和数据 (FdcanReadFifo0/FdcanReadFifo1/FdcanReadBuf 共用)
 * @param  base  SRAM word 偏移 (需调用方提前计算)
 * @param  id    输出: CAN ID
 * @param  xtd   输出: 0=标准帧, 1=扩展帧
 * @param  data  输出: 数据缓冲区
 * @param  len   输出: DLC
 * @param  ts    输出: Rx 时间戳 (NULL=不需要)
 */
static void FdcanReadElement(uint32_t base, uint32_t *id, int *xtd,
                             uint8_t *data, uint32_t *len, uint16_t *ts)
{
    volatile uint32_t *ram = FDCAN_SRAM;
    uint32_t r0 = ram[base + 0];
    uint32_t r1 = ram[base + 1];

    if (id)   *id   = r0 & 0x1FFFFFFFU;
    if (xtd)  *xtd  = (r0 >> 30) & 1;
    if (len)  *len  = (r1 >> 16) & 0xFU;
    if (ts)   *ts   = (uint16_t)(r1 & 0xFFFFU);

    if (data) {
        uint32_t dlc = (r1 >> 16) & 0xFU;
        uint32_t nbytes;
        if (dlc < 9)           nbytes = dlc;
        else if (dlc < 13)     nbytes = 8 + (dlc - 8) * 4;
        else if (dlc == 13)    nbytes = 32;
        else if (dlc == 14)    nbytes = 48;
        else                   nbytes = 64;

        memcpy(data, (void *)&ram[base + 2], nbytes);
    }
}


/**
 * @brief 从 Rx FIFO 0 读一帧 (轮询)
 * @param fdcan  FDCAN 实例
 * @param id     输出: CAN ID
 * @param xtd    输出: 0=标准, 1=扩展
 * @param data   输出: 数据缓冲区 (至少 8 字节)
 * @param len    输出: DLC
 * @param ts     输出: Rx 时间戳 (NULL=不需要)
 * @return 0=读到一帧, -1=FIFO 空
 */
int FdcanReadFifo0(volatile FdcanRegs_t *fdcan, uint32_t *id, int *xtd,
                   uint8_t *data, uint32_t *len, uint16_t *ts)
{
    /* ---- 1. 查 FIFO 状态 ---- */
    uint32_t rxf0s = fdcan->RXF0S;

    /* F0FL: Rx FIFO 0 当前待读帧数 */
    uint32_t fill = (rxf0s & FDCAN_RXF0S_F0FL_Msk) >> FDCAN_RXF0S_F0FL_Pos;
    if (fill == 0)
        return -1;

    /* F0GI: 硬件指向下一个待读元素的 index (硬件自动维护) */
    uint32_t get_idx = (rxf0s & FDCAN_RXF0S_F0GI_Msk) >> FDCAN_RXF0S_F0GI_Pos;

    /* F0SA: Rx FIFO 0 在 SRAM 中的起始 word 偏移 */
    uint32_t f0sa = (fdcan->RXF0C & FDCAN_RXF0C_F0SA_Msk) >> FDCAN_RXF0C_F0SA_Pos;

    /* ---- 2. 计算该元素在 SRAM 中的地址 ---- */
    /* 每元素 word 数 = 2(头字 R0/R1) + 数据字, 由 RXESC.F0DS 决定 */
    uint32_t f0ds = (fdcan->RXESC & FDCAN_RXESC_F0DS_Msk) >> FDCAN_RXESC_F0DS_Pos;
    uint32_t words_per = 2 + DsizeDataWords[f0ds & 7];

    uint32_t base = f0sa + get_idx * words_per;

    /* ---- 3. 读元素 ---- */
    FdcanReadElement(base, id, xtd, data, len, ts);

    /* ---- 4. 确认读取 ---- */
    /* F0AI = get_idx → 硬件释放该元素, 自动推进 F0GI, F0FL-- */
    fdcan->RXF0A = get_idx & FDCAN_RXF0A_F0AI_Msk;

    return 0;
}


/* ================================================================== */
/*  接收 — 轮询 Rx FIFO 1 (同 FIFO 0, 不同寄存器)                      */
/* ================================================================== */

/**
 * @brief  从 Rx FIFO 1 读一帧 (轮询)
 * @param  fdcan FDCAN 实例
 * @param  id    输出: CAN ID
 * @param  xtd   输出: 0=标准帧, 1=扩展帧
 * @param  data  输出: 数据缓冲区
 * @param  len   输出: DLC
 * @param  ts    输出: Rx 时间戳 (NULL=不需要)
 * @return 0=读到一帧, -1=FIFO 空
 */
int FdcanReadFifo1(volatile FdcanRegs_t *fdcan, uint32_t *id, int *xtd,
                   uint8_t *data, uint32_t *len, uint16_t *ts)
{
    /* F1FL: FIFO 1 待读帧数 */
    uint32_t rxf1s = fdcan->RXF1S;
    uint32_t fill  = (rxf1s & FDCAN_RXF1S_F1FL_Msk) >> FDCAN_RXF1S_F1FL_Pos;
    if (fill == 0)
        return -1;

    /* F1GI: 硬件指向的下一个待读元素 */
    uint32_t get_idx = (rxf1s & FDCAN_RXF1S_F1GI_Msk) >> FDCAN_RXF1S_F1GI_Pos;

    /* F1SA: FIFO 1 在 SRAM 中的起始偏移 */
    uint32_t f1sa = (fdcan->RXF1C & FDCAN_RXF1C_F1SA_Msk) >> FDCAN_RXF1C_F1SA_Pos;

    uint32_t f1ds = (fdcan->RXESC & FDCAN_RXESC_F1DS_Msk) >> FDCAN_RXESC_F1DS_Pos;
    uint32_t words_per = 2 + DsizeDataWords[f1ds & 7];

    uint32_t base = f1sa + get_idx * words_per;

    FdcanReadElement(base, id, xtd, data, len, ts);

    /* F1AI = get_idx → 硬件释放元素, 推进 F1GI, F1FL-- */
    fdcan->RXF1A = get_idx & FDCAN_RXF1A_F1AI_Msk;

    return 0;
}


/* ================================================================== */
/*  接收 — Rx Buffer (NDAT 机制, 独立于 FIFO)                          */
/* ================================================================== */

/**
 * @brief  从 Rx Buffer (NDAT 机制) 读一帧
 * @param  fdcan   FDCAN 实例
 * @param  buf_idx Rx buffer 索引
 * @param  id      输出: CAN ID
 * @param  xtd     输出: 0=标准帧, 1=扩展帧
 * @param  data    输出: 数据缓冲区
 * @param  len     输出: DLC
 * @param  ts      输出: Rx 时间戳 (NULL=不需要)
 * @return 0=读到一帧, -1=无新数据
 *
 *   不同于 FIFO: 每个 buffer 独立, 硬件通过 NDAT 标记新数据.
 *     NDAT1[31:0] → buffer 0..31, bit=1 有新帧
 *     NDAT2[31:0] → buffer 32..63
 *   读完后写 NDAT 对应位清零.
 */
int FdcanReadBuf(volatile FdcanRegs_t *fdcan, uint32_t buf_idx,
                 uint32_t *id, int *xtd, uint8_t *data, uint32_t *len,
                 uint16_t *ts)
{
    /* 检查 NDAT: 该 buffer 是否有新数据 */
    uint32_t ndat;
    if (buf_idx < 32) {
        ndat = fdcan->NDAT1 >> buf_idx;
        if (!(ndat & 1)) return -1;
    } else {
        ndat = fdcan->NDAT2 >> (buf_idx - 32);
        if (!(ndat & 1)) return -1;
    }

    /* RBSA: Rx Buffer 在 SRAM 中的起始偏移 */
    uint32_t rbsa = (fdcan->RXBC & FDCAN_RXBC_RBSA_Msk) >> FDCAN_RXBC_RBSA_Pos;

    uint32_t rbds = (fdcan->RXESC & FDCAN_RXESC_RBDS_Msk) >> FDCAN_RXESC_RBDS_Pos;
    uint32_t words_per = 2 + DsizeDataWords[rbds & 7];

    uint32_t base = rbsa + buf_idx * words_per;

    FdcanReadElement(base, id, xtd, data, len, ts);

    /* 清 NDAT 位: 写对应 bit 清 1, 通知硬件已读完 */
    if (buf_idx < 32)
        fdcan->NDAT1 = (1U << buf_idx);
    else
        fdcan->NDAT2 = (1U << (buf_idx - 32));

    return 0;
}


/* ================================================================== */
/*  时间戳                                                               */
/* ================================================================== */

/**
 * @brief  配置时间戳: 分频 + 时钟源
 * @param  fdcan     FDCAN 实例
 * @param  prescaler TCP 预分频 (0~15, 实际分频 = prescaler + 1)
 * @param  source    时钟源: FDCAN_TSCC_TSS_ZERO / _INC / _EXT
 */
void FdcanTimestampInit(volatile FdcanRegs_t *fdcan,
                        uint32_t prescaler, uint32_t source)
{
    fdcan->TSCC = ((prescaler & 0xFU) << FDCAN_TSCC_TCP_Pos) |
                  (source & FDCAN_TSCC_TSS_Msk);
}

/**
 * @brief  读当前时间戳计数值
 * @param  fdcan FDCAN 实例
 * @return TSCV[15:0]
 */
uint16_t FdcanTimestampRead(volatile FdcanRegs_t *fdcan)
{
    return (uint16_t)(fdcan->TSCV & FDCAN_TSCV_TSC_Msk);
}


/* ================================================================== */
/*  Tx Event FIFO                                                       */
/* ================================================================== */

/**
 * @brief  从 Tx Event FIFO 读一条事件 (轮询)
 * @param  fdcan FDCAN 实例
 * @param  id    输出: CAN ID
 * @param  xtd   输出: 0=标准帧, 1=扩展帧
 * @param  mm    输出: Message Marker (NULL=不需要)
 * @param  ts    输出: Tx 时间戳 (NULL=不需要)
 * @param  et    输出: Event Type (NULL=不需要, 1=正常, 2=取消后仍发送)
 * @return 0=读到, -1=FIFO 空
 *
 *   Tx Event 元素格式 (固定 2 words):
 *     E0: ESI[31] | XTD[30] | RTR[29] | ID[28:0]
 *     E1: MM[31:24] | ET[23:22] | EDL[21] | BRS[20] | DLC[19:16] | TXTS[15:0]
 */
int FdcanTxEventRead(volatile FdcanRegs_t *fdcan, uint32_t *id, int *xtd,
                     uint8_t *mm, uint16_t *ts, uint8_t *et)
{
    uint32_t txefs = fdcan->TXEFS;
    uint32_t fill  = (txefs & FDCAN_TXEFS_EFFL_Msk) >> FDCAN_TXEFS_EFFL_Pos;
    if (fill == 0)
        return -1;

    uint32_t get_idx = (txefs & FDCAN_TXEFS_EFGI_Msk) >> FDCAN_TXEFS_EFGI_Pos;
    uint32_t efsa    = (fdcan->TXEFC & FDCAN_TXEFC_EFSA_Msk) >> FDCAN_TXEFC_EFSA_Pos;

    volatile uint32_t *ram = FDCAN_SRAM;
    uint32_t base = efsa + get_idx * 2;
    uint32_t e0 = ram[base + 0];
    uint32_t e1 = ram[base + 1];

    if (id)  *id  = e0 & 0x1FFFFFFFU;
    if (xtd) *xtd = (e0 >> 30) & 1;
    if (mm)  *mm  = (uint8_t)(e1 >> 24);          /* MM  在 bit[31:24] */
    if (ts)  *ts  = (uint16_t)(e1 & 0xFFFFU);     /* TXTS 在 bit[15:0]  */
    if (et)  *et  = (uint8_t)((e1 >> 22) & 3);    /* ET  在 bit[23:22] */

    /* 确认读取 */
    fdcan->TXEFA = get_idx & FDCAN_TXEFA_EFAI_Msk;

    return 0;
}


/* ================================================================== */
/*  过滤器                                                               */
/* ================================================================== */

/**
 * @brief  写标准 ID 过滤元素到 SRAM
 * @param  fdcan FDCAN 实例
 * @param  idx   过滤元素索引 [0, std_filters)
 * @param  sft   过滤类型 (Range / Dual / Classic)
 * @param  sfec  匹配动作 (FIFO0 / FIFO1 / Reject / RxBuf 等)
 * @param  sfid1 11-bit 标准 ID1 (或目标 ID)
 * @param  sfid2 11-bit 标准 ID2 (或掩码)
 *
 *   标准 ID 元素格式: SFT[31:30] | SFEC[29:27] | SFID1[26:16] | SFID2[10:0]
 */
void FdcanFilterStd(volatile FdcanRegs_t *fdcan, uint32_t idx,
                    FdcanSft_t sft, FdcanFilterAction_t sfec,
                    uint32_t sfid1, uint32_t sfid2)
{
    uint32_t sa = (fdcan->SIDFC & FDCAN_SIDFC_FLSSA_Msk) >> FDCAN_SIDFC_FLSSA_Pos;
    uint32_t val = ((uint32_t)sft   << 30) |
                   ((uint32_t)sfec  << 27) |
                   ((sfid1 & 0x7FFU) << 16) |
                   (sfid2  & 0x7FFU);
    FDCAN_SRAM[sa + idx] = val;
}


/**
 * @brief  写扩展 ID 过滤元素到 SRAM
 * @param  fdcan FDCAN 实例
 * @param  idx   过滤元素索引 [0, ext_filters)
 * @param  eft   过滤类型 (Range / Dual / Classic / RangeNoXidam)
 * @param  efec  匹配动作 (FIFO0 / FIFO1 / Reject / RxBuf 等)
 * @param  efid1 29-bit 扩展 ID1
 * @param  efid2 29-bit 扩展 ID2 (或掩码)
 *
 *   扩展 ID 元素格式 (2 words):
 *     E0: EFEC[31:29] | EFID1[28:0]
 *     E1: EFT[31:30]  | EFID2[28:0]
 */
void FdcanFilterExt(volatile FdcanRegs_t *fdcan, uint32_t idx,
                    FdcanEft_t eft, FdcanFilterAction_t efec,
                    uint32_t efid1, uint32_t efid2)
{
    uint32_t sa = (fdcan->XIDFC & FDCAN_XIDFC_FLESA_Msk) >> FDCAN_XIDFC_FLESA_Pos;
    uint32_t base = sa + idx * 2;
    FDCAN_SRAM[base + 0] = ((uint32_t)efec << 29) | (efid1 & 0x1FFFFFFFU);
    FDCAN_SRAM[base + 1] = ((uint32_t)eft  << 30) | (efid2 & 0x1FFFFFFFU);
}


/* ================================================================== */
/*  模式切换 / 调试辅助                                                  */
/* ================================================================== */

/**
 * @brief  进入回环模式 (TEST.LBCK), 自发自收, 不需要 CAN 总线
 * @param  fdcan FDCAN 实例
 */
void FdcanLoopbackEnable(volatile FdcanRegs_t *fdcan)
{
    fdcan->CCCR |= FDCAN_CCCR_INIT;
    while (!(fdcan->CCCR & FDCAN_CCCR_INIT));
    fdcan->CCCR |= FDCAN_CCCR_CCE;

    fdcan->CCCR |= FDCAN_CCCR_TEST;
    fdcan->TEST  = FDCAN_TEST_LBCK;

    fdcan->CCCR &= ~FDCAN_CCCR_INIT;
    while (fdcan->CCCR & FDCAN_CCCR_INIT);
}

/**
 * @brief  退出回环模式
 * @param  fdcan FDCAN 实例
 */
void FdcanLoopbackDisable(volatile FdcanRegs_t *fdcan)
{
    fdcan->CCCR |= FDCAN_CCCR_INIT;
    while (!(fdcan->CCCR & FDCAN_CCCR_INIT));
    fdcan->CCCR |= FDCAN_CCCR_CCE;

    fdcan->TEST  = 0;
    fdcan->CCCR &= ~FDCAN_CCCR_TEST;

    fdcan->CCCR &= ~FDCAN_CCCR_INIT;
    while (fdcan->CCCR & FDCAN_CCCR_INIT);
}
