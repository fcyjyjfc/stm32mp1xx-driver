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

#define FDCAN_KER_CK_HZ  64000000U

static const FdcanCfg_t g_cfg = {
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


/* ================================================================== */
/*  测试项                                                               */
/* ================================================================== */

static void TestClassicCan(void)
{
    volatile FdcanRegs_t *fdcan = FDCAN1;

    PRINT("\r\n--- Classic CAN Loopback ---\r\n");

    RCC->MP_APB2ENSETR |= 1 << 24;

    static const char * const src_name[] = { "HSE", "PLL3_Q", "PLL4_Q", "PLL4_R" };
    uint32_t src = RCC->FDCANCKSELR & 3;
    PRINT("FDCAN clk source: ");
    PRINT(src_name[src]);
    PRINT("\r\n");

    FdcanLoopbackEnable(fdcan);
    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 125000, 0, &g_cfg, FDCAN_MODE_CAN);
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

    /* 发送 */
    for (int i = 0; i < n; i++)
        FdcanSend(fdcan, frames[i].id, 0, frames[i].data, 8, 0);
    PRINT("Sent 4 frames\r\n");

    /* 接收并验证 */
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

static void TestCanFd(void)
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

    FdcanLoopbackEnable(fdcan);
    FdcanInit(fdcan, FDCAN_KER_CK_HZ, 1000000, 2000000, &fd_cfg, FDCAN_MODE_FD);
    FdcanFilterExt(fdcan, 0, FDCAN_EFT_RANGE, FDCAN_FEC_FIFO0, 0x00000000, 0x1FFFFFFF);
    PRINT("Init done (1M/2M bit/s)\r\n");

    static const uint8_t dlc_bytes[] = { 0,1,2,3,4,5,6,7,8, 12,16,20,24, 32,48,64 };

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
/*  菜单                                                                */
/* ================================================================== */

static const MenuEntry_t fdcan_menu[] = {
    { "1", "Classic CAN loopback", TestClassicCan },
    { "2", "CAN FD loopback",      TestCanFd      },
};

void FdcanTest(void)
{
    RunSubMenu("FDCAN Test", fdcan_menu, sizeof(fdcan_menu) / sizeof(fdcan_menu[0]));
}
