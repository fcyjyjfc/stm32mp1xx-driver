/*
 * fdcan_test.c — FDCAN 测试
 */

#include <string.h>
#include "stm32mp1xx_fdcan.h"
#include "stm32mp1xx_rcc.h"
#include "common_test.h"


/* ================================================================== */
/*  测试配置                                                              */
/* ================================================================== */

#define FDCAN_KER_CK_HZ  12000000U


/* ================================================================== */
/*  辅助函数                                                              */
/* ================================================================== */

/*
 * DLC → 实际字节数
 */
static const uint8_t dlc_bytes[] = { 0,1,2,3,4,5,6,7,8, 12,16,20,24, 32,48,64 };


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
        uint32_t id, len;
        int xtd;
        uint8_t rx[8];
        uint16_t ts;
        uint32_t timeout = 5000000;

        while (FdcanReadFifo0(fdcan, &id, &xtd, rx, &len, &ts) != 0
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

        if (id != frames[i].id || len != 8
            || memcmp(rx, frames[i].data, 8) != 0) {
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
        uint32_t id, len, nbytes = dlc_bytes[frames[i].dlc];
        int xtd;
        uint8_t rx[64];
        uint16_t ts;
        uint32_t timeout = 5000000;

        while (FdcanReadFifo0(fdcan, &id, &xtd, rx, &len, &ts) != 0
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

        if (id != frames[i].id || len != frames[i].dlc || !xtd
            || memcmp(rx, frames[i].data, nbytes) != 0) {
            pass = 0;
        }
    }

    if (pass) PRINT("PASS\r\n");
    else      PRINT("FAIL\r\n");

    FdcanLoopbackDisable(fdcan);
    PRINT("--- Test done ---\r\n");
}


/* ================================================================== */
/*  测试 3: 过滤 + 发送 (正常模式, 无回环, DAR)                            */
/*         验证: filter SRAM 写入 / Tx FIFO 发送 / TXBTO / Tx Event FIFO */
/* ================================================================== */

static void TestFilterSend(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- Filter + Send (Normal) ---\r\n");

    /*
     * 配置: 4 条标准帧过滤 + 4 条扩展帧过滤 + Tx FIFO 4 + Tx Event FIFO 4
     * DAR=1: 无 ACK 时只发一次, 避免无限重传
     */
    static const FdcanCfg_t cfg = {
        .start_word       = 0,
        .rxf0_elements    = 8,
        .rxf0_dsize       = FDCAN_DSIZE_8,
        .tx_fifo_elements = 4,
        .tx_dsize         = FDCAN_DSIZE_8,
        .tx_evt_elements  = 4,
        .std_filters      = 4,
        .ext_filters      = 4,
        .xidam            = FDCAN_XIDAM_DISABLED,
        .anfs             = FDCAN_GFC_REJECT,
        .anfe             = FDCAN_GFC_REJECT,
        .disable_auto_retrans = 1,
    };

    RCC->MP_APB2ENSETR |= 1 << 24;
    RCC->FDCANCKSELR = 0;

    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 125000, 0, &cfg, FDCAN_MODE_CAN);
    PRINT("Init done (125 kbit/s, DAR=1)\r\n");

    /* ---- 配置标准帧过滤 ---- */
    FdcanFilterStd(fdcan, 0, FDCAN_SFT_RANGE,   FDCAN_FEC_FIFO0, 0x100, 0x1FF);
    FdcanFilterStd(fdcan, 1, FDCAN_SFT_DUAL,    FDCAN_FEC_FIFO0, 0x200, 0x201);
    FdcanFilterStd(fdcan, 2, FDCAN_SFT_CLASSIC, FDCAN_FEC_FIFO0, 0x300, 0x7F0);
    FdcanFilterStd(fdcan, 3, FDCAN_SFT_RANGE,   FDCAN_FEC_REJECT, 0x500, 0x5FF);

    /* ---- 配置扩展帧过滤 ---- */
    FdcanFilterExt(fdcan, 0, FDCAN_EFT_RANGE,         FDCAN_FEC_FIFO0, 0x10000000, 0x100000FF);
    FdcanFilterExt(fdcan, 1, FDCAN_EFT_DUAL,          FDCAN_FEC_FIFO0, 0x10000100, 0x10000101);
    FdcanFilterExt(fdcan, 2, FDCAN_EFT_CLASSIC,       FDCAN_FEC_FIFO0, 0x10000200, 0x1FFFFFF0);
    FdcanFilterExt(fdcan, 3, FDCAN_EFT_RANGE_NO_XIDAM, FDCAN_FEC_REJECT, 0x1FFF0000, 0x1FFFFFFF);

    PRINT("Filters configured: 4 std + 4 ext\r\n");

    /* ---- 验证过滤 SRAM 内容 ---- */
    uint32_t sid_sa = (fdcan->SIDFC & FDCAN_SIDFC_FLSSA_Msk) >> FDCAN_SIDFC_FLSSA_Pos;
    uint32_t xid_sa = (fdcan->XIDFC & FDCAN_XIDFC_FLESA_Msk) >> FDCAN_XIDFC_FLESA_Pos;

    int filter_ok = 1;

    /* 标准帧 0: SFT=Range(0) SFEC=FIFO0(1) SFID1=0x100 SFID2=0x1FF */
    uint32_t s0 = FDCAN_SRAM[sid_sa + 0];
    if (s0 != (0x100U << 16 | 0x1FF)) filter_ok = 0;

    /* 标准帧 1: SFT=Dual(1) SFEC=FIFO0(1) SFID1=0x200 SFID2=0x201 */
    uint32_t s1 = FDCAN_SRAM[sid_sa + 1];
    if (s1 != ((1U << 30) | (1U << 27) | (0x200U << 16) | 0x201)) filter_ok = 0;

    /* 标准帧 2: SFT=Classic(2) SFEC=FIFO0(1) SFID1=0x300 SFID2=0x7F0 */
    uint32_t s2 = FDCAN_SRAM[sid_sa + 2];
    if (s2 != ((2U << 30) | (1U << 27) | (0x300U << 16) | 0x7F0)) filter_ok = 0;

    /* 标准帧 3: SFT=Range(0) SFEC=Reject(3) SFID1=0x500 SFID2=0x5FF */
    uint32_t s3 = FDCAN_SRAM[sid_sa + 3];
    if (s3 != ((3U << 27) | (0x500U << 16) | 0x5FF)) filter_ok = 0;

    /* 扩展帧 0: EFEC=FIFO0(1) EFID1=0x10000000 / EFT=Range(0) EFID2=0x100000FF */
    uint32_t e0_0 = FDCAN_SRAM[xid_sa + 0];
    uint32_t e0_1 = FDCAN_SRAM[xid_sa + 1];
    if (e0_0 != ((1U << 29) | 0x10000000) || e0_1 != 0x100000FF) filter_ok = 0;

    /* 扩展帧 3: EFEC=Reject(3) EFID1=0x1FFF0000 / EFT=RangeNoXidam(3) EFID2=0x1FFFFFFF */
    uint32_t e3_0 = FDCAN_SRAM[xid_sa + 6];
    uint32_t e3_1 = FDCAN_SRAM[xid_sa + 7];
    if (e3_0 != ((3U << 29) | 0x1FFF0000) || e3_1 != ((3U << 30) | 0x1FFFFFFF)) filter_ok = 0;

    if (filter_ok)
        PRINT("Filter SRAM verify OK\r\n");
    else
        PRINT("Filter SRAM verify FAIL\r\n");

    /* ---- 发送标准帧 (匹配过滤 0~2 和一条不匹配的) ---- */
    static const uint8_t data[8] = {0xA1,0xB2,0xC3,0xD4,0xE5,0xF6,0x07,0x18};

    uint32_t tx_ids[] = { 0x150, 0x200, 0x304, 0x600 };
    for (int i = 0; i < 4; i++) {
        if (FdcanSend(fdcan, tx_ids[i], 0, data, 8, 0) != 0) {
            PRINT("FAIL: Tx FIFO full at frame ");
            char buf[4];
            NumToStr(buf, i);
            PRINT(buf);
            PRINT("\r\n");
            break;
        }
    }
    PRINT("Sent 4 std frames\r\n");

    /* ---- 等待 TXBTO (DAR=1, 单次发送后即置位) ---- */
    {
        uint32_t timeout = 5000000;
        uint32_t txbto;
        do {
            txbto = fdcan->TXBTO;
        } while (txbto != 0x0FU && --timeout);

        if (timeout == 0)
            PRINT("TXBTO timeout (no ACK on bus)\r\n");
        else
            PRINT("TXBTO=0x0F OK\r\n");
    }

    /* ---- 读 Tx Event FIFO ---- */
    {
        int evt = 0;
        for (int i = 0; i < 4; i++) {
            uint32_t eid;
            int extd;
            uint8_t mm, et;
            uint16_t ts;
            if (FdcanTxEventRead(fdcan, &eid, &extd, &mm, &ts, &et) == 0)
                evt++;
        }

        if (evt == 4) {
            PRINT("Tx Event FIFO: 4 events OK\r\n");
            PRINT("PASS\r\n");
        } else {
            PRINT("Tx Event FIFO: ");
            char buf[4];
            NumToStr(buf, evt);
            PRINT(buf);
            PRINT(" events (no ACK on bus)\r\n");
        }
    }

    /* ---- 检查 PSR: 确认模块状态 ---- */
    {
        uint32_t psr = fdcan->PSR;
        uint32_t act = (psr & FDCAN_PSR_ACT_Msk) >> FDCAN_PSR_ACT_Pos;
        uint32_t lec = (psr & FDCAN_PSR_LEC_Msk) >> FDCAN_PSR_LEC_Pos;
        uint32_t ecr = fdcan->ECR;
        uint32_t tec = (ecr & FDCAN_ECR_TEC_Msk) >> FDCAN_ECR_TEC_Pos;
        uint32_t rec = (ecr & FDCAN_ECR_REC_Msk) >> FDCAN_ECR_REC_Pos;

        PRINT("PSR ACT=");
        char buf[4];
        NumToStr(buf, act);
        PRINT(buf);
        PRINT(" LEC=");
        NumToStr(buf, lec);
        PRINT(buf);
        PRINT(" TEC=");
        NumToStr(buf, tec);
        PRINT(buf);
        PRINT(" REC=");
        NumToStr(buf, rec);
        PRINT(buf);
        PRINT("\r\n");
    }

    PRINT("--- Test done ---\r\n");
}


/* ================================================================== */
/*  测试 4: 专用 Tx Buffer 发送 (正常模式, DAR)                          */
/* ================================================================== */

static void TestSendBuf(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- SendBuf (Normal) ---\r\n");

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
            uint32_t eid;
            int extd;
            uint8_t mm, et;
            uint16_t ts;
            if (FdcanTxEventRead(fdcan, &eid, &extd, &mm, &ts, &et) == 0)
                evt++;
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
/*  菜单                                                                */
/* ================================================================== */

static const MenuEntry_t fdcan_menu[] = {
    { "1", "Classic CAN loopback",  TestClassicCanLoopback },
    { "2", "CAN FD loopback",       TestCanFdLoopback      },
    { "3", "Filter + Tx FIFO send", TestFilterSend },
    { "4", "SendBuf send",          TestSendBuf    },
};

void FdcanTest(void)
{
    RunSubMenu("FDCAN Test", fdcan_menu, sizeof(fdcan_menu) / sizeof(fdcan_menu[0]));
}
