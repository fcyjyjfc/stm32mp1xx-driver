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
                              const SdmmcIdmaDesc_t *desc, uint8_t block_size_log);
SdCardErr_t SdCardWriteMultiLL(SdCard_t *card, uint32_t block,
                               const SdmmcIdmaDesc_t *desc, uint8_t block_size_log);

/* 中断模式: CMD23 用轮询, 数据传输用中断, 返回后前台轮询 g_sdmmc1_it_done */
void SdCardStartWriteMultiLL(SdCard_t *card, uint32_t block,
                             const SdmmcIdmaDesc_t *desc, uint8_t block_size_log);
void SdCardStartReadMultiLL(SdCard_t *card, uint32_t block,
                            const SdmmcIdmaDesc_t *desc, uint8_t block_size_log);


#endif /* SD_CARD_H_ */
