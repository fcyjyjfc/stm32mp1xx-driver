/*
 * sd_card.c — SD 卡协议层驱动
 *
 * 初始化: CMD0 → CMD8 → ACMD41 → CMD2 → CMD3 → CMD9 → CMD7 → ACMD6 → SdCardSetSpeed
 * SdCardSetSpeed: 根据 target_MBps 计算 CK, >50MHz 时查 S18A 做 1.8V 切换
 * 数据读写: CMD17/CMD24 (单块), CMD23+CMD18/25 (多块)
 *
 *  Created on: 2025-6-21
 *      Author: gjsbr
 */

#include <stddef.h>
#include "sd_card.h"

/* TRAN_SPEED 解码表 (SD Spec v9.10 Table 5-6)
 * max_clk = speed_unit[ts & 7] / 10 * speed_mult[ts >> 3] */
static const uint32_t speed_unit[] = {100000, 1000000, 10000000, 100000000};
static const uint8_t  speed_mult[] = {0,10,12,13,15,20,25,30,35,40,45,50,55,60,70,80};

/* ACMD41 最大重试次数 (规范要求1秒内完成, 实际几十次就够) */
#define ACMD41_RETRY_MAX  10000


/* ---- 内部辅助 ---- */

static SdmmcErr_t SendCmd(volatile SdmmcRegs_t *sdmmc,
                          uint8_t index, uint32_t arg,
                          SdmmcRespType_t resp_type,
                          uint32_t resp[4])
{
    SdmmcCmd_t cmd = {
        .index = index,
        .arg = arg,
        .resp_type = resp_type,
        .cmdtrans = SDMMC_CMD_NODATA,
        .cmdstop = SDMMC_CMD_NOSTOP,
    };
    return SdmmcSendCmd(sdmmc, &cmd, resp);
}

/* CMD55 + ACMDx */
static SdmmcErr_t SendAcmd(volatile SdmmcRegs_t *sdmmc,
                           uint32_t rca,
                           uint8_t index, uint32_t arg,
                           SdmmcRespType_t resp_type,
                           uint32_t resp[4])
{
    uint32_t tmp[4];
    SdmmcErr_t err = SendCmd(sdmmc, SD_CMD_APP_CMD, rca << 16, SDMMC_RESP_SHORT, tmp);
    if (err != SDMMC_OK)
        return err;
    /* 检查 APP_CMD 位确认卡准备接收 ACMD */
    if (!(tmp[0] & SD_R1_APP_CMD))
        return SDMMC_ERR_RESP_MISMATCH;
    return SendCmd(sdmmc, index, arg, resp_type, resp);
}



/* ---- 公开接口 ---- */

/* 初始化: 上电→卡识别→选卡→4-bit→设定目标速率 */
SdCardErr_t SdCardInit(SdCard_t *card, uint32_t target_MBps)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t resp[4];
    SdmmcErr_t err;

    /* 上电: ≤400kHz, 1-bit (识别模式时钟要求见 Section 4.2) */
    SdmmcPowerOn(sdmmc, card->ker_ck_hz);

    /* CMD0: GO_IDLE_STATE */
    err = SendCmd(sdmmc, SD_CMD_GO_IDLE_STATE, 0, SDMMC_RESP_NONE, NULL);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* CMD8: SEND_IF_COND — arg 格式见 SD Spec v9.10 Section 4.3.13
     * [11:8]=VHS(1=2.7~3.6V), [7:0]=check pattern(任选, 惯例0xAA) */
    err = SendCmd(sdmmc, SD_CMD_SEND_IF_COND, SD_CMD8_ARG, SDMMC_RESP_SHORT, resp);
    if (err == SDMMC_ERR_CTIMEOUT)
        return SD_ERR_UNUSABLE;  /* SD v1.x 或非SD卡 */
    if (err != SDMMC_OK)
        return SD_ERR_IO;
    if ((resp[0] & SD_CMD8_RESP_MASK) != SD_CMD8_ARG)
        return SD_ERR_UNUSABLE;

    /* ACMD41 循环 — arg/resp 位域见 SD Spec v9.10 Section 5.1 (OCR) + Table 4-41
     * arg: [30]=HCS(1=支持SDHC), [24]=S18R(1=主机支持1.8V切换),
     *      [23:15]=电压窗口(0xFF80=2.7~3.6V全覆盖)
     * resp: [31]=Busy(1=完成), [30]=CCS(1=SDHC), [24]=S18A(1=卡支持1.8V) — R3 无CRC */
    uint32_t ocr = 0;
    uint32_t retry;
    for (retry = 0; retry < ACMD41_RETRY_MAX; retry++)
    {
        err = SendAcmd(sdmmc, 0, SD_ACMD_SEND_OP_COND, SD_ACMD41_ARG, SDMMC_RESP_SHORT_NOCRC, resp);
        if (err != SDMMC_OK)
            return SD_ERR_IO;
        ocr = resp[0];
        if (ocr & SD_OCR_BUSY)
            break;
    }
    if (retry == ACMD41_RETRY_MAX)
        return SD_ERR_INIT_TIMEOUT;
    card->is_sdhc = (ocr & SD_OCR_CCS) ? 1 : 0;
    card->s18a = (ocr & SD_OCR_S18A) ? 1 : 0;

    /* CMD2: ALL_SEND_CID → 解析卡身份信息
     * CID 位域定义见 SD Spec v9.10 Table 5-2 (Section 5.2)
     * resp[0..3] = CID[127:96], [95:64], [63:32], [31:0] (RM0436 58.6.3) */
    err = SendCmd(sdmmc, SD_CMD_ALL_SEND_CID, 0, SDMMC_RESP_LONG, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    card->mfr_id = (resp[0] >> 24) & 0xFF;
    card->oem_id[0] = (resp[0] >> 16) & 0xFF;
    card->oem_id[1] = (resp[0] >> 8) & 0xFF;
    card->oem_id[2] = '\0';
    card->product[0] = resp[0] & 0xFF;
    card->product[1] = (resp[1] >> 24) & 0xFF;
    card->product[2] = (resp[1] >> 16) & 0xFF;
    card->product[3] = (resp[1] >> 8) & 0xFF;
    card->product[4] = resp[1] & 0xFF;
    card->product[5] = '\0';
    uint8_t prv = (resp[2] >> 24) & 0xFF;
    card->prv_major = prv >> 4;
    card->prv_minor = prv & 0xF;
    card->serial = ((resp[2] & 0x00FFFFFF) << 8) | ((resp[3] >> 24) & 0xFF);
    card->mfr_year = ((resp[3] >> 12) & 0xFF) + 2000;
    card->mfr_month = (resp[3] >> 8) & 0xF;

    /* CMD3: SEND_RELATIVE_ADDR — R6 格式见 SD Spec v9.10 Section 4.9.5
     * resp[0] 高16位=RCA, 低16位=部分Card Status */
    err = SendCmd(sdmmc, SD_CMD_SET_REL_ADDR, 0, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;
    card->rca = resp[0] >> 16;

    /* CMD9: SEND_CSD → 解析容量
     * arg=[31:16]=RCA (ac 类命令通用格式, 见 Table 4-22)
     * CSD 位域定义见 SD Spec v9.10 Table 5-3 (V2.0) / Table 5-4 (V1.0)
     * resp[] 映射同 CID: resp[0]=CSD[127:96] ... resp[3]=CSD[31:0] */
    err = SendCmd(sdmmc, SD_CMD_SEND_CSD, card->rca << 16, SDMMC_RESP_LONG, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* TRAN_SPEED = CSD[103:96], 编码规则见 Table 5-6 */
    uint8_t ts = resp[0] & 0xFF;
    card->max_clk_hz = speed_unit[ts & 0x7] / 10 * speed_mult[(ts >> 3) & 0xF];

    if (card->is_sdhc)
    {
        /* V2.0: capacity = (C_SIZE+1) × 512KB, C_SIZE = CSD[69:48] */
        uint32_t c_size = ((resp[1] & 0x3F) << 16) | (resp[2] >> 16);
        card->total_blocks = (c_size + 1) * 1024;
    }
    else
    {
        /* V1.0: capacity = (C_SIZE+1) × 2^(MULT+2) × 2^READ_BL_LEN */
        uint32_t c_size = ((resp[1] & 0x3FF) << 2) | (resp[2] >> 30);
        uint32_t c_size_mult = (resp[2] >> 15) & 0x7;
        uint32_t read_bl_len = (resp[1] >> 16) & 0xF;
        uint32_t block_nr = (c_size + 1) * (1U << (c_size_mult + 2));
        card->total_blocks = block_nr * (1U << read_bl_len) / SD_BLOCK_SIZE;
    }
    card->capacity_mb = card->total_blocks / SD_BLOCKS_PER_MB;

    /* CMD7: SELECT_CARD → Transfer State
     * arg=[31:16]=RCA (Table 4-22), 响应 R1b (带 Busy) */
    err = SendCmd(sdmmc, SD_CMD_SEL_DESEL_CARD, card->rca << 16, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* ACMD6: SET_BUS_WIDTH — arg: 0=1-bit, 2=4-bit (Table 4-43) */
    err = SendAcmd(sdmmc, card->rca, SD_ACMD_SET_BUS_WIDTH, SD_ACMD6_BUS_4BIT, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;
    SdmmcSetBusWidth(sdmmc, SDMMC_BUS_4BIT);

    /* 设定目标速率 (包含时钟提频、UHS-I 电压切换 + 升级速率) */
    return SdCardSetSpeed(card, target_MBps);
}


/* 设定 SD 卡工作速率 (MB/s), 4-bit 总线下:
 *   CK (MHz) = target_MBps × 2
 *   超过 50MHz (即 >25MB/s) 需要 UHS-I 1.8V */
SdCardErr_t SdCardSetSpeed(SdCard_t *card, uint32_t target_MBps)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t bus_width = 4;
    uint32_t target_hz;

    if (target_MBps == 0)
        return SD_OK;

    /* 4-bit: CK = target_MBps × 8 / 4 = target_MBps × 2 (MHz) */
    target_hz = target_MBps * 8000000 / bus_width;

    /* High Speed 模式上限 CK=50MHz, 超过需 UHS-I 电压切换 */
    if (target_hz > SD_HS_MAX_CLK_HZ)
    {
        if (card->s18a)
        {
            /* Step1: DTMODE=01 (SD voltage switching mode) */
            SdmmcSetDtMode(sdmmc, 1);

            /* Step2: CMD11 — 卡收到后拉 DAT0 低, SDMMC 硬件自动等 DAT0 释放 */
            SdmmcErr_t err = SendCmd(sdmmc, SD_CMD_VOLTAGE_SWITCH, SD_CMD11_SWITCH_ARG, SDMMC_RESP_SHORT, NULL);
            if (err != SDMMC_OK)
            {
                SdmmcSetDtMode(sdmmc, 0);
                return SD_ERR_IO;
            }

            /* Step3: 等 DATAEND (硬件检测到 DAT0 释放), 无数据超时 */
            err = SdmmcWaitDataEnd(sdmmc);
            SdmmcClearFlags(sdmmc);
            if (err != SDMMC_OK)
            {
                SdmmcSetDtMode(sdmmc, 0);
                return SD_ERR_IO;
            }

            /* Step4: TODO — 板级电压切换 3.3V→1.8V (需根据具体硬件实现) */
            /* 切换后等待 ≥5ms 让电压稳定 */

            /* Step5: 清除 SD mode, 恢复普通数据传输 */
            SdmmcSetDtMode(sdmmc, 0);
        }
        else
        {
            target_hz = SD_HS_MAX_CLK_HZ;
        }
    }

    SdmmcSetClock(sdmmc, card->ker_ck_hz, target_hz);
    return SD_OK;
}


/* CMD17 (READ_SINGLE_BLOCK) 逐块读取, 命令定义见 Table 4-22 */
SdCardErr_t SdCardReadBlocks(SdCard_t *card, uint32_t block, void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    for (uint32_t i = 0; i < count; i++)
    {
        /* SDHC: arg=块号; SDSC: arg=字节地址 (Section 4.3.14) */
        uint32_t addr = card->is_sdhc ? (block + i) : (block + i) * SD_BLOCK_SIZE;

        SdmmcCmd_t cmd;
        cmd.index     = SD_CMD_READ_SINGLE;
        cmd.arg       = addr;                  /* 数据起始地址 */
        cmd.resp_type = SDMMC_RESP_SHORT;      /* R1 响应 */
        cmd.cmdtrans  = SDMMC_CMD_DATA;
        cmd.cmdstop   = SDMMC_CMD_NOSTOP;

        SdmmcData_t data;
        data.buf_addr      = (uint32_t)((uint8_t *)buf + i * SD_BLOCK_SIZE);  /* IDMA 目标地址 */
        data.len           = SD_BLOCK_SIZE;               /* 单块 = 512 字节 */
        data.block_size_log = SDMMC_BLKSIZ_512B;                /* log2(512) = 9 */
        data.dir_read      = SDMMC_DIR_READ;                 /* 卡→主机 */

        SdmmcErr_t err = SdmmcReadData(sdmmc, &cmd, &data, NULL);
        if (err != SDMMC_OK)
            return SD_ERR_IO;
    }
    return SD_OK;
}


/* CMD24 (WRITE_BLOCK) 逐块写入, 命令定义见 Table 4-22 */
SdCardErr_t SdCardWriteBlocks(SdCard_t *card, uint32_t block, const void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t addr = card->is_sdhc ? (block + i) : (block + i) * SD_BLOCK_SIZE;

        SdmmcCmd_t cmd;
        cmd.index     = SD_CMD_WRITE_SINGLE;
        cmd.arg       = addr;                  /* 数据目标地址 */
        cmd.resp_type = SDMMC_RESP_SHORT;      /* R1 响应 */
        cmd.cmdtrans  = SDMMC_CMD_DATA;
        cmd.cmdstop   = SDMMC_CMD_NOSTOP;

        SdmmcData_t data;
        data.buf_addr      = (uint32_t)((const uint8_t *)buf + i * SD_BLOCK_SIZE);  /* IDMA 源地址 */
        data.len           = SD_BLOCK_SIZE;               /* 单块 = 512 字节 */
        data.block_size_log = SDMMC_BLKSIZ_512B;                /* log2(512) = 9 */
        data.dir_read      = SDMMC_DIR_WRITE;                 /* 主机→卡 */

        SdmmcErr_t err = SdmmcWriteData(sdmmc, &cmd, &data, NULL);
        if (err != SDMMC_OK)
            return SD_ERR_IO;
    }
    return SD_OK;
}


/*
 * CMD23 + CMD18 多块读:
 *   CMD23 (SET_BLOCK_COUNT): 预告诉卡后面要传 count 块,
 *     卡收到后预分配内部资源, 并知道传输何时结束.
 *   CMD18 (READ_MULTIPLE_BLOCK): 开始连续读, 传完 count 块后卡自动停止.
 *   对比纯 CMD17 循环: 省去每块的命令交互开销, DMA 一次搬完.
 */
SdCardErr_t SdCardReadMulti(SdCard_t *card, uint32_t block, void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t addr = card->is_sdhc ? block : block * SD_BLOCK_SIZE;

    /* CMD23: 预设块数, 后续 CMD18 传完自动停, 不需要 CMD12 中止 */
    SdmmcErr_t err = SendCmd(sdmmc, SD_CMD_SET_BLOCK_COUNT, count, SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    SdmmcCmd_t cmd;
    cmd.index     = SD_CMD_READ_MULTI;                    /* CMD18: READ_MULTIPLE_BLOCK */
    cmd.arg       = addr;                  /* 数据起始地址 (或块号) */
    cmd.resp_type = SDMMC_RESP_SHORT;      /* R1 响应 */
    cmd.cmdtrans  = SDMMC_CMD_DATA;
    cmd.cmdstop   = SDMMC_CMD_NOSTOP;

    SdmmcData_t data;
    data.buf_addr      = (uint32_t)buf;    /* IDMA 目标缓冲区 */
    data.len           = count * SD_BLOCK_SIZE;
    data.block_size_log = SDMMC_BLKSIZ_512B;
    data.dir_read      = SDMMC_DIR_READ;

    err = SdmmcReadData(sdmmc, &cmd, &data, NULL);
    return (err == SDMMC_OK) ? SD_OK : SD_ERR_IO;
}


/*
 * CMD23 + CMD25 多块写:
 *   CMD23 (SET_BLOCK_COUNT): 同读, 预设定块数.
 *   CMD25 (WRITE_MULTIPLE_BLOCK): 连续写, 传完 count 块后卡自动停止.
 *   对比纯 CMD24 循环: 省去每块的命令交互和卡编程等待开销.
 */
SdCardErr_t SdCardWriteMulti(SdCard_t *card, uint32_t block, const void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t addr = card->is_sdhc ? block : block * SD_BLOCK_SIZE;

    /* CMD23: 预设块数, 后续 CMD25 传完自动停 */
    SdmmcErr_t err = SendCmd(sdmmc, SD_CMD_SET_BLOCK_COUNT, count, SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    SdmmcCmd_t cmd;
    cmd.index     = SD_CMD_WRITE_MULTI;
    cmd.arg       = addr;
    cmd.resp_type = SDMMC_RESP_SHORT;
    cmd.cmdtrans  = SDMMC_CMD_DATA;
    cmd.cmdstop   = SDMMC_CMD_NOSTOP;                     /* 卡自动停 */

    SdmmcData_t data;
    data.buf_addr      = (uint32_t)buf;    /* IDMA 源缓冲区 */
    data.len           = count * SD_BLOCK_SIZE;      /* 总字节数 */
    data.block_size_log = SDMMC_BLKSIZ_512B;               /* log2(512) = 9 */
    data.dir_read      = SDMMC_DIR_WRITE;                /* 主机→卡 */

    err = SdmmcWriteData(sdmmc, &cmd, &data, NULL);
    return (err == SDMMC_OK) ? SD_OK : SD_ERR_IO;
}


/* CMD32 + CMD33 + CMD38: 擦除指定块范围 (R1b, 卡拉低 DAT0 表示忙) */
SdCardErr_t SdCardErase(SdCard_t *card, uint32_t start_block, uint32_t end_block)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t start_addr = card->is_sdhc ? start_block : start_block * SD_BLOCK_SIZE;
    uint32_t end_addr   = card->is_sdhc ? end_block   : end_block * SD_BLOCK_SIZE;
    SdmmcErr_t err;

    /* CMD32: ERASE_WR_BLK_START */
    err = SendCmd(sdmmc, SD_CMD_ERASE_WR_BLK_START, start_addr, SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* CMD33: ERASE_WR_BLK_END */
    err = SendCmd(sdmmc, SD_CMD_ERASE_WR_BLK_END, end_addr, SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* CMD38: ERASE (R1b), 设 DTIMER 防硬件误报 DTIMEOUT */
    SdmmcSetDataTimeout(sdmmc, 0xFFFFFFFFU);
    SdmmcClearFlags(sdmmc);

    err = SendCmd(sdmmc, SD_CMD_ERASE, 0, SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* 等卡内部擦除完成: DAT0 释放 (BUSYD0=0) */
    err = SdmmcWaitBusy(sdmmc, SDMMC_BUSY_TIMEOUT);
    SdmmcClearFlags(sdmmc);
    return (err == SDMMC_OK) ? SD_OK : SD_ERR_IO;
}

/* 等待写操作后 BUSYD0 释放 (卡内部编程完成) */
SdCardErr_t SdCardWaitBusy(SdCard_t *card)
{
    SdmmcErr_t err = SdmmcWaitBusy(card->sdmmc, SDMMC_BUSY_TIMEOUT);
    return (err == SDMMC_OK) ? SD_OK : SD_ERR_IO;
}

/* 中止卡正在进行的传输 (CMD12), 使卡回到 Transfer 状态 */
SdCardErr_t SdCardStopTransfer(SdCard_t *card)
{
    SdmmcErr_t err = SdmmcSendCmd(card->sdmmc,
        &(SdmmcCmd_t){.index=SD_CMD_STOP_TRANSMISSION, .arg=0,
                      .resp_type=SDMMC_RESP_SHORT,
                      .cmdtrans=SDMMC_CMD_NODATA,
                      .cmdstop=SDMMC_CMD_NOSTOP},
        NULL);
    return (err == SDMMC_OK) ? SD_OK : SD_ERR_IO;
}


/* CMD23 + CMD18 多块读, IDMA 链表 scatter-gather
 * 只计算总块数给 CMD23, 不重复校验描述符 (底层 LlValidate 负责) */
SdCardErr_t SdCardReadMultiLL(SdCard_t *card, uint32_t block,
                              const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t addr = card->is_sdhc ? block : block * SD_BLOCK_SIZE;
    const SdmmcIdmaDesc_t *d = desc;
    uint32_t total = 0;

    if (!desc)
        return SD_ERR_IO;

    while (1)
    {
        total += d->buf_size;
        if (!(d->idmalar & SDMMC_IDMALAR_ULA))
            break;
        d = (const SdmmcIdmaDesc_t *)((const uint8_t *)desc
               + (d->idmalar & 0x0000FFFCU));
    }

    uint32_t n_blocks = total >> block_size_log;

    if (SdmmcSendCmd(sdmmc,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_SET_BLOCK_COUNT,
	            .arg       = n_blocks,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_NODATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP}, NULL) != SDMMC_OK)
        return SD_ERR_IO;

    SdmmcErr_t err = SdmmcReadDataLL(sdmmc,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_READ_MULTI,
	            .arg       = addr,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_DATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP},
            desc, block_size_log, NULL);
    return (err == SDMMC_OK) ? SD_OK : SD_ERR_IO;
}


/* CMD23 + CMD25 多块写, IDMA 链表 scatter-gather
 * 只计算总块数给 CMD23, 不重复校验描述符 (底层 LlValidate 负责) */
SdCardErr_t SdCardWriteMultiLL(SdCard_t *card, uint32_t block,
                               const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t addr = card->is_sdhc ? block : block * SD_BLOCK_SIZE;
    const SdmmcIdmaDesc_t *d = desc;
    uint32_t total = 0;

    if (!desc)
        return SD_ERR_IO;

    while (1)
    {
        total += d->buf_size;
        if (!(d->idmalar & SDMMC_IDMALAR_ULA))
            break;
        d = (const SdmmcIdmaDesc_t *)((const uint8_t *)desc
               + (d->idmalar & 0x0000FFFCU));
    }

    uint32_t n_blocks = total >> block_size_log;

    if (SdmmcSendCmd(sdmmc,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_SET_BLOCK_COUNT,
	            .arg       = n_blocks,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_NODATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP}, NULL) != SDMMC_OK)
        return SD_ERR_IO;

    SdmmcErr_t err = SdmmcWriteDataLL(sdmmc,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_WRITE_MULTI,
	            .arg       = addr,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_DATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP},
            desc, block_size_log, NULL);
    return (err == SDMMC_OK) ? SD_OK : SD_ERR_IO;
}


/* 中断模式多块写: CMD23(轮询) + CMD25(中断, SdmmcStartWriteLL) */
void SdCardStartWriteMultiLL(SdCard_t *card, uint32_t block,
                             const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t addr = card->is_sdhc ? block : block * SD_BLOCK_SIZE;
    const SdmmcIdmaDesc_t *d = desc;
    uint32_t total = 0;

    if (!desc)
    {
        g_sdmmc1_irq.it_result = SDMMC_ERR_PARAM;
        g_sdmmc1_irq.it_done = 1;
        return;
    }

    while (1)
    {
        total += d->buf_size;
        if (!(d->idmalar & SDMMC_IDMALAR_ULA))
            break;
        d = (const SdmmcIdmaDesc_t *)((const uint8_t *)desc
               + (d->idmalar & 0x0000FFFCU));
    }

    uint32_t n_blocks = total >> block_size_log;

    if (SdmmcSendCmd(sdmmc,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_SET_BLOCK_COUNT,
	            .arg       = n_blocks,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_NODATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP}, NULL) != SDMMC_OK)
    {
        g_sdmmc1_irq.it_result = SDMMC_ERR_CTIMEOUT;
        g_sdmmc1_irq.it_done = 1;
        return;
    }

    SdmmcStartWriteLL(sdmmc, &g_sdmmc1_irq,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_WRITE_MULTI,
	            .arg       = addr,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_DATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP},
            desc, block_size_log);
}


/* 中断模式多块读: CMD23(轮询) + CMD18(中断, SdmmcStartReadLL) */
void SdCardStartReadMultiLL(SdCard_t *card, uint32_t block,
                            const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t addr = card->is_sdhc ? block : block * SD_BLOCK_SIZE;
    const SdmmcIdmaDesc_t *d = desc;
    uint32_t total = 0;

    if (!desc)
    {
        g_sdmmc1_irq.it_result = SDMMC_ERR_PARAM;
        g_sdmmc1_irq.it_done = 1;
        return;
    }

    while (1)
    {
        total += d->buf_size;
        if (!(d->idmalar & SDMMC_IDMALAR_ULA))
            break;
        d = (const SdmmcIdmaDesc_t *)((const uint8_t *)desc
               + (d->idmalar & 0x0000FFFCU));
    }

    uint32_t n_blocks = total >> block_size_log;

    if (SdmmcSendCmd(sdmmc,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_SET_BLOCK_COUNT,
	            .arg       = n_blocks,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_NODATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP}, NULL) != SDMMC_OK)
    {
        g_sdmmc1_irq.it_result = SDMMC_ERR_CTIMEOUT;
        g_sdmmc1_irq.it_done = 1;
        return;
    }

    SdmmcStartReadLL(sdmmc, &g_sdmmc1_irq,
            &(SdmmcCmd_t){
	            .index     = SD_CMD_READ_MULTI,
	            .arg       = addr,
	            .resp_type = SDMMC_RESP_SHORT,
	            .cmdtrans  = SDMMC_CMD_DATA,
	            .cmdstop   = SDMMC_CMD_NOSTOP},
            desc, block_size_log);
}
