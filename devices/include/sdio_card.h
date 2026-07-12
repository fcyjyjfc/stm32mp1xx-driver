/*
 * sdio_card.h — SDIO 协议层驱动
 *
 * 负责: SDIO 卡识别 (CMD0→CMD5→CMD3→CMD7)
 * 依赖: stm32mp1xx_sdmmc.h (底层外设驱动)
 *
 *  Created on: 2026-7-9
 *      Author: gjsbr
 */

#ifndef SDIO_CARD_H_
#define SDIO_CARD_H_

#include "stm32mp1xx_sdmmc.h"


/* SDIO 固定块大小 (512 字节, CMD53 Block 模式) */
#define SDIO_BLOCK_SIZE  (1U << SDMMC_BLKSIZ_512B)

/* SDIO 固定 RCA (由主机分配) */
#define SDIO_RCA         0x0002U


/* ===== SDIO 命令索引 ===== */
typedef enum {
    SDIO_CMD_GO_IDLE_STATE    = 0,
    SDIO_CMD_SEND_IF_COND     = 8,   /* CMD8: SEND_IF_COND, R7 */
    SDIO_CMD_SET_REL_ADDR     = 3,   /* CMD3: 设 RCA, R6 */
    SDIO_CMD_IO_SEND_OP_COND  = 5,   /* CMD5: IO 操作条件, R4 */
    SDIO_CMD_SEL_DESEL_CARD   = 7,   /* CMD7: 选卡, R1b */
    SDIO_CMD_IO_RW_DIRECT     = 52,  /* CMD52: 单字节读写, R5 */
    SDIO_CMD_IO_RW_EXTENDED   = 53,  /* CMD53: 多字节/块传输, R5 */
} SdioCmdIndex_t;


/* ---- CMD5: IO_SEND_OP_COND ---- */
/* arg 位域 */
#define SDIO_CMD5_S18R         (1U << 28)  /* 1.8V 切换请求 */
#define SDIO_CMD5_FUNC(x)      ((uint32_t)(x) << 24)  /* I/O 功能数 (写 0 探测) */

/* R4 响应 [31:0] 位域 */
#define SDIO_R4_C              (1U << 31)  /* Card Ready, 0=忙 1=就绪 */
#define SDIO_R4_NF(r)          (((r) >> 28) & 7)  /* Number of I/O Functions (1~7) */
#define SDIO_R4_MP             (1U << 26)  /* Memory Present (Combo 卡) */
#define SDIO_R4_S18A           (1U << 25)  /* 1.8V Switch Acknowledge */
#define SDIO_R4_OCR(r)         ((r) & 0x00FFFFFF)  /* I/O OCR */

/* IO-only 卡固定返回 CURRENT_STATE=0xF (R1 bits 12:9) */
#define SDIO_R1_IO_STATE       0xF000U     /* CMD13 R1 bits[12:9]=0xF 标识 IO-only */


/* ---- CMD53: IO_RW_EXTENDED ---- */
/* arg 位域 */
#define SDIO_CMD53_RW           (1U << 31)  /* 1=写, 0=读 */
#define SDIO_CMD53_FUNC(x)      ((uint32_t)(x) << 28)
#define SDIO_CMD53_BLOCK_MODE   (1U << 26)  /* 0=byte mode, 1=block mode */
#define SDIO_CMD53_OP_INC       (1U << 25)  /* 0=fixed addr, 1=incrementing */
#define SDIO_CMD53_ADDR(x)      (((uint32_t)(x) & 0x1FFFF) << 9)
#define SDIO_CMD53_CNT(x)       ((uint32_t)(x) & 0x1FF)


/* ---- CMD52: IO_RW_DIRECT ---- */
/* arg 位域 */
#define SDIO_CMD52_RW          (1U << 31)  /* 1=写, 0=读 */
#define SDIO_CMD52_FUNC(x)     ((uint32_t)(x) << 28)  /* Function 号 (0=CCCR) */
#define SDIO_CMD52_RAW         (1U << 27)  /* Read-After-Write */
/* 地址 [25:9] 17-bit, 范围 0~0x1FFFF */
#define SDIO_CMD52_ADDR(x)     (((uint32_t)(x) & 0x1FFFF) << 9)
#define SDIO_CMD52_DATA(x)     ((uint32_t)(x) & 0xFF)  /* 写数据 */

/* R5 响应位域 */
#define SDIO_R5_COM_CRC_ERROR  (1U << 15)  /* 前一命令 CRC 失败 */
#define SDIO_R5_ILLEGAL_CMD    (1U << 14)  /* 非法命令 */
#define SDIO_R5_IO_CURRENT_STATE(r) (((r) >> 12) & 3)  /* I/O 控制器状态 */
#define SDIO_R5_ERROR          (1U << 11)  /* 未知错误 */
#define SDIO_R5_FUNC_NUM       (1U << 9)   /* Function 号无效 */
#define SDIO_R5_OUT_OF_RANGE   (1U << 8)   /* 参数超范围 */
/* R5 数据字节在 bits[7:0] */
#define SDIO_R5_DATA(r)        ((r) & 0xFF)

/* IO_CURRENT_STATE 编码 */
typedef enum {
    SDIO_IO_STATE_DIS = 0,  /* Disabled */
    SDIO_IO_STATE_CMD = 1,  /* Command (DAT free) */
    SDIO_IO_STATE_TRN = 2,  /* Transfer (DAT active) */
} SdioIoState_t;


/* ===== CIS 元组 ID ===== */
#define SDIO_CISTPL_NULL      0x00
#define SDIO_CISTPL_END       0xFF
#define SDIO_CISTPL_MANFID    0x20
#define SDIO_CISTPL_FUNCE     0x22
#define SDIO_CISTPL_VERS_1    0x15
#define SDIO_CISTPL_SDIO_STD  0x91


/* ===== CCCR 寄存器地址 (Function 0) ===== */
#define SDIO_CCCR_REV          0x00   /* CCCR/SDIO 版本 (R/O) */
#define SDIO_CCCR_SD_REV       0x01   /* SD 物理层版本 (R/O) */
#define SDIO_CCCR_IO_ENABLE    0x02   /* I/O 功能使能 (R/W) */
#define SDIO_CCCR_IO_READY     0x03   /* I/O 功能就绪 (R/O) */
#define SDIO_CCCR_INT_ENABLE   0x04   /* 中断使能 (R/W) */
#define SDIO_CCCR_INT_PENDING  0x05   /* 中断挂起 (R/O) */
#define SDIO_CCCR_IO_ABORT     0x06   /* I/O 中止 (W/O) + RES */
#define SDIO_CCCR_BUS_CTRL     0x07   /* 总线接口控制 (R/W) */
#define SDIO_CCCR_CARD_CAPS    0x08   /* 卡能力 (R/O) */
#define SDIO_CCCR_CIS_PTR      0x09   /* Common CIS 指针 (24-bit, LE) */
#define SDIO_CCCR_BUS_SUSPEND  0x0C   /* 总线挂起 (R/W) */
#define SDIO_CCCR_FUNC_SELECT  0x0D   /* Function 选择 (R/W) */
#define SDIO_CCCR_EXEC_FLAGS   0x0E   /* 执行标志 (R/O) */
#define SDIO_CCCR_READY_FLAGS  0x0F   /* 就绪标志 (R/O) */
#define SDIO_CCCR_FN0_BLOCK_SZ 0x10   /* Function 0 块大小 (16-bit, LE) */
#define SDIO_CCCR_POWER_CTRL   0x12   /* 电源控制 (R/W) */
#define SDIO_CCCR_BUS_SPEED    0x13   /* 总线速度选择 (R/W) */
#define SDIO_CCCR_UHS_SUPPORT  0x14   /* UHS-I 支持 (R/O) */


/* ===== 错误码 ===== */
typedef enum {
    SDIO_OK = 0,
    SDIO_ERR_NO_CARD,          /* 无 SDIO 卡响应 */
    SDIO_ERR_INIT_TIMEOUT,     /* CMD5 轮询超时 (C 位未置 1) */
    SDIO_ERR_IO,               /* 底层 I/O 错误 */
} SdioCardErr_t;


/* ===== CIS 信息 ===== */
#define SDIO_MAX_FUNC  7

typedef struct {
    uint16_t manf_id;       /* 制造商代码 (CISTPL_MANFID) */
    uint16_t manf_info;     /* 制造商信息 */
    uint32_t cis_ptr;       /* CIS 指针地址 (来自 CCCR 0x09~0x0B) */
    uint32_t func_blksz[SDIO_MAX_FUNC]; /* 各 Function 最大块大小 (来自 CISTPL_FUNCE) */
    uint8_t  sdio_ver;      /* SDIO 标准版本 (CISTPL_SDIO_STD) */
    uint8_t  func_cnt;      /* CIS 中声明的 Function 数 */
} SdioCisInfo_t;


/* ===== 卡结构体 ===== */
typedef struct {
    volatile SdmmcRegs_t *sdmmc;   /* SDMMC 寄存器基址 */
    uint32_t ker_ck_hz;             /* SDMMC 内核时钟 (Hz) */
    uint32_t rca;                   /* 相对地址 */
    uint8_t  nf;                    /* I/O 功能数 (1~7) */
    uint8_t  mp;                    /* 1=Combo 卡 (有存储) */
    uint8_t  s18a;                  /* 1=支持 1.8V 切换 */
    uint8_t  vhs;                   /* 1=CMD8 响应 (卡支持 2.7~3.6V) */
    uint32_t io_ocr;                /* I/O OCR 值 */
    uint32_t raw_r4;                /* 最后一条 CMD5 R4 原始值 (调试) */
    uint32_t raw_r6;                /* CMD3 R6 原始值 (调试) */
    uint8_t  cccr_rev;             /* CCCR 版本 */
    uint8_t  sdio_rev;             /* SDIO 版本 */
    uint8_t  sd_spec_rev;          /* SD 物理层版本 */
    SdioCisInfo_t cis;             /* CIS 信息 */
} SdioCard_t;


/* ===== 函数声明 ===== */

/* 识别: CMD0→CMD5(poll)→CMD3→CMD7, 卡进 Transfer 态 */
SdioCardErr_t SdioCardIdentify(SdioCard_t *card);

/* CMD52 单字节读 (Function 0, RAW=0) */
SdioCardErr_t SdioCardReadB(SdioCard_t *card, uint16_t addr, uint8_t *val);

/* CMD52 单字节读 (指定 Function, RAW=0) */
SdioCardErr_t SdioCardReadFnB(SdioCard_t *card, uint8_t fn, uint16_t addr, uint8_t *val);

/* CMD52 单字节写 (Function 0, RAW=0) */
SdioCardErr_t SdioCardWriteB(SdioCard_t *card, uint16_t addr, uint8_t val);

/* CMD52 单字节写后读回 (Function 0, RAW=1) */
SdioCardErr_t SdioCardWriteReadB(SdioCard_t *card, uint16_t addr, uint8_t write_val, uint8_t *read_val);

/* 读 CCCR 寄存器验证卡 (CMD52 读 SDIO_CCCR_REV 等) */
SdioCardErr_t SdioCardReadCccr(SdioCard_t *card);

/* 读 CIS: 制造商 ID 等 */
SdioCardErr_t SdioCardReadCis(SdioCard_t *card);

/* 读全部卡信息: CCCR + CIS (封装 SdioCardReadCccr+SdioCardReadCis) */
SdioCardErr_t SdioCardReadInfo(SdioCard_t *card);

/* CMD53 多字节读 (byte mode, 增量地址) */
SdioCardErr_t SdioCardReadMultiB(SdioCard_t *card, uint16_t addr,
                                  uint8_t fn, uint8_t *buf, uint16_t cnt);

/* CMD53 多字节写 (byte mode, 增量地址) */
SdioCardErr_t SdioCardWriteMultiB(SdioCard_t *card, uint16_t addr,
                                   uint8_t fn, const uint8_t *buf, uint16_t cnt);


#endif /* SDIO_CARD_H_ */
