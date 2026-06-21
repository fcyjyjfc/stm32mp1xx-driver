/*
 * sd_card.c — SD 卡协议层驱动
 *
 * 初始化序列: CMD0 → CMD8 → ACMD41 → CMD2 → CMD3 → CMD9 → CMD7 → ACMD6
 * 数据读写: CMD17 (单块读) / CMD24 (单块写), 多块时循环调用
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
        .cmdtrans = 0,
        .cmdstop = 0,
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
    SdmmcErr_t err = SendCmd(sdmmc, 55, rca << 16, SDMMC_RESP_SHORT, tmp);
    if (err != SDMMC_OK)
        return err;
    /* 检查 APP_CMD 位确认卡准备接收 ACMD */
    if (!(tmp[0] & (1U << 5)))
        return SDMMC_ERR_RESP_MISMATCH;
    return SendCmd(sdmmc, index, arg, resp_type, resp);
}



/* ---- 公开接口 ---- */

/* 初始化序列见 SD Spec v9.10 Figure 4-2 (Card Initialization Flow) */
SdCardErr_t SdCardInit(SdCard_t *card)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t resp[4];
    SdmmcErr_t err;

    /* 上电: ≤400kHz, 1-bit (识别模式时钟要求见 Section 4.2) */
    SdmmcPowerOn(sdmmc, card->ker_ck_hz);

    /* CMD0: GO_IDLE_STATE */
    err = SendCmd(sdmmc, 0, 0, SDMMC_RESP_NONE, NULL);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* CMD8: SEND_IF_COND — arg 格式见 SD Spec v9.10 Section 4.3.13
     * [11:8]=VHS(1=2.7~3.6V), [7:0]=check pattern(任选, 惯例0xAA) */
    err = SendCmd(sdmmc, 8, 0x000001AA, SDMMC_RESP_SHORT, resp);
    if (err == SDMMC_ERR_CTIMEOUT)
        return SD_ERR_UNUSABLE;  /* SD v1.x 或非SD卡 */
    if (err != SDMMC_OK)
        return SD_ERR_IO;
    if ((resp[0] & 0xFFF) != 0x1AA)
        return SD_ERR_UNUSABLE;

    /* ACMD41 循环 — arg/resp 位域见 SD Spec v9.10 Section 5.1 (OCR) + Table 4-41
     * arg: [30]=HCS(1=支持SDHC), [23:15]=电压窗口(0xFF80=2.7~3.6V全覆盖)
     * resp: [31]=Busy(1=完成), [30]=CCS(1=SDHC) — R3 无CRC */
    uint32_t ocr = 0;
    uint32_t retry;
    for (retry = 0; retry < ACMD41_RETRY_MAX; retry++)
    {
        err = SendAcmd(sdmmc, 0, 41, 0x40FF8000, SDMMC_RESP_SHORT_NOCRC, resp);
        if (err != SDMMC_OK)
            return SD_ERR_IO;
        ocr = resp[0];
        if (ocr & (1U << 31))
            break;
    }
    if (retry == ACMD41_RETRY_MAX)
        return SD_ERR_INIT_TIMEOUT;
    card->is_sdhc = (ocr >> 30) & 1;

    /* CMD2: ALL_SEND_CID → 解析卡身份信息
     * CID 位域定义见 SD Spec v9.10 Table 5-2 (Section 5.2)
     * resp[0..3] = CID[127:96], [95:64], [63:32], [31:0] (RM0436 58.6.3) */
    err = SendCmd(sdmmc, 2, 0, SDMMC_RESP_LONG, resp);
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
    err = SendCmd(sdmmc, 3, 0, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;
    card->rca = resp[0] >> 16;

    /* CMD9: SEND_CSD → 解析容量
     * arg=[31:16]=RCA (ac 类命令通用格式, 见 Table 4-22)
     * CSD 位域定义见 SD Spec v9.10 Table 5-3 (V2.0) / Table 5-4 (V1.0)
     * resp[] 映射同 CID: resp[0]=CSD[127:96] ... resp[3]=CSD[31:0] */
    err = SendCmd(sdmmc, 9, card->rca << 16, SDMMC_RESP_LONG, resp);
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
        card->total_blocks = block_nr * (1U << read_bl_len) / 512;
    }
    card->capacity_mb = card->total_blocks / 2048;

    /* 提频到 25MHz */
    SdmmcSetClock(sdmmc, card->ker_ck_hz, 25000000);

    /* CMD7: SELECT_CARD → Transfer State
     * arg=[31:16]=RCA (Table 4-22), 响应 R1b (带 Busy) */
    err = SendCmd(sdmmc, 7, card->rca << 16, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;

    /* ACMD6: SET_BUS_WIDTH — arg: 0=1-bit, 2=4-bit (Table 4-43) */
    err = SendAcmd(sdmmc, card->rca, 6, 2, SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return SD_ERR_IO;
    SdmmcSetBusWidth(sdmmc, SDMMC_BUS_4BIT);

    return SD_OK;
}


/* CMD17 (READ_SINGLE_BLOCK) 逐块读取, 命令定义见 Table 4-22 */
SdCardErr_t SdCardReadBlocks(SdCard_t *card, uint32_t block, void *buf, uint32_t count)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;

    for (uint32_t i = 0; i < count; i++)
    {
        /* SDHC: arg=块号; SDSC: arg=字节地址 (Section 4.3.14) */
        uint32_t addr = card->is_sdhc ? (block + i) : (block + i) * 512;

        SdmmcCmd_t cmd = {
            .index = 17,           /* CMD17: READ_SINGLE_BLOCK */
            .arg = addr,           /* 数据起始地址 */
            .resp_type = SDMMC_RESP_SHORT,  /* R1 响应 */
            .cmdtrans = 1,         /* 命令后跟数据传输, 触发 DPSM */
            .cmdstop = 0,
        };
        SdmmcData_t data = {
            .buf_addr = (uint32_t)((uint8_t *)buf + i * 512),  /* IDMA 目标地址 */
            .len = 512,            /* 单块 = 512 字节 */
            .block_size_log = 9,   /* log2(512) = 9 */
            .dir_read = 1,         /* 卡→主机 */
        };

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
        /* SDHC: arg=块号; SDSC: arg=字节地址 (Section 4.3.14) */
        uint32_t addr = card->is_sdhc ? (block + i) : (block + i) * 512;

        SdmmcCmd_t cmd = {
            .index = 24,           /* CMD24: WRITE_BLOCK */
            .arg = addr,           /* 数据目标地址 */
            .resp_type = SDMMC_RESP_SHORT,  /* R1 响应 */
            .cmdtrans = 1,         /* 命令后跟数据传输, 触发 DPSM */
            .cmdstop = 0,
        };
        SdmmcData_t data = {
            .buf_addr = (uint32_t)((const uint8_t *)buf + i * 512),  /* IDMA 源地址 */
            .len = 512,            /* 单块 = 512 字节 */
            .block_size_log = 9,   /* log2(512) = 9 */
            .dir_read = 0,         /* 主机→卡 */
        };

        SdmmcErr_t err = SdmmcWriteData(sdmmc, &cmd, &data, NULL);
        if (err != SDMMC_OK)
            return SD_ERR_IO;
    }
    return SD_OK;
}
