/*
 * emmc_card.h — eMMC 协议层驱动
 *
 * 负责: eMMC 初始化序列、块读写
 * 依赖: stm32mp1xx_sdmmc.h (底层外设驱动)
 *
 *  Created on: 2026-6-27
 *      Author: gjsbr
 */

#ifndef EMMC_CARD_H_
#define EMMC_CARD_H_

#include "stm32mp1xx_sdmmc.h"


/* eMMC 固定块大小 (512 字节) */
#define EMMC_BLOCK_SIZE  (1U << SDMMC_BLKSIZ_512B)

/* eMMC 固定 RCA */
#define EMMC_RCA         0x0001U

/* 容量换算: 1MB = 2048 块 (512B/块) */
#define EMMC_BLOCKS_PER_MB  2048


/* ===== eMMC 命令索引 ===== */
typedef enum {
    EMMC_CMD_GO_IDLE_STATE    = 0,
    EMMC_CMD_SEND_OP_COND     = 1,   /* CMD1: 查询 OCR, R3 无 CRC */
    EMMC_CMD_ALL_SEND_CID     = 2,
    EMMC_CMD_SET_REL_ADDR     = 3,
    EMMC_CMD_SLEEP_AWAKE      = 5,   /* CMD5: Sleep/Awake */
    EMMC_CMD_SWITCH           = 6,   /* CMD6: 写 EXT_CSD, R1b */
    EMMC_CMD_SEL_DESEL_CARD   = 7,
    EMMC_CMD_SEND_EXT_CSD     = 8,   /* CMD8: 读 512 字节 EXT_CSD */
    EMMC_CMD_SEND_CSD         = 9,
    EMMC_CMD_SEND_CID         = 10,
    EMMC_CMD_STOP_TRANSMISSION = 12,
    EMMC_CMD_SEND_STATUS      = 13,
    EMMC_CMD_READ_SINGLE      = 17,
    EMMC_CMD_READ_MULTI       = 18,
    EMMC_CMD_SET_BLOCK_COUNT  = 23,
    EMMC_CMD_WRITE_SINGLE     = 24,
    EMMC_CMD_WRITE_MULTI      = 25,
    EMMC_CMD_SET_WRITE_PROT   = 28,  /* CMD28: 设写保护, R1b */
    EMMC_CMD_CLR_WRITE_PROT   = 29,  /* CMD29: 清写保护, R1b */
    EMMC_CMD_SEND_WRITE_PROT  = 30,  /* CMD30: 读 WP 状态, R1 + 32B */
    EMMC_CMD_SEND_WRITE_PROT_TYPE = 31, /* CMD31: 读 WP 类型, R1 + 32B */
    EMMC_CMD_ERASE_GRP_START  = 35,
    EMMC_CMD_ERASE_GRP_END    = 36,
    EMMC_CMD_ERASE            = 38,
    EMMC_CMD_APP_CMD          = 55,  /* ACMD 前缀, 与 SD 一致 */
} EmmcCmdIndex_t;


/* ---- CMD1: SEND_OP_COND ---- */
#define EMMC_OCR_BUSY         (1U << 31)  /* bit31: 0=忙, 1=就绪 */
#define EMMC_OCR_ACCESS_MODE  (1U << 30)  /* bit30: 0=字节, 1=扇区 (HCS) */
#define EMMC_OCR_VDD_27_36V   0x00FF8000U /* 2.7~3.6V 全窗口 */
#define EMMC_CMD1_ARG         (EMMC_OCR_ACCESS_MODE | EMMC_OCR_VDD_27_36V)  /* 0x40FF8000 */


/* ---- CMD5: SLEEP_AWAKE, arg [31:16]=RCA, [15]=Sleep/Awake ---- */
#define EMMC_CMD5_SLEEP  (1U << 15)  /* arg bit15=1: 进入 Sleep */
#define EMMC_CMD5_AWAKE  0U           /* arg bit15=0: 唤醒 */


/* ===== CMD6 SWITCH 参数 ===== */

/* CMD6 Access Mode: bits[25:24] */
typedef enum {
    EMMC_SWITCH_ACCESS_CMD_SET    = 0,  /* 切换命令集 */
    EMMC_SWITCH_ACCESS_SET_BITS   = 1,  /* 置位 */
    EMMC_SWITCH_ACCESS_CLR_BITS   = 2,  /* 清零 */
    EMMC_SWITCH_ACCESS_WRITE_BYTE = 3,  /* 写字节 (最常用) */
} EmmcSwitchAccess_t;

/* 构造 CMD6 SWITCH 参数: access<<24 | index<<16 | value<<8 */
static inline uint32_t EmmcSwitchArg(EmmcSwitchAccess_t access, uint8_t index, uint8_t value)
{
    return ((uint32_t)access << 24) | ((uint32_t)index << 16) | ((uint32_t)value << 8);
}


/* ===== EXT_CSD 字节索引 ===== */
#define EMMC_EXT_CSD_BUS_WIDTH        183   /* 总线宽度 (W/E_P) */
#define EMMC_EXT_CSD_HS_TIMING        185   /* HS时序接口 (R/W/E_P) */
#define EMMC_EXT_CSD_EXT_CSD_REV      192   /* EXT_CSD 版本 (R) */
#define EMMC_EXT_CSD_SEC_COUNT        212   /* 扇区数 [215:212] (R) */
#define EMMC_EXT_CSD_HC_WP_GRP_SIZE   221   /* HC 写保护组大小 (R) */
#define EMMC_EXT_CSD_HC_ERASE_GRP_SIZE 224  /* HC 擦除组大小 (R) */


/* ===== EXT_CSD BUS_WIDTH[183] 值 ===== */
/*
 * BUS_WIDTH[183] 值 (Table 145, JESD84-B51):
 *   0 = 1-bit SDR,  1 = 4-bit SDR,  2 = 8-bit SDR
 *   5 = 4-bit DDR,  6 = 8-bit DDR
 * HS_TIMING 必须在设 DDR BUS_WIDTH 前设为 0x1
 */
typedef enum {
    EMMC_BUS_WIDTH_1BIT      = 0,  /* 1-bit SDR (上电默认) */
    EMMC_BUS_WIDTH_4BIT      = 1,  /* 4-bit SDR */
    EMMC_BUS_WIDTH_8BIT      = 2,  /* 8-bit SDR */
    EMMC_BUS_WIDTH_4BIT_DDR  = 5,  /* 4-bit DDR (HS_TIMING=1 前提) */
    EMMC_BUS_WIDTH_8BIT_DDR  = 6,  /* 8-bit DDR (HS_TIMING=1 前提) */
} EmmcBusWidth_t;


/* ===== EXT_CSD HS_TIMING[185] 值 ===== */
typedef enum {
    EMMC_HS_TIMING_BC = 0,  /* Backward Compatible (上电默认) */
    EMMC_HS_TIMING_HS = 1,  /* High Speed (≤52MHz) */
} EmmcHsTiming_t;


/* ===== eMMC 卡状态 (R1 bits 12:9) ===== */
typedef enum {
    EMMC_STATE_IDLE  = 0,
    EMMC_STATE_READY = 1,
    EMMC_STATE_IDENT = 2,
    EMMC_STATE_STBY  = 3,
    EMMC_STATE_TRAN  = 4,  /* Transfer: 正常读写 */
    EMMC_STATE_DATA  = 5,
    EMMC_STATE_RCV   = 6,
    EMMC_STATE_PRG   = 7,
    EMMC_STATE_DIS   = 8,
    EMMC_STATE_BTST  = 9,
    EMMC_STATE_SLP   = 10, /* Sleep: 低功耗 */
} EmmcCardState_t;


/* ===== 卡详细状态 (EmmcCardReadStatus) ===== */
typedef struct {
    /* CMD13 R1 原始值 */
    uint32_t r1;

    /* 卡状态 */
    EmmcCardState_t  card_state;         /* bits 12:9 */
    uint8_t  ready_for_data;     /* bit 8 */
    uint8_t  switch_error;       /* bit 7 */
    uint8_t  app_cmd;            /* bit 5 */

    /* 主机侧实时配置 (CLKCR) */
    uint8_t  host_bus_width;     /* 1/4/8 */
    uint8_t  host_ddr;           /* 0=SDR, 1=DDR */
    uint8_t  host_bus_speed;     /* BUSSPEED */
    uint8_t  host_negedge;       /* NEGEDGE */
    uint8_t  host_hwfc;          /* HWFC_EN */
    uint8_t  host_selclkrx;      /* SELCLKRX[1:0] */
    uint16_t host_clkdiv;        /* CLKDIV[9:0] */

    /* 卡侧缓存配置 (上次写入值, BUS_WIDTH/HS_TIMING 为只写不可读回) */
    uint8_t  cached_bus_width;   /* BUS_WIDTH[183] 缓存 */
    uint8_t  cached_hs_timing;   /* HS_TIMING[185] 缓存 */

    /* 时钟 (Hz) */
    uint32_t ker_ck_hz;          /* 内核时钟 */
    uint32_t sdmmc_ck_hz;        /* SDMMC_CK = ker_ck / (2*CLKDIV) */
} EmmcCardDetailedStatus_t;


/* ---- R1 状态位 ---- */
#define EMMC_R1_SWITCH_ERROR   (1U << 7)  /* CMD6 SWITCH 失败 */
#define EMMC_R1_APP_CMD        (1U << 5)  /* APP_CMD 标志 */
#define EMMC_R1_READY_FOR_DATA (1U << 8)  /* 缓冲区就绪 */

/* ---- CMD13 返回的状态 (R1) 解析 ---- */
typedef struct {
    uint32_t          raw;            /* 原始 R1 状态值 */
    EmmcCardState_t   state;          /* bits 12:9 — 卡状态 */
    uint8_t           ready_for_data; /* bit 8 */
    uint8_t           switch_error;   /* bit 7 */
    uint8_t           app_cmd;        /* bit 5 */
    uint8_t           ake_seq_error;  /* bit 3 */
    uint8_t           wp_violation;   /* bit 0 */
} EmmcCardStatus_t;


/* ===== 错误码 ===== */
typedef enum {
    EMMC_OK = 0,
    EMMC_ERR_NO_CARD,
    EMMC_ERR_INIT_TIMEOUT,
    EMMC_ERR_IO,
    EMMC_ERR_PARAM,
    EMMC_ERR_WRONG_STATE,  /* 卡不在期望的状态 (如 Sleep 时调用读写) */
    EMMC_ERR_SLEEP_CMD7,   /* CMD7 deselect 失败 */
    EMMC_ERR_SLEEP_CMD5,   /* CMD5 sleep 失败 */
    EMMC_ERR_SLEEP_BUSY,   /* Sleep DAT0 busy 超时 */
    EMMC_ERR_AWAKE_CMD5,   /* CMD5 awake 失败 */
    EMMC_ERR_AWAKE_BUSY,   /* Awake DAT0 busy 超时 */
    EMMC_ERR_AWAKE_CMD7,   /* CMD7 reselect 失败 */
} EmmcCardErr_t;


/* ===== 卡结构体 ===== */
typedef struct {
    volatile SdmmcRegs_t *sdmmc;
    uint32_t ker_ck_hz;
    uint32_t rca;
    uint32_t total_blocks;
    uint32_t capacity_mb;
    uint32_t max_clk_hz;
    uint32_t wp_grp_size_blocks;    /* 写保护组大小 (块数) */
    uint16_t ccc;                   /* 命令类支持位图 (CSD bits 83:72) */
    uint32_t csd_resp[4];           /* 原始 CSD R2 响应, 供调试 */
    /* CID */
    uint8_t  mfr_id;
    char     oem_id[3];
    char     product[6];
    uint8_t  prv_major;
    uint8_t  prv_minor;
    uint32_t serial;
    uint16_t mfr_year;
    uint8_t  mfr_month;
    /* EXT_CSD */
    uint8_t  ext_csd_rev;
    uint8_t  ext_csd[512];
} EmmcCard_t;


/* ===== 函数声明 ===== */

/* 识别: CMD0→CMD1→CMD2→CMD3, 卡进 Stand-by (跳过热启动, 用 u-boot 配置) */
EmmcCardErr_t EmmcCardIdentify(EmmcCard_t *card);

/* 选择卡: CMD7 SEL_DESEL_CARD (Stand-by → Transfer), 卡进 Transfer 态 */
EmmcCardErr_t EmmcCardSelect(EmmcCard_t *card);

/* 设置总线宽度: CMD6 EXT_CSD[183] + SdmmcSetBusWidth, 同时设置卡与外设 */
EmmcCardErr_t EmmcCardSetBusWidth(EmmcCard_t *card, EmmcBusWidth_t width);

/* 设置时钟: CMD6 EXT_CSD[185] HS Timing + SdmmcSetClock, target_hz>26MHz 自动切 HS */
EmmcCardErr_t EmmcCardSetClock(EmmcCard_t *card, uint32_t target_hz);

/* DDR 模式切换: EmmcCardSetClock(HS_TIMING=1) → EmmcCardSetBusWidth(DDR), JESD84 规范顺序 */
EmmcCardErr_t EmmcCardSetDdrMode(EmmcCard_t *card, EmmcBusWidth_t width, uint32_t target_hz);

/* 获取卡信息: CMD9(CSD) + CMD8(EXT_CSD), 需卡在 Transfer 态 */
EmmcCardErr_t EmmcCardGetInfo(EmmcCard_t *card);

/* 单块读写 */
EmmcCardErr_t EmmcCardReadBlocks(EmmcCard_t *card, uint32_t block, void *buf, uint32_t count);
EmmcCardErr_t EmmcCardWriteBlocks(EmmcCard_t *card, uint32_t block, const void *buf, uint32_t count);

/* 多块读写 (CMD23 预定义块数 + CMD18/25) */
EmmcCardErr_t EmmcCardReadMulti(EmmcCard_t *card, uint32_t block, void *buf, uint32_t count);
EmmcCardErr_t EmmcCardWriteMulti(EmmcCard_t *card, uint32_t block, const void *buf, uint32_t count);

/* 中断模式: CMD23 用轮询, 数据传输用中断, 返回后前台轮询 g_sdmmc2_irq.it_done */
void EmmcCardStartWriteMultiLL(EmmcCard_t *card, uint32_t block,
                                const SdmmcIdmaDesc_t *desc,
                                SdmmcBlockSize_t block_size_log);
void EmmcCardStartReadMultiLL(EmmcCard_t *card, uint32_t block,
                               const SdmmcIdmaDesc_t *desc,
                               SdmmcBlockSize_t block_size_log);

/* 擦除: CMD35(ERASE_GRP_START) → CMD36(ERASE_GRP_END) → CMD38(ERASE), R1b */
EmmcCardErr_t EmmcCardErase(EmmcCard_t *card, uint32_t start_block, uint32_t end_block);

/* 写保护: CMD28 SET_WRITE_PROT / CMD29 CLR_WRITE_PROT, R1b */
EmmcCardErr_t EmmcCardSetWriteProtect(EmmcCard_t *card, uint32_t block);
EmmcCardErr_t EmmcCardClearWriteProtect(EmmcCard_t *card, uint32_t block);

/* 读取写保护状态: CMD30 SEND_WRITE_PROT, 返回 32 bits (4 字节, 32 组)
 *   block: 块地址, 设备返回包含该块的 WP 组及其后 31 组的 bitmap
 *   JESD84-B51 Table 53 Note 2: 32 bits + 16 bits CRC, 硬件自动处理 CRC */
EmmcCardErr_t EmmcCardGetWriteProtect(EmmcCard_t *card, uint32_t block, uint8_t wp_status[4]);

/* 读取写保护类型: CMD31 SEND_WRITE_PROT_TYPE, 返回 64 bits (8 字节, 32 组, 每组 2 bits)
 *   block: 块地址, 设备返回包含该块的 WP 组及其后 31 组的类型
 *   编码 (JESD84-B51 Table 53 Note 4):
 *     00=未保护, 01=临时保护, 10=上电保护, 11=永久保护 */
EmmcCardErr_t EmmcCardGetWriteProtectType(EmmcCard_t *card, uint32_t block, uint8_t wp_type[8]);

/* 等待写操作后 BUSYD0 释放 (设备内部编程完成) */
EmmcCardErr_t EmmcCardWaitBusy(EmmcCard_t *card);

/* Sleep/Awake: CMD5, R1b. Sleep 前卡需在 Transfer 态, 唤醒后回到 Transfer 态 */
EmmcCardErr_t EmmcCardSleep(EmmcCard_t *card);
EmmcCardErr_t EmmcCardAwake(EmmcCard_t *card);

/* 发 CMD13 SEND_STATUS 查询当前设备状态, 自动解析至 EmmcCardStatus_t */
EmmcCardErr_t EmmcCardSendStatus(EmmcCard_t *card, EmmcCardStatus_t *sts);

/* 读取卡详细状态: CMD13 + CLKCR + 缓存, 一站式获取状态/配置/时钟 */
EmmcCardErr_t EmmcCardReadStatus(EmmcCard_t *card, EmmcCardDetailedStatus_t *sts);


#endif /* EMMC_CARD_H_ */
