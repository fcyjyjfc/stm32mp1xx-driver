/*
 * emmc_test.c — eMMC 协议层驱动测试框架
 *
 *  Created on: 2026-6-28
 *      Author: gjsbr
 */

#include <string.h>
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_rcc.h"
#include "emmc_card.h"
#include "test_common.h"

static EmmcCard_t g_emmc;
/* 多块测试用缓冲区: 128 块 × 512B × 2 (写+读) = 128KB */
static uint8_t g_buf[128 * 512 * 2] __attribute__((aligned(4)));
#define WR_BUF  (g_buf)                /* 写缓冲区 0..65535 (64 KB) */
#define RD_BUF  (g_buf + 65536)        /* 读缓冲区 65536..131071 (64 KB) */


/** 打印 eMMC 卡信息 (CID/CSD/EXT_CSD/CMD13) */
static void PrintCardInfo(EmmcCard_t *card)
{
    char hex[12], dec[16];

    PRINT("  Product: ");
    PRINT(card->product);
    PRINT(" v");
    PrintDec(dec, card->prv_major);
    PRINT(dec);
    PRINT(".");
    PrintDec(dec, card->prv_minor);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  OEM: ");
    PRINT(card->oem_id);
    PRINT("  MFR: 0x");
    PrintHex8(card->mfr_id);
    PRINT("\r\n");

    PRINT("  Serial: 0x");
    PrintHex32(hex, card->serial);
    PRINT(hex);
    PRINT("\r\n");

    PRINT("  Date: ");
    PrintDec(dec, card->mfr_year);
    PRINT(dec);
    PRINT("/");
    PrintDec(dec, card->mfr_month);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  EXT_CSD Rev: ");
    PrintDec(dec, card->ext_csd_rev);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  Capacity: ");
    PrintDec(dec, (int32_t)card->capacity_mb);
    PRINT(dec);
    PRINT(" MB\r\n");

    PRINT("  Max Clock: ");
    PrintDec(dec, (int32_t)(card->max_clk_hz / 1000000));
    PRINT(dec);
    PRINT(" MHz\r\n");

    PRINT("  RCA: 0x");
    PrintHex8((uint8_t)(card->rca >> 8));
    PrintHex8((uint8_t)card->rca);
    PRINT("\r\n");

    /* ---- EXT_CSD 详细信息 ---- */
    {
        uint8_t *e = card->ext_csd;
        uint32_t cache_sz = (uint32_t)e[252] | ((uint32_t)e[251] << 8)
                          | ((uint32_t)e[250] << 16) | ((uint32_t)e[249] << 24);
        PRINT("\r\n--- EXT_CSD ---\r\n");
        PRINT("  DEVICE_TYPE[196]: 0x");
        PrintHex8(e[196]);
        PRINT(" (");
        if (e[196] & 0x01) PRINT("HS ");
        if (e[196] & 0x02) PRINT("HS52 ");
        if (e[196] & 0x04) PRINT("DDR52 ");
        if (e[196] & 0x08) PRINT("HS200 ");
        if (e[196] & 0x10) PRINT("HS400 ");
        PRINT(")\r\n");
        PRINT("  HS_TIMING[185]: ");
        PrintDec(dec, e[185]);
        PRINT(dec);
        PRINT(" (");
        PRINT(e[185] == 0 ? "BC" : e[185] == 1 ? "HS" : "?");
        PRINT(")\r\n");
        PRINT("  BUS_WIDTH[183]: ");
        PrintDec(dec, e[183]);
        PRINT(dec);
        PRINT(" (");
        PRINT(e[183] == 0 ? "1-bit" : e[183] == 1 ? "4-bit" : e[183] == 2 ? "8-bit" : "?");
        PRINT(")\r\n");
        PRINT("  PARTITION_CONFIG[179]: 0x");
        PrintHex8(e[179]);
        PRINT("  BOOT_INFO[228]: 0x");
        PrintHex8(e[228]);
        PRINT("\r\n");
        PRINT("  BOOT_SIZE[226]: ");
        PrintDec(dec, e[226]);
        PRINT(dec);
        PRINT(" x128KB\r\n");
        PRINT("  RPMB_SIZE[168]: ");
        PrintDec(dec, e[168]);
        PRINT(dec);
        PRINT(" x128KB\r\n");
        PRINT("  ERASE_GRP_SIZE[175+176]: ");
        PrintDec(dec, e[175]);
        PRINT(dec);
        PRINT(" x ");
        PrintDec(dec, e[176]);
        PRINT(dec);
        PRINT(" x 512\r\n");
        PRINT("  CACHE_SIZE[252:249]: ");
        PrintDec(dec, cache_sz);
        PRINT(dec);
        PRINT(" bytes\r\n");
        PRINT("  POWER_CLASS[187]: ");
        PrintDec(dec, e[187]);
        PRINT(dec);
        PRINT("\r\n");
    }

    /* ---- CMD13 查卡状态 ---- */
    {
        EmmcCardStatus_t sts;
        if (EmmcCardSendStatus(card, &sts) == EMMC_OK)
        {
            static const char *state_names[] = {
                "Idle","Ready","Ident","Stby","Tran","Data","Rcv","Pgm",
                "Dis","Btst","Slp"};
            PRINT("\r\n--- CMD13 Status ---\r\n");
            PRINT("  State: ");
            if (sts.state < sizeof(state_names)/sizeof(state_names[0]))
                PRINT(state_names[sts.state]);
            PRINT(" (");
            PrintDec(dec, sts.state);
            PRINT(dec);
            PRINT(")\r\n");
            PRINT("  ReadyForData: ");
            PRINT(sts.ready_for_data ? "1" : "0");
            PRINT("  Cmd: ");
            PRINT(sts.app_cmd ? "1" : "0");
            PRINT("\r\n");
            if (sts.switch_error) PRINT("  SWITCH_ERROR\r\n");
            if (sts.ake_seq_error) PRINT("  AKE_SEQ_ERROR\r\n");
            if (sts.wp_violation) PRINT("  WP_VIOLATION\r\n");
        }
    }
}


/** @brief eMMC 硬件初始化: SdmmcPowerOn → 卡识别/选卡 → 速度切换 → 读信息 */
static int EmmcHwInit(void)
{
    char dec[16];

    PRINT("\r\n===== eMMC Init =====\r\n");

    g_emmc.sdmmc = SDMMC2;
    IwdgKickDog(IWDG2);

    SdmmcPowerOn(g_emmc.sdmmc, 200000000);
    g_emmc.ker_ck_hz = 200000000;

    EmmcCardErr_t err = EmmcCardIdentify(&g_emmc);
    if (err != EMMC_OK)
    {
        PRINT("Identify FAILED, err=");
        PrintDec(dec, (int32_t)err);
        PRINT(dec);
        PRINT("\r\n");
        return -1;
    }

    IwdgKickDog(IWDG2);

    err = EmmcCardSelect(&g_emmc);
    if (err != EMMC_OK)
    {
        PRINT("Select FAILED\r\n");
        return -1;
    }

    err = EmmcCardSetClock(&g_emmc, 52000000);
    if (err != EMMC_OK)
    {
        PRINT("SetClock FAILED\r\n");
        return -1;
    }

    err = EmmcCardSetBusWidth(&g_emmc, EMMC_BUS_WIDTH_8BIT);
    if (err != EMMC_OK)
    {
        PRINT("SetBusWidth FAILED\r\n");
        return -1;
    }

    err = EmmcCardGetInfo(&g_emmc);
    if (err != EMMC_OK)
    {
        PRINT("GetInfo FAILED\r\n");
        return -1;
    }

    PRINT("Init OK\r\n");
    PrintCardInfo(&g_emmc);
    return 0;
}


/* ================================
 *  1. 单块读写测试 — block 15000000
 *     写 0xA5 pattern → 回读校验
 * ================================ */
static void SingleBlockTest(void)
{
    char dec[16];
    uint8_t buf[EMMC_BLOCK_SIZE], saved[EMMC_BLOCK_SIZE];
    const uint32_t test_block = 15000000;

    PRINT("\r\n----- Single Block R/W -----\r\n");
    PRINT("  Purpose: single-block write + read-back verify\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Data:    512B, all 0xA5\r\n");
    PRINT("  Write:   CMD24,  Read: CMD17\r\n");
    PRINT("  Expect:  all 512 bytes match\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 读 */
    PRINT("  Reading...\r\n");
    EmmcCardErr_t err = EmmcCardReadBlocks(&g_emmc, test_block, buf, 1);
    if (err != EMMC_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    PRINT("  Read OK: ");
    PrintHex8(buf[0]); PRINT(" ");
    PrintHex8(buf[1]); PRINT(" ");
    PrintHex8(buf[2]); PRINT(" ");
    PrintHex8(buf[3]);
    PRINT(" ...\r\n");
    IwdgKickDog(IWDG2);

    /* 写 */
    PRINT("  Writing...\r\n");
    memset(buf, 0xA5, sizeof(buf));
    memcpy(saved, buf, sizeof(buf));
    err = EmmcCardWriteBlocks(&g_emmc, test_block, buf, 1);
    if (err != EMMC_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 回读 */
    PRINT("  Reading back...\r\n");
    memset(buf, 0, sizeof(buf));
    err = EmmcCardReadBlocks(&g_emmc, test_block, buf, 1);
    if (err != EMMC_OK)
    {
        PRINT("  Readback FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 校验 */
    if (memcmp(buf, saved, sizeof(buf)) == 0)
        PRINT("  Verify PASSED\r\n");
    else
        PRINT("  Verify FAILED\r\n");
}


/* ================================
 *  2. 多块读写测试 — block 200000, 128 块
 *     uint16_t 从 0 递增到 32767
 * ================================ */
static void MultiBlockTest(void)
{
    char dec[16];
    const uint32_t test_block = 15000128;
    const uint32_t n_blocks = 128;
    const uint32_t n_vals = n_blocks * 256;

    PRINT("\r\n----- Multi-block Write/Read -----\r\n");
    PRINT("  Purpose: multi-block (CMD23+CMD25/CMD18) write+read\r\n");
    PRINT("  Blocks:  ");
    PrintDec(dec, n_blocks);
    PRINT(dec);
    PRINT(" (");
    PrintDec(dec, n_blocks / 2);
    PRINT(dec);
    PRINT(" KB) @ block ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Data:    uint16_t 0~");
    PrintDec(dec, n_vals - 1);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Write:   CMD25,  Read: CMD18\r\n");
    PRINT("  Expect:  memcmp(WR_BUF, RD_BUF) == 0\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 填 WR_BUF: uint16_t 0~32767 */
    {
        int i;
        uint16_t *p = (uint16_t *)WR_BUF;
        for (i = 0; i < (int)n_vals; i++)
            p[i] = (uint16_t)i;
    }

    PRINT("  Writing ");
    PrintDec(dec, n_blocks);
    PRINT(dec);
    PRINT(" blocks...\r\n");
    EmmcCardErr_t err = EmmcCardWriteMulti(&g_emmc, test_block, WR_BUF, n_blocks);
    if (err != EMMC_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    memset(RD_BUF, 0, n_blocks * 512);
    PRINT("  Reading back...\r\n");
    err = EmmcCardReadMulti(&g_emmc, test_block, RD_BUF, n_blocks);
    if (err != EMMC_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF, RD_BUF, n_blocks * 512) == 0)
    {
        PRINT("  Verify PASSED (");
        PrintDec(dec, n_vals);
        PRINT(dec);
        PRINT(" values)\r\n");
    }
    else
    {
        uint16_t *wp = (uint16_t *)WR_BUF;
        uint16_t *rp = (uint16_t *)RD_BUF;
        int i;
        for (i = 0; i < (int)n_vals; i++)
        {
            if (wp[i] != rp[i])
            {
                PRINT("  Mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": expect 0x");
                PrintHex8((uint8_t)(wp[i] >> 8));
                PrintHex8((uint8_t)wp[i]);
                PRINT(" got 0x");
                PrintHex8((uint8_t)(rp[i] >> 8));
                PrintHex8((uint8_t)rp[i]);
                PRINT("\r\n");
                break;
            }
        }
    }
}


/* ================================
 *  3. 擦除测试 — 写16块, 擦除前8块, 验证后8块保留
 * ================================ */
static void EraseTest(void)
{
    char dec[16];
    const uint32_t test_block = 15001000;
    int i;

    PRINT("\r\n----- Erase Test -----\r\n");
    PRINT("  Purpose: verify eMMC erase (CMD35+CMD36+CMD38 R1b)\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT(" (total 16 blocks)\r\n");
    PRINT("  Write:   CMD25, 16 blocks, uint16_t 0~4095 (8 KB)\r\n");
    PRINT("  Erase:   blocks ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT("~");
    PrintDec(dec, test_block + 7);
    PRINT(dec);
    PRINT(" (first 8)\r\n");
    PRINT("  Expect:  erased = all 0x00, preserved = original data\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 填 WR_BUF: uint16_t 0~4095 (16 块 = 8192 字节 = 4096 个 uint16_t) */
    {
        uint16_t *p = (uint16_t *)WR_BUF;
        for (i = 0; i < 4096; i++)
            p[i] = (uint16_t)i;
    }

    /* CMD23 + CMD25: 写 16 块 */
    PRINT("  Writing 16 blocks...\r\n");
    EmmcCardErr_t err = EmmcCardWriteMulti(&g_emmc, test_block, WR_BUF, 16);
    if (err != EMMC_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 读回校验 */
    memset(RD_BUF, 0, 8192);
    err = EmmcCardReadMulti(&g_emmc, test_block, RD_BUF, 16);
    if (err != EMMC_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF, RD_BUF, 8192) == 0)
        PRINT("  Write verify PASSED\r\n");
    else
    {
        uint16_t *wp = (uint16_t *)WR_BUF;
        uint16_t *rp = (uint16_t *)RD_BUF;
        for (i = 0; i < 4096; i++)
        {
            if (wp[i] != rp[i])
            {
                PRINT("  Write verify mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT("\r\n");
                break;
            }
        }
        return;
    }

    /* 擦除前 8 块 */
    PRINT("  Erasing blocks ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT("~");
    PrintDec(dec, test_block + 7);
    PRINT(dec);
    PRINT("...\r\n");
    err = EmmcCardErase(&g_emmc, test_block, test_block + 7);
    if (err != EMMC_OK)
    {
        PRINT("  Erase FAILED\r\n");
        return;
    }
    PRINT("  Erase done\r\n");
    IwdgKickDog(IWDG2);

    /* 重新读回 */
    memset(RD_BUF, 0, 8192);
    err = EmmcCardReadMulti(&g_emmc, test_block, RD_BUF, 16);
    if (err != EMMC_OK)
    {
        PRINT("  Post-erase read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /*
     * 校验策略:
     *   后 8 块 (8~15) → memcmp 与原始 WR_BUF 对比, 应完全一致
     *   前 8 块 (0~7)  → eMMC 擦除后读回全 0x00, 逐字节检查
     */

    /* 校验后 8 块保留不变 */
    if (memcmp(WR_BUF + 4096, RD_BUF + 4096, 4096) == 0)
        PRINT("  Preserved blocks (8~15) verify PASSED\r\n");
    else
    {
        uint16_t *wp = (uint16_t *)WR_BUF;
        uint16_t *rp = (uint16_t *)RD_BUF;
        for (i = 2048; i < 4096; i++)
        {
            if (wp[i] != rp[i])
            {
                PRINT("  Preserved mismatch @[");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT("]\r\n");
                break;
            }
        }
        return;
    }

    /* 校验前 8 块擦除后全 0x00 */
    {
        int errors = 0;
        for (i = 0; i < 4096; i++)
        {
            if (RD_BUF[i] != 0x00)
            {
                if (errors < 4)
                    PRINT("  Erased byte mismatch\r\n");
                errors++;
            }
        }

        if (errors == 0)
            PRINT("  Erased blocks (0~7) verify: all 0x00 PASSED\r\n");
        else
        {
            PRINT("  Erased blocks verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" non-00 bytes\r\n");
        }
    }
}


static const MenuEntry_t emmc_menu[] = {
    { "1", "Single Block R/W",   SingleBlockTest },
    { "2", "Multi Block R/W",    MultiBlockTest },
    { "3", "Erase Test",         EraseTest },
};


void EmmcTest(void)
{
    if (EmmcHwInit() != 0)
        return;

    RunSubMenu("eMMC Test", emmc_menu, sizeof(emmc_menu) / sizeof(emmc_menu[0]));
}
