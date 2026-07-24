/*
 * fdcan_test.c — FDCAN 测试
 */

#include <string.h>
#include "stm32mp1xx_fdcan.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gic.h"
#include "common_test.h"


/* ================================================================== */
/*  测试配置                                                              */
/* ================================================================== */

#define FDCAN_KER_CK_HZ  24000000U


/* ================================================================== */
/*  辅助函数                                                              */
/* ================================================================== */

/*
 * DLC → 实际字节数
 */
static const uint8_t dlc_bytes[] = { 0,1,2,3,4,5,6,7,8, 12,16,20,24, 32,48,64 };

static void PrintRxFrame(const FdcanRxFrame_t *f)
{
    char buf[16];

    if (f->r0.B.xtd) PRINT("ext "); else PRINT("std ");
    PRINT("0x");
    PrintHex32(buf, f->r0.B.xtd ? f->r0.B.id : (f->r0.B.id >> 18));
    PRINT(buf);
    PRINT("  L=");
    NumToStr(buf, f->r1.B.dlc);
    PRINT(buf);
    if (f->r1.B.anmf)
        PRINT("  ANMF");
    else {
        PRINT("  Fidx=");
        NumToStr(buf, f->r1.B.fidx);
        PRINT(buf);
    }
    PRINT("  ");
    {
        uint32_t n = dlc_bytes[f->r1.B.dlc & 0xFU];
        for (uint32_t j = 0; j < n && j < 8; j++) {
            PrintHex8(f->data[j]);
            PRINT(" ");
        }
    }
    PRINT("\r\n");
}

/*
 * FDCAN1 GPIO 初始化 — 正常模式需要,
 * 回环模式不需外部引脚, 可不调此函数.
 *   TX: PH13, AF9
 *   RX: PI9,  AF9
 */
static void Fdcan1GpioInit(void)
{
    RCC->MP_AHB4ENSETR |= (1U << 7) | (1U << 8);  /* GPIOH(bit7), GPIOI(bit8) clock enable */

    /* TX: PH13, AF9, push-pull, high speed, no pull */
    GpioMode(GPIO_H, 13, GPIO_MODER_AF);
    GpioOtype(GPIO_H, 13, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_H, 13, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_H, 13, GPIO_PUPDR_NO);
    GpioAf(GPIO_H, 13, 9);

    /* RX: PI9, AF9, high speed, pull-up */
    GpioMode(GPIO_I, 9, GPIO_MODER_AF);
    GpioOspeed(GPIO_I, 9, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_I, 9, GPIO_PUPDR_PULL_UP);
    GpioAf(GPIO_I, 9, 9);
}


/* ================================================================== */
/*  测试 1: 经典 CAN 回环                                                      */
/* ================================================================== */

static const FdcanCfg_t g_cfg_loop_can = {
    .start_word     = 0,
    .rxf0_elements  = 8,
    .rxf0_dsize     = FDCAN_DSIZE_8,
    .tx_fifo_elements = 4,
    .tx_dsize       = FDCAN_DSIZE_8,
    .xidam          = FDCAN_XIDAM_DISABLED,
    .std_filters    = 1,
    .anfs           = FDCAN_GFC_REJECT,
    .anfe           = FDCAN_GFC_REJECT,
};

static void TestClassicCanLoopback(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- Classic CAN Loopback ---\r\n");

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanLoopbackEnable(fdcan);
    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 125000, 0, &g_cfg_loop_can, FDCAN_MODE_CAN);
    FdcanFilterStd(fdcan, 0, FDCAN_SFT_RANGE, FDCAN_FEC_FIFO0, 0x000, 0x7FF);
    PRINT("Init done (125 kbit/s)\r\n");

    static const struct {
        uint32_t id;
        uint8_t  data[8];
    } frames[] = {
        { 0x100, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 } },
        { 0x200, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 } },
        { 0x300, { 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88 } },
        { 0x400, { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 } },
    };
    int n = sizeof(frames) / sizeof(frames[0]);

    for (int i = 0; i < n; i++)
        FdcanSend(fdcan, frames[i].id, 0, frames[i].data, 8, 0);
    PRINT("Sent 4 frames\r\n");

    int pass = 1;
    for (int i = 0; i < n; i++) {
        FdcanRxFrame_t f;
        uint32_t timeout = 5000000;

        while (FdcanReadFifo0(fdcan, &f) != 0
               && --timeout);

        if (timeout == 0) {
            PRINT("FAIL: timeout on frame ");
            char buf[4];
            NumToStr(buf, i + 1);
            PRINT(buf);
            PRINT("\r\n");
            pass = 0;
            break;
        }

        uint32_t id = f.r0.B.xtd ? f.r0.B.id : (f.r0.B.id >> 18);
        if (id != frames[i].id || f.r1.B.dlc != 8
            || memcmp(f.data, frames[i].data, 8) != 0) {
            pass = 0;
        }
    }

    if (pass) PRINT("PASS\r\n");
    else      PRINT("FAIL\r\n");

    FdcanLoopbackDisable(fdcan);
    PRINT("--- Test done ---\r\n");
}


/* ================================================================== */
/*  测试 2: CAN FD 回环                                                   */
/* ================================================================== */

static void TestCanFdLoopback(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- CAN FD Loopback ---\r\n");

    static const FdcanCfg_t fd_cfg = {
        .start_word     = 0,
        .rxf0_elements  = 8,
        .rxf0_dsize     = FDCAN_DSIZE_64,
        .tx_fifo_elements = 8,
        .tx_dsize       = FDCAN_DSIZE_64,
        .ext_filters    = 1,
        .xidam          = FDCAN_XIDAM_DISABLED,
        .anfs           = FDCAN_GFC_REJECT,
        .anfe           = FDCAN_GFC_REJECT,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanLoopbackEnable(fdcan);
    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 500000, 500000, &fd_cfg, FDCAN_MODE_FD);
    FdcanFilterExt(fdcan, 0, FDCAN_EFT_RANGE, FDCAN_FEC_FIFO0, 0x00000000, 0x1FFFFFFF);
    PRINT("Init done (500k/500k bit/s)\r\n");

    static struct {
        uint32_t id;
        uint32_t dlc;
        uint8_t  data[64];
    } frames[] = {
        { 0x10001234,  9 },
        { 0x10005678, 10 },
        { 0x10009ABC, 12 },
        { 0x1000DEF0, 13 },
        { 0x1000AAAA, 14 },
        { 0x1000BBBB, 15 },
    };
    int n = sizeof(frames) / sizeof(frames[0]);

    for (int i = 0; i < n; i++)
        memset(frames[i].data, 0xA0 + i, dlc_bytes[frames[i].dlc]);

    for (int i = 0; i < n; i++)
        FdcanSend(fdcan, frames[i].id, 1, frames[i].data, frames[i].dlc, 1);
    PRINT("Sent frames\r\n");

    int pass = 1;
    for (int i = 0; i < n; i++) {
        uint32_t nbytes = dlc_bytes[frames[i].dlc];
        FdcanRxFrame_t f;
        uint32_t timeout = 5000000;

        while (FdcanReadFifo0(fdcan, &f) != 0
               && --timeout);

        if (timeout == 0) {
            PRINT("FAIL: timeout on frame ");
            char buf[4];
            NumToStr(buf, i + 1);
            PRINT(buf);
            PRINT("\r\n");
            pass = 0;
            break;
        }

        uint32_t id = f.r0.B.xtd ? f.r0.B.id : (f.r0.B.id >> 18);
        if (id != frames[i].id || f.r1.B.dlc != frames[i].dlc || !f.r0.B.xtd
            || memcmp(f.data, frames[i].data, nbytes) != 0) {
            pass = 0;
        }
    }

    if (pass) PRINT("PASS\r\n");
    else      PRINT("FAIL\r\n");

    FdcanLoopbackDisable(fdcan);
    PRINT("--- Test done ---\r\n");
}


/* ================================================================== */
/*  测试 3: 过滤组合测试 — 验证 filter SRAM + 接收确认 (需 PC USB-CAN)     */
/*         覆盖: 标准(Range/Dual/Classic/Reject/RxBuf ×5)                */
/*               扩展(Range/Dual/Classic/Reject/RxBuf ×5)                */
/*               GFC(ANFS=REJECT, ANFE=FIFO1)                          */
/* ================================================================== */

static void TestFilterCombo(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- Filter Combo Test ---\r\n");
    Fdcan1GpioInit();

    /*
     * 过滤规划:
     *   Std 0: Range   0x100~0x1FF       → FIFO0   (范围匹配-接收)
     *   Std 1: Dual    0x200 / 0x201      → FIFO0   (双ID匹配-接收)
     *   Std 2: Classic 0x300 / 0x7F0      → FIFO0   (经典掩码 0x300~0x30F)
     *   Std 3: Range   0x500~0x5FF       → REJECT  (范围匹配-拒收)
     *   Std 4: ID=0x400                        → RxBuf0  (单ID-专用缓冲, SFEC=7 时 SFT 无效)
     *   ANFS = REJECT                            (未匹配标准帧全部拒绝)
     *
     *   Ext 0: Range   0x10000000~0x100000FF → FIFO0
     *   Ext 1: Dual    0x10000100/0x10000101  → FIFO0
     *   Ext 2: Classic 0x10000200/0x1FFFFFF0  → FIFO0   (掩码: 低4位不关心)
     *   Ext 3: Range   0x1FFF0000~0x1FFFFFFF → REJECT
     *   Ext 4: ID=0x10001000                   → RxBuf1  (单ID-专用缓冲)
     *   ANFE = FIFO1                                   (未匹配扩展帧→FIFO1)
     */
    static const FdcanCfg_t cfg = {
        .start_word     = 0,
        .rxf0_elements  = 8,
        .rxf0_dsize     = FDCAN_DSIZE_8,
        .rxf1_elements  = 4,
        .rxf1_dsize     = FDCAN_DSIZE_8,
        .rx_buffers     = 2,
        .rx_dsize       = FDCAN_DSIZE_8,
        .std_filters    = 5,
        .ext_filters    = 5,
        .xidam          = FDCAN_XIDAM_DISABLED,
        .anfs           = FDCAN_GFC_REJECT,
        .anfe           = FDCAN_GFC_ACCEPT_F1,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 500000, 0, &cfg, FDCAN_MODE_CAN);
    PRINT("Init done (500 kbit/s)\r\n");

    /* ---- 标准帧过滤 ---- */
    FdcanFilterStd(fdcan, 0, FDCAN_SFT_RANGE,   FDCAN_FEC_FIFO0,  0x100, 0x1FF);
    FdcanFilterStd(fdcan, 1, FDCAN_SFT_DUAL,    FDCAN_FEC_FIFO0,  0x200, 0x201);
    FdcanFilterStd(fdcan, 2, FDCAN_SFT_CLASSIC, FDCAN_FEC_FIFO0,  0x300, 0x7F0);
    FdcanFilterStd(fdcan, 3, FDCAN_SFT_RANGE,   FDCAN_FEC_REJECT, 0x500, 0x5FF);
    FdcanFilterStdRxBuf(fdcan, 4, 0x400, 0);

    /* ---- 扩展帧过滤 ---- */
    FdcanFilterExt(fdcan, 0, FDCAN_EFT_RANGE,   FDCAN_FEC_FIFO0,  0x10000000, 0x100000FF);
    FdcanFilterExt(fdcan, 1, FDCAN_EFT_DUAL,    FDCAN_FEC_FIFO0,  0x10000100, 0x10000101);
    FdcanFilterExt(fdcan, 2, FDCAN_EFT_CLASSIC, FDCAN_FEC_FIFO0,  0x10000200, 0x1FFFFFF0);
    FdcanFilterExt(fdcan, 3, FDCAN_EFT_RANGE,   FDCAN_FEC_REJECT, 0x1FFF0000, 0x1FFFFFFF);
    FdcanFilterExtRxBuf(fdcan, 4, 0x10001000, 1);

    /* ---- 过滤配置摘要 ---- */
    PRINT("\r\n=== Std Filters ===\r\n");
    PRINT(" F0: 0x100~0x1FF       → FIFO0   (Range)\r\n");
    PRINT(" F1: 0x200, 0x201       → FIFO0   (Dual)\r\n");
    PRINT(" F2: 0x300/0x7F0        → FIFO0   (Classic: 0x300~0x30F)\r\n");
    PRINT(" F3: 0x500~0x5FF        → REJECT\r\n");
    PRINT(" F4: 0x400              → RxBuf0  (single ID)\r\n");
    PRINT("    ANFS=REJECT\r\n");

    PRINT("\r\n=== Ext Filters ===\r\n");
    PRINT(" F0: 0x10000000~0x100000FF → FIFO0   (Range)\r\n");
    PRINT(" F1: 0x10000100,0x10000101 → FIFO0   (Dual)\r\n");
    PRINT(" F2: 0x10000200/0x1FFFFFF0 → FIFO0   (Classic: 0x10000200~F)\r\n");
    PRINT(" F3: 0x1FFF0000~0x1FFFFFFF → REJECT\r\n");
    PRINT(" F4: 0x10001000            → RxBuf1  (single ID)\r\n");
    PRINT("    ANFE=FIFO1\r\n");

    PRINT("\r\n[F0]=FIFO0 [F1]=FIFO1 [RB]=RxBuf  |  press key to stop\r\n\r\n");

    /* ---- 实时监控循环 ---- */
    while (!FramePoll(0))
    {
        IwdgKickDog(IWDG2);

        FdcanRxFrame_t f;
        int buf_idx;

        if (FdcanReadFifo0(fdcan, &f) == 0) {
            PRINT("[F0] ");
            PrintRxFrame(&f);
        } else if (FdcanReadFifo1(fdcan, &f) == 0) {
            PRINT("[F1] ");
            PrintRxFrame(&f);
        } else {
            for (buf_idx = 0; buf_idx < 2; buf_idx++) {
                if (FdcanReadBuf(fdcan, buf_idx, &f) == 0) {
                    PRINT("[RB");
                    char b[4];
                    NumToStr(b, buf_idx);
                    PRINT(b);
                    PRINT("] ");
                    PrintRxFrame(&f);
                    break;
                }
            }
        }
    }

    PRINT("Stopped.\r\n");
}


/* ================================================================== */
/*  测试 4: 专用 Tx Buffer 发送 (正常模式, DAR)                          */
/* ================================================================== */

static void TestSendBuf(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- SendBuf (Normal) ---\r\n");
    Fdcan1GpioInit();

    static const FdcanCfg_t cfg = {
        .start_word      = 0,
        .tx_dedicated    = 4,
        .tx_dsize        = FDCAN_DSIZE_8,
        .tx_evt_elements = 4,
        .std_filters     = 0,
        .ext_filters     = 0,
        .xidam           = FDCAN_XIDAM_DISABLED,
        .anfs            = FDCAN_GFC_REJECT,
        .anfe            = FDCAN_GFC_REJECT,
        .disable_auto_retrans = 1,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 125000, 0, &cfg, FDCAN_MODE_CAN);
    FdcanTimestampInit(fdcan, 0, FDCAN_TSCC_TSS_INC);
    PRINT("Init done (4 dedicated Tx, DAR=1)\r\n");

    static const uint8_t data[8] = {0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0x00,0x01};

    for (int i = 0; i < 4; i++) {
        if (FdcanSendBuf(fdcan, i, 0x100 + i, 0, data, 8, 0) != 0) {
            PRINT("FAIL: SendBuf busy at idx ");
            char buf[4];
            NumToStr(buf, i);
            PRINT(buf);
            PRINT("\r\n");
            break;
        }
    }
    PRINT("Sent 4 via SendBuf\r\n");

    /* 等 TXBTO */
    {
        uint32_t timeout = 5000000;
        uint32_t txbto;
        do {
            txbto = fdcan->TXBTO;
        } while (txbto != 0x0FU && --timeout);

        if (timeout == 0)
            PRINT("TXBTO timeout (no ACK)\r\n");
        else
            PRINT("TXBTO=0x0F OK\r\n");
    }

    /* 读 Tx Event FIFO */
    {
        int evt = 0;
        for (int i = 0; i < 4; i++) {
            FdcanTxEvent_t e;
            if (FdcanTxEventRead(fdcan, &e) != 0)
                break;
            evt++;

            char buf[16];
            PRINT("[TxEvt ");
            char b[4];
            NumToStr(b, i);
            PRINT(b);
            PRINT("] ");
            if (e.r0.B.xtd) PRINT("ext "); else PRINT("std ");
            PRINT("0x");
            PrintHex32(buf, e.r0.B.xtd ? e.r0.B.id : (e.r0.B.id >> 18));
            PRINT(buf);
            PRINT("  ET=");
            NumToStr(b, e.r1.B.et);
            PRINT(b);
            PRINT("  MM=");
            NumToStr(b, e.r1.B.mm);
            PRINT(b);
            PRINT("  TXTS=");
            NumToStr(buf, e.r1.B.txts);
            PRINT(buf);
            PRINT("\r\n");
        }

        if (evt == 4) {
            PRINT("Tx Event FIFO: 4 events OK\r\n");
            PRINT("PASS\r\n");
        } else {
            PRINT("Tx Event FIFO: ");
            char buf[4];
            NumToStr(buf, evt);
            PRINT(buf);
            PRINT(" events\r\n");
        }
    }

    PRINT("--- Test done ---\r\n");
}


/* ================================================================== */
/*  测试 5: 持续发送, 串口任意键停止, 64-bit 递增计数器         */
/* ================================================================== */

static void TestContinuousSend(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- Continuous Send ---\r\n");
    Fdcan1GpioInit();

    static const FdcanCfg_t cfg = {
        .start_word      = 0,
        .tx_fifo_elements = 4,
        .tx_dsize        = FDCAN_DSIZE_8,
        .tx_evt_elements = 4,
        .std_filters     = 0,
        .ext_filters     = 0,
        .xidam           = FDCAN_XIDAM_DISABLED,
        .anfs            = FDCAN_GFC_REJECT,
        .anfe            = FDCAN_GFC_REJECT,
        .disable_auto_retrans = 1,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 500000, 0, &cfg, FDCAN_MODE_CAN);
    PRINT("Init done (125 kbit/s, DAR=1)\r\n");
    PRINT("Sending... press any key to stop.\r\n");

    uint32_t counter_lo = 0;
    uint32_t counter_hi = 0;
    uint32_t sent = 0;

    while (!FramePoll(0))
    {
        IwdgKickDog(IWDG2);

        uint8_t data[8];
        /* 64-bit counter 拆成两个 uint32_t, 避免 yagarto 工具链的 64-bit 问题 */
        data[0] = (uint8_t)(counter_lo >> 0);
        data[1] = (uint8_t)(counter_lo >> 8);
        data[2] = (uint8_t)(counter_lo >> 16);
        data[3] = (uint8_t)(counter_lo >> 24);
        data[4] = (uint8_t)(counter_hi >> 0);
        data[5] = (uint8_t)(counter_hi >> 8);
        data[6] = (uint8_t)(counter_hi >> 16);
        data[7] = (uint8_t)(counter_hi >> 24);

        if (FdcanSend(fdcan, 0x100, 0, data, 8, 0) != 0)
        {
            /* FIFO full - wait for TXBTO */
            uint32_t to = 5000000;
            while ((fdcan->TXBTO & 0xFU) != 0xFU && --to);
            if (to == 0) break;
            continue;
        }

        sent++;
        /* 手动 64-bit 递增 (counter_hi:counter_lo) */
        if (++counter_lo == 0)
            counter_hi++;

        /* 等当前帧发送完成再发下一帧 */
        {
            uint32_t to = 5000000;
            while ((fdcan->TXBTO & 1) == 0 && --to);
        }
    }

    char buf[16];
    PRINT("\r\nStopped. Sent ");
    NumToStr(buf, sent);
    PRINT(buf);
    PRINT(" frames\r\n");

    uint32_t psr = fdcan->PSR;
    uint32_t lec = (psr & FDCAN_PSR_LEC_Msk) >> FDCAN_PSR_LEC_Pos;
    PRINT("LEC=");
    NumToStr(buf, lec);
    PRINT(buf);
    PRINT("\r\n--- Test done ---\r\n");
}


/* ================================================================== */
/*  测试 6: CAN 监控 + 周期发送, 时间戳回绕, 统计                        */
/* ================================================================== */

static void TestCanMonitor(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- CAN Monitor + Periodic Send ---\r\n");
    Fdcan1GpioInit();

    static const FdcanCfg_t cfg = {
        .start_word      = 0,
        .rxf0_elements   = 32,
        .rxf0_dsize      = FDCAN_DSIZE_8,
        .tx_fifo_elements = 4,
        .tx_dsize        = FDCAN_DSIZE_8,
        .tx_evt_elements = 4,
        .std_filters     = 1,
        .ext_filters     = 1,
        .xidam           = FDCAN_XIDAM_DISABLED,
        .anfs            = FDCAN_GFC_ACCEPT_F0,
        .anfe            = FDCAN_GFC_ACCEPT_F0,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 500000, 0, &cfg, FDCAN_MODE_CAN);
    FdcanFilterStd(fdcan, 0, FDCAN_SFT_RANGE, FDCAN_FEC_FIFO0, 0x000, 0x7FF);
    FdcanFilterExt(fdcan, 0, FDCAN_EFT_RANGE, FDCAN_FEC_FIFO0, 0x00000000, 0x1FFFFFFF);
    /* prescaler=15: 1 tick = 16 bit time = 32us @500k, 16-bit wrap ≈2.1s */
    FdcanTimestampInit(fdcan, 15, FDCAN_TSCC_TSS_INC);
    PRINT("Init done (500 kbit/s, prescaler=15)\r\n\r\n");

    uint16_t prev_ts = FdcanTimestampRead(fdcan);
    uint32_t ts_wraps = 0;

    uint32_t rx_total = 0;
    uint32_t tx_count = 0;

    uint32_t last_send_ticks  = 0;
    uint32_t last_print_ticks = 0;

    PRINT("press any key to stop\r\n\r\n");

    while (!FramePoll(0))
    {
        IwdgKickDog(IWDG2);

        /* ---- time (wrap-safe) ---- */
        uint16_t cur_ts = FdcanTimestampRead(fdcan);
        if (cur_ts < prev_ts) ts_wraps++;
        prev_ts = cur_ts;
        uint32_t ticks = (ts_wraps << 16) | cur_ts;

        /* ---- receive + print ---- */
        {
            FdcanRxFrame_t f;
            while (FdcanReadFifo0(fdcan, &f) == 0) {
                rx_total++;
                PrintRxFrame(&f);
            }
        }

        /* ---- periodic send (every 1s = 31250 ticks) ---- */
        if (ticks - last_send_ticks >= 31250) {
            last_send_ticks = ticks;
            uint8_t data[8];
            data[0] = (uint8_t)(tx_count >> 0);
            data[1] = (uint8_t)(tx_count >> 8);
            data[2] = (uint8_t)(tx_count >> 16);
            data[3] = (uint8_t)(tx_count >> 24);
            data[4] = (uint8_t)(cur_ts >> 0);
            data[5] = (uint8_t)(cur_ts >> 8);
            data[6] = 0;
            data[7] = 0;
            if (FdcanSend(fdcan, 0x7E0, 0, data, 8, 0) == 0)
                tx_count++;
        }

        /* ---- periodic print (every 1s) ---- */
        if (ticks - last_print_ticks >= 31250) {
            last_print_ticks = ticks;

            uint32_t ecr = FdcanEcr(fdcan);
            uint32_t tec = (ecr & FDCAN_ECR_TEC_Msk) >> FDCAN_ECR_TEC_Pos;
            uint32_t rec = (ecr & FDCAN_ECR_REC_Msk) >> FDCAN_ECR_REC_Pos;
            uint32_t sec = ticks / 31250;

            char buf[16];
            PRINT("RX=");
            NumToStr(buf, rx_total);
            PRINT(buf);
            PRINT("  TX=");
            NumToStr(buf, tx_count);
            PRINT(buf);
            PRINT("  TEC=");
            NumToStr(buf, tec);
            PRINT(buf);
            PRINT("  REC=");
            NumToStr(buf, rec);
            PRINT(buf);
            PRINT("  s=");
            NumToStr(buf, sec);
            PRINT(buf);
            PRINT("\r\n");
        }
    }

    /* final */
    uint16_t end_ts = FdcanTimestampRead(fdcan);
    if (end_ts < prev_ts) ts_wraps++;
    uint32_t final_ticks = (ts_wraps << 16) | end_ts;

    PRINT("\r\n--- Final Stats ---\r\n");
    char buf[16];
    PRINT("Elapsed: ");
    NumToStr(buf, final_ticks / 31250);
    PRINT(buf);
    PRINT(" s  RX: ");
    NumToStr(buf, rx_total);
    PRINT(buf);
    PRINT("  TX: ");
    NumToStr(buf, tx_count);
    PRINT(buf);
    PRINT("\r\n--- Test done ---\r\n");
}


/* ================================================================== */
/*  通用 IRQ 上下文 + 核心 (FDCAN1/2 共用)                              */
/*  ISR 先读 IR, 按位分发处理, 不复用之前各自循环判断方式                 */
/* ================================================================== */

#define IRQ_RING_SIZE 64

typedef struct {
    uint8_t  source;   /* 0=FIFO0, 1=FIFO1, 2=RxBuf */
    uint8_t  buf_idx;  /* valid when source==2 */
    uint16_t rxts;     /* RXTS 硬件时间戳 */
    FdcanRxFrame_t frame;
} IrqRxEntry_t;

typedef struct {
    volatile FdcanRegs_t *fdcan;   /* 实例指针               */
    IrqRxEntry_t  ring[IRQ_RING_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t rx[3];       /* FIFO0 / FIFO1 / RxBuf  */
    volatile uint32_t tx_evt;      /* Tx Event 计数          */
    volatile uint32_t ts_wraps;    /* TSW 中断计数            */
    volatile uint32_t overrun;
    volatile FdcanTxEvent_t last_evt;  /* 最新 Tx Event 副本  */
} FdcanIrqCtx_t;

static FdcanIrqCtx_t irq_ctx1;
static FdcanIrqCtx_t irq_ctx2;

/* 通用中断核心: 读 IR→按位分发 → 清 */
static void FdcanIrqCore(FdcanIrqCtx_t *ctx)
{
    volatile FdcanRegs_t *fdcan = ctx->fdcan;
    uint32_t ir = FdcanIrqGet(fdcan);
    uint32_t next;

    if (ir & FDCAN_IR_RF0N) {
        FdcanRxFrame_t f;
        while (FdcanReadFifo0(fdcan, &f) == 0) {
            next = (ctx->head + 1) % IRQ_RING_SIZE;
            if (next == ctx->tail) { ctx->overrun++; break; }
            ctx->ring[ctx->head].frame  = f;
            ctx->ring[ctx->head].source = 0;
            ctx->ring[ctx->head].rxts   = f.r1.B.rxts;
            ctx->head = next;
            ctx->rx[0]++;
        }
    }

    if (ir & FDCAN_IR_RF1N) {
        FdcanRxFrame_t f;
        while (FdcanReadFifo1(fdcan, &f) == 0) {
            next = (ctx->head + 1) % IRQ_RING_SIZE;
            if (next == ctx->tail) { ctx->overrun++; break; }
            ctx->ring[ctx->head].frame  = f;
            ctx->ring[ctx->head].source = 1;
            ctx->ring[ctx->head].rxts   = f.r1.B.rxts;
            ctx->head = next;
            ctx->rx[1]++;
        }
    }

    if (ir & FDCAN_IR_DRX) {
        uint32_t ndat = FdcanNdat1(fdcan);
        for (uint32_t i = 0; i < 4 && ndat; i++) {
            if (ndat & (1U << i)) {
                FdcanRxFrame_t f;
                if (FdcanReadBuf(fdcan, i, &f) == 0) {
                    next = (ctx->head + 1) % IRQ_RING_SIZE;
                    if (next == ctx->tail) { ctx->overrun++; break; }
                    ctx->ring[ctx->head].frame   = f;
                    ctx->ring[ctx->head].source  = 2;
                    ctx->ring[ctx->head].buf_idx = (uint8_t)i;
                    ctx->ring[ctx->head].rxts    = f.r1.B.rxts;
                    ctx->head = next;
                    ctx->rx[2]++;
                }
                ndat &= ~(1U << i);
            }
        }
    }

    if (ir & FDCAN_IR_TEFN) {
        FdcanTxEvent_t e;
        while (FdcanTxEventRead(fdcan, &e) == 0) {
            ctx->last_evt = e;
            ctx->tx_evt++;
        }
    }

    /* TSW 在 Rx 之后处理, 避免帧赋到回绕后的计数值 */
    if (ir & FDCAN_IR_TSW) {
        ctx->ts_wraps++;
    }

    /* 所有 IR 标志都是 w1c, 统一清已处理的 */
    FdcanIrqClear(fdcan, ir);
}

static void Fdcan1IrqHandler(void) { FdcanIrqCore(&irq_ctx1); }
static void Fdcan2IrqHandler(void) { FdcanIrqCore(&irq_ctx2); }

static void FdcanIrqGicConfig(void)
{
    GicdSetGroup(GIC_FDCAN1_IT0);
    GicdSetPriority(GIC_FDCAN1_IT0, 5);
    GicdSetTarget(GIC_FDCAN1_IT0, 1);
    GicdSetTrigMode(GIC_FDCAN1_IT0, 0);
    GicRegisterIrq(GIC_FDCAN1_IT0, Fdcan1IrqHandler);
    GicdEnableInt(GIC_FDCAN1_IT0);
    GicdInit();
    GiccInit(10, 2);
    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :
        :
        : "r0"
    );
}

static void FdcanIrqGicCleanup(void)
{
    GicdDisableInt(GIC_FDCAN1_IT0);
}


/* ================================================================== */
/*  测试 7: IRQ 版 CAN 监控 + 周期发送 (同测试 6 行为, 中断实现)         */
/*         RX/TX 均打印绝对时间戳 [sec.mmm]                             */
/* ================================================================== */

static void PrintTimestamp(uint32_t ticks)
{
    char buf[16];
    uint32_t sec = ticks / 31250;
    uint32_t ms  = ((ticks % 31250) * 32) / 1000;
    NumToStr(buf, sec); PRINT(buf);
    PRINT(".");
    if (ms < 10)      { PRINT("00"); }
    else if (ms < 100){ PRINT("0");  }
    NumToStr(buf, ms); PRINT(buf);
}

static void TestIrqReceive(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;
    FdcanIrqCtx_t *ctx = &irq_ctx1;

    PRINT("\r\n--- IRQ Monitor + Periodic Send ---\r\n");
    Fdcan1GpioInit();

    memset(ctx, 0, sizeof(*ctx));
    ctx->fdcan = fdcan;

    static const FdcanCfg_t cfg = {
        .start_word      = 0,
        .rxf0_elements   = 32,
        .rxf0_dsize      = FDCAN_DSIZE_8,
        .rx_buffers      = 4,
        .rx_dsize        = FDCAN_DSIZE_8,
        .tx_fifo_elements = 4,
        .tx_dsize        = FDCAN_DSIZE_8,
        .tx_evt_elements = 4,
        .std_filters     = 5,
        .ext_filters     = 1,
        .xidam           = FDCAN_XIDAM_DISABLED,
        .anfs            = FDCAN_GFC_ACCEPT_F0,
        .anfe            = FDCAN_GFC_ACCEPT_F0,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 500000, 0, &cfg, FDCAN_MODE_CAN);

    FdcanEnterCfg(fdcan);
    /* 3 个专用 ID 在 range 之前 → 优先匹配 Rx Buffer */
    FdcanFilterStdRxBuf(fdcan, 0, 0x100, 0);
    FdcanFilterStdRxBuf(fdcan, 1, 0x200, 1);
    FdcanFilterStdRxBuf(fdcan, 2, 0x300, 2);
    /* range 兜底 → FIFO0 */
    FdcanFilterStd(fdcan, 3, FDCAN_SFT_RANGE, FDCAN_FEC_FIFO0, 0x000, 0x7FF);
    /* 专用 ID 在 range 之后 → 永不被匹配 (range 先命中了) */
    FdcanFilterStdRxBuf(fdcan, 4, 0x400, 3);
    FdcanFilterExt(fdcan, 0, FDCAN_EFT_RANGE, FDCAN_FEC_FIFO0, 0x00000000, 0x1FFFFFFF);
    FdcanExitCfg(fdcan);

    FdcanTimestampInit(fdcan, 15, FDCAN_TSCC_TSS_INC);
    PRINT("Init done (500 kbit/s, prescaler=15)\r\n");
    PRINT("Filter: 0x100/0x200/0x300→RxBuf | range→FIFO0 | 0x400→RxBuf[dead]\r\n");

    FdcanIrqLineEnable(fdcan, FDCAN_ILE_EINT0);
    FdcanIrqEnable(fdcan, FDCAN_IE_RF0NE | FDCAN_IE_TEFNE | FDCAN_IE_DRXE | FDCAN_IE_TSWE);
    FdcanIrqGicConfig();
    PRINT("IRQ enabled (int0: RF0NE TEFNE DRXE TSWE)\r\n\r\n");

    uint32_t last_send_ticks  = 0;
    uint32_t tx_count = 0;
    uint32_t rx_total = 0;
    uint32_t prev_tx_evt = 0;

    PRINT("press any key to stop\r\n\r\n");

    while (!FramePoll(0))
    {
        IwdgKickDog(IWDG2);

        /* ---- time ---- */
        uint16_t cur_ts = FdcanTimestampRead(fdcan);
        uint32_t ticks = (ctx->ts_wraps << 16) | cur_ts;

        /* ---- consume ring buffer + print with timestamp ---- */
        while (ctx->tail != ctx->head)
        {
            IrqRxEntry_t *e = &ctx->ring[ctx->tail];
            rx_total++;

            uint32_t abs_ticks = (ctx->ts_wraps << 16) | e->rxts;
            PRINT("[");
            PrintTimestamp(abs_ticks);
            PRINT("] ");
            PrintRxFrame(&e->frame);

            ctx->tail = (ctx->tail + 1) % IRQ_RING_SIZE;
        }

        /* ---- Tx Event arrived (ISR 已放入 last_evt) ---- */
        if (ctx->tx_evt != prev_tx_evt) {
            prev_tx_evt = ctx->tx_evt;
            FdcanTxEvent_t *evt = (FdcanTxEvent_t *)&ctx->last_evt;
            uint32_t abs_ticks = (ctx->ts_wraps << 16) | evt->r1.B.txts;
            char b[16];
            PRINT("[");
            PrintTimestamp(abs_ticks);
            PRINT("] TX std 0x");
            PrintHex32(b, evt->r0.B.xtd ? evt->r0.B.id : (evt->r0.B.id >> 18));
            PRINT(b);
            PRINT("  MM=");
            NumToStr(b, evt->r1.B.mm); PRINT(b);
            PRINT("  ET=");
            NumToStr(b, evt->r1.B.et); PRINT(b);
            PRINT("  #");
            NumToStr(b, tx_count); PRINT(b);
            PRINT("\r\n");
        }

        /* ---- periodic send (every 1s = 31250 ticks) ---- */
        if (ticks - last_send_ticks >= 31250) {
            last_send_ticks = ticks;
            uint8_t data[8];
            data[0] = (uint8_t)(tx_count >> 0);
            data[1] = (uint8_t)(tx_count >> 8);
            data[2] = (uint8_t)(tx_count >> 16);
            data[3] = (uint8_t)(tx_count >> 24);
            data[4] = (uint8_t)(cur_ts >> 0);
            data[5] = (uint8_t)(cur_ts >> 8);
            data[6] = 0;
            data[7] = 0;
            if (FdcanSend(fdcan, 0x7E0, 0, data, 8, 0) == 0)
                tx_count++;
        }
    }

    /* ---- cleanup ---- */
    FdcanIrqDisable(fdcan, FDCAN_IE_RF0NE | FDCAN_IE_TEFNE | FDCAN_IE_DRXE | FDCAN_IE_TSWE);
    FdcanIrqLineDisable(fdcan);
    FdcanIrqGicCleanup();

    /* ---- final stats ---- */
    uint16_t end_ts = FdcanTimestampRead(fdcan);
    uint32_t final_ticks = (ctx->ts_wraps << 16) | end_ts;

    PRINT("\r\n--- Final Stats ---\r\n");
    char buf[16];
    PRINT("Elapsed: ");
    NumToStr(buf, final_ticks / 31250); PRINT(buf);
    PRINT(" s  RX: ");
    NumToStr(buf, rx_total); PRINT(buf);
    PRINT("  TX: ");
    NumToStr(buf, tx_count); PRINT(buf);
    if (ctx->overrun) {
        PRINT("  OVR: ");
        NumToStr(buf, ctx->overrun); PRINT(buf);
    }
    PRINT("\r\n--- Test done ---\r\n");
}


/* ================================================================== */
/*  测试 8: CAN FD 发送 (1M/3M, DAR=1, 串口停止)                         */
/* ================================================================== */

static void TestCanFdSend(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- CAN FD Send (1M/3M) ---\r\n");
    Fdcan1GpioInit();

    static const FdcanCfg_t cfg = {
        .start_word      = 0,
        .tx_fifo_elements = 4,
        .tx_dsize        = FDCAN_DSIZE_64,
        .tx_evt_elements = 4,
        .std_filters     = 0,
        .ext_filters     = 0,
        .xidam           = FDCAN_XIDAM_DISABLED,
        .anfs            = FDCAN_GFC_REJECT,
        .anfe            = FDCAN_GFC_REJECT,
        .disable_auto_retrans = 1,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 1000000, 3000000, &cfg, FDCAN_MODE_FD);
    PRINT("Init done (1M/3M bit/s, DLC15=64B, DAR=1)\r\n");
    PRINT("Sending FD frames... press any key to stop.\r\n");

    uint32_t counter_lo = 0;
    uint32_t counter_hi = 0;
    uint32_t sent = 0;

    while (!FramePoll(0))
    {
        IwdgKickDog(IWDG2);

        static const uint8_t dlc_codes[] = { 8, 9, 10, 11, 12, 13, 14, 15 };
        static const uint8_t dlc_bytes[] = { 8, 12, 16, 20, 24, 32, 48, 64 };
        uint32_t dlc_idx  = (sent & 7);
        uint32_t dlc_code = dlc_codes[dlc_idx];
        uint32_t data_len = dlc_bytes[dlc_idx];

        uint8_t data[64];
        for (uint32_t i = 0; i < 8; i++)
            data[i] = (uint8_t)(counter_lo >> (i * 8));
        for (uint32_t i = 0; i < 8 && (8 + i) < data_len; i++)
            data[8 + i] = (uint8_t)(counter_hi >> (i * 8));

        if (FdcanSend(fdcan, 0x100, 0, data, dlc_code, 1) != 0)
        {
            uint32_t to = 5000000;
            while ((fdcan->TXBTO & 0xFU) != 0xFU && --to);
            if (to == 0) break;
            continue;
        }

        sent++;
        if (++counter_lo == 0)
            counter_hi++;

        {
            uint32_t to = 5000000;
            while ((fdcan->TXBTO & 1) == 0 && --to);
        }
    }

    char buf[16];
    PRINT("\r\nStopped. Sent ");
    NumToStr(buf, sent);
    PRINT(buf);
    PRINT(" frames\r\n");

    uint32_t psr = fdcan->PSR;
    uint32_t lec = (psr & FDCAN_PSR_LEC_Msk) >> FDCAN_PSR_LEC_Pos;
    PRINT("LEC=");
    NumToStr(buf, lec);
    PRINT(buf);
    PRINT("\r\n--- Test done ---\r\n");
}


/* ================================================================== */
/*  菜单                                                                */
/* ================================================================== */

static const MenuEntry_t fdcan_menu[] = {
    { "1", "Classic CAN loopback",  TestClassicCanLoopback },
    { "2", "CAN FD loopback",       TestCanFdLoopback      },
    { "3", "Filter combo (PC assist)", TestFilterCombo },
    { "4", "SendBuf send",          TestSendBuf    },
    { "5", "Continuous send (key to stop)", TestContinuousSend },
    { "6", "CAN Monitor + periodic send",  TestCanMonitor },
    { "7", "IRQ receive",           TestIrqReceive },
    { "8", "CAN FD send 1M/3M",    TestCanFdSend },
};

void FdcanTest(void)
{
    RunSubMenu("FDCAN Test", fdcan_menu, sizeof(fdcan_menu) / sizeof(fdcan_menu[0]));
}
