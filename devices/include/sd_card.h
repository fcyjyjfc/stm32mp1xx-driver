/*
 * sd_card.h — SD 卡协议层驱动
 *
 * 负责: SD 卡初始化序列、块读写
 * 依赖: stm32mp1xx_sdmmc.h (底层外设驱动)
 *
 *  Created on: 2025-6-21
 *      Author: gjsbr
 */

#ifndef SD_CARD_H_
#define SD_CARD_H_

#include "stm32mp1xx_sdmmc.h"


/* SD 卡固定块大小 (512 字节) */
#define SD_BLOCK_SIZE  (1U << SDMMC_BLKSIZ_512B)


/* SD 命令索引 */
typedef enum {
    SD_CMD_GO_IDLE_STATE      = 0,
    SD_CMD_ALL_SEND_CID       = 2,
    SD_CMD_SET_REL_ADDR       = 3,
    SD_CMD_SEL_DESEL_CARD     = 7,
    SD_CMD_SEND_IF_COND       = 8,
    SD_CMD_SEND_CSD           = 9,
    SD_CMD_VOLTAGE_SWITCH     = 11,
    SD_CMD_STOP_TRANSMISSION  = 12,
    SD_CMD_READ_SINGLE        = 17,
    SD_CMD_READ_MULTI         = 18,
    SD_CMD_SET_BLOCK_COUNT    = 23,
    SD_CMD_WRITE_SINGLE       = 24,
    SD_CMD_WRITE_MULTI        = 25,
    SD_CMD_ERASE_WR_BLK_START = 32,
    SD_CMD_ERASE_WR_BLK_END   = 33,
    SD_CMD_ERASE              = 38,
    SD_CMD_APP_CMD            = 55,
} SdCmdIndex_t;

/* ACMD (经 CMD55 前缀后发送) */
#define SD_ACMD_SET_BUS_WIDTH  6
#define SD_ACMD_SEND_OP_COND   41

/* ---- SD 协议常数 ---- */

/* CMD8: SEND_IF_COND */
#define SD_CMD8_VHS_27_36V  (1U << 8)   /* arg[11:8]=1: 2.7~3.6V */
#define SD_CMD8_PATTERN     0xAAU        /* arg[7:0]: check pattern */
#define SD_CMD8_ARG         (SD_CMD8_VHS_27_36V | SD_CMD8_PATTERN)  /* 0x1AA */
#define SD_CMD8_RESP_MASK   0xFFFU       /* R7 低12位 (VHS+pattern) */

/* OCR 位域 (ACMD41 arg / 响应共用) */
#define SD_OCR_BUSY         (1U << 31)   /* 卡初始化完成 */
#define SD_OCR_CCS          (1U << 30)   /* SDHC/SDXC 容量支持 */
#define SD_OCR_S18A         (1U << 24)   /* 1.8V 切换接受 (UHS-I) */

/* ACMD41 arg: HCS + S18R + 全电压窗口 */
#define SD_OCR_HCS          (1U << 30)   /* 主机容量支持 */
#define SD_OCR_S18R         (1U << 24)   /* 1.8V 切换请求 */
#define SD_OCR_VOLTAGE_ALL  0x1FFU       /* 电压窗口位 [23:15] 全选 */
#define SD_ACMD41_ARG       (SD_OCR_HCS | SD_OCR_S18R | (SD_OCR_VOLTAGE_ALL << 15))

/* CMD11: 电压切换 arg — 全电压窗口 */
#define SD_CMD11_SWITCH_ARG  0x7FFF

/* ACMD6: SET_BUS_WIDTH */
#define SD_ACMD6_BUS_4BIT   2            /* 4-bit 总线 */

/* R1 状态位 */
#define SD_R1_APP_CMD       (1U << 5)    /* APP_CMD 标志 */

/* High Speed 模式时钟上限 */
#define SD_HS_MAX_CLK_HZ    50000000U

/* 容量换算: 1MB = 2048 块 (512B/块) */
#define SD_BLOCKS_PER_MB    2048


typedef enum {
    SD_OK = 0,
    SD_ERR_NO_CARD,
    SD_ERR_UNUSABLE,
    SD_ERR_INIT_TIMEOUT,
    SD_ERR_IO,
} SdCardErr_t;

typedef struct {
    volatile SdmmcRegs_t *sdmmc;
    uint32_t ker_ck_hz;
    uint32_t rca;
    uint32_t total_blocks;
    uint32_t capacity_mb;
    uint32_t max_clk_hz;
    uint8_t  is_sdhc;
    uint8_t  s18a;           /* ACMD41 OCR bit24: 1=卡支持1.8V (UHS-I) */
    /* CID 信息 */
    uint8_t  mfr_id;
    char     oem_id[3];
    char     product[6];
    uint8_t  prv_major;
    uint8_t  prv_minor;
    uint32_t serial;
    uint16_t mfr_year;
    uint8_t  mfr_month;
} SdCard_t;


SdCardErr_t SdCardInit(SdCard_t *card, uint32_t target_MBps);
SdCardErr_t SdCardSetSpeed(SdCard_t *card, uint32_t target_MBps);
SdCardErr_t SdCardReadBlocks(SdCard_t *card, uint32_t block, void *buf, uint32_t count);
SdCardErr_t SdCardWriteBlocks(SdCard_t *card, uint32_t block, const void *buf, uint32_t count);

SdCardErr_t SdCardReadMulti(SdCard_t *card, uint32_t block, void *buf, uint32_t count);
SdCardErr_t SdCardWriteMulti(SdCard_t *card, uint32_t block, const void *buf, uint32_t count);

SdCardErr_t SdCardReadMultiLL(SdCard_t *card, uint32_t block,
                              const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log);
SdCardErr_t SdCardWriteMultiLL(SdCard_t *card, uint32_t block,
                               const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log);

/* 等待写操作后 BUSYD0 释放 (卡内部编程完成) */
SdCardErr_t SdCardWaitBusy(SdCard_t *card);

/* 中止卡正在进行的传输 (CMD12), 使卡回到 Transfer 状态 */
SdCardErr_t SdCardStopTransfer(SdCard_t *card);

/* 擦除: CMD32 + CMD33 + CMD38 (R1b), 阻塞等卡忙结束 */
SdCardErr_t SdCardErase(SdCard_t *card, uint32_t start_block, uint32_t end_block);

/* 中断模式: CMD23 用轮询, 数据传输用中断, 返回后前台轮询 g_sdmmc1_irq.it_done */
void SdCardStartWriteMultiLL(SdCard_t *card, uint32_t block,
                             const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log);
void SdCardStartReadMultiLL(SdCard_t *card, uint32_t block,
                            const SdmmcIdmaDesc_t *desc, SdmmcBlockSize_t block_size_log);


#endif /* SD_CARD_H_ */
