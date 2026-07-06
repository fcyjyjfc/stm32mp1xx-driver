/*
 * emmc_card.c — eMMC 协议层驱动
 *
 * 初始化: CMD0 → CMD1 → CMD2 → CMD3 → CMD7 → CMD9/CMD8 (EXT_CSD)
 * 数据读写: CMD17/CMD24 (单块), CMD23+CMD18/25 (多块, 预定义块数)
 *
 *  Created on: 2026-6-27
 *      Author: gjsbr
 */

#include <stddef.h>
#include "emmc_card.h"

/* 调试用: 导出 CMD1 过程中的 OCR 响应和 RESPCMDR */
volatile uint32_t g_emmc_dbg_ocr;
volatile uint32_t g_emmc_dbg_respcmd;
/* g_sdmmc_dbg_last_star 来自 stm32mp1xx_sdmmc.c */
extern volatile uint32_t g_sdmmc_dbg_last_star;


/* TRAN_SPEED 解码表 (MMC Spec v5.1 Table 7-7 / SD Spec Table 5-6 同编码) */
static const uint32_t speed_unit[] = {100000, 1000000, 10000000, 100000000};
static const uint8_t  speed_mult[] = {0,10,12,13,15,20,25,30,35,40,45,50,55,60,70,80};

/* CMD1 最大重试次数 */
#define CMD1_RETRY_MAX  10000


/* ---- 内部辅助 ---- */

/**
 * @brief 封装 SdmmcSendCmd: 纯命令 (无数据), 统一填充 SdmmcCmd_t
 * @param sdmmc     SDMMC 寄存器基址
 * @param index     命令索引
 * @param arg       命令参数 (32-bit)
 * @param resp_type 响应类型 (NONE/SHORT/SHORT_NOCRC/LONG)
 * @param resp      响应缓冲区 [4], 可为 NULL (R1 等短响应只用 resp[0])
 * @return SDMMC_OK 或 SDMMC_ERR_*
 */
static SdmmcErr_t SendCmd(volatile SdmmcRegs_t *sdmmc,
                          uint8_t index, uint32_t arg,
                          SdmmcRespType_t resp_type,
                          uint32_t resp[4])
{
    SdmmcCmd_t cmd = {
        .index     = index,
        .arg       = arg,
        .resp_type = resp_type,
        .cmdtrans  = SDMMC_CMD_NODATA,
        .cmdstop   = SDMMC_CMD_NOSTOP,
    };
    return SdmmcSendCmd(sdmmc, &cmd, resp);
}


/* ---- 公开接口 ---- */

/**
 * @brief eMMC 识别 (跳过热启动, 用于 u-boot 已上电场景)
 *
 * 与 EmmcCardInit 完全相同, 但不调 SdmmcPowerOn, 保留 u-boot
 * 的 POWER/CLKCR 寄存器配置. 调用方需保证 SDMMC 外设已上电且有时钟.
 *
 * @return EMMC_OK / EMMC_ERR_NO_CARD / EMMC_ERR_INIT_TIMEOUT / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardIdentify(EmmcCard_t *card)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t resp[4];
    SdmmcErr_t err;

    /*
     * CMD0: GO_IDLE_STATE
     * 不管卡当前在什么状态 (可能是 u-boot 初始化后的 Transfer), 一律复位到 Idle.
     * Idle 态下卡只认 CMD1, 之前配的 HS 时序/总线宽度全部失效.
     */
    err = SendCmd(sdmmc, EMMC_CMD_GO_IDLE_STATE, 0, SDMMC_RESP_NONE, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /*
     * 降时钟至 ~400kHz, 总线切 1-bit:
     * CMD0 后卡回到上电默认的 1-bit BC 模式, 主机侧必须匹配.
     * 识别阶段时钟必须 ≤ 400kHz (MMC 规范要求).
     */
    SdmmcSetClock(sdmmc, 200000000, 400000);
    SdmmcSetBusWidth(sdmmc, SDMMC_BUS_1BIT);

    /*
     * CMD1: SEND_OP_COND
     * 首轮发 arg=0 只读卡返回的 OCR 寄存器, 获知卡支持的电压范围.
     * 第二轮起用卡返回的电压位 + HCS(扇区寻址) 轮询, 直到 OCR.BUSY bit31=1 表示初始化完成.
     * 使用 SHORT_NOCRC 因为 R3 响应不包含 CRC (MMC 特性).
     */
    uint32_t ocr_arg;
    err = SendCmd(sdmmc, EMMC_CMD_SEND_OP_COND, 0,
                  SDMMC_RESP_SHORT_NOCRC, resp);
    g_emmc_dbg_ocr     = resp[0];
    g_emmc_dbg_respcmd = sdmmc->RESPCMDR & 0x3F;
    if (err == SDMMC_ERR_CTIMEOUT)
        return EMMC_ERR_NO_CARD;
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    ocr_arg = resp[0] & ~EMMC_OCR_BUSY;
    ocr_arg |= EMMC_OCR_ACCESS_MODE;
    if (ocr_arg == 0)
        return EMMC_ERR_NO_CARD;

    uint32_t retry;
    for (retry = 0; retry < CMD1_RETRY_MAX; retry++)
    {
        err = SendCmd(sdmmc, EMMC_CMD_SEND_OP_COND, ocr_arg,
                      SDMMC_RESP_SHORT_NOCRC, resp);
        g_emmc_dbg_ocr = resp[0];
        if (err == SDMMC_ERR_CTIMEOUT)
            return EMMC_ERR_NO_CARD;
        if (err != SDMMC_OK)
            return EMMC_ERR_IO;
        if (resp[0] & EMMC_OCR_BUSY)
            break;
    }
    if (retry == CMD1_RETRY_MAX)
        return EMMC_ERR_INIT_TIMEOUT;

    /*
     * CMD2: ALL_SEND_CID
     * CMD1 完成后多卡都进 Ready 态, 广播 CMD2 让每张卡依次返回 CID.
     * 单卡环境下直接拿到唯一的 CID. R2 长响应包含 128-bit CID.
     * CID 解析: MID(制造商) + OID(OEM) + PNM(产品名6字节) + PRV(版本) + PSN(序列号) + MDT(日期)
     */
    err = SendCmd(sdmmc, EMMC_CMD_ALL_SEND_CID, 0, SDMMC_RESP_LONG, resp);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    card->mfr_id = (resp[0] >> 24) & 0xFF;
    card->oem_id[0] = (resp[0] >> 16) & 0xFF;
    card->oem_id[1] = (resp[0] >> 8) & 0xFF;
    card->oem_id[2] = '\0';
    card->product[0] = resp[0] & 0xFF;
    card->product[1] = (resp[1] >> 24) & 0xFF;
    card->product[2] = (resp[1] >> 16) & 0xFF;
    card->product[3] = (resp[1] >> 8) & 0xFF;
    card->product[4] = resp[1] & 0xFF;
    card->product[5] = (resp[2] >> 24) & 0xFF;
    card->product[6] = '\0';
    uint8_t prv = (resp[2] >> 16) & 0xFF;
    card->prv_major = prv >> 4;
    card->prv_minor = prv & 0xF;
    card->serial = ((resp[2] & 0xFFFF) << 16) | ((resp[3] >> 16) & 0xFFFF);
    card->mfr_year = ((resp[3] >> 12) & 0xF) + 2000;
    card->mfr_month = (resp[3] >> 8) & 0xF;

    /*
     * CMD3: SET_REL_ADDR
     * eMMC 的 RCA 由主机指定, 与 SD 卡动态生成不同. 这里用固定 RCA=1.
     * CMD3 后卡从 Ready 进入 Stand-by 态, 之后可以用 RCA 来选卡.
     */
    card->rca = EMMC_RCA;
    err = SendCmd(sdmmc, EMMC_CMD_SET_REL_ADDR, card->rca << 16,
                  SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    return EMMC_OK;
}


/**
 * @brief 选择卡: CMD7 SEL_DESEL_CARD (Stand-by → Transfer)
 *
 * 卡必须在 Stand-by 态 (EmmcCardIdentify 之后).
 * 成功后卡进入 Transfer 态, 可发 CMD6/CMD17 等.
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardSelect(EmmcCard_t *card)
{
    SdmmcErr_t err = SendCmd(card->sdmmc, EMMC_CMD_SEL_DESEL_CARD,
                              card->rca << 16, SDMMC_RESP_SHORT, NULL);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief 设置总线宽度: CMD6 EXT_CSD[183] + SdmmcSetBusWidth + SdmmcSetDdrMode
 *
 * 同时设置卡端 (CMD6 SWITCH) 和主机端 (SDMMC CLKCR.WIDBUS + DDR).
 * 顺序: 先卡后主机. DDR 宽度 (4BIT_DDR/8BIT_DDR) 自动映射到基本宽度并使能 CLKCR_DDR.
 * @param width EMMC_BUS_WIDTH_1BIT / _4BIT / _8BIT / _4BIT_DDR / _8BIT_DDR
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardSetBusWidth(EmmcCard_t *card, EmmcBusWidth_t width)
{
    uint32_t resp[4];

    /* CMD6: WRITE_BYTE 模式写 EXT_CSD[183] BUS_WIDTH (Table 145: 0/1/2=SDR, 5/6=DDR) */
    SdmmcErr_t err = SendCmd(card->sdmmc, EMMC_CMD_SWITCH,
        EmmcSwitchArg(EMMC_SWITCH_ACCESS_WRITE_BYTE, EMMC_EXT_CSD_BUS_WIDTH, (uint8_t)width),
        SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* 检查 R1 SWITCH_ERROR 位: bit 7 = 1 表示卡拒绝了该 SWITCH */
    if (resp[0] & EMMC_R1_SWITCH_ERROR)
        return EMMC_ERR_IO;

    /* 等待卡内部切换完成 (R1b busy) */
    err = SdmmcWaitBusy(card->sdmmc, SDMMC_BUSY_TIMEOUT);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* 映射: DDR 宽度转基本宽度 + DDR 使能 */
    SdmmcBusWidth_t base_width;
    int ddr_enable;
    switch (width) {
    case EMMC_BUS_WIDTH_4BIT_DDR:
        base_width = SDMMC_BUS_4BIT;
        ddr_enable = 1;
        break;
    case EMMC_BUS_WIDTH_8BIT_DDR:
        base_width = SDMMC_BUS_8BIT;
        ddr_enable = 1;
        break;
    default:
        base_width = (SdmmcBusWidth_t)width;
        ddr_enable = 0;
        break;
    }

    /* 原子化主机端配置: 一次 CLKCR 写入完成宽度 + DDR, 避免中间态 */
    SdmmcSetBusConfig(card->sdmmc, base_width, ddr_enable);
    return EMMC_OK;
}


/**
 * @brief 设置时钟: CMD6 EXT_CSD[185] HS Timing + SdmmcSetClock
 *
 * target_hz > 26MHz 自动切 HS_TIMING=1 (否则 HS_TIMING=0 BC),
 * 然后设置主机时钟到 target_hz. 卡和外设始终一致.
 *
 * 顺序必须: 先让卡切换到目标时序模式 (BC或HS), 再提速.
 * 如果先提频再切时序, 卡在 BC 模式下收到超规格高频时钟可能误动作.
 * @param target_hz 目标频率, 卡 HS 模式上限 52MHz
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardSetClock(EmmcCard_t *card, uint32_t target_hz)
{
    /* >26MHz 需要 HS 时序, ≤26MHz 用 BC (Backward Compatible) */
    uint8_t hs_timing = (target_hz > 26000000) ? EMMC_HS_TIMING_HS : EMMC_HS_TIMING_BC;

    /* CMD6: WRITE_BYTE 写 EXT_CSD[185] HS_TIMING */
    uint32_t resp[4];
    SdmmcErr_t err = SendCmd(card->sdmmc, EMMC_CMD_SWITCH,
        EmmcSwitchArg(EMMC_SWITCH_ACCESS_WRITE_BYTE, EMMC_EXT_CSD_HS_TIMING, hs_timing),
        SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;
    if (resp[0] & EMMC_R1_SWITCH_ERROR)
        return EMMC_ERR_IO;

    /* 等待卡内部时序切换完成 (R1b busy) */
    err = SdmmcWaitBusy(card->sdmmc, SDMMC_BUSY_TIMEOUT);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* 主机时钟匹配目标频率: CK = ker_ck / (2 * CLKDIV) */
    SdmmcSetClock(card->sdmmc, card->ker_ck_hz, target_hz);
    return EMMC_OK;
}


/**
 * @brief 切换到 DDR 模式 (封装规范要求的完整顺序)
 *
 * JESD84-B51 6.6.23 Dual Data Rate mode selection:
 *   "HS_TIMING must be set to 0x1 before setting BUS_WIDTH
 *    for dual data rate operation (values 5 or 6)"
 *
 * 顺序:
 *   1. EmmcCardSetClock → HS_TIMING=1 + 提频到 target_hz
 *   2. EmmcCardSetBusWidth → BUS_WIDTH=5/6 + 主机 CLKCR_DDR=1
 *
 * @param card      eMMC 卡结构体
 * @param width     EMMC_BUS_WIDTH_4BIT_DDR 或 EMMC_BUS_WIDTH_8BIT_DDR
 * @param target_hz 目标时钟频率 (DDR52 ≤ 52MHz)
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardSetDdrMode(EmmcCard_t *card, EmmcBusWidth_t width, uint32_t target_hz)
{
    /* 步骤1: HS_TIMING=1 + 时钟提频 */
    EmmcCardErr_t err = EmmcCardSetClock(card, target_hz);
    if (err != EMMC_OK)
        return err;

    /* 步骤2: BUS_WIDTH 设 DDR 值 (5 或 6), 同时配置主机 DDR 模式 */
    err = EmmcCardSetBusWidth(card, width);
    if (err != EMMC_OK)
        return err;

    return EMMC_OK;
}


/**
 * @brief 获取卡信息: CMD9(CSD) + CMD8(EXT_CSD)
 *
 * 卡应在 Transfer 态 (EmmcCardSelect 之后).
 * 读取 CSD 获得最大时钟频率, 读取 512 字节 EXT_CSD 并解析容量/版本.
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardGetInfo(EmmcCard_t *card)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t resp[4];
    SdmmcErr_t err;

    /*
     * CMD9: SEND_CSD
     * R2 长响应, 解析 CSD 寄存器中的 TRAN_SPEED 字段获得卡支持的最大时钟.
     * 编码方式: rate_unit[2:0] × time_value[6:3], 查表 speed_unit × speed_mult.
     */
    err = SendCmd(sdmmc, EMMC_CMD_SEND_CSD, card->rca << 16,
                  SDMMC_RESP_LONG, resp);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    uint8_t ts = resp[0] & 0xFF;
    card->max_clk_hz = speed_unit[ts & 0x7] / 10 * speed_mult[(ts >> 3) & 0xF];
    /* 保存原始 CSD 响应供调试 */
    card->csd_resp[0] = resp[0];
    card->csd_resp[1] = resp[1];
    card->csd_resp[2] = resp[2];
    card->csd_resp[3] = resp[3];
    /* CCC: CSD bits[95:84], 即 resp[1] bits[31:20]
     * JEDEC JESD84-B51 §7.3.6 — 与 SD CSD v2.0 位置一致 */
    card->ccc = (resp[1] >> 20) & 0xFFF;

    /*
     * CMD8: SEND_EXT_CSD
     * eMMC 的 CMD8 是 SEND_EXT_CSD (与 SD 卡的 SEND_IF_COND 不同).
     * 数据阶段读 512 字节 EXT_CSD, 包含容量(SEC_COUNT[215:212])、版本、总线宽度等信息.
     */
    SdmmcCmd_t cmd8 = {
        .index     = EMMC_CMD_SEND_EXT_CSD,
        .arg       = 0,
        .resp_type = SDMMC_RESP_SHORT,
        .cmdtrans  = SDMMC_CMD_DATA,
        .cmdstop   = SDMMC_CMD_NOSTOP,
    };
    SdmmcData_t data8 = {
        .buf_addr       = (uint32_t)card->ext_csd,
        .len            = 512,
        .block_size_log = SDMMC_BLKSIZ_512B,
        .dir_read       = SDMMC_DIR_READ,
    };
    err = SdmmcReadData(sdmmc, &cmd8, &data8, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* 解析 EXT_CSD: 版本 + 容量 (SEC_COUNT 4 字节小端, 单位扇区) */
    card->ext_csd_rev = card->ext_csd[EMMC_EXT_CSD_EXT_CSD_REV];
    card->total_blocks = (uint32_t)card->ext_csd[EMMC_EXT_CSD_SEC_COUNT]
                       | ((uint32_t)card->ext_csd[EMMC_EXT_CSD_SEC_COUNT + 1] << 8)
                       | ((uint32_t)card->ext_csd[EMMC_EXT_CSD_SEC_COUNT + 2] << 16)
                       | ((uint32_t)card->ext_csd[EMMC_EXT_CSD_SEC_COUNT + 3] << 24);
    card->capacity_mb = card->total_blocks / EMMC_BLOCKS_PER_MB;

    /* 写保护组大小: WP_GRP_SIZE = HC_ERASE_GRP_SIZE × HC_WP_GRP_SIZE × 512KB */
    {
        uint8_t hc_erase_sz = card->ext_csd[EMMC_EXT_CSD_HC_ERASE_GRP_SIZE];
        uint8_t hc_wp_sz    = card->ext_csd[EMMC_EXT_CSD_HC_WP_GRP_SIZE];
        if (hc_erase_sz && hc_wp_sz)
            card->wp_grp_size_blocks = (uint32_t)hc_erase_sz * hc_wp_sz * 1024;
        else
            card->wp_grp_size_blocks = 0;
    }

    return EMMC_OK;
}


/**
 * @brief 等待写操作后 BUSYD0 释放 (设备内部编程完成)
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardWaitBusy(EmmcCard_t *card)
{
    SdmmcErr_t err = SdmmcWaitBusy(card->sdmmc, SDMMC_BUSY_TIMEOUT);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief CMD17 (READ_SINGLE_BLOCK) 逐块读
 *
 * eMMC 始终块寻址, 无 SD 的 byte/sector 区分.
 * 使用循环单块读而非多块读, 适用于小数量非连续块场景.
 * @param card  卡结构体
 * @param block 起始块号
 * @param buf   读缓冲区
 * @param count 块数
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardReadBlocks(EmmcCard_t *card, uint32_t block, void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    for (uint32_t i = 0; i < count; i++)
    {
        SdmmcCmd_t cmd = {
            .index     = EMMC_CMD_READ_SINGLE,
            .arg       = block + i,
            .resp_type = SDMMC_RESP_SHORT,
            .cmdtrans  = SDMMC_CMD_DATA,
            .cmdstop   = SDMMC_CMD_NOSTOP,
        };
        SdmmcData_t data = {
            .buf_addr       = (uint32_t)((uint8_t *)buf + i * EMMC_BLOCK_SIZE),
            .len            = EMMC_BLOCK_SIZE,
            .block_size_log = SDMMC_BLKSIZ_512B,
            .dir_read       = SDMMC_DIR_READ,
        };
        SdmmcErr_t err = SdmmcReadData(sdmmc, &cmd, &data, NULL);
        if (err != SDMMC_OK)
            return EMMC_ERR_IO;
    }
    return EMMC_OK;
}


/**
 * @brief CMD24 (WRITE_BLOCK) 逐块写
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardWriteBlocks(EmmcCard_t *card, uint32_t block, const void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    for (uint32_t i = 0; i < count; i++)
    {
        SdmmcCmd_t cmd = {
            .index     = EMMC_CMD_WRITE_SINGLE,
            .arg       = block + i,
            .resp_type = SDMMC_RESP_SHORT,
            .cmdtrans  = SDMMC_CMD_DATA,
            .cmdstop   = SDMMC_CMD_NOSTOP,
        };
        SdmmcData_t data = {
            .buf_addr       = (uint32_t)((const uint8_t *)buf + i * EMMC_BLOCK_SIZE),
            .len            = EMMC_BLOCK_SIZE,
            .block_size_log = SDMMC_BLKSIZ_512B,
            .dir_read       = SDMMC_DIR_WRITE,
        };
        SdmmcErr_t err = SdmmcWriteData(sdmmc, &cmd, &data, NULL);
        if (err != SDMMC_OK)
            return EMMC_ERR_IO;
    }
    return EMMC_OK;
}


/**
 * @brief CMD23 + CMD18 多块读
 *
 * 先发 CMD23 SET_BLOCK_COUNT 告诉卡要读的块数,
 * 再发 CMD18 READ_MULTIPLE, 卡自动传输 count 块后停止.
 *
 * eMMC 支持"预定义块数"模式 (DTMODE=00), 无需 CMD12 STOP_TRANSMISSION.
 * 与 SD 卡不同, eMMC 不推荐使用 DTMODE=11 + CMD12 的方式.
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardReadMulti(EmmcCard_t *card, uint32_t block, void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    /* CMD23: 预设定块数, 传输完成后自动终止 */
    SdmmcErr_t err = SendCmd(sdmmc, EMMC_CMD_SET_BLOCK_COUNT, count,
                             SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* CMD18: 多块读, DPSM 根据 CMD23 设定的块数决定何时停止 */
    SdmmcCmd_t cmd = {
        .index     = EMMC_CMD_READ_MULTI,
        .arg       = block,
        .resp_type = SDMMC_RESP_SHORT,
        .cmdtrans  = SDMMC_CMD_DATA,
        .cmdstop   = SDMMC_CMD_NOSTOP,
    };
    SdmmcData_t data = {
        .buf_addr       = (uint32_t)buf,
        .len            = count * EMMC_BLOCK_SIZE,
        .block_size_log = SDMMC_BLKSIZ_512B,
        .dir_read       = SDMMC_DIR_READ,
    };
    err = SdmmcReadData(sdmmc, &cmd, &data, NULL);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief CMD23 + CMD25 多块写
 *
 * CMD23 预设定块数, CMD25 连续写 count 块后自动停止.
 * 与多块读相同, 使用预定义块数模式避免 CMD12 中止.
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardWriteMulti(EmmcCard_t *card, uint32_t block, const void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    /* CMD23: 预设定块数 */
    SdmmcErr_t err = SendCmd(sdmmc, EMMC_CMD_SET_BLOCK_COUNT, count,
                             SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* CMD25: 多块写 */
    SdmmcCmd_t cmd = {
        .index     = EMMC_CMD_WRITE_MULTI,
        .arg       = block,
        .resp_type = SDMMC_RESP_SHORT,
        .cmdtrans  = SDMMC_CMD_DATA,
        .cmdstop   = SDMMC_CMD_NOSTOP,
    };
    SdmmcData_t data = {
        .buf_addr       = (uint32_t)buf,
        .len            = count * EMMC_BLOCK_SIZE,
        .block_size_log = SDMMC_BLKSIZ_512B,
        .dir_read       = SDMMC_DIR_WRITE,
    };
    err = SdmmcWriteData(sdmmc, &cmd, &data, NULL);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief 擦除: CMD35(ERASE_GRP_START) → CMD36(ERASE_GRP_END) → CMD38(ERASE)
 *
 * eMMC 擦除是三步操作:
 *   1. CMD35: 设定擦除起始地址 (块号)
 *   2. CMD36: 设定擦除结束地址 (含)
 *   3. CMD38: 执行擦除 (R1b, 需等 BUSYD0 释放)
 * CMD38 执行期间卡内部对 NAND 进行块擦除, 耗时可能较长.
 * @param start_block 起始块号
 * @param end_block   结束块号 (含)
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardErase(EmmcCard_t *card, uint32_t start_block, uint32_t end_block)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    /* CMD35: 指定擦除范围起始 */
    SdmmcErr_t err = SendCmd(sdmmc, EMMC_CMD_ERASE_GRP_START, start_block,
                              SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* CMD36: 指定擦除范围结束 (含) */
    err = SendCmd(sdmmc, EMMC_CMD_ERASE_GRP_END, end_block,
                  SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* CMD38: 执行擦除, R1b 响应, 卡内部编程期间 D0 拉低 */
    err = SendCmd(sdmmc, EMMC_CMD_ERASE, 0, SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    err = SdmmcWaitBusy(sdmmc, SDMMC_BUSY_TIMEOUT);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    return EMMC_OK;
}


/**
 * @brief CMD28 SET_WRITE_PROT — 设置写保护组
 *
 * 写保护组由块地址确定, 设备自动选择包含该块的 WP 组.
 * 设置后该组内所有块被写保护, CMD24/CMD25 会触发 WP_VIOLATION.
 * @param block 组内的任意块号
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardSetWriteProtect(EmmcCard_t *card, uint32_t block)
{
    SdmmcErr_t err = SendCmd(card->sdmmc, EMMC_CMD_SET_WRITE_PROT, block,
                              SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    /* CMD28 是 R1b: 等 DAT0 释放 (内部编程 WP) */
    err = SdmmcWaitBusy(card->sdmmc, SDMMC_BUSY_TIMEOUT);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief CMD29 CLR_WRITE_PROT — 清除写保护组
 *
 * 仅对非永久写保护有效. 永久写保护 (CMD31 类型为 0) 无法清除.
 * @param block 组内的任意块号
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardClearWriteProtect(EmmcCard_t *card, uint32_t block)
{
    SdmmcErr_t err = SendCmd(card->sdmmc, EMMC_CMD_CLR_WRITE_PROT, block,
                              SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    err = SdmmcWaitBusy(card->sdmmc, SDMMC_BUSY_TIMEOUT);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief CMD30 SEND_WRITE_PROT — 读取写保护状态
 *
 * 返回 32 字节数据, 每个 bit 代表一个 WP 组的保护状态:
 *   bit=0: 未保护, bit=1: 已保护
 * 32 字节共 256 bit, 覆盖包含 block 的 WP 组及其后 255 组.
 *
 * @param block 块地址, 设备自动映射到 WP 组
 * @param[out] wp_status 32 字节缓冲区, 每组 1 bit
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardGetWriteProtect(EmmcCard_t *card, uint32_t block,
                                       uint8_t wp_status[32])
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    SdmmcCmd_t cmd = {
        .index     = EMMC_CMD_SEND_WRITE_PROT,
        .arg       = block,
        .resp_type = SDMMC_RESP_SHORT,
        .cmdtrans  = SDMMC_CMD_DATA,
        .cmdstop   = SDMMC_CMD_NOSTOP,
    };
    SdmmcData_t data = {
        .buf_addr       = (uint32_t)wp_status,
        .len            = 4,            /* 32 bits = 4 bytes (JESD84-B51 Table 53 Note 2) */
        .block_size_log = SDMMC_BLKSIZ_4B,
        .dir_read       = SDMMC_DIR_READ,
    };
    SdmmcErr_t err = SdmmcReadData(sdmmc, &cmd, &data, NULL);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief CMD31 SEND_WRITE_PROT_TYPE — 读取写保护类型
 *
 * 返回 64 bits (8 字节), 每 2 bits 表示一个 WP 组的保护类型:
 *   00 = 未保护, 01 = 临时保护, 10 = 上电保护, 11 = 永久保护
 * 覆盖包含 block 的 WP 组及其后 31 组 (共 32 组).
 * (JESD84-B51 Table 53 Note 4)
 *
 * @param block 块地址, 设备自动映射到 WP 组
 * @param[out] wp_type 缓冲区 (至少 8 字节), 每组 2 bits
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardGetWriteProtectType(EmmcCard_t *card, uint32_t block,
                                           uint8_t wp_type[8])
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    SdmmcCmd_t cmd = {
        .index     = EMMC_CMD_SEND_WRITE_PROT_TYPE,
        .arg       = block,
        .resp_type = SDMMC_RESP_SHORT,
        .cmdtrans  = SDMMC_CMD_DATA,
        .cmdstop   = SDMMC_CMD_NOSTOP,
    };
    SdmmcData_t data = {
        .buf_addr       = (uint32_t)wp_type,
        .len            = 8,            /* 64 bits = 8 bytes (JESD84-B51 Table 53 Note 4) */
        .block_size_log = SDMMC_BLKSIZ_8B,
        .dir_read       = SDMMC_DIR_READ,
    };
    SdmmcErr_t err = SdmmcReadData(sdmmc, &cmd, &data, NULL);
    return (err == SDMMC_OK) ? EMMC_OK : EMMC_ERR_IO;
}


/**
 * @brief CMD13 SEND_STATUS — 查询当前设备状态 (R1), 自动解析
 * @param[out] sts 解析后的状态结构体, 可为 NULL
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardSendStatus(EmmcCard_t *card, EmmcCardStatus_t *sts)
{
    uint32_t resp[4];
    SdmmcErr_t err = SendCmd(card->sdmmc, EMMC_CMD_SEND_STATUS,
                             card->rca << 16, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;
    if (sts)
    {
        uint32_t r = resp[0];
        sts->raw             = r;
        sts->state           = (r >> 9) & 0xF;
        sts->ready_for_data  = (r >> 8) & 1;
        sts->switch_error    = (r >> 7) & 1;
        sts->app_cmd         = (r >> 5) & 1;
        sts->ake_seq_error   = (r >> 3) & 1;
        sts->wp_violation    = r & 1;
    }
    return EMMC_OK;
}


/**
 * @brief 读取卡详细状态 — CMD13 + 主机 CLKCR + 结构体缓存
 *
 * 一站式获取:
 *   - CARD_STATE (CMD13 R1 bits 12:9)
 *   - 主机侧 CLKCR 实时配置 (位宽/DDR/时钟分频)
 *   - 卡侧缓存配置 (BUS_WIDTH/HS_TIMING 上次写入值)
 *   - 实际 SDMMC_CK 频率
 *
 * @param[out] sts 详细状态结构体
 * @return EMMC_OK / EMMC_ERR_IO
 */
EmmcCardErr_t EmmcCardReadStatus(EmmcCard_t *card, EmmcCardDetailedStatus_t *sts)
{
    if (!sts)
        return EMMC_ERR_PARAM;

    /* ---- CMD13: 卡状态 ---- */
    uint32_t resp[4];
    SdmmcErr_t err = SendCmd(card->sdmmc, EMMC_CMD_SEND_STATUS,
                             card->rca << 16, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return EMMC_ERR_IO;

    uint32_t r = resp[0];
    sts->r1             = r;
    sts->card_state     = (r >> 9) & 0xF;
    sts->ready_for_data = (r >> 8) & 1;
    sts->switch_error   = (r >> 7) & 1;
    sts->app_cmd        = (r >> 5) & 1;

    /* ---- 主机侧 CLKCR ---- */
    uint32_t clkcr = card->sdmmc->CLKCR;
    uint32_t wid   = (clkcr >> SDMMC_CLKCR_WIDBUS_Pos) & 3;
    static const uint8_t bus_width_map[4] = {1, 4, 8, 8};
    sts->host_bus_width  = bus_width_map[wid & 3];
    sts->host_ddr        = (clkcr & SDMMC_CLKCR_DDR) ? 1 : 0;
    sts->host_bus_speed  = (clkcr & SDMMC_CLKCR_BUSSPEED) ? 1 : 0;
    sts->host_negedge    = (clkcr & SDMMC_CLKCR_NEGEDGE) ? 1 : 0;
    sts->host_hwfc       = (clkcr & SDMMC_CLKCR_HWFC_EN) ? 1 : 0;
    sts->host_selclkrx   = (clkcr >> SDMMC_CLKCR_SELCLKRX_Pos) & 3;
    sts->host_clkdiv     = clkcr & SDMMC_CLKCR_CLKDIV_Msk;

    /* ---- 卡侧缓存 (BUS_WIDTH/HS_TIMING 为只写不可读回, 只能读缓存的) ---- */
    sts->cached_bus_width  = card->ext_csd[EMMC_EXT_CSD_BUS_WIDTH];
    sts->cached_hs_timing  = card->ext_csd[EMMC_EXT_CSD_HS_TIMING];

    /* ---- 时钟 ---- */
    sts->ker_ck_hz   = card->ker_ck_hz;
    uint32_t div     = sts->host_clkdiv;
    if (div == 0) div = 1;
    sts->sdmmc_ck_hz = card->ker_ck_hz / (2 * div);

    return EMMC_OK;
}
