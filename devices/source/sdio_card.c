/*
 * sdio_card.c — SDIO 协议层驱动
 *
 * 初始化: CMD0 → CMD5(poll C=1) → CMD3 → CMD7
 * IO-only 卡跳过 CMD2 (无 CID), 由主机分配 RCA.
 *
 *  Created on: 2026-7-9
 *      Author: gjsbr
 */

#include <stddef.h>
#include "sdio_card.h"

/* CMD5 最大重试次数 (典型充满电卡 5ms, 新卡最坏 1s, 折中) */
#define CMD5_RETRY_MAX  100000


/* ---- 内部辅助: 封装纯命令 (无数据) ---- */
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
 * @brief SDIO 卡识别
 *
 * 流程: CMD0 → CMD5(arg=0 探测) → CMD5(arg=OCR, 轮询 C=1)
 *       → CMD3(设 RCA) → CMD7(选卡, 进 Transfer)
 *
 * IO-only 卡不支持 CMD2, 跳过 CID 读取.
 * R4 无 CRC7, 使用 SDMMC_RESP_SHORT_NOCRC.
 *
 * @return SDIO_OK / SDIO_ERR_NO_CARD / SDIO_ERR_INIT_TIMEOUT / SDIO_ERR_IO
 */
SdioCardErr_t SdioCardIdentify(SdioCard_t *card)
{
    volatile SdmmcRegs_t *sdmmc = card->sdmmc;
    uint32_t resp[4];
    SdmmcErr_t err;
    uint32_t retry;

    /*
     * CMD0: GO_IDLE_STATE
     * 复位卡的 SD 接口部分 (I/O 功能不受 CMD0 影响).
     * 卡从任何状态回到 Idle, 后续只认 CMD5.
     */
    err = SendCmd(sdmmc, SDIO_CMD_GO_IDLE_STATE, 0, SDMMC_RESP_NONE, NULL);
    if (err != SDMMC_OK)
        return SDIO_ERR_IO;

    /*
     * 降时钟至 ≤400kHz, 总线 1-bit:
     * 识别阶段时钟必须 ≤400kHz (SD 物理层规范), 主机侧匹配.
     */
    SdmmcSetClock(sdmmc, card->ker_ck_hz, 400000);
    SdmmcSetBusWidth(sdmmc, SDMMC_BUS_1BIT);

    /*
     * CMD8: SEND_IF_COND — 探测 SDHC/SDXC 支持
     * 纯 SDIO 卡不响应, 超时正常, 忽略.
     */
    card->vhs = 0;
    err = SendCmd(sdmmc, SDIO_CMD_SEND_IF_COND, 0x1AA,
                  SDMMC_RESP_SHORT_NOCRC, resp);
    if (err == SDMMC_OK)
        card->vhs = 1;

    /*
     * CMD5(arg=0): IO_SEND_OP_COND 探测
     * 发 arg=0 仅探测卡是否 SDIO, 不设电压.
     * 如果卡响应 R4 → SDIO 卡; 超时 → 非 SDIO.
     *
     * R4 无 CRC7 → SDMMC_RESP_SHORT_NOCRC.
     * 正常 48-bit 响应中 response data (RESP1R[31:0]) 格式:
     *   [31]=C, [30:28]=NF, [26]=MP, [25]=S18A, [24:0]=I/O OCR
     */
    err = SendCmd(sdmmc, SDIO_CMD_IO_SEND_OP_COND, 0,
                  SDMMC_RESP_SHORT_NOCRC, resp);
    if (err == SDMMC_ERR_CTIMEOUT)
        return SDIO_ERR_NO_CARD;
    if (err != SDMMC_OK)
        return SDIO_ERR_IO;

    /* 提取 I/O OCR (bits[24:0]), 后续写回协商电压 */
    uint32_t io_ocr = SDIO_R4_OCR(resp[0]);
    if (io_ocr == 0)
        return SDIO_ERR_NO_CARD;

    /*
     * CMD5(arg=OCR): 设置工作电压, 轮询等待卡就绪
     * arg = S18R(1.8V 切换请求) + I/O OCR 位.
     * 卡内部初始化所有 I/O Function, C=1 时表示所有功能已就绪.
     */
    {
        uint32_t ocr_arg = io_ocr | SDIO_CMD5_S18R;
        for (retry = 0; retry < CMD5_RETRY_MAX; retry++)
        {
            err = SendCmd(sdmmc, SDIO_CMD_IO_SEND_OP_COND, ocr_arg,
                          SDMMC_RESP_SHORT_NOCRC, resp);
            if (err == SDMMC_ERR_CTIMEOUT)
                return SDIO_ERR_NO_CARD;
            if (err != SDMMC_OK)
                return SDIO_ERR_IO;

            if (resp[0] & SDIO_R4_C)
                break;
        }
        if (retry == CMD5_RETRY_MAX)
            return SDIO_ERR_INIT_TIMEOUT;
    }

    /* 保存 R4 信息 */
    card->raw_r4 = resp[0];
    card->nf    = SDIO_R4_NF(resp[0]);
    card->mp    = (resp[0] & SDIO_R4_MP) ? 1 : 0;
    card->s18a  = (resp[0] & SDIO_R4_S18A) ? 1 : 0;
    card->io_ocr = io_ocr;

    /*
     * CMD3: SET_REL_ADDR
     * SDIO 的 RCA 由主机分配. 先发预置 RCA, 从 R6 响应提取实际 RCA.
     * 命令后卡从 Ready 进 Stand-by 态.
     * R6 响应有 CRC7 → SDMMC_RESP_SHORT.
     */
    card->rca = SDIO_RCA;
    err = SendCmd(sdmmc, SDIO_CMD_SET_REL_ADDR, card->rca << 16,
                  SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
        return SDIO_ERR_IO;
    card->raw_r6 = resp[0];
    card->rca    = (resp[0] >> 16) & 0xFFFF;

    /*
     * CMD7: SEL_DESEL_CARD
     * 选卡, 从 Stand-by 进 Transfer 态, 之后可发 CMD52 访问 CCCR.
     * R1b 响应 → SDMMC_RESP_SHORT.
     */
    err = SendCmd(sdmmc, SDIO_CMD_SEL_DESEL_CARD, card->rca << 16,
                  SDMMC_RESP_SHORT, NULL);
    if (err != SDMMC_OK)
        return SDIO_ERR_IO;

    return SDIO_OK;
}


/**
 * @brief CMD52 单字节读 (Function 0, RAW=0)
 *
 * 读取 CCCR/FBR 或 Function 寄存器中的单字节.
 * R5 响应的 bits[7:0] = 读回数据.
 *
 * @param addr  寄存器地址 (0~0x1FFFF)
 * @param val   输出缓冲区, 存放读回的数据
 * @return SDIO_OK / SDIO_ERR_IO
 */
SdioCardErr_t SdioCardReadB(SdioCard_t *card, uint16_t addr, uint8_t *val)
{
    uint32_t resp[4];
    uint32_t arg = SDIO_CMD52_ADDR(addr);  /* RW=0, Func=0, RAW=0 */

    SdmmcErr_t err = SendCmd(card->sdmmc, SDIO_CMD_IO_RW_DIRECT, arg,
                              SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
    {
        if (val) *val = 0;
        return SDIO_ERR_IO;
    }

    if (val)
        *val = (uint8_t)(resp[0] & 0xFF);
    return SDIO_OK;
}


/**
 * @brief CMD52 单字节写 (Function 0, RAW=0)
 *
 * 写指定数据到寄存器. RAW=0: R5 返回写入值, 不回读.
 *
 * @param addr  寄存器地址 (0~0x1FFFF)
 * @param val   要写入的数据
 * @return SDIO_OK / SDIO_ERR_IO
 */
SdioCardErr_t SdioCardWriteB(SdioCard_t *card, uint16_t addr, uint8_t val)
{
    uint32_t arg = SDIO_CMD52_RW | SDIO_CMD52_ADDR(addr) | SDIO_CMD52_DATA(val);

    SdmmcErr_t err = SendCmd(card->sdmmc, SDIO_CMD_IO_RW_DIRECT, arg,
                              SDMMC_RESP_SHORT, NULL);
    return (err == SDMMC_OK) ? SDIO_OK : SDIO_ERR_IO;
}


/**
 * @brief CMD52 单字节写后读回 (Function 0, RAW=1)
 *
 * RAW=1 模式: 写操作完成后自动回读该寄存器, R5 bits[7:0] 返回实际值.
 * 适用于检查配置是否生效, 或一次命令完成写+读.
 *
 * @param addr      寄存器地址 (0~0x1FFFF)
 * @param write_val 要写入的数据
 * @param read_val  输出缓冲区, 存放写后读回的数据
 * @return SDIO_OK / SDIO_ERR_IO
 */
SdioCardErr_t SdioCardWriteReadB(SdioCard_t *card, uint16_t addr,
                                  uint8_t write_val, uint8_t *read_val)
{
    uint32_t resp[4];
    uint32_t arg = SDIO_CMD52_RW | SDIO_CMD52_RAW
                 | SDIO_CMD52_ADDR(addr) | SDIO_CMD52_DATA(write_val);

    SdmmcErr_t err = SendCmd(card->sdmmc, SDIO_CMD_IO_RW_DIRECT, arg,
                              SDMMC_RESP_SHORT, resp);
    if (err != SDMMC_OK)
    {
        if (read_val) *read_val = 0;
        return SDIO_ERR_IO;
    }

    if (read_val)
        *read_val = (uint8_t)(resp[0] & 0xFF);
    return SDIO_OK;
}


/**
 * @brief 读 CCCR 寄存器验证卡
 *
 * 通过 CMD52 读取 CCCR 区域的头几个寄存器确认卡是否正确初始化.
 * 填充 card->cccr_rev, card->sdio_rev, card->sd_spec_rev.
 *
 * @return SDIO_OK / SDIO_ERR_IO
 */
SdioCardErr_t SdioCardReadCccr(SdioCard_t *card)
{
    uint8_t rev, sd_rev;

    /* 读 CCCR 版本 [3:0] + SDIO 版本 [7:4] */
    SdioCardErr_t err = SdioCardReadB(card, SDIO_CCCR_REV, &rev);
    if (err != SDIO_OK)
        return err;

    card->cccr_rev  = rev & 0x0F;    /* bits [3:0] = CCCR/FBR 版本 */
    card->sdio_rev  = (rev >> 4) & 0x0F;

    /* 读 SD 物理层版本 */
    err = SdioCardReadB(card, SDIO_CCCR_SD_REV, &sd_rev);
    if (err != SDIO_OK)
        return err;

    card->sd_spec_rev = sd_rev & 0x0F;

    return SDIO_OK;
}


/**
 * @brief 读 CIS: 遍历元组, 提取制造商 / SDIO 版本 / Function 块大小
 *
 * 从 CCCR 0x09~0x0B 读 CIS 指针, 逐元组扫描:
 *   - CISTPL_MANFID(0x20): 制造商代码 + 信息
 *   - CISTPL_SDIO_STD(0x91): SDIO 版本 + Function 计数
 *   - CISTPL_FUNCE(0x22): Function 最大块大小 (TPLFE_TYPE=0x0C)
 *   - CISTPL_END(0xFF): 结束
 *
 * @return SDIO_OK / SDIO_ERR_IO
 */
SdioCardErr_t SdioCardReadCis(SdioCard_t *card)
{
    uint8_t b[3];
    uint32_t addr;
    SdioCardErr_t err;

    /* 清 CIS 结构 */
    SdioCisInfo_t *cis = &card->cis;
    cis->manf_id  = 0;
    cis->manf_info = 0;
    cis->sdio_ver = 0;
    cis->func_cnt = 0;
    for (int i = 0; i < SDIO_MAX_FUNC; i++)
        cis->func_blksz[i] = 0;

    /* 读 CIS 指针 (CCCR 0x09~0x0B, 24-bit LE) */
    err  = SdioCardReadB(card, SDIO_CCCR_CIS_PTR,     &b[0]);
    err |= SdioCardReadB(card, SDIO_CCCR_CIS_PTR + 1, &b[1]);
    err |= SdioCardReadB(card, SDIO_CCCR_CIS_PTR + 2, &b[2]);
    if (err != SDIO_OK) return SDIO_ERR_IO;

    addr = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
    cis->cis_ptr = addr;

    /* 遍历 CIS 元组 */
    while (addr < 0x10000)
    {
        uint8_t tuple, link;

        err = SdioCardReadB(card, (uint16_t)addr, &tuple);
        if (err != SDIO_OK) return SDIO_ERR_IO;

        if (tuple == SDIO_CISTPL_END)
            break;

        err = SdioCardReadB(card, (uint16_t)(addr + 1), &link);
        if (err != SDIO_OK) return SDIO_ERR_IO;

        switch (tuple)
        {
        case SDIO_CISTPL_MANFID:  /* 0x20 */
            if (link >= 2)
            {
                uint8_t lo, hi;
                SdioCardReadB(card, (uint16_t)(addr + 2), &lo);
                SdioCardReadB(card, (uint16_t)(addr + 3), &hi);
                cis->manf_id = (uint16_t)lo | ((uint16_t)hi << 8);
            }
            if (link >= 4)
            {
                uint8_t lo, hi;
                SdioCardReadB(card, (uint16_t)(addr + 4), &lo);
                SdioCardReadB(card, (uint16_t)(addr + 5), &hi);
                cis->manf_info = (uint16_t)lo | ((uint16_t)hi << 8);
            }
            break;

        case SDIO_CISTPL_SDIO_STD:  /* 0x91 */
            if (link >= 1)
            {
                uint8_t v;
                SdioCardReadB(card, (uint16_t)(addr + 2), &v);
                cis->sdio_ver = (v >> 4) & 0x0F;
                cis->func_cnt = v & 0x0F;
            }
            break;

        case SDIO_CISTPL_FUNCE:  /* 0x22 */
            /* SDIO FUNCE: TPLFE_TYPE 在 body[0] */
            if (link >= 2)
            {
                uint8_t fe_type;
                SdioCardReadB(card, (uint16_t)(addr + 2), &fe_type);
                if (fe_type == 0x0C && link >= 5)
                {
                    /* body[1]=fn, body[2..4]=max_blksz (24-bit LE) */
                    uint8_t fn, sz0, sz1, sz2;
                    SdioCardReadB(card, (uint16_t)(addr + 3), &fn);
                    SdioCardReadB(card, (uint16_t)(addr + 4), &sz0);
                    SdioCardReadB(card, (uint16_t)(addr + 5), &sz1);
                    SdioCardReadB(card, (uint16_t)(addr + 6), &sz2);
                    if (fn >= 1 && fn <= SDIO_MAX_FUNC)
                        cis->func_blksz[fn - 1] = (uint32_t)sz0
                                                  | ((uint32_t)sz1 << 8)
                                                  | ((uint32_t)sz2 << 16);
                }
            }
            break;

        default:
            break;
        }

        addr += 2 + link;
    }

    return SDIO_OK;
}


/**
 * @brief 读全部卡信息: CCCR + CIS
 *
 * 封装 SdioCardReadCccr + SdioCardReadCis, 一次取出所有信息.
 *
 * @return SDIO_OK / SDIO_ERR_IO
 */
SdioCardErr_t SdioCardReadInfo(SdioCard_t *card)
{
    SdioCardErr_t err;

    err = SdioCardReadCccr(card);
    if (err != SDIO_OK)
        return err;

    err = SdioCardReadCis(card);
    if (err != SDIO_OK)
        return err;

    return SDIO_OK;
}
