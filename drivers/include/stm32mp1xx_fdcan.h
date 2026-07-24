/*
 * stm32mp1xx_fdcan.h
 *
 *  FDCAN 寄存器结构体 — 基于 RM0436 §59.5 (3013-3049) + §59.6 TTCAN
 */

#ifndef STM32MP1XX_FDCAN_H_
#define STM32MP1XX_FDCAN_H_

#include <stdint.h>


/* ================================================================== */
/*  FDCAN 寄存器结构体                                                  */
/*  遵循 Bosch M_CAN + STM32MP1 TTCAN 扩展，按地址递增排列               */
/* ================================================================== */
typedef struct {
    /* ---- 0x0000 - 0x003C: Core (标准 M_CAN 寄存器) ---- */
    uint32_t CREL;          // 0x000  Core release (只读)
    uint32_t ENDN;          // 0x004  Endian (只读)
    uint32_t RSVD0;         // 0x008  Reserved
    uint32_t DBTP;          // 0x00C  数据相位位时序: TDC/DBRP/DTSEG1/DTSEG2/DSJW
    uint32_t TEST;          // 0x010  测试: RX/TX/LBCK
    uint32_t RWD;           // 0x014  RAM 看门狗: WDV/WDC
    uint32_t CCCR;          // 0x018  CC 控制: NISO/TXP/EFBI/PXHD/BRSE/FDOE/TEST/DAR/MON/CSR/CSA/ASM/CCE/INIT
    uint32_t NBTP;          // 0x01C  标称相位位时序: NSJW/NBRP/NTSEG1/NTSEG2
    uint32_t TSCC;          // 0x020  时间戳配置: TCP/TSS
    uint32_t TSCV;          // 0x024  时间戳值: TSC[15:0] (写清除)
    uint32_t TOCC;          // 0x028  超时计数器配置: TOP/TOS/ETOC
    uint32_t TOCV;          // 0x02C  超时计数器值: TOC[15:0] (写清除)

    uint8_t  RSVD1[0x10];   // 0x030~0x03F Reserved

    /* ---- 0x0040 - 0x004C: 状态/错误 ---- */
    uint32_t ECR;           // 0x040  错误计数器: CEL/RP/REC/TEC
    uint32_t PSR;           // 0x044  协议状态: TDCV/PXE/REDL/RBRS/RESI/DLEC/BO/EW/EP/ACT/LEC
    uint32_t TDCR;          // 0x048  发送延迟补偿: TDCO/TDCF
    uint32_t RSVD2;         // 0x04C  Reserved

    /* ---- 0x0050 - 0x005C: 中断 ---- */
    uint32_t IR;            // 0x050  中断标志 (写1清)
    uint32_t IE;            // 0x054  中断使能
    uint32_t ILS;           // 0x058  中断线选择 (0=intr0, 1=intr1)
    uint32_t ILE;           // 0x05C  中断线使能: EINT0/EINT1

    uint8_t  RSVD3[0x20];   // 0x060~0x07F Reserved

    /* ---- 0x0080 - 0x009C: 过滤与新数据 ---- */
    uint32_t GFC;           // 0x080  全局过滤配置: ANFS/ANFE/RRFS/RRFE
    uint32_t SIDFC;         // 0x084  标准 ID 过滤配置: LSS/FLSSA
    uint32_t XIDFC;         // 0x088  扩展 ID 过滤配置: LSE/FLESA
    uint32_t RSVD4;         // 0x08C  Reserved
    uint32_t XIDAM;         // 0x090  扩展 ID 掩码: EIDM[28:0] (default all-1)
    uint32_t HPMS;          // 0x094  高优先级消息状态: FLST/FIDX/MSI/BIDX
    uint32_t NDAT1;         // 0x098  New Data 1: ND[31:0] (Rx buffers 0-31)
    uint32_t NDAT2;         // 0x09C  New Data 2: ND[63:32] (Rx buffers 32-63)

    /* ---- 0x00A0 - 0x00BC: Rx FIFO/Buffer 配置 ---- */
    uint32_t RXF0C;         // 0x0A0  Rx FIFO 0 配置: F0OM/F0WM/F0S/F0SA
    uint32_t RXF0S;         // 0x0A4  Rx FIFO 0 状态: RF0L/F0F/F0PI/F0GI/F0FL
    uint32_t RXF0A;         // 0x0A8  Rx FIFO 0 确认: F0AI[5:0]
    uint32_t RXBC;          // 0x0AC  Rx Buffer 起始地址: RBSA[13:0]
    uint32_t RXF1C;         // 0x0B0  Rx FIFO 1 配置: F1OM/F1WM/F1S/F1SA
    uint32_t RXF1S;         // 0x0B4  Rx FIFO 1 状态: DMS/RF1L/F1F/F1PI/F1GI/F1FL
    uint32_t RXF1A;         // 0x0B8  Rx FIFO 1 确认: F1AI[5:0]
    uint32_t RXESC;         // 0x0BC  Rx 元素大小: RBDS/F1DS/F0DS

    /* ---- 0x00C0 - 0x00E4: Tx Buffer 配置/控制 ---- */
    uint32_t TXBC;          // 0x0C0  Tx Buffer 配置: TFQM/TFQS/NDTB/TBSA
    uint32_t TXFQS;         // 0x0C4  Tx FIFO/Queue 状态: TFQF/TFQPI/TFGI/TFFL
    uint32_t TXESC;         // 0x0C8  Tx 元素大小: TBDS[2:0]
    uint32_t TXBRP;         // 0x0CC  Tx 请求待决 (只读): TRP[31:0]
    uint32_t TXBAR;         // 0x0D0  Tx 添加请求: AR[31:0]
    uint32_t TXBCR;         // 0x0D4  Tx 取消请求: CR[31:0]
    uint32_t TXBTO;         // 0x0D8  Tx 发送完成 (只读): TO[31:0]
    uint32_t TXBCF;         // 0x0DC  Tx 取消完成 (只读): CF[31:0]
    uint32_t TXBTIE;        // 0x0E0  Tx 发送中断使能: TIE[31:0]
    uint32_t TXBCIE;        // 0x0E4  Tx 取消中断使能: CFIE[31:0]

    uint8_t  RSVD5[0x08];   // 0x0E8~0x0EF Reserved

    /* ---- 0x00F0 - 0x00FC: Tx Event FIFO ---- */
    uint32_t TXEFC;         // 0x0F0  Tx Event FIFO 配置: EFWM/EFS/EFSA
    uint32_t TXEFS;         // 0x0F4  Tx Event FIFO 状态: TEFL/EFF/EFPI/EFGI/EFFL
    uint32_t TXEFA;         // 0x0F8  Tx Event FIFO 确认: EFAI[4:0]
    uint8_t  RSVD6[0x04];   // 0x0FC Reserved

    /* ---- 0x0100 - 0x0140: TTCAN 寄存器 (FDCAN1 专用) ---- */
    uint32_t TTTMC;         // 0x100  TT trigger memory 配置: TME/TMSA
    uint32_t TTRMC;         // 0x104  TT reference message 配置: RMPS/XTD/RID
    uint32_t TTOCF;         // 0x108  TT 操作配置: OM/GEN/TM/LDSDL/IRTO/EECS/EVTP/EGTF
    uint32_t TTMLM;         // 0x10C  TT 矩阵限制: ENTT/TXEW/CSS/CCM
    uint32_t TURCF;         // 0x110  TT TUR 配置: ELT/DC/NCL
    uint32_t TTOCN;         // 0x114  TT 操作控制: SGT/ECS/SWP/SWS/TMC/RTIE/GCS/FGP/TMG/NIG/ESCN
    uint32_t TTGTP;         // 0x118  TT 全局时间预设: CTP/TP
    uint32_t TTTMK;         // 0x11C  TT time mark: LCKM/TICC/TM
    uint32_t TTIR;          // 0x120  TT 中断标志 (写1清)
    uint32_t TTIE;          // 0x124  TT 中断使能
    uint32_t TTILS;         // 0x128  TT 中断线选择
    uint32_t TTOST;         // 0x12C  TT 操作状态: SPL/WECS/AWE/WFE/GSI/TMP/GFI/WGTD/RTO/...
    uint32_t TURNA;         // 0x130  TT TUR 实际值: NAV
    uint32_t TTLGT;         // 0x134  TT 本地和全局时间: GT/LT
    uint32_t TTCTC;         // 0x138  TT 循环时间和计数: CC/CT
    uint32_t TTCPT;         // 0x13C  TT 捕获时间: SWV/CCV
    uint32_t TTCSM;         // 0x140  TT cycle sync mark: CSM
    uint8_t  RSVD7[0x1BC];  // 0x144~0x2FF Reserved
    uint32_t TTTS;          // 0x300  TT 触发选择: EVTSEL/SWTDEL
} FdcanRegs_t;


/* ================================================================== */
/*  基地址变量 (变量在 .c 文件中定义)                                     */
/* ================================================================== */

extern volatile FdcanRegs_t *const FDCAN1;
extern volatile FdcanRegs_t *const FDCAN2;


/* ================================================================== */
/*  CCU (Clock Calibration Unit)                                       */
/*  基地址 0x4400F200, 独立于 FDCAN1/FDCAN2，不能合并到 FdcanRegs_t     */
/* ================================================================== */
typedef struct {
    uint32_t CREL;      // 0x000  Core release (只读)
    uint32_t CCFG;      // 0x004  校准配置
    uint32_t CSTAT;     // 0x008  校准状态 (只读)
    uint32_t CWD;       // 0x00C  校准看门狗
    uint32_t IR;        // 0x010  中断标志 (写1清)
    uint32_t IE;        // 0x014  中断使能
} FdcanCcuRegs_t;

extern volatile FdcanCcuRegs_t *const FDCAN_CCU;

/* CAN SRAM — FDCAN1/FDCAN2 共享 10KB */
extern volatile uint32_t *const FDCAN_SRAM;


/* ================================================================== */
/*  初始化配置                                                         */
/*  调用方指定各区域, 驱动从 start_word 开始自动分配 SA + 过滤/掩码      */
/* ================================================================== */

typedef struct {
    uint32_t start_word;        /* 本控制器在 SRAM 中的起始 word 偏移   */

    /* Rx FIFO 0 */
    uint32_t rxf0_elements;     /* 元素数 (0=关闭)                     */
    uint32_t rxf0_dsize;        /* 数据字段大小 (FDCAN_DSIZE_8..64)    */
    uint32_t rxf0_mode;         /* 0=阻塞, 1=覆盖 (FDCAN_RXF0C_F0OM)   */
    uint32_t rxf0_wm;           /* 水位 (0=关, 1~rxf0_elements)        */

    /* Rx FIFO 1 */
    uint32_t rxf1_elements;
    uint32_t rxf1_dsize;
    uint32_t rxf1_mode;
    uint32_t rxf1_wm;

    /* Rx Buffers (NDAT 机制, 不归 FIFO) */
    uint32_t rx_buffers;        /* 专用 Rx buffer 数 (0=关闭)          */
    uint32_t rx_dsize;          /* Rx buffer 数据大小                  */

    /* Tx */
    uint32_t tx_fifo_elements;  /* Tx FIFO/Queue 缓冲区数              */
    uint32_t tx_dedicated;      /* 专用 Tx buffer 数                   */
    uint32_t tx_dsize;          /* Tx 数据字段大小                     */
    uint32_t tx_mode;           /* 0=FIFO, 1=Queue (FDCAN_TXBC_TFQM)   */

    /* Tx Event FIFO */
    uint32_t tx_evt_elements;   /* Tx Event FIFO 元素数 (0=关闭)        */
    uint32_t tx_evt_wm;         /* 水位 (0=关)                          */

    int     disable_auto_retrans;/* 0=自动重传, 1=禁用自动重传 (DAR)     */

    /* Filters */
    uint32_t std_filters;       /* 标准 ID 过滤元素数                  */
    uint32_t ext_filters;       /* 扩展 ID 过滤元素数                  */
    uint32_t reject_remote;     /* 0=接受远程帧, 1=拒绝 (GREC_RRFS/RRFE) */
    uint32_t xidam;             /* 扩展 ID 掩码 EIDM[28:0]              */
    uint32_t anfs;              /* 未匹配标准帧: 0=FIFO0, 1=FIFO1, 2=拒绝 */
    uint32_t anfe;              /* 未匹配扩展帧: 0=FIFO0, 1=FIFO1, 2=拒绝 */
} FdcanCfg_t;


/* ================================================================== */
/*  类型定义                                                           */
/* ================================================================== */

typedef enum {
    FDCAN_MODE_CAN,     /* 经典 CAN */
    FDCAN_MODE_FD,      /* CAN FD   */
} FdcanMode_t;

typedef enum {
    FDCAN_SFT_RANGE    = 0,  /* SFID1 ~ SFID2 范围匹配       */
    FDCAN_SFT_DUAL     = 1,  /* SFID1 或 SFID2 任一匹配      */
    FDCAN_SFT_CLASSIC  = 2,  /* SFID1=目标, SFID2=掩码        */
    FDCAN_SFT_DISABLED = 3,  /* 禁用该过滤元素                */
} FdcanSft_t;

typedef enum {
    FDCAN_EFT_RANGE         = 0,  /* EFID1 ~ EFID2 范围匹配         */
    FDCAN_EFT_DUAL          = 1,  /* EFID1 或 EFID2 任一匹配        */
    FDCAN_EFT_CLASSIC       = 2,  /* EFID1=目标, EFID2=掩码          */
    FDCAN_EFT_RANGE_NO_XIDAM = 3, /* 范围匹配, 不经过 XIDAM 掩码     */
} FdcanEft_t;

typedef enum {
    FDCAN_FEC_DISABLE     = 0,  /* 禁用              */
    FDCAN_FEC_FIFO0       = 1,  /* 存入 Rx FIFO 0    */
    FDCAN_FEC_FIFO1       = 2,  /* 存入 Rx FIFO 1    */
    FDCAN_FEC_REJECT      = 3,  /* 拒绝              */
    FDCAN_FEC_PRIO        = 4,  /* 置高优先级中断     */
    FDCAN_FEC_PRIO_FIFO0  = 5,  /* 高优先级 + FIFO 0 */
    FDCAN_FEC_PRIO_FIFO1  = 6,  /* 高优先级 + FIFO 1 */
    FDCAN_FEC_RXBUF       = 7,  /* 存入 Rx Buffer    */
} FdcanFilterAction_t;


/* ================================================================== */
/*  Rx 元素 — 按 RM0436 §59.5.2 Table 462 位定义                       */
/*  R0: ESI[31]|XTD[30]|RTR[29]|ID[28:0]                              */
/*  R1: ANMF[31]|FIDX[30:24]|-|FDF[21]|BRS[20]|DLC[19:16]|RXTS[15:0] */
/* ================================================================== */
typedef union {
    uint32_t R;
    struct {
        uint32_t id  : 29;
        uint32_t rtr : 1;
        uint32_t xtd : 1;
        uint32_t esi : 1;
    } B;
} FdcanRxR0_t;

typedef union {
    uint32_t R;
    struct {
        uint32_t rxts : 16;
        uint32_t dlc  : 4;
        uint32_t brs  : 1;
        uint32_t fdf  : 1;
        uint32_t      : 2;
        uint32_t fidx : 7;
        uint32_t anmf : 1;
    } B;
} FdcanRxR1_t;

typedef struct {
    FdcanRxR0_t r0;
    FdcanRxR1_t r1;
    uint8_t     data[64];
} FdcanRxFrame_t;

typedef union {
    uint32_t R;
    struct {
        uint32_t id  : 29;
        uint32_t rtr : 1;
        uint32_t xtd : 1;
        uint32_t esi : 1;
    } B;
} FdcanTxEventR0_t;

typedef union {
    uint32_t R;
    struct {
        uint32_t txts : 16;
        uint32_t dlc  : 4;
        uint32_t brs  : 1;
        uint32_t edl  : 1;
        uint32_t et   : 2;
        uint32_t mm   : 8;
    } B;
} FdcanTxEventR1_t;

typedef struct {
    FdcanTxEventR0_t r0;
    FdcanTxEventR1_t r1;
} FdcanTxEvent_t;


/* ================================================================== */
/*  函数声明                                                           */
/* ================================================================== */

extern void FdcanInit(volatile FdcanRegs_t *fdcan, uint32_t ker_ck_hz,
                      uint32_t bitrate, uint32_t data_bitrate,
                      const FdcanCfg_t *cfg, FdcanMode_t mode);

extern int  FdcanSend(volatile FdcanRegs_t *fdcan, uint32_t id, int xtd,
                      const uint8_t *data, uint32_t len, int fd);

extern int  FdcanSendBuf(volatile FdcanRegs_t *fdcan, uint32_t buf_idx,
                         uint32_t id, int xtd, const uint8_t *data, uint32_t len,
                         int fd);

extern int  FdcanReadFifo0(volatile FdcanRegs_t *fdcan, FdcanRxFrame_t *f);

extern int  FdcanReadFifo1(volatile FdcanRegs_t *fdcan, FdcanRxFrame_t *f);

extern int  FdcanReadBuf(volatile FdcanRegs_t *fdcan, uint32_t buf_idx,
                         FdcanRxFrame_t *f);

extern void FdcanTimestampInit(volatile FdcanRegs_t *fdcan,
                            uint32_t prescaler, uint32_t source);
extern uint16_t FdcanTimestampRead(volatile FdcanRegs_t *fdcan);

extern int  FdcanTxEventRead(volatile FdcanRegs_t *fdcan, FdcanTxEvent_t *e);

extern void FdcanFilterStd(volatile FdcanRegs_t *fdcan, uint32_t idx,
                        FdcanSft_t sft, FdcanFilterAction_t sfec,
                        uint32_t sfid1, uint32_t sfid2);

extern void FdcanFilterExt(volatile FdcanRegs_t *fdcan, uint32_t idx,
                        FdcanEft_t eft, FdcanFilterAction_t efec,
                        uint32_t efid1, uint32_t efid2);

extern void FdcanFilterStdRxBuf(volatile FdcanRegs_t *fdcan, uint32_t idx,
                                uint32_t sfid, uint32_t buf_idx);

extern void FdcanFilterExtRxBuf(volatile FdcanRegs_t *fdcan, uint32_t idx,
                                uint32_t efid, uint32_t buf_idx);

extern void FdcanLoopbackEnable(volatile FdcanRegs_t *fdcan);
extern void FdcanLoopbackDisable(volatile FdcanRegs_t *fdcan);


/* ================================================================== */
/*  CREL — Core Release (只读)                                         */
/* ================================================================== */
#define FDCAN_CREL_REL_Pos     28
#define FDCAN_CREL_REL_Msk     (0xFU << 28)
#define FDCAN_CREL_STEP_Pos    24
#define FDCAN_CREL_STEP_Msk    (0xFU << 24)
#define FDCAN_CREL_SUBSTEP_Pos 20
#define FDCAN_CREL_SUBSTEP_Msk (0xFU << 20)
#define FDCAN_CREL_YEAR_Pos    16
#define FDCAN_CREL_YEAR_Msk    (0xFU << 16)
#define FDCAN_CREL_MON_Pos     8
#define FDCAN_CREL_MON_Msk     (0xFFU << 8)
#define FDCAN_CREL_DAY_Pos     0
#define FDCAN_CREL_DAY_Msk     (0xFFU << 0)

/* ================================================================== */
/*  ENDN — Endian (只读)                                               */
/* ================================================================== */
#define FDCAN_ENDN_ETV_Pos     0
#define FDCAN_ENDN_ETV_Msk     (0xFFFFFFFFU << 0)
#define FDCAN_ENDN_VALUE       0x87654321U

/* ================================================================== */
/*  DBTP — 数据相位位时序                                              */
/* ================================================================== */
#define FDCAN_DBTP_TDC         (1U << 23)
#define FDCAN_DBTP_DBRP_Pos    16
#define FDCAN_DBTP_DBRP_Msk    (0x1FU << 16)
#define FDCAN_DBTP_DTSEG1_Pos  8
#define FDCAN_DBTP_DTSEG1_Msk  (0x1FU << 8)
#define FDCAN_DBTP_DTSEG2_Pos  4
#define FDCAN_DBTP_DTSEG2_Msk  (0xFU << 4)
#define FDCAN_DBTP_DSJW_Pos    0
#define FDCAN_DBTP_DSJW_Msk    (0xFU << 0)

/* ================================================================== */
/*  TEST — 测试                                                        */
/* ================================================================== */
#define FDCAN_TEST_RX          (1U << 7)
#define FDCAN_TEST_TX_Pos      5
#define FDCAN_TEST_TX_Msk      (3U << 5)
#define FDCAN_TEST_TX_NORMAL   0U          /* CAN core 控制 TX */
#define FDCAN_TEST_TX_SAMPLE   (1U << 5)   /* 采样点监控 */
#define FDCAN_TEST_TX_DOM      (2U << 5)   /* 强制显性 */
#define FDCAN_TEST_TX_REC      (3U << 5)   /* 强制隐性 */
#define FDCAN_TEST_LBCK        (1U << 4)   /* 回环模式 */

/* ================================================================== */
/*  RWD — RAM 看门狗                                                    */
/* ================================================================== */
#define FDCAN_RWD_WDV_Pos      8
#define FDCAN_RWD_WDV_Msk      (0xFFU << 8)
#define FDCAN_RWD_WDC_Pos      0
#define FDCAN_RWD_WDC_Msk      (0xFFU << 0)

/* ================================================================== */
/*  CCCR — CC 控制                                                      */
/* ================================================================== */
#define FDCAN_CCCR_NISO        (1U << 15)
#define FDCAN_CCCR_TXP         (1U << 14)
#define FDCAN_CCCR_EFBI        (1U << 13)
#define FDCAN_CCCR_PXHD        (1U << 12)
#define FDCAN_CCCR_BRSE        (1U << 9)
#define FDCAN_CCCR_FDOE        (1U << 8)
#define FDCAN_CCCR_TEST        (1U << 7)
#define FDCAN_CCCR_DAR         (1U << 6)
#define FDCAN_CCCR_MON         (1U << 5)
#define FDCAN_CCCR_CSR         (1U << 4)
#define FDCAN_CCCR_CSA         (1U << 3)
#define FDCAN_CCCR_ASM         (1U << 2)
#define FDCAN_CCCR_CCE         (1U << 1)
#define FDCAN_CCCR_INIT        (1U << 0)

/* CCCR 初始化完成标志: 读取 CCCR 直到 INIT 回到预期值 */
#define FDCAN_CCCR_INIT_MASK   (FDCAN_CCCR_CCE | FDCAN_CCCR_INIT)

/* ================================================================== */
/*  NBTP — 标称相位位时序                                               */
/* ================================================================== */
#define FDCAN_NBTP_NSJW_Pos    25
#define FDCAN_NBTP_NSJW_Msk    (0x7FU << 25)
#define FDCAN_NBTP_NBRP_Pos    16
#define FDCAN_NBTP_NBRP_Msk    (0x1FFU << 16)
#define FDCAN_NBTP_NTSEG1_Pos  8
#define FDCAN_NBTP_NTSEG1_Msk  (0xFFU << 8)
#define FDCAN_NBTP_NTSEG2_Pos  0
#define FDCAN_NBTP_NTSEG2_Msk  (0x7FU << 0)

/* ================================================================== */
/*  TSCC — 时间戳配置                                                   */
/* ================================================================== */
#define FDCAN_TSCC_TCP_Pos     16
#define FDCAN_TSCC_TCP_Msk     (0xFU << 16)
#define FDCAN_TSCC_TSS_Pos     0
#define FDCAN_TSCC_TSS_Msk     (3U << 0)
#define FDCAN_TSCC_TSS_ZERO    0U          /* 恒为 0 */
#define FDCAN_TSCC_TSS_INC     1U          /* 内部递增 */
#define FDCAN_TSCC_TSS_EXT     2U          /* TIM3 外部 */

/* ================================================================== */
/*  TSCV — 时间戳值 (写清除)                                            */
/* ================================================================== */
#define FDCAN_TSCV_TSC_Pos     0
#define FDCAN_TSCV_TSC_Msk     (0xFFFFU << 0)

/* ================================================================== */
/*  TOCC — 超时计数器配置                                                */
/* ================================================================== */
#define FDCAN_TOCC_TOP_Pos     16
#define FDCAN_TOCC_TOP_Msk     (0xFFFFU << 16)
#define FDCAN_TOCC_TOS_Pos     1
#define FDCAN_TOCC_TOS_Msk     (3U << 1)
#define FDCAN_TOCC_TOS_CONT    0U          /* 连续 */
#define FDCAN_TOCC_TOS_TXEVT   1U          /* Tx event FIFO 控制 */
#define FDCAN_TOCC_TOS_RXF0    2U          /* Rx FIFO 0 控制 */
#define FDCAN_TOCC_TOS_RXF1    3U          /* Rx FIFO 1 控制 */
#define FDCAN_TOCC_ETOC        (1U << 0)

/* ================================================================== */
/*  TOCV — 超时计数器值 (写清除)                                         */
/* ================================================================== */
#define FDCAN_TOCV_TOC_Pos     0
#define FDCAN_TOCV_TOC_Msk     (0xFFFFU << 0)

/* ================================================================== */
/*  ECR — 错误计数器                                                    */
/* ================================================================== */
#define FDCAN_ECR_CEL_Pos      16
#define FDCAN_ECR_CEL_Msk      (0xFFU << 16)
#define FDCAN_ECR_RP           (1U << 15)
#define FDCAN_ECR_REC_Pos      8
#define FDCAN_ECR_REC_Msk      (0x7FU << 8)
#define FDCAN_ECR_TEC_Pos      0
#define FDCAN_ECR_TEC_Msk      (0xFFU << 0)

/* ================================================================== */
/*  PSR — 协议状态                                                      */
/* ================================================================== */
#define FDCAN_PSR_TDCV_Pos     16
#define FDCAN_PSR_TDCV_Msk     (0x7FU << 16)
#define FDCAN_PSR_PXE          (1U << 14)
#define FDCAN_PSR_REDL         (1U << 13)
#define FDCAN_PSR_RBRS         (1U << 12)
#define FDCAN_PSR_RESI         (1U << 11)
#define FDCAN_PSR_DLEC_Pos     8
#define FDCAN_PSR_DLEC_Msk     (7U << 8)
#define FDCAN_PSR_BO           (1U << 7)
#define FDCAN_PSR_EW           (1U << 6)
#define FDCAN_PSR_EP           (1U << 5)
#define FDCAN_PSR_ACT_Pos      3
#define FDCAN_PSR_ACT_Msk      (3U << 3)
#define FDCAN_PSR_ACT_SYNC     0U          /* 同步中 */
#define FDCAN_PSR_ACT_IDLE     1U          /* 空闲 */
#define FDCAN_PSR_ACT_RX       2U          /* 接收 */
#define FDCAN_PSR_ACT_TX       3U          /* 发送 */
#define FDCAN_PSR_LEC_Pos      0
#define FDCAN_PSR_LEC_Msk      (7U << 0)
#define FDCAN_PSR_LEC_NONE     0U          /* 无错误 */
#define FDCAN_PSR_LEC_STUFF    1U          /* 填充错误 */
#define FDCAN_PSR_LEC_FORM     2U          /* 格式错误 */
#define FDCAN_PSR_LEC_ACK      3U          /* 应答错误 */
#define FDCAN_PSR_LEC_BIT1     4U          /* 隐形位错误 */
#define FDCAN_PSR_LEC_BIT0     5U          /* 显性位错误 */
#define FDCAN_PSR_LEC_CRC      6U          /* CRC 错误 */
#define FDCAN_PSR_LEC_NC       7U          /* 无变化 */

/* ================================================================== */
/*  TDCR — 发送延迟补偿                                                  */
/* ================================================================== */
#define FDCAN_TDCR_TDCO_Pos    8
#define FDCAN_TDCR_TDCO_Msk    (0x7FU << 8)
#define FDCAN_TDCR_TDCF_Pos    0
#define FDCAN_TDCR_TDCF_Msk    (0x7FU << 0)

/* ================================================================== */
/*  IR — 中断标志 (写 1 清)                                             */
/* ================================================================== */
#define FDCAN_IR_ARA           (1U << 29)
#define FDCAN_IR_PED           (1U << 28)
#define FDCAN_IR_PEA           (1U << 27)
#define FDCAN_IR_WDI           (1U << 26)
#define FDCAN_IR_BO            (1U << 25)
#define FDCAN_IR_EW            (1U << 24)
#define FDCAN_IR_EP            (1U << 23)
#define FDCAN_IR_ELO           (1U << 22)
#define FDCAN_IR_DRX           (1U << 19)
#define FDCAN_IR_TOO           (1U << 18)
#define FDCAN_IR_MRAF          (1U << 17)
#define FDCAN_IR_TSW           (1U << 16)
#define FDCAN_IR_TEFL          (1U << 15)
#define FDCAN_IR_TEFF          (1U << 14)
#define FDCAN_IR_TEFW          (1U << 13)
#define FDCAN_IR_TEFN          (1U << 12)
#define FDCAN_IR_TFE           (1U << 11)
#define FDCAN_IR_TCF           (1U << 10)
#define FDCAN_IR_TC            (1U << 9)
#define FDCAN_IR_HPM           (1U << 8)
#define FDCAN_IR_RF1L          (1U << 7)
#define FDCAN_IR_RF1F          (1U << 6)
#define FDCAN_IR_RF1W          (1U << 5)
#define FDCAN_IR_RF1N          (1U << 4)
#define FDCAN_IR_RF0L          (1U << 3)
#define FDCAN_IR_RF0F          (1U << 2)
#define FDCAN_IR_RF0W          (1U << 1)
#define FDCAN_IR_RF0N          (1U << 0)

/* ================================================================== */
/*  IE — 中断使能 (每位置 1 使能对应 IR 中断)                            */
/* ================================================================== */
#define FDCAN_IE_ARAE          (1U << 29)
#define FDCAN_IE_PEDE          (1U << 28)
#define FDCAN_IE_PEAE          (1U << 27)
#define FDCAN_IE_WDIE          (1U << 26)
#define FDCAN_IE_BOE           (1U << 25)
#define FDCAN_IE_EWE           (1U << 24)
#define FDCAN_IE_EPE           (1U << 23)
#define FDCAN_IE_ELOE          (1U << 22)
#define FDCAN_IE_DRXE          (1U << 19)
#define FDCAN_IE_TOOE          (1U << 18)
#define FDCAN_IE_MRAFE         (1U << 17)
#define FDCAN_IE_TSWE          (1U << 16)
#define FDCAN_IE_TEFLE         (1U << 15)
#define FDCAN_IE_TEFFE         (1U << 14)
#define FDCAN_IE_TEFWE         (1U << 13)
#define FDCAN_IE_TEFNE         (1U << 12)
#define FDCAN_IE_TFEE          (1U << 11)
#define FDCAN_IE_TCFE          (1U << 10)
#define FDCAN_IE_TCE           (1U << 9)
#define FDCAN_IE_HPME          (1U << 8)
#define FDCAN_IE_RF1LE         (1U << 7)
#define FDCAN_IE_RF1FE         (1U << 6)
#define FDCAN_IE_RF1WE         (1U << 5)
#define FDCAN_IE_RF1NE         (1U << 4)
#define FDCAN_IE_RF0LE         (1U << 3)
#define FDCAN_IE_RF0FE         (1U << 2)
#define FDCAN_IE_RF0WE         (1U << 1)
#define FDCAN_IE_RF0NE         (1U << 0)

/* ================================================================== */
/*  ILS — 中断线选择 (0=intr0, 1=intr1)                                */
/*  ILE — 中断线使能 (每位置 1 使能对应中断线)                           */
/* ================================================================== */
#define FDCAN_ILE_EINT1        (1U << 1)
#define FDCAN_ILE_EINT0        (1U << 0)

/* ================================================================== */
/*  GFC — 全局过滤配置                                                  */
/* ================================================================== */
#define FDCAN_GFC_ANFS_Pos     4
#define FDCAN_GFC_ANFS_Msk     (3U << 4)
#define FDCAN_GFC_ANFE_Pos     2
#define FDCAN_GFC_ANFE_Msk     (3U << 2)
#define FDCAN_GFC_RRFS         (1U << 1)
#define FDCAN_GFC_RRFE         (1U << 0)

/* ANFS/ANFE: 未匹配帧动作 */
#define FDCAN_GFC_ACCEPT_F0    0U          /* 存 FIFO 0 */
#define FDCAN_GFC_ACCEPT_F1    1U          /* 存 FIFO 1 */
#define FDCAN_GFC_REJECT       2U          /* 拒绝 */

/* ================================================================== */
/*  SIDFC — 标准 ID 过滤配置                                            */
/* ================================================================== */
#define FDCAN_SIDFC_LSS_Pos    16
#define FDCAN_SIDFC_LSS_Msk    (0xFFU << 16)
#define FDCAN_SIDFC_FLSSA_Pos  2
#define FDCAN_SIDFC_FLSSA_Msk  (0x3FFFU << 2)

/* ================================================================== */
/*  XIDFC — 扩展 ID 过滤配置                                            */
/* ================================================================== */
#define FDCAN_XIDFC_LSE_Pos    16
#define FDCAN_XIDFC_LSE_Msk    (0xFFU << 16)
#define FDCAN_XIDFC_FLESA_Pos  2
#define FDCAN_XIDFC_FLESA_Msk  (0x3FFFU << 2)

/* ================================================================== */
/*  XIDAM — 扩展 ID 掩码                                                */
/* ================================================================== */
#define FDCAN_XIDAM_EIDM_Pos   0
#define FDCAN_XIDAM_EIDM_Msk   (0x1FFFFFFFU << 0)
#define FDCAN_XIDAM_DISABLED   0x1FFFFFFFU  /* 全 1 = 掩码不生效 */

/* ================================================================== */
/*  HPMS — 高优先级消息状态 (只读)                                       */
/* ================================================================== */
#define FDCAN_HPMS_FLST        (1U << 15)
#define FDCAN_HPMS_FIDX_Pos    8
#define FDCAN_HPMS_FIDX_Msk    (0x7FU << 8)
#define FDCAN_HPMS_MSI_Pos     6
#define FDCAN_HPMS_MSI_Msk     (3U << 6)
#define FDCAN_HPMS_BIDX_Pos    0
#define FDCAN_HPMS_BIDX_Msk    (0x3FU << 0)

/* ================================================================== */
/*  NDAT1/NDAT2 — New Data (写 1 清)                                   */
/* ================================================================== */
#define FDCAN_NDAT1_ND_Pos     0
#define FDCAN_NDAT1_ND_Msk     (0xFFFFFFFFU << 0)
#define FDCAN_NDAT2_ND_Pos     0
#define FDCAN_NDAT2_ND_Msk     (0xFFFFFFFFU << 0)

/* ================================================================== */
/*  RXF0C — Rx FIFO 0 配置                                             */
/* ================================================================== */
#define FDCAN_RXF0C_F0OM       (1U << 31)
#define FDCAN_RXF0C_F0WM_Pos   24
#define FDCAN_RXF0C_F0WM_Msk   (0x7FU << 24)
#define FDCAN_RXF0C_F0S_Pos    16
#define FDCAN_RXF0C_F0S_Msk    (0x7FU << 16)
#define FDCAN_RXF0C_F0SA_Pos   2
#define FDCAN_RXF0C_F0SA_Msk   (0x3FFFU << 2)

/* ================================================================== */
/*  RXF0S — Rx FIFO 0 状态 (只读)                                      */
/* ================================================================== */
#define FDCAN_RXF0S_RF0L       (1U << 25)
#define FDCAN_RXF0S_F0F        (1U << 24)
#define FDCAN_RXF0S_F0PI_Pos   16
#define FDCAN_RXF0S_F0PI_Msk   (0x3FU << 16)
#define FDCAN_RXF0S_F0GI_Pos   8
#define FDCAN_RXF0S_F0GI_Msk   (0x3FU << 8)
#define FDCAN_RXF0S_F0FL_Pos   0
#define FDCAN_RXF0S_F0FL_Msk   (0x7FU << 0)

/* ================================================================== */
/*  RXF0A — Rx FIFO 0 确认                                              */
/* ================================================================== */
#define FDCAN_RXF0A_F0AI_Pos   0
#define FDCAN_RXF0A_F0AI_Msk   (0x3FU << 0)

/* ================================================================== */
/*  RXBC — Rx Buffer 起始地址                                           */
/* ================================================================== */
#define FDCAN_RXBC_RBSA_Pos    2
#define FDCAN_RXBC_RBSA_Msk    (0x3FFFU << 2)

/* ================================================================== */
/*  RXF1C — Rx FIFO 1 配置                                             */
/* ================================================================== */
#define FDCAN_RXF1C_F1OM       (1U << 31)
#define FDCAN_RXF1C_F1WM_Pos   24
#define FDCAN_RXF1C_F1WM_Msk   (0x7FU << 24)
#define FDCAN_RXF1C_F1S_Pos    16
#define FDCAN_RXF1C_F1S_Msk    (0x7FU << 16)
#define FDCAN_RXF1C_F1SA_Pos   2
#define FDCAN_RXF1C_F1SA_Msk   (0x3FFFU << 2)

/* ================================================================== */
/*  RXF1S — Rx FIFO 1 状态 (只读)                                      */
/* ================================================================== */
#define FDCAN_RXF1S_DMS_Pos    30
#define FDCAN_RXF1S_DMS_Msk    (3U << 30)
#define FDCAN_RXF1S_RF1L       (1U << 25)
#define FDCAN_RXF1S_F1F        (1U << 24)
#define FDCAN_RXF1S_F1PI_Pos   16
#define FDCAN_RXF1S_F1PI_Msk   (0x3FU << 16)
#define FDCAN_RXF1S_F1GI_Pos   8
#define FDCAN_RXF1S_F1GI_Msk   (0x3FU << 8)
#define FDCAN_RXF1S_F1FL_Pos   0
#define FDCAN_RXF1S_F1FL_Msk   (0x7FU << 0)

/* ================================================================== */
/*  RXF1A — Rx FIFO 1 确认                                              */
/* ================================================================== */
#define FDCAN_RXF1A_F1AI_Pos   0
#define FDCAN_RXF1A_F1AI_Msk   (0x3FU << 0)

/* ================================================================== */
/*  RXESC — Rx 元素大小                                                 */
/* ================================================================== */
#define FDCAN_RXESC_RBDS_Pos   8
#define FDCAN_RXESC_RBDS_Msk   (7U << 8)
#define FDCAN_RXESC_F1DS_Pos   4
#define FDCAN_RXESC_F1DS_Msk   (7U << 4)
#define FDCAN_RXESC_F0DS_Pos   0
#define FDCAN_RXESC_F0DS_Msk   (7U << 0)

/* RXESC 数据字段大小编码 (共用) */
#define FDCAN_DSIZE_8          0U
#define FDCAN_DSIZE_12         1U
#define FDCAN_DSIZE_16         2U
#define FDCAN_DSIZE_20         3U
#define FDCAN_DSIZE_24         4U
#define FDCAN_DSIZE_32         5U
#define FDCAN_DSIZE_48         6U
#define FDCAN_DSIZE_64         7U

/* ================================================================== */
/*  TXBC — Tx Buffer 配置                                              */
/* ================================================================== */
#define FDCAN_TXBC_TFQM        (1U << 30)   /* 1=Queue, 0=FIFO */
#define FDCAN_TXBC_TFQS_Pos    24
#define FDCAN_TXBC_TFQS_Msk    (0x3FU << 24)
#define FDCAN_TXBC_NDTB_Pos    16
#define FDCAN_TXBC_NDTB_Msk    (0x3FU << 16)
#define FDCAN_TXBC_TBSA_Pos    2
#define FDCAN_TXBC_TBSA_Msk    (0x3FFFU << 2)

/* ================================================================== */
/*  TXFQS — Tx FIFO/Queue 状态 (只读)                                  */
/* ================================================================== */
#define FDCAN_TXFQS_TFQF       (1U << 21)
#define FDCAN_TXFQS_TFQPI_Pos  16
#define FDCAN_TXFQS_TFQPI_Msk  (0x1FU << 16)
#define FDCAN_TXFQS_TFGI_Pos   8
#define FDCAN_TXFQS_TFGI_Msk   (0x1FU << 8)
#define FDCAN_TXFQS_TFFL_Pos   0
#define FDCAN_TXFQS_TFFL_Msk   (0x3FU << 0)

/* ================================================================== */
/*  TXESC — Tx 元素大小                                                 */
/* ================================================================== */
#define FDCAN_TXESC_TBDS_Pos   0
#define FDCAN_TXESC_TBDS_Msk   (7U << 0)

/* ================================================================== */
/*  TXBRP — Tx 请求待决 (只读)                                          */
/*  TXBAR — Tx 添加请求 (写 1 置位)                                     */
/*  TXBCR — Tx 取消请求 (写 1 置位)                                     */
/*  TXBTO — Tx 发送完成 (只读)                                          */
/*  TXBCF — Tx 取消完成 (只读)                                          */
/*  TXBTIE — Tx 发送中断使能                                            */
/*  TXBCIE — Tx 取消中断使能                                            */
/* ================================================================== */
#define FDCAN_TXBRP_TRP_Pos    0
#define FDCAN_TXBRP_TRP_Msk    (0xFFFFFFFFU << 0)
#define FDCAN_TXBAR_AR_Pos     0
#define FDCAN_TXBAR_AR_Msk     (0xFFFFFFFFU << 0)
#define FDCAN_TXBCR_CR_Pos     0
#define FDCAN_TXBCR_CR_Msk     (0xFFFFFFFFU << 0)
#define FDCAN_TXBTO_TO_Pos     0
#define FDCAN_TXBTO_TO_Msk     (0xFFFFFFFFU << 0)
#define FDCAN_TXBCF_CF_Pos     0
#define FDCAN_TXBCF_CF_Msk     (0xFFFFFFFFU << 0)
#define FDCAN_TXBTIE_TIE_Pos   0
#define FDCAN_TXBTIE_TIE_Msk   (0xFFFFFFFFU << 0)
#define FDCAN_TXBCIE_CFIE_Pos  0
#define FDCAN_TXBCIE_CFIE_Msk  (0xFFFFFFFFU << 0)

/* ================================================================== */
/*  TXEFC — Tx Event FIFO 配置                                         */
/* ================================================================== */
#define FDCAN_TXEFC_EFWM_Pos   24
#define FDCAN_TXEFC_EFWM_Msk   (0x3FU << 24)
#define FDCAN_TXEFC_EFS_Pos    16
#define FDCAN_TXEFC_EFS_Msk    (0x3FU << 16)
#define FDCAN_TXEFC_EFSA_Pos   2
#define FDCAN_TXEFC_EFSA_Msk   (0x3FFFU << 2)

/* ================================================================== */
/*  TXEFS — Tx Event FIFO 状态 (只读)                                  */
/* ================================================================== */
#define FDCAN_TXEFS_TEFL       (1U << 25)
#define FDCAN_TXEFS_EFF        (1U << 24)
#define FDCAN_TXEFS_EFPI_Pos   16
#define FDCAN_TXEFS_EFPI_Msk   (0x1FU << 16)
#define FDCAN_TXEFS_EFGI_Pos   8
#define FDCAN_TXEFS_EFGI_Msk   (0x1FU << 8)
#define FDCAN_TXEFS_EFFL_Pos   0
#define FDCAN_TXEFS_EFFL_Msk   (0x3FU << 0)

/* ================================================================== */
/*  TXEFA — Tx Event FIFO 确认                                          */
/* ================================================================== */
#define FDCAN_TXEFA_EFAI_Pos   0
#define FDCAN_TXEFA_EFAI_Msk   (0x1FU << 0)


/* ================================================================== */
/*  59.6 — TTCAN 寄存器 (FDCAN1 专用)                                    */
/* ================================================================== */

/* ---- TTTMC — TT trigger memory 配置 ---- */
#define FDCAN_TTTMC_TME_Pos     16
#define FDCAN_TTTMC_TME_Msk     (0x7FU << 16)
#define FDCAN_TTTMC_TMSA_Pos    2
#define FDCAN_TTTMC_TMSA_Msk    (0x3FFFU << 2)

/* ---- TTRMC — TT reference message 配置 ---- */
#define FDCAN_TTRMC_RMPS        (1U << 31)
#define FDCAN_TTRMC_XTD         (1U << 30)
#define FDCAN_TTRMC_RID_Pos     0
#define FDCAN_TTRMC_RID_Msk     (0x1FFFFFFFU << 0)

/* ---- TTOCF — TT 操作配置 ---- */
#define FDCAN_TTOCF_EVTP        (1U << 26)
#define FDCAN_TTOCF_ECC         (1U << 25)
#define FDCAN_TTOCF_EGTF        (1U << 24)
#define FDCAN_TTOCF_AWL_Pos     16
#define FDCAN_TTOCF_AWL_Msk     (0xFFU << 16)
#define FDCAN_TTOCF_EECS        (1U << 15)
#define FDCAN_TTOCF_IRTO_Pos    8
#define FDCAN_TTOCF_IRTO_Msk    (0x7FU << 8)
#define FDCAN_TTOCF_LDSDL_Pos   5
#define FDCAN_TTOCF_LDSDL_Msk   (7U << 5)
#define FDCAN_TTOCF_TM          (1U << 4)
#define FDCAN_TTOCF_GEN         (1U << 3)
#define FDCAN_TTOCF_OM_Pos      0
#define FDCAN_TTOCF_OM_Msk      (3U << 0)
#define FDCAN_TTOCF_OM_EVT      0U          /* event-driven */
#define FDCAN_TTOCF_OM_L1       1U          /* TTCAN level 1 */
#define FDCAN_TTOCF_OM_L2       2U          /* TTCAN level 2 */
#define FDCAN_TTOCF_OM_L0       3U          /* TTCAN level 0 */

/* ---- TTMLM — TT 矩阵限制 ---- */
#define FDCAN_TTMLM_ENTT_Pos    16
#define FDCAN_TTMLM_ENTT_Msk    (0xFFFU << 16)
#define FDCAN_TTMLM_TXEW_Pos    8
#define FDCAN_TTMLM_TXEW_Msk    (0xFU << 8)
#define FDCAN_TTMLM_CSS_Pos     6
#define FDCAN_TTMLM_CSS_Msk     (3U << 6)
#define FDCAN_TTMLM_CCM_Pos     0
#define FDCAN_TTMLM_CCM_Msk     (0x3FU << 0)

/* ---- TURCF — TT TUR 配置 ---- */
#define FDCAN_TURCF_ELT         (1U << 31)
#define FDCAN_TURCF_DC_Pos      16
#define FDCAN_TURCF_DC_Msk      (0x3FFFU << 16)
#define FDCAN_TURCF_NCL_Pos     0
#define FDCAN_TURCF_NCL_Msk     (0xFFFFU << 0)

/* ---- TTOCN — TT 操作控制 ---- */
#define FDCAN_TTOCN_LCKC        (1U << 15)
#define FDCAN_TTOCN_ESCN        (1U << 13)
#define FDCAN_TTOCN_NIG         (1U << 12)
#define FDCAN_TTOCN_TMG         (1U << 11)
#define FDCAN_TTOCN_FGP         (1U << 10)
#define FDCAN_TTOCN_GCS         (1U << 9)
#define FDCAN_TTOCN_TTIE        (1U << 8)
#define FDCAN_TTOCN_TMC_Pos     6
#define FDCAN_TTOCN_TMC_Msk     (3U << 6)
#define FDCAN_TTOCN_RTIE        (1U << 5)
#define FDCAN_TTOCN_SWS_Pos     3
#define FDCAN_TTOCN_SWS_Msk     (3U << 3)
#define FDCAN_TTOCN_SWP         (1U << 2)
#define FDCAN_TTOCN_ECS         (1U << 1)
#define FDCAN_TTOCN_SGT         (1U << 0)

/* ---- TTGTP — TT 全局时间预设 ---- */
#define FDCAN_TTGTP_CTP_Pos     16
#define FDCAN_TTGTP_CTP_Msk     (0xFFFFU << 16)
#define FDCAN_TTGTP_TP_Pos      0
#define FDCAN_TTGTP_TP_Msk      (0xFFFFU << 0)

/* ---- TTTMK — TT time mark ---- */
#define FDCAN_TTTMK_LCKM        (1U << 31)
#define FDCAN_TTTMK_TICC_Pos    16
#define FDCAN_TTTMK_TICC_Msk    (0x7FU << 16)
#define FDCAN_TTTMK_TM_Pos      0
#define FDCAN_TTTMK_TM_Msk      (0xFFFFU << 0)

/* ---- TTIR — TT 中断 (写1清) ---- */
#define FDCAN_TTIR_CER          (1U << 18)
#define FDCAN_TTIR_AW           (1U << 17)
#define FDCAN_TTIR_WT           (1U << 16)
#define FDCAN_TTIR_IWTG         (1U << 15)
#define FDCAN_TTIR_ELC          (1U << 14)
#define FDCAN_TTIR_SE2          (1U << 13)
#define FDCAN_TTIR_SE1          (1U << 12)
#define FDCAN_TTIR_TXO          (1U << 11)
#define FDCAN_TTIR_TXU          (1U << 10)
#define FDCAN_TTIR_GTE          (1U << 9)
#define FDCAN_TTIR_GTD          (1U << 8)
#define FDCAN_TTIR_GTW          (1U << 7)
#define FDCAN_TTIR_SWE          (1U << 6)
#define FDCAN_TTIR_TTMI         (1U << 5)
#define FDCAN_TTIR_RTMI         (1U << 4)
#define FDCAN_TTIR_SOG          (1U << 3)
#define FDCAN_TTIR_CSM          (1U << 2)
#define FDCAN_TTIR_SMC          (1U << 1)
#define FDCAN_TTIR_SBC          (1U << 0)

/* ---- TTIE — TT 中断使能 ---- */
#define FDCAN_TTIE_CERE         (1U << 18)
#define FDCAN_TTIE_AWE          (1U << 17)
#define FDCAN_TTIE_WTE          (1U << 16)
#define FDCAN_TTIE_IWTE         (1U << 15)
#define FDCAN_TTIE_ELCE         (1U << 14)
#define FDCAN_TTIE_SE2E         (1U << 13)
#define FDCAN_TTIE_SE1E         (1U << 12)
#define FDCAN_TTIE_TXOE         (1U << 11)
#define FDCAN_TTIE_TXUE         (1U << 10)
#define FDCAN_TTIE_GTEE         (1U << 9)
#define FDCAN_TTIE_GTDE         (1U << 8)
#define FDCAN_TTIE_GTWE         (1U << 7)
#define FDCAN_TTIE_SWEE         (1U << 6)
#define FDCAN_TTIE_TTMIE        (1U << 5)
#define FDCAN_TTIE_RTMIE        (1U << 4)
#define FDCAN_TTIE_SOGE         (1U << 3)
#define FDCAN_TTIE_CSME         (1U << 2)
#define FDCAN_TTIE_SMCE         (1U << 1)
#define FDCAN_TTIE_SBCE         (1U << 0)

/* ---- TTILS — TT 中断线选择 ---- */
#define FDCAN_TTILS_CERL        (1U << 18)
#define FDCAN_TTILS_AWL         (1U << 17)
#define FDCAN_TTILS_WTL         (1U << 16)
#define FDCAN_TTILS_IWTL        (1U << 15)
#define FDCAN_TTILS_ELCL        (1U << 14)
#define FDCAN_TTILS_SE2L        (1U << 13)
#define FDCAN_TTILS_SE1L        (1U << 12)
#define FDCAN_TTILS_TXOL        (1U << 11)
#define FDCAN_TTILS_TXUL        (1U << 10)
#define FDCAN_TTILS_GTEL        (1U << 9)
#define FDCAN_TTILS_GTDL        (1U << 8)
#define FDCAN_TTILS_GTWL        (1U << 7)
#define FDCAN_TTILS_SWEL        (1U << 6)
#define FDCAN_TTILS_TTMIL       (1U << 5)
#define FDCAN_TTILS_RTMIL       (1U << 4)
#define FDCAN_TTILS_SOGL        (1U << 3)
#define FDCAN_TTILS_CSML        (1U << 2)
#define FDCAN_TTILS_SMCL        (1U << 1)
#define FDCAN_TTILS_SBCL        (1U << 0)

/* ---- TTOST — TT 操作状态 (只读) ---- */
#define FDCAN_TTOST_SPL         (1U << 31)
#define FDCAN_TTOST_WECS        (1U << 30)
#define FDCAN_TTOST_AWE         (1U << 29)
#define FDCAN_TTOST_WFE         (1U << 28)
#define FDCAN_TTOST_GSI         (1U << 27)
#define FDCAN_TTOST_TMP_Pos     24
#define FDCAN_TTOST_TMP_Msk     (7U << 24)
#define FDCAN_TTOST_GFI         (1U << 23)
#define FDCAN_TTOST_WGTD        (1U << 22)
#define FDCAN_TTOST_RTO_Pos     8
#define FDCAN_TTOST_RTO_Msk     (0xFFU << 8)
#define FDCAN_TTOST_QCS         (1U << 7)
#define FDCAN_TTOST_QGTP        (1U << 6)
#define FDCAN_TTOST_SYS_Pos     4
#define FDCAN_TTOST_SYS_Msk     (3U << 4)
#define FDCAN_TTOST_MS_Pos      2
#define FDCAN_TTOST_MS_Msk      (3U << 2)
#define FDCAN_TTOST_EL_Pos      0
#define FDCAN_TTOST_EL_Msk      (3U << 0)

/* ---- TURNA — TT TUR 实际值 (只读) ---- */
#define FDCAN_TURNA_NAV_Pos     0
#define FDCAN_TURNA_NAV_Msk     (0x3FFFFU << 0)

/* ---- TTLGT — TT 本地和全局时间 (只读) ---- */
#define FDCAN_TTLGT_GT_Pos      16
#define FDCAN_TTLGT_GT_Msk      (0xFFFFU << 16)
#define FDCAN_TTLGT_LT_Pos      0
#define FDCAN_TTLGT_LT_Msk      (0xFFFFU << 0)

/* ---- TTCTC — TT 循环时间和计数 (只读) ---- */
#define FDCAN_TTCTC_CC_Pos      16
#define FDCAN_TTCTC_CC_Msk      (0x3FU << 16)
#define FDCAN_TTCTC_CT_Pos      0
#define FDCAN_TTCTC_CT_Msk      (0xFFFFU << 0)

/* ---- TTCPT — TT 捕获时间 (只读) ---- */
#define FDCAN_TTCPT_SWV_Pos     16
#define FDCAN_TTCPT_SWV_Msk     (0xFFFFU << 16)
#define FDCAN_TTCPT_CCV_Pos     0
#define FDCAN_TTCPT_CCV_Msk     (0x3FU << 0)

/* ---- TTCSM — TT cycle sync mark (只读) ---- */
#define FDCAN_TTCSM_CSM_Pos     0
#define FDCAN_TTCSM_CSM_Msk     (0xFFFFU << 0)

/* ---- TTTS — TT 触发选择 ---- */
#define FDCAN_TTTS_EVTSEL_Pos   4
#define FDCAN_TTTS_EVTSEL_Msk   (3U << 4)
#define FDCAN_TTTS_SWTDEL_Pos   0
#define FDCAN_TTTS_SWTDEL_Msk   (3U << 0)


/* ================================================================== */
/*  CCU — Clock Calibration Unit                                       */
/* ================================================================== */

/* ---- CCU_CCFG — 校准配置 ---- */
#define FDCAN_CCU_CCFG_SWR      (1U << 31)
#define FDCAN_CCU_CCFG_CDIV_Pos 16
#define FDCAN_CCU_CCFG_CDIV_Msk (0xFU << 16)
#define FDCAN_CCU_CCFG_OCPM_Pos 8
#define FDCAN_CCU_CCFG_OCPM_Msk (0xFFU << 8)
#define FDCAN_CCU_CCFG_CFL      (1U << 7)
#define FDCAN_CCU_CCFG_BCC      (1U << 6)
#define FDCAN_CCU_CCFG_TQBT_Pos 0
#define FDCAN_CCU_CCFG_TQBT_Msk (0x1FU << 0)

/* ---- CCU_CSTAT — 校准状态 (只读) ---- */
#define FDCAN_CCU_CSTAT_CALS_Pos    30
#define FDCAN_CCU_CSTAT_CALS_Msk    (3U << 30)
#define FDCAN_CCU_CSTAT_CALS_NCAL   0U      /* Not_Calibrated */
#define FDCAN_CCU_CSTAT_CALS_BASIC  1U      /* Basic_Calibrated */
#define FDCAN_CCU_CSTAT_CALS_PREC   2U      /* Precision_Calibrated */
#define FDCAN_CCU_CSTAT_TQC_Pos     18
#define FDCAN_CCU_CSTAT_TQC_Msk     (0x7FFU << 18)
#define FDCAN_CCU_CSTAT_OCPC_Pos    0
#define FDCAN_CCU_CSTAT_OCPC_Msk    (0x3FFFFU << 0)

/* ---- CCU_CWD — 校准看门狗 ---- */
#define FDCAN_CCU_CWD_WDV_Pos      16
#define FDCAN_CCU_CWD_WDV_Msk      (0xFFFFU << 16)
#define FDCAN_CCU_CWD_WDC_Pos      0
#define FDCAN_CCU_CWD_WDC_Msk      (0xFFFFU << 0)

/* ---- CCU_IR — 中断 (写1清) ---- */
#define FDCAN_CCU_IR_CSC           (1U << 1)
#define FDCAN_CCU_IR_CWE           (1U << 0)

/* ---- CCU_IE — 中断使能 ---- */
#define FDCAN_CCU_IE_CSCE          (1U << 1)
#define FDCAN_CCU_IE_CWEE          (1U << 0)


/* ================================================================== */
/*  中断控制 (内联)                                                      */
/* ================================================================== */

static inline void FdcanIrqEnable(volatile FdcanRegs_t *fdcan, uint32_t mask)
{
    fdcan->IE |= mask;
}

static inline void FdcanIrqDisable(volatile FdcanRegs_t *fdcan, uint32_t mask)
{
    fdcan->IE &= ~mask;
}

static inline void FdcanIrqClear(volatile FdcanRegs_t *fdcan, uint32_t mask)
{
    fdcan->IR = mask;
}

static inline uint32_t FdcanIrqGet(volatile FdcanRegs_t *fdcan)
{
    return fdcan->IR & fdcan->IE;
}

static inline void FdcanIrqLineEnable(volatile FdcanRegs_t *fdcan, uint32_t lines)
{
    fdcan->ILE = lines & (FDCAN_ILE_EINT0 | FDCAN_ILE_EINT1);
}

static inline void FdcanIrqLineDisable(volatile FdcanRegs_t *fdcan)
{
    fdcan->ILE = 0;
}

static inline void FdcanEnterCfg(volatile FdcanRegs_t *fdcan)
{
    fdcan->CCCR |= FDCAN_CCCR_INIT;
    while (!(fdcan->CCCR & FDCAN_CCCR_INIT));
    fdcan->CCCR |= FDCAN_CCCR_CCE;
}

static inline void FdcanExitCfg(volatile FdcanRegs_t *fdcan)
{
    fdcan->CCCR &= ~FDCAN_CCCR_INIT;
    while (fdcan->CCCR & FDCAN_CCCR_INIT);
}

/* ================================================================== */
/*  其他寄存器读取 (封装直操作)                                           */
/* ================================================================== */

static inline uint32_t FdcanEcr(volatile FdcanRegs_t *fdcan)
{
    return fdcan->ECR;
}

static inline uint32_t FdcanNdat1(volatile FdcanRegs_t *fdcan)
{
    return fdcan->NDAT1;
}

#endif /* STM32MP1XX_FDCAN_H_ */
