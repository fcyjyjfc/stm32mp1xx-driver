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
#include "common_test.h"

static EmmcCard_t g_emmc;
/* 多块测试用缓冲区: 128 块 × 512B × 2 (写+读) = 128KB */
static uint8_t g_buf[128 * 512 * 2] __attribute__((aligned(4)));
#define WR_BUF  (g_buf)                /* 写缓冲区 0..65535 (64 KB) */
#define RD_BUF  (g_buf + 65536)        /* 读缓冲区 65536..131071 (64 KB) */


/* SDMMC2 GPIO: PG6(CMD) + PE3(CK) + PB14/15(D0/D1) + PB3/4(D2/D3)
 *               + PA8/9(D4/D5) + PE5(D6) + PD3(D7), AF9, 高速+上拉 */
static void EmmcGpioInit(void)
{
    RCC->MP_AHB4ENSETR |= (1U << 0) | (1U << 1) | (1U << 3)
                        | (1U << 4) | (1U << 6); /* GPIOA,B,D,E,G */

    /* PG6 = CMD */
    GpioMode(GPIO_G, 6, GPIO_MODER_AF);
    GpioAf(GPIO_G, 6, 9);
    GpioOtype(GPIO_G, 6, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_G, 6, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_G, 6, GPIO_PUPDR_PULL_UP);

    /* PE3 = CLK */
    GpioMode(GPIO_E, 3, GPIO_MODER_AF);
    GpioAf(GPIO_E, 3, 9);
    GpioOtype(GPIO_E, 3, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_E, 3, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_E, 3, GPIO_PUPDR_NO);

    /* PB14=D0, PB15=D1, PB3=D2, PB4=D3 */
    int pins_b[] = {14, 15, 3, 4};
    for (int i = 0; i < 4; i++)
    {
        GpioMode(GPIO_B, pins_b[i], GPIO_MODER_AF);
        GpioAf(GPIO_B, pins_b[i], 9);
        GpioOtype(GPIO_B, pins_b[i], GPIO_OTYPE_PUSH_PULL);
        GpioOspeed(GPIO_B, pins_b[i], GPIO_OSPEED_HI);
        GpioPullUpDown(GPIO_B, pins_b[i], GPIO_PUPDR_PULL_UP);
    }

    /* PA8=D4, PA9=D5 */
    GpioMode(GPIO_A, 8, GPIO_MODER_AF);
    GpioAf(GPIO_A, 8, 9);
    GpioOtype(GPIO_A, 8, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_A, 8, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_A, 8, GPIO_PUPDR_PULL_UP);

    GpioMode(GPIO_A, 9, GPIO_MODER_AF);
    GpioAf(GPIO_A, 9, 9);
    GpioOtype(GPIO_A, 9, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_A, 9, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_A, 9, GPIO_PUPDR_PULL_UP);

    /* PE5 = D6 */
    GpioMode(GPIO_E, 5, GPIO_MODER_AF);
    GpioAf(GPIO_E, 5, 9);
    GpioOtype(GPIO_E, 5, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_E, 5, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_E, 5, GPIO_PUPDR_PULL_UP);

    /* PD3 = D7 */
    GpioMode(GPIO_D, 3, GPIO_MODER_AF);
    GpioAf(GPIO_D, 3, 9);
    GpioOtype(GPIO_D, 3, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_D, 3, GPIO_OSPEED_HI);
    GpioPullUpDown(GPIO_D, 3, GPIO_PUPDR_PULL_UP);
}

static void EmmcClkInit(void)
{
    /* ker_ck 选 HSI (64MHz), PLL4P 未使能不可用 */
    RCC->SDMMC12CKSELR = 2;
    /* 使能 SDMMC2 (AHB6 bit17) */
    RCC->MP_AHB6ENSETR = (1U << 17);
}


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

    PRINT("  CCC: 0x");
    PrintHex8((uint8_t)(card->ccc >> 8));
    PrintHex8((uint8_t)card->ccc);
    {
        uint32_t alt_ccc = (card->csd_resp[1] >> 20) & 0xFFF;
        PRINT(" (bits[83:72]) alt[95:84]=0x");
        PrintHex8((uint8_t)(alt_ccc >> 8));
        PrintHex8((uint8_t)alt_ccc);
        PRINT(" (Cls5_83=");
        PRINT((card->ccc & (1 << 5)) ? "1" : "0");
        PRINT(" Cls6_83=");
        PRINT((card->ccc & (1 << 6)) ? "1" : "0");
        PRINT(" Cls6_95=");
        PRINT((alt_ccc & (1 << 6)) ? "1" : "0");
        PRINT(")");
    }
    PRINT("\r\n");

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
        PRINT("  HC_ERASE_GRP_SIZE[224]: ");
        PrintDec(dec, e[224]);
        PRINT(dec);
        PRINT(" x 512KB\r\n");
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


/** @brief eMMC 硬件初始化: GPIO + 时钟 → 卡识别/选卡 → 速度切换 → 读信息 */
static int EmmcHwInit(void)
{
    char dec[16];

    PRINT("\r\n===== eMMC Init =====\r\n");

//    EmmcGpioInit(); // TODO 配置后无法正常工作，暂时屏蔽，依赖u-boot的配置
    EmmcClkInit();

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
 *  3. 擦除测试 — 两组区间相隔 1MB (> 擦除组 512KB)
 *     写两组 16 块 → 擦除 Group A → 验证 Group A 全 0 + Group B 保留
 * ================================ */
static void EraseTest(void)
{
    char dec[16];
    const uint32_t blk_a = 15000000;        /* Group A */
    const uint32_t blk_b = 15002048;        /* Group B: 1MB 后 */
    const uint32_t n_blks = 16;
    int i;

    PRINT("\r\n----- Erase Test -----\r\n");
    PRINT("  Purpose: verify eMMC erase (CMD35+CMD36+CMD38 R1b)\r\n");
    PRINT("  Group A: ");
    PrintDec(dec, blk_a);
    PRINT(dec);
    PRINT(" (16 blocks)\r\n");
    PRINT("  Group B: ");
    PrintDec(dec, blk_b);
    PRINT(dec);
    PRINT(" (16 blocks, 1MB apart)\r\n");
    PRINT("  Data:    uint16_t 0~4095 (8 KB each group)\r\n");
    PRINT("  Erase:   blk ");
    PrintDec(dec, blk_a);
    PRINT(dec);
    PRINT("~");
    PrintDec(dec, blk_a + 7);
    PRINT(dec);
    PRINT(" (first 8 of Group A)\r\n");
    PRINT("  Expect:  Group A = all 0x00, Group B = original data\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 填 WR_BUF: uint16_t 0~4095 (16 块 = 8192 字节) */
    {
        uint16_t *p = (uint16_t *)WR_BUF;
        for (i = 0; i < 4096; i++)
            p[i] = (uint16_t)i;
    }

    /* 写 Group A + Group B */
    PRINT("  Writing Group A...\r\n");
    EmmcCardErr_t err = EmmcCardWriteMulti(&g_emmc, blk_a, WR_BUF, n_blks);
    if (err != EMMC_OK) { PRINT("  Write A FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    PRINT("  Writing Group B...\r\n");
    err = EmmcCardWriteMulti(&g_emmc, blk_b, WR_BUF, n_blks);
    if (err != EMMC_OK) { PRINT("  Write B FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    /* 读回校验两组写入 */
    memset(RD_BUF, 0, 8192);
    err = EmmcCardReadMulti(&g_emmc, blk_a, RD_BUF, n_blks);
    if (err != EMMC_OK) { PRINT("  Read A FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);
    if (memcmp(WR_BUF, RD_BUF, 8192) != 0)
    {
        PRINT("  Write A verify FAILED\r\n");
        return;
    }
    PRINT("  Write A verify PASSED\r\n");

    memset(RD_BUF, 0, 8192);
    err = EmmcCardReadMulti(&g_emmc, blk_b, RD_BUF, n_blks);
    if (err != EMMC_OK) { PRINT("  Read B FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);
    if (memcmp(WR_BUF, RD_BUF, 8192) != 0)
    {
        PRINT("  Write B verify FAILED\r\n");
        return;
    }
    PRINT("  Write B verify PASSED\r\n");

    /* 擦除 Group A 前 8 块（整个擦除组被擦除，≥ 512KB） */
    PRINT("  Erasing Group A blk ");
    PrintDec(dec, blk_a);
    PRINT(dec);
    PRINT("~");
    PrintDec(dec, blk_a + 7);
    PRINT(dec);
    PRINT("...\r\n");
    err = EmmcCardErase(&g_emmc, blk_a, blk_a + 7);
    if (err != EMMC_OK) { PRINT("  Erase FAILED\r\n"); return; }
    PRINT("  Erase done\r\n");
    IwdgKickDog(IWDG2);

    /* 读回 Group A → 验证全 0x00 */
    memset(RD_BUF, 0, 8192);
    err = EmmcCardReadMulti(&g_emmc, blk_a, RD_BUF, n_blks);
    if (err != EMMC_OK) { PRINT("  Post-erase read A FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    {
        int errors = 0;
        for (i = 0; i < (int)(n_blks * 512); i++)
        {
            if (RD_BUF[i] != 0x00)
            {
                if (errors < 4)
                    PRINT("  Erased byte mismatch\r\n");
                errors++;
            }
        }
        if (errors == 0)
            PRINT("  Group A erased verify: all 0x00 PASSED\r\n");
        else
        {
            PRINT("  Group A erased verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" non-00 bytes\r\n");
        }
    }

    /* 读回 Group B → 验证保留不变 */
    memset(RD_BUF, 0, 8192);
    err = EmmcCardReadMulti(&g_emmc, blk_b, RD_BUF, n_blks);
    if (err != EMMC_OK) { PRINT("  Post-erase read B FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF, RD_BUF, 8192) == 0)
        PRINT("  Group B preserved verify PASSED\r\n");
    else
    {
        uint16_t *wp = (uint16_t *)WR_BUF;
        uint16_t *rp = (uint16_t *)RD_BUF;
        for (i = 0; i < 4096; i++)
        {
            if (wp[i] != rp[i])
            {
                PRINT("  Group B mismatch @[");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT("]\r\n");
                break;
            }
        }
    }
}


/* ================================
 *  4. DDR52 模式 — 从 HS 8-bit SDR 切换到 DDR 8-bit
 *     写单一 block 0xA5 pattern 回读校验
 * ================================ */
static void Ddr52Test(void)
{
    char dec[16];
    uint8_t buf[EMMC_BLOCK_SIZE], saved[EMMC_BLOCK_SIZE];
    const uint32_t test_block = 15000000;

    PRINT("\r\n----- DDR52 Mode Test -----\r\n");
    PRINT("  Purpose: switch to 8-bit DDR at 52MHz\r\n");
    PRINT("  Current: HS SDR 8-bit ~50MHz (52 MB/s)\r\n");
    PRINT("  Target:  HS DDR 8-bit ~50MHz (104 MB/s)\r\n");
    PRINT("  CMD6:    HS_TIMING=1 + BUS_WIDTH=6 (8-bit DDR)\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* EmmcCardSetDdrMode: 规范顺序 → HS_TIMING=1, 提频, 设 BUS_WIDTH=6, 主机 DDR 模式 */
    EmmcCardErr_t err = EmmcCardSetDdrMode(&g_emmc, EMMC_BUS_WIDTH_8BIT_DDR, 52000000);
    if (err != EMMC_OK)
    {
        PRINT("  DDR52 switch FAILED\r\n");
        return;
    }
    PRINT("  DDR52 switch OK\r\n");
    IwdgKickDog(IWDG2);

    /* 打印 CLKCR 和 EXT_CSD[183] 调试 */
    {
        uint32_t clkcr = g_emmc.sdmmc->CLKCR;
        PRINT("  CLKCR=0x");
        PrintHex8((uint8_t)(clkcr >> 24));
        PrintHex8((uint8_t)(clkcr >> 16));
        PrintHex8((uint8_t)(clkcr >> 8));
        PrintHex8((uint8_t)clkcr);
        PRINT(" (DDR=");
        PrintDec(dec, (clkcr >> 18) & 1);
        PRINT(dec);
        PRINT(" NEG=");
        PrintDec(dec, (clkcr >> 16) & 1);
        PRINT(dec);
        PRINT(" BUSSPEED=");
        PrintDec(dec, (clkcr >> 19) & 1);
        PRINT(dec);
        PRINT(" WID=");
        PrintDec(dec, (clkcr >> 14) & 3);
        PRINT(dec);
        PRINT(")\r\n");
    }

    /* CMD13 查卡状态 */
    {
        EmmcCardStatus_t sts;
        if (EmmcCardSendStatus(&g_emmc, &sts) == EMMC_OK)
        {
            static const char *state_names[] = {
                "Idle","Ready","Ident","Stby","Tran","Data","Rcv","Pgm",
                "Dis","Btst","Slp"};
            PRINT("  Card state: ");
            if (sts.state < 11) PRINT(state_names[sts.state]);
            PRINT(" (");
            PrintDec(dec, sts.state);
            PRINT(dec);
            PRINT(")\r\n");
        }
        IwdgKickDog(IWDG2);
    }

    /* 单块写+回读校验验证 DDR 模式可用 */
    PRINT("  Reading block ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT("...\r\n");
    err = EmmcCardReadBlocks(&g_emmc, test_block, buf, 1);
    if (err != EMMC_OK) { PRINT("  Read FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    PRINT("  Writing 0xA5 pattern...\r\n");
    memset(buf, 0xA5, sizeof(buf));
    memcpy(saved, buf, sizeof(buf));
    err = EmmcCardWriteBlocks(&g_emmc, test_block, buf, 1);
    if (err != EMMC_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    memset(buf, 0, sizeof(buf));
    PRINT("  Reading back...\r\n");
    err = EmmcCardReadBlocks(&g_emmc, test_block, buf, 1);
    if (err != EMMC_OK) { PRINT("  Readback FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    if (memcmp(buf, saved, sizeof(buf)) == 0)
        PRINT("  DDR52 Verify PASSED\r\n");
    else
        PRINT("  DDR52 Verify FAILED\r\n");

    /* 切回 SDR 8-bit, 保持后续测试兼容 */
    PRINT("  Switching back to SDR 8-bit...\r\n");
    err = EmmcCardSetBusWidth(&g_emmc, EMMC_BUS_WIDTH_8BIT);
    if (err != EMMC_OK)
        PRINT("  SDR restore FAILED\r\n");
    else
        PRINT("  SDR restore OK\r\n");
}


/* ================================
 *  5. DDR52 多块读写 — DDR 模式下 CMD23+CMD25/CMD18
 *     写 16 块 uint16_t 0~4095 回读校验
 * ================================ */
static void Ddr52MultiBlockTest(void)
{
    char dec[16];
    const uint32_t test_block = 15003000;
    const uint32_t n_blocks = 16;

    PRINT("\r\n----- DDR52 Multi-Block Test -----\r\n");
    PRINT("  Block: ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT(" (");
    PrintDec(dec, n_blocks);
    PRINT(dec);
    PRINT(" blocks, 8 KB)\r\n");

    IwdgKickDog(IWDG2);

    /* 切换到 DDR52 */
    EmmcCardErr_t err = EmmcCardSetDdrMode(&g_emmc, EMMC_BUS_WIDTH_8BIT_DDR, 52000000);
    if (err != EMMC_OK)
    {
        PRINT("  DDR52 switch FAILED\r\n");
        return;
    }
    PRINT("  DDR52 switch OK\r\n");
    IwdgKickDog(IWDG2);

    /* 填 WR_BUF: uint16_t 0~4095 */
    {
        uint16_t *p = (uint16_t *)WR_BUF;
        for (int i = 0; i < 4096; i++)
            p[i] = (uint16_t)i;
    }

    /* CMD23 + CMD25: 多块写 */
    PRINT("  Writing ");
    PrintDec(dec, n_blocks);
    PRINT(dec);
    PRINT(" blocks...\r\n");
    err = EmmcCardWriteMulti(&g_emmc, test_block, WR_BUF, n_blocks);
    if (err != EMMC_OK)
    {
        PRINT("  Write FAILED\r\n");
        goto restore;
    }
    IwdgKickDog(IWDG2);

    /* CMD23 + CMD18: 多块读回 */
    memset(RD_BUF, 0, n_blocks * 512);
    PRINT("  Reading back...\r\n");
    err = EmmcCardReadMulti(&g_emmc, test_block, RD_BUF, n_blocks);
    if (err != EMMC_OK)
    {
        PRINT("  Read FAILED\r\n");
        goto restore;
    }
    IwdgKickDog(IWDG2);

    /* memcmp 校验 */
    if (memcmp(WR_BUF, RD_BUF, n_blocks * 512) == 0)
        PRINT("  DDR52 Multi-Block Verify PASSED\r\n");
    else
    {
        PRINT("  DDR52 Multi-Block Verify FAILED\r\n");
        uint16_t *wp = (uint16_t *)WR_BUF;
        uint16_t *rp = (uint16_t *)RD_BUF;
        for (int i = 0; i < 4096; i++)
        {
            if (wp[i] != rp[i])
            {
                PRINT("  Mismatch @[");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT("]\r\n");
                break;
            }
        }
    }

restore:
    /* 切回 SDR 8-bit */
    PRINT("  Switching back to SDR 8-bit...\r\n");
    err = EmmcCardSetBusWidth(&g_emmc, EMMC_BUS_WIDTH_8BIT);
    if (err != EMMC_OK)
        PRINT("  SDR restore FAILED\r\n");
    else
        PRINT("  SDR restore OK\r\n");
}


/* ================================
 *  6. 写保护测试 — CMD28/29/30 功能验证
 *     写 A → 设 WP → 写 B(应拒) → 回读 A → 清 WP → 写 B → 回读 B
 * ================================ */
static void WriteProtectTest(void)
{
    char dec[16];
    const uint32_t test_block = 15000000;
    uint32_t wp_group;

    PRINT("\r\n----- Write Protect Test -----\r\n");
    PRINT("  Block: ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* ---- WP 组大小 ---- */
    if (g_emmc.wp_grp_size_blocks == 0)
    {
        PRINT("  WP_GRP_SIZE=0, WP not supported?\r\n");
        return;
    }
    wp_group = test_block / g_emmc.wp_grp_size_blocks;
    PRINT("  WP_GRP_SIZE: ");
    PrintDec(dec, (int32_t)g_emmc.wp_grp_size_blocks);
    PRINT(dec);
    PRINT(" blocks, group ");
    PrintDec(dec, wp_group);
    PRINT(dec);
    PRINT("\r\n\r\n");

    /* ---- 准备数据 A(0xAA) 和 B(0xBB) ---- */
    memset(WR_BUF, 0xAA, 512);
    memset(WR_BUF + 512, 0xBB, 512);

    /* ================================================================
     *  1. 写 A + 回读校验 (确认区域可写)
     * ================================================================ */
    PRINT("  [1/6] Write pattern A...\r\n");
    EmmcCardErr_t err = EmmcCardWriteBlocks(&g_emmc, test_block, WR_BUF, 1);
    if (err != EMMC_OK) { PRINT("  Write A FAILED - abort\r\n"); return; }

    memset(RD_BUF, 0, 512);
    err = EmmcCardReadBlocks(&g_emmc, test_block, RD_BUF, 1);
    if (err != EMMC_OK) { PRINT("  Readback A FAILED\r\n"); return; }
    PRINT("  Verify A: ");
    PRINT(memcmp(WR_BUF, RD_BUF, 512) == 0 ? "PASSED\r\n" : "FAILED\r\n");
    if (memcmp(WR_BUF, RD_BUF, 512) != 0) return;
    IwdgKickDog(IWDG2);

    /* ================================================================
     *  2. 设 WP (CMD28)
     * ================================================================ */
    PRINT("  [2/6] SET WP (CMD28)...\r\n");
    err = EmmcCardSetWriteProtect(&g_emmc, test_block);
    if (err != EMMC_OK) { PRINT("  CMD28 FAILED\r\n"); return; }
    {
        uint8_t wp_sts[32], wp_type[32];
        EmmcCardStatus_t sts;

        if (EmmcCardGetWriteProtect(&g_emmc, test_block, wp_sts) != EMMC_OK)
            PRINT("  CMD30 FAILED\r\n  ");
        if (EmmcCardGetWriteProtectType(&g_emmc, test_block, wp_type) != EMMC_OK)
            PRINT("  CMD31 FAILED\r\n  ");
        EmmcCardSendStatus(&g_emmc, &sts);

        PRINT("  CMD30 byte[0]=0x");
        PrintHex8(wp_sts[0]);
        PRINT(" byte[1]=0x");
        PrintHex8(wp_sts[1]);
        PRINT(" byte[2]=0x");
        PrintHex8(wp_sts[2]);
        PRINT("\r\n  CMD31 byte[0]=0x");
        PrintHex8(wp_type[0]);
        PRINT(" (type[1:0]=");
        {
            static const char *wp_type_name[] = {
                "unprotected", "temp", "power-on", "permanent"};
            uint8_t t = wp_type[0] & 3;
            if (t < 4) PRINT(wp_type_name[t]); else PRINT("?");
        }
        PRINT(")\r\n  CMD13: state=");
        {
            static const char *sn[] = {"Idle","Ready","Ident","Stby","Tran","Data","Rcv","Pgm","Dis","Btst","Slp"};
            if (sts.state < 11) PRINT(sn[sts.state]); else PRINT("?");
        }
        PRINT(" Rdy=");
        PrintDec(dec, sts.ready_for_data);
        PRINT(" WPv=");
        PrintDec(dec, sts.wp_violation);
        PRINT(" SwErr=");
        PrintDec(dec, sts.switch_error);
        PRINT(" AKE=");
        PrintDec(dec, sts.ake_seq_error);
        PRINT("\r\n");

        /* WP 状态判断: bit0 对应包含 test_block 的组 */
        uint8_t b0 = wp_sts[0];
        if (b0 & 1)
            PRINT("  WP: PROTECTED\r\n");
        else
            PRINT("  WP: unprotected\r\n");
    }
    IwdgKickDog(IWDG2);

    /* ================================================================
     *  3. 写 B — 应被 WP 拒绝, CMD13 应有 WP_VIOLATION
     * ================================================================ */
    PRINT("  [3/6] Write pattern B (expect WP reject)...\r\n");
    err = EmmcCardWriteBlocks(&g_emmc, test_block, WR_BUF + 512, 1);
    if (err != EMMC_OK)
        PRINT("  Write B rejected (expected)\r\n");
    else
    {
        EmmcCardStatus_t sts;
        EmmcCardSendStatus(&g_emmc, &sts);
        if (sts.wp_violation)
            PRINT("  Write B returned OK but CMD13 shows WP_VIOLATION\r\n");
        else
            PRINT("  Write B unexpectedly succeeded (WP may not work)\r\n");
    }
    IwdgKickDog(IWDG2);

    /* ================================================================
     *  4. 回读 — 数据应仍是 A (B 被挡住了)
     * ================================================================ */
    PRINT("  [4/6] Readback (expect pattern A preserved)...\r\n");
    memset(RD_BUF, 0, 512);
    err = EmmcCardReadBlocks(&g_emmc, test_block, RD_BUF, 1);
    if (err != EMMC_OK) { PRINT("  Readback FAILED\r\n"); goto clear_wp; }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF, RD_BUF, 512) == 0)
        PRINT("  Data preserved: A (WP blocked B)\r\n");
    else if (memcmp(WR_BUF + 512, RD_BUF, 512) == 0)
        PRINT("  Data changed to B (WP NOT effective!)\r\n");
    else
        PRINT("  Data corrupted (unexpected)\r\n");

clear_wp:
    /* ================================================================
     *  5. 清 WP + 写 B (应成功)
     * ================================================================ */
    PRINT("  [5/6] CLR WP (CMD29) + write B...\r\n");
    err = EmmcCardClearWriteProtect(&g_emmc, test_block);
    if (err != EMMC_OK) { PRINT("  CMD29 FAILED\r\n"); return; }
    {
        uint8_t wp[32];
        EmmcCardGetWriteProtect(&g_emmc, test_block, wp);
        PRINT("  CMD30 byte[0]=0x");
        PrintHex8(wp[0]);
        PRINT(" (");
        PRINT((wp[0] & 1) ? "PROTECTED" : "cleared");
        PRINT(")\r\n");
    }
    IwdgKickDog(IWDG2);

    err = EmmcCardWriteBlocks(&g_emmc, test_block, WR_BUF + 512, 1);
    if (err != EMMC_OK) { PRINT("  Write B after CLR FAILED\r\n"); return; }
    PRINT("  Write B after CLR: OK\r\n");
    IwdgKickDog(IWDG2);

    /* ================================================================
     *  6. 回读验证 B
     * ================================================================ */
    PRINT("  [6/6] Readback (expect pattern B)...\r\n");
    memset(RD_BUF, 0, 512);
    err = EmmcCardReadBlocks(&g_emmc, test_block, RD_BUF, 1);
    if (err != EMMC_OK) { PRINT("  Readback FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF + 512, RD_BUF, 512) == 0)
        PRINT("  Verify B: PASSED (WP -> CLR -> write works)\r\n");
    else
        PRINT("  Verify B: FAILED\r\n");

    /* ---- 结果总结 ---- */
    PRINT("\r\n--- Summary ---\r\n");
    PRINT("  WP set+blocked write: ");
    /* 重新查 WP 状态看当前是否已清除 */
    {
        uint8_t wp[32];
        EmmcCardGetWriteProtect(&g_emmc, test_block, wp);
        PRINT((wp[0] & 1) ? "FAILED (still protected)\r\n" : "OK\r\n");
    }
}


/* ================================
 *  7. Sleep/Awake 测试
 * ================================ */
static void SleepAwakeTest(void)
{
    char dec[16];
    EmmcCardStatus_t sts;

    PRINT("\r\n----- Sleep/Awake Test -----\r\n");
    PRINT("  S_A_TIMEOUT[216]=0x");
    PrintHex8(g_emmc.ext_csd[216]);
    PRINT(" S_C_VCC[220]=0x");
    PrintHex8(g_emmc.ext_csd[220]);
    PRINT(" CLASS_6_CTRL[59]=0x");
    PrintHex8(g_emmc.ext_csd[59]);
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 记录 sleep 前状态 */
    EmmcCardSendStatus(&g_emmc, &sts);
    PRINT("  State before sleep: ");
    {
        static const char *sn[] = {"Idle","Ready","Ident","Stby","Tran","Data","Rcv","Pgm","Dis","Btst","Slp"};
        PRINT((sts.state < 11) ? sn[sts.state] : "?");
    }
    PRINT("\r\n");

    /* Sleep */
    PRINT("  Sleep...\r\n");
    EmmcCardErr_t err = EmmcCardSleep(&g_emmc);
    if (err != EMMC_OK)
    {
        static const char *err_names[] = {
            [EMMC_ERR_SLEEP_CMD7] = "CMD7 deselect",
            [EMMC_ERR_SLEEP_CMD5] = "CMD5 sleep",
            [EMMC_ERR_SLEEP_BUSY] = "DAT0 busy timeout",
        };
        uint32_t star = g_emmc.sdmmc->STAR;
        uint32_t respcmd = g_emmc.sdmmc->RESPCMDR;
        uint32_t resp1 = g_emmc.sdmmc->RESP1R;
        PRINT("  Sleep FAILED: ");
        if (err >= EMMC_ERR_SLEEP_CMD7 && err <= EMMC_ERR_SLEEP_BUSY)
            PRINT(err_names[err]);
        else
            { PRINT("unknown("); PrintDec(dec, err); PRINT(dec); PRINT(")"); }
        PRINT(" STAR=0x");
        PrintHex32(dec, star);
        PRINT(dec);
        if (star & (1U << 3))  PRINT(" CTIMEOUT");
        if (star & (1U << 10)) PRINT(" CCRCFAIL");
        if (star & (1U << 5))  PRINT(" CMDREND");
        PRINT(" RESPCMDR=0x");
        PrintHex32(dec, respcmd);
        PRINT(dec);
        PRINT(" RESP1R=0x");
        PrintHex32(dec, resp1);
        PRINT(dec);
        PRINT("\r\n");
        return;
    }
    PRINT("  Sleep OK\r\n");
    IwdgKickDog(IWDG2);

    /* Sleep 中 CMD13 应返回 Sleep 态 */
    EmmcCardSendStatus(&g_emmc, &sts);
    PRINT("  State after sleep: ");
    {
        static const char *sn[] = {"Idle","Ready","Ident","Stby","Tran","Data","Rcv","Pgm","Dis","Btst","Slp"};
        PRINT((sts.state < 11) ? sn[sts.state] : "?");
    }
    PRINT("\r\n");
    IwdgKickDog(IWDG2);

    /* Sleep 中试读块, 应被状态检查拒绝 (EMMC_ERR_WRONG_STATE) */
    {
        EmmcCardErr_t rerr = EmmcCardReadBlocks(&g_emmc, 0, (void *)WR_BUF, 1);
        if (rerr == EMMC_ERR_WRONG_STATE)
        {
            PRINT("  Read during sleep: WRONG_STATE (expected)\r\n");
        }
        else
        {
            PRINT("  Read during sleep: unexpected err=");
            PrintDec(dec, (int32_t)rerr);
            PRINT(dec);
            PRINT("\r\n");
        }
    }
    IwdgKickDog(IWDG2);

    /* Awake */
    PRINT("  Awake...\r\n");
    err = EmmcCardAwake(&g_emmc);
    if (err != EMMC_OK) { PRINT("  Awake FAILED\r\n"); return; }
    PRINT("  Awake OK\r\n");
    IwdgKickDog(IWDG2);

    /* 唤醒后应回到 Transfer 态, 验证可读写 */
    EmmcCardSendStatus(&g_emmc, &sts);
    PRINT("  State after wake: ");
    {
        static const char *sn[] = {"Idle","Ready","Ident","Stby","Tran","Data","Rcv","Pgm","Dis","Btst","Slp"};
        PRINT((sts.state < 11) ? sn[sts.state] : "?");
    }
    PRINT(" (expect Tran)\r\n");

    if (sts.state != EMMC_STATE_TRAN)
    {
        PRINT("  State != Tran, skip R/W verify\r\n");
        return;
    }

    /* 唤醒后验证读写: 单块写 + 回读 */
    PRINT("  Verify R/W after wake...\r\n");
    memset(WR_BUF, 0xAA, 512);
    err = EmmcCardWriteBlocks(&g_emmc, 15000000, WR_BUF, 1);
    if (err != EMMC_OK) { PRINT("  Write FAILED\r\n"); return; }
    IwdgKickDog(IWDG2);

    memset(RD_BUF, 0, 512);
    err = EmmcCardReadBlocks(&g_emmc, 15000000, RD_BUF, 1);
    if (err != EMMC_OK) { PRINT("  Read FAILED\r\n"); return; }

    if (memcmp(WR_BUF, RD_BUF, 512) == 0)
        PRINT("  R/W Verify PASSED\r\n");
    else
        PRINT("  R/W Verify FAILED\r\n");
}


/* GIC 中断配置: SDMMC2 IRQ 156, priority 5 */
static void EmmcGicConfig(void)
{
    GicdSetGroup(GIC_SDMMC2);
    GicdSetPriority(GIC_SDMMC2, 5);
    GicdSetTarget(GIC_SDMMC2, 1);
    GicdSetTrigMode(GIC_SDMMC2, 0);
    GicRegisterIrq(GIC_SDMMC2, Sdmmc2IrqHandler);
    GicdEnableInt(GIC_SDMMC2);
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

static void EmmcGicCleanup(void)
{
    GicdDisableInt(GIC_SDMMC2);
    g_sdmmc2_irq.it_done = 0;
}


/* ================================
 *  8. DDR52 + 中断 LL 多节点测试
 *     切 DDR52 → 中断 LL 写 8 节点 → 中断 LL 读 2 节点 → 校验 → 恢复 SDR
 * ================================ */
static void Ddr52LlItMultiBlockTest(void)
{
    char dec[16];
    const uint32_t test_block = 15004000;
    const uint32_t n_wr_nodes  = 8;
    const uint32_t node_size   = 4096;       /* 8 × 4096 = 32 KB */
    const uint32_t rd_sizes[2] = {16384, 16384};
    int i;

    PRINT("\r\n----- DDR52 + Interrupt LL Multi-Node -----\r\n");
    PRINT("  Purpose: verify DDR52 + ISR + IDMA LL combo\r\n");
    PRINT("  Mode:    DDR52 (8-bit, 52MHz) | interrupt | IDMA LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Write:   ");
    PrintDec(dec, n_wr_nodes);
    PRINT(dec);
    PRINT(" nodes x 4096B = 32 KB, all ABR=1\r\n");
    PRINT("  Read:    2 nodes (16384+16384=32 KB), non-contiguous desc\r\n");
    PRINT("  Data:    uint16_t 0~16383\r\n");
    PRINT("  Expect:  16384 values match, BTC=8(wr)+2(rd)\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* ---- 切换 DDR52 ---- */
    PRINT("  Switching to DDR52 (8-bit DDR, 52MHz)...\r\n");
    EmmcCardErr_t err = EmmcCardSetDdrMode(&g_emmc, EMMC_BUS_WIDTH_8BIT_DDR, 52000000);
    if (err != EMMC_OK)
    {
        PRINT("  DDR52 switch FAILED\r\n");
        return;
    }
    PRINT("  DDR52 switch OK\r\n");
    IwdgKickDog(IWDG2);

    /* ---- 写缓冲区: 8 节点 × 4096B, 填递增 uint16_t 0~16383 ---- */
    uint8_t *wr_buf[8];
    for (i = 0; i < 8; i++)
        wr_buf[i] = g_buf + i * node_size;

    uint8_t *rd_buf[2];
    rd_buf[0] = g_buf + 65536;
    rd_buf[1] = g_buf + 81920;

    uint16_t v = 0;
    for (i = 0; i < 8; i++)
    {
        uint16_t *p = (uint16_t *)wr_buf[i];
        for (uint32_t j = 0; j < node_size / 2; j++)
            p[j] = v++;
    }

    /* ---- 写描述符: 8 节点全 ABR=1, ULS=0 (所有节点相同 size,
     *      由 SdmmcStartLL 的 IDMABSIZER 寄存器预设, 后续复用) ---- */
    SdmmcIdmaDesc_t desc_wr[8];
    for (i = 0; i < 8; i++)
    {
        desc_wr[i].buf_addr = (uint32_t)(uintptr_t)wr_buf[i];
        desc_wr[i].buf_size = node_size;
        uint32_t idmalar = SDMMC_IDMALAR_ABR;
        if (i < 7)
        {
            idmalar |= SDMMC_IDMALAR_ULA;
            idmalar |= (uint32_t)((i + 1) * sizeof(SdmmcIdmaDesc_t));
        }
        desc_wr[i].idmalar = idmalar;
    }

    /* ---- 读描述符: pool[2] → pool[7] 非连续, ULS=0 复用前 size ---- */
    SdmmcIdmaDesc_t desc_rd_pool[10];
    uint32_t idmala_rd = (7 - 2) * (uint32_t)sizeof(SdmmcIdmaDesc_t);

    desc_rd_pool[2].buf_addr = (uint32_t)(uintptr_t)rd_buf[0];
    desc_rd_pool[2].buf_size = rd_sizes[0];
    desc_rd_pool[2].idmalar  = SDMMC_IDMALAR_ABR | SDMMC_IDMALAR_ULA | idmala_rd;

    desc_rd_pool[7].buf_addr = (uint32_t)(uintptr_t)rd_buf[1];
    desc_rd_pool[7].buf_size = rd_sizes[1];
    desc_rd_pool[7].idmalar  = SDMMC_IDMALAR_ABR;

    /* ---- GIC 配置 ---- */
    EmmcGicConfig();

    /* ---- 中断写: CMD23(轮询) + CMD25(中断) ---- */
    g_sdmmc2_irq.it_done  = 0;
    g_sdmmc2_irq.btc_cnt = 0;
    PRINT("  Write via ISR...\r\n");
    EmmcCardStartWriteMultiLL(&g_emmc, test_block, desc_wr, SDMMC_BLKSIZ_512B);

    while (!g_sdmmc2_irq.it_done)
        IwdgKickDog(IWDG2);

    PRINT("  Write done, BTC=");
    PrintDec(dec, (int32_t)g_sdmmc2_irq.btc_cnt);
    PRINT(dec);
    PRINT(" (expect 8)\r\n");

    if (g_sdmmc2_irq.it_result != SDMMC_OK)
    {
        PRINT("  IT Write FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc2_irq.it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    EmmcCardWaitBusy(&g_emmc);
    IwdgKickDog(IWDG2);

    /* ---- 清读缓冲区 ---- */
    for (i = 0; i < 2; i++)
        memset(rd_buf[i], 0, rd_sizes[i]);

    /* ---- 中断读: CMD23(轮询) + CMD18(中断) ---- */
    g_sdmmc2_irq.it_done  = 0;
    g_sdmmc2_irq.btc_cnt = 0;
    PRINT("  Read via ISR...\r\n");
    EmmcCardStartReadMultiLL(&g_emmc, test_block, &desc_rd_pool[2], SDMMC_BLKSIZ_512B);

    while (!g_sdmmc2_irq.it_done)
        IwdgKickDog(IWDG2);

    PRINT("  Read done, BTC=");
    PrintDec(dec, (int32_t)g_sdmmc2_irq.btc_cnt);
    PRINT(dec);
    PRINT(" (expect 2)\r\n");

    if (g_sdmmc2_irq.it_result != SDMMC_OK)
    {
        PRINT("  IT Read FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc2_irq.it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    /* ---- 校验: 跨 2 个读 buffer 比对 uint16_t ---- */
    {
        int errors = 0;
        v = 0;
        for (i = 0; i < 2; i++)
        {
            uint16_t *p = (uint16_t *)rd_buf[i];
            for (uint32_t j = 0; j < rd_sizes[i] / 2; j++)
            {
                if (p[j] != v)
                {
                    if (errors < 4)
                    {
                        PRINT("  Mismatch: rd_buf[");
                        PrintDec(dec, i);
                        PRINT(dec);
                        PRINT("][");
                        PrintDec(dec, j);
                        PRINT(dec);
                        PRINT("] expect ");
                        PrintDec(dec, v);
                        PRINT(dec);
                        PRINT(" got ");
                        PrintDec(dec, p[j]);
                        PRINT(dec);
                        PRINT("\r\n");
                    }
                    errors++;
                }
                v++;
            }
        }

        if (errors == 0)
            PRINT("  Verify PASSED (16384 values, DDR52+ISR+LL)\r\n");
        else
        {
            PRINT("  Verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" errors\r\n");
        }
    }

cleanup:
    EmmcGicCleanup();

    /* ---- 恢复 SDR 8-bit, 保持后续测试兼容 ---- */
    PRINT("  Restoring SDR 8-bit...\r\n");
    err = EmmcCardSetBusWidth(&g_emmc, EMMC_BUS_WIDTH_8BIT);
    if (err != EMMC_OK)
        PRINT("  SDR restore FAILED\r\n");
    else
        PRINT("  SDR restore OK\r\n");
}


static const MenuEntry_t emmc_menu[] = {
    { "1", "Single Block R/W",   SingleBlockTest },
    { "2", "Multi Block R/W",    MultiBlockTest },
    { "3", "Erase Test",         EraseTest },
    { "4", "DDR52 Mode Test",    Ddr52Test },
    { "5", "DDR52 Multi-Block",  Ddr52MultiBlockTest },
    { "6", "Write Protect",      WriteProtectTest },
    { "7", "Sleep/Awake",        SleepAwakeTest },
    { "8", "DDR52+ISR+LL Multi", Ddr52LlItMultiBlockTest },
};


void EmmcTest(void)
{
    if (EmmcHwInit() != 0)
        return;

    RunSubMenu("eMMC Test", emmc_menu, sizeof(emmc_menu) / sizeof(emmc_menu[0]));
}
