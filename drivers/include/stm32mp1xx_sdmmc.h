/*
 * stm32mp1xx_sdmmc.h
 *
 *  Created on: 2025-6-21
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_SDMMC_H_
#define STM32MP1XX_SDMMC_H_

#include <stdint.h>


/* SDMMC 寄存器结构体, 必须 32 位字访问 */
typedef struct {
    uint32_t POWER;         // 0x000 电源控制: PWRCTRL/VSWITCH/VSWITCHEN/DIRPOL
    uint32_t CLKCR;         // 0x004 时钟控制: CLKDIV/WIDBUS/DDR/HWFC_EN/NEGEDGE/PWRSAV/BUSSPEED/SELCLKRX
    uint32_t ARGR;          // 0x008 命令参数: CMDARG[31:0]
    uint32_t CMDR;          // 0x00C 命令寄存器: CMDINDEX/WAITRESP/CPSMEN/CMDTRANS/CMDSTOP/WAITPEND/WAITINT/DTHOLD/BOOTEN/BOOTMODE/CMDSUSPEND
    uint32_t RESPCMDR;      // 0x010 响应命令索引: RESPCMD[5:0] (只读)
    uint32_t RESP1R;        // 0x014 响应1: 短响应数据 / 长响应[127:96]
    uint32_t RESP2R;        // 0x018 响应2: 长响应[95:64]
    uint32_t RESP3R;        // 0x01C 响应3: 长响应[63:32]
    uint32_t RESP4R;        // 0x020 响应4: 长响应[31:0]
    uint32_t DTIMER;        // 0x024 数据/Busy超时: DATATIME[31:0] (CK周期数)
    uint32_t DLENR;         // 0x028 数据长度: DATALENGTH[24:0]
    uint32_t DCTRL;         // 0x02C 数据控制: DTEN/DTDIR/DTMODE/DBLOCKSIZE/RWSTART/RWSTOP/RWMOD/SDIOEN/BOOTACKEN/FIFORST
    uint32_t DCNTR;         // 0x030 数据计数 (只读): DATACOUNT[24:0]
    uint32_t STAR;          // 0x034 状态寄存器 (只读)
    uint32_t ICR;           // 0x038 中断清除 (写1清)
    uint32_t MASKR;         // 0x03C 中断使能掩码
    uint32_t ACKTIMER;      // 0x040 Boot确认超时: ACKTIME[24:0]
    uint8_t  RSVD0[0x0C];  // 0x044~0x04F
    uint32_t IDMACTRLR;    // 0x050 IDMA控制: IDMAEN/IDMABMODE
    uint32_t IDMABSIZER;   // 0x054 IDMA buffer大小: IDMABNDT[11:0] (×32=字节数)
    uint32_t IDMABASER;    // 0x058 IDMA buffer基地址 (word对齐)
    uint8_t  RSVD1[0x08];  // 0x05C~0x063
    uint32_t IDMALAR;       // 0x064 IDMA链表地址: ULA/ULS/ABR/IDMALA[13:0]
    uint32_t IDMABAR;       // 0x068 IDMA链表基地址: IDMABA[29:0]
    uint8_t  RSVD2[0x14];  // 0x06C~0x07F
    uint32_t FIFOR[16];     // 0x080~0x0BC 数据FIFO (16×32-bit = 64B)
    uint8_t  RSVD3[0x334]; // 0x0C0~0x3F3
    uint32_t VERR;          // 0x3F4 版本号
    uint32_t IPIDR;         // 0x3F8 IP标识
    uint32_t SIDR;          // 0x3FC Size标识
} SdmmcRegs_t;


#define SDMMC1_BASE     0x58005000U
#define SDMMC2_BASE     0x58007000U
#define SDMMC3_BASE     0x48004000U

#define SDMMC1          ((volatile SdmmcRegs_t *)SDMMC1_BASE)
#define SDMMC2          ((volatile SdmmcRegs_t *)SDMMC2_BASE)
#define SDMMC3          ((volatile SdmmcRegs_t *)SDMMC3_BASE)


/* ---- STAR 状态位 ---- */
#define SDMMC_STA_CCRCFAIL      (1U << 0)
#define SDMMC_STA_DCRCFAIL      (1U << 1)
#define SDMMC_STA_CTIMEOUT      (1U << 2)
#define SDMMC_STA_DTIMEOUT      (1U << 3)
#define SDMMC_STA_TXUNDERR      (1U << 4)
#define SDMMC_STA_RXOVERR       (1U << 5)
#define SDMMC_STA_CMDREND       (1U << 6)
#define SDMMC_STA_CMDSENT       (1U << 7)
#define SDMMC_STA_DATAEND       (1U << 8)
#define SDMMC_STA_DHOLD         (1U << 9)
#define SDMMC_STA_DBCKEND       (1U << 10)
#define SDMMC_STA_DABORT        (1U << 11)
#define SDMMC_STA_DPSMACT       (1U << 12)
#define SDMMC_STA_CPSMACT       (1U << 13)
#define SDMMC_STA_BUSYD0        (1U << 20)
#define SDMMC_STA_BUSYD0END     (1U << 21)
#define SDMMC_STA_IDMATE        (1U << 27)
#define SDMMC_STA_IDMABTC       (1U << 28)

/* ---- ICR 清除位 (与 STAR 对应) ---- */
#define SDMMC_ICR_ALLFLAGS      0x1FE00FFFU

/* ---- CLKCR 位域 ---- */
#define SDMMC_CLKCR_CLKDIV_Pos  0
#define SDMMC_CLKCR_CLKDIV_Msk  (0x3FFU << 0)
#define SDMMC_CLKCR_PWRSAV      (1U << 12)
#define SDMMC_CLKCR_WIDBUS_Pos  14
#define SDMMC_CLKCR_WIDBUS_Msk  (3U << 14)
#define SDMMC_CLKCR_NEGEDGE     (1U << 16)
#define SDMMC_CLKCR_HWFC_EN     (1U << 17)
#define SDMMC_CLKCR_DDR         (1U << 18)
#define SDMMC_CLKCR_BUSSPEED    (1U << 19)
#define SDMMC_CLKCR_SELCLKRX_Pos 20

/* ---- CMDR 位域 ---- */
#define SDMMC_CMDR_CMDINDEX_Pos 0
#define SDMMC_CMDR_CMDTRANS     (1U << 6)
#define SDMMC_CMDR_CMDSTOP      (1U << 7)
#define SDMMC_CMDR_WAITRESP_Pos 8
#define SDMMC_CMDR_WAITRESP_Msk (3U << 8)
#define SDMMC_CMDR_WAITINT      (1U << 10)
#define SDMMC_CMDR_WAITPEND     (1U << 11)
#define SDMMC_CMDR_CPSMEN       (1U << 12)
#define SDMMC_CMDR_DTHOLD       (1U << 13)
#define SDMMC_CMDR_BOOTEN       (1U << 15)
#define SDMMC_CMDR_CMDSUSPEND   (1U << 16)

/* ---- DCTRL 位域 ---- */
#define SDMMC_DCTRL_DTEN        (1U << 0)
#define SDMMC_DCTRL_DTDIR       (1U << 1)
#define SDMMC_DCTRL_DTMODE_Pos  2
#define SDMMC_DCTRL_DTMODE_Msk  (3U << 2)
#define SDMMC_DCTRL_SDMODE      (1U << 2)   /* DTMODE=01: SD电压切换模式 */
#define SDMMC_DCTRL_DBLOCKSIZE_Pos 4
#define SDMMC_DCTRL_FIFORST     (1U << 13)

/* ---- IDMACTRLR 位域 ---- */
#define SDMMC_IDMA_IDMAEN       (1U << 0)
#define SDMMC_IDMA_IDMABMODE    (1U << 1)

/* ---- IDMALAR 位域 ---- */
#define SDMMC_IDMALAR_ULA       (1U << 31)  /* 0=最后, 1=继续下一节点 */
#define SDMMC_IDMALAR_ULS       (1U << 30)  /* 0=保持size(2-word), 1=更新size(3-word) */
#define SDMMC_IDMALAR_ABR       (1U << 29)  /* 0=下一节点未就绪(报错), 1=就绪 */

/* ---- POWER 位域 ---- */
#define SDMMC_POWER_PWRCTRL_Pos 0
#define SDMMC_POWER_PWRCTRL_Msk (3U << 0)


/* ===== DBLOCKSIZE 块大小枚举 (实际字节数 = 1U << value) ===== */
typedef enum {
    SDMMC_BLKSIZ_1B    = 0,
    SDMMC_BLKSIZ_2B    = 1,
    SDMMC_BLKSIZ_4B    = 2,
    SDMMC_BLKSIZ_8B    = 3,
    SDMMC_BLKSIZ_16B   = 4,
    SDMMC_BLKSIZ_32B   = 5,
    SDMMC_BLKSIZ_64B   = 6,
    SDMMC_BLKSIZ_128B  = 7,
    SDMMC_BLKSIZ_256B  = 8,
    SDMMC_BLKSIZ_512B  = 9,
    SDMMC_BLKSIZ_1KB   = 10,
    SDMMC_BLKSIZ_2KB   = 11,
    SDMMC_BLKSIZ_4KB   = 12,
    SDMMC_BLKSIZ_8KB   = 13,
    SDMMC_BLKSIZ_16KB  = 14,
    SDMMC_BLKSIZ_32KB  = 15,
} SdmmcBlockSize_t;

/* SD 卡固定块大小 (512 字节) — DBLOCKSIZE 对应值 */
#define SDMMC_BLKSIZ_BYTES(x)  (1U << (x))

/* SDMMC 外设常数 */
#define SDMMC_INIT_CLK_HZ    400000U     /* 识别模式时钟上限 (Hz) */
#define SDMMC_BUSY_TIMEOUT   500000000U  /* BUSYD0 轮询循环上限 */
#define SDMMC_DTIMER_MAX     0xFFFFFFFFU /* DTIMER 最大超时值 */

/* ===== 类型定义 ===== */

typedef enum {
    SDMMC_OK = 0,
    SDMMC_ERR_CTIMEOUT,
    SDMMC_ERR_CCRCFAIL,
    SDMMC_ERR_DCRCFAIL,
    SDMMC_ERR_DTIMEOUT,
    SDMMC_ERR_RXOVERR,
    SDMMC_ERR_TXUNDERR,
    SDMMC_ERR_RESP_MISMATCH,
    SDMMC_ERR_PARAM,
    SDMMC_ERR_BUSY,
} SdmmcErr_t;

typedef enum {
    SDMMC_RESP_NONE       = 0,
    SDMMC_RESP_SHORT      = 1,
    SDMMC_RESP_SHORT_NOCRC = 2,
    SDMMC_RESP_LONG       = 3,
} SdmmcRespType_t;

typedef enum {
    SDMMC_BUS_1BIT = 0,
    SDMMC_BUS_4BIT = 1,
    SDMMC_BUS_8BIT = 2,
} SdmmcBusWidth_t;

/* 传输方向: SdmmcData_t.dir_read 取值 */
typedef enum {
    SDMMC_DIR_WRITE = 0, /* 主机→卡 */
    SDMMC_DIR_READ  = 1, /* 卡→主机 */
} SdmmcDir_t;

/* cmdtrans: 命令是否后跟数据传输 */
typedef enum {
    SDMMC_CMD_NODATA = 0, /* 纯命令 */
    SDMMC_CMD_DATA   = 1, /* 命令后跟数据传输, 触发 DPSM */
} SdmmcCmdTrans_t;

/* cmdstop: 是否中止当前传输 */
typedef enum {
    SDMMC_CMD_NOSTOP = 0, /* 正常传输 */
    SDMMC_CMD_STOP   = 1, /* 中止 (CMD12 STOP_TRANSMISSION) */
} SdmmcCmdStop_t;

typedef struct {
    uint8_t          index;
    uint32_t         arg;
    SdmmcRespType_t  resp_type;
    SdmmcCmdTrans_t  cmdtrans;
    SdmmcCmdStop_t   cmdstop;
    uint8_t          waitpend;   /* 置位后 CPSM 等 DAT0 忙释放再设 CMDREND (R1b 命令) */
} SdmmcCmd_t;

typedef struct {
    uint32_t          buf_addr;
    uint32_t          len;
    SdmmcBlockSize_t  block_size_log;
    SdmmcDir_t        dir_read;
} SdmmcData_t;

/* IDMA 链表描述符 — 3-word 可变大小模式 (ULS=1)
 * 12 字节/节点, 直接被硬件加载到 IDMALAR/IDMABASER/IDMABSIZER 三寄存器
 * buf_size 直接填字节数 (32 倍数, bit[4:0] 保留区自动为 0)
 * idmalar.IDMALA = 从 IDMABA 到下一描述符的字节偏移 (4 字节对齐) */
typedef struct {
    uint32_t idmalar;       /* → IDMALAR: ULA(31)|ULS(30)|ABR(29)|IDMALA[15:2] */
    uint32_t buf_addr;      /* → IDMABASER (word 对齐) */
    uint32_t buf_size;      /* → IDMABSIZER (字节, 32 倍数) */
} SdmmcIdmaDesc_t;


/* ===== 函数声明 ===== */

extern void SdmmcPowerOn(volatile SdmmcRegs_t *sdmmc, uint32_t ker_ck_hz);
extern void SdmmcPowerOff(volatile SdmmcRegs_t *sdmmc);
extern void SdmmcSetClock(volatile SdmmcRegs_t *sdmmc, uint32_t ker_ck_hz, uint32_t target_hz);
extern void SdmmcSetBusWidth(volatile SdmmcRegs_t *sdmmc, SdmmcBusWidth_t width);

/* 一次 CLKCR 写入配置总线宽度 + DDR 模式, 避免 DDR/宽度分别写入的中间态 */
extern void SdmmcSetBusConfig(volatile SdmmcRegs_t *sdmmc, SdmmcBusWidth_t width, int ddr_enable);

extern SdmmcErr_t SdmmcSendCmd(volatile SdmmcRegs_t *sdmmc,
                               const SdmmcCmd_t *cmd,
                               uint32_t resp[4]);

extern SdmmcErr_t SdmmcReadData(volatile SdmmcRegs_t *sdmmc,
                                const SdmmcCmd_t *cmd,
                                const SdmmcData_t *data,
                                uint32_t resp[4]);

extern SdmmcErr_t SdmmcWriteData(volatile SdmmcRegs_t *sdmmc,
                                 const SdmmcCmd_t *cmd,
                                 const SdmmcData_t *data,
                                 uint32_t resp[4]);

/* IDMA 链表模式读写 (scatter-gather, 描述符数组在内存中) */
extern SdmmcErr_t SdmmcReadDataLL(volatile SdmmcRegs_t *sdmmc,
                                   const SdmmcCmd_t *cmd,
                                   const SdmmcIdmaDesc_t *desc_first,
                                   SdmmcBlockSize_t  block_size_log,
                                   uint32_t resp[4]);

extern SdmmcErr_t SdmmcWriteDataLL(volatile SdmmcRegs_t *sdmmc,
                                    const SdmmcCmd_t *cmd,
                                    const SdmmcIdmaDesc_t *desc_first,
                                    SdmmcBlockSize_t  block_size_log,
                                    uint32_t resp[4]);


/* ===== 中断模式 (非阻塞) — 每个 SDMMC 实例独立状态 ===== */

#define SDMMC_DBG_TRACE_SIZE  64

/* 实例中断状态, 前台轮询字段 it_done / it_result / btc_cnt */
typedef struct {
    volatile uint32_t   it_done;
    volatile SdmmcErr_t it_result;
    volatile uint32_t   btc_cnt;
    volatile uint32_t   busy0end;
    volatile uint32_t   dbg_trace[SDMMC_DBG_TRACE_SIZE];
    volatile uint32_t   dbg_idx;
} SdmmcIrqState_t;

extern SdmmcIrqState_t g_sdmmc1_irq;
extern SdmmcIrqState_t g_sdmmc2_irq;
extern SdmmcIrqState_t g_sdmmc3_irq;

/* 启动 LL 传输 (不阻塞), irq 指向对应实例的 SdmmcIrqState_t, ISR 接管后续 */
void SdmmcStartWriteLL(volatile SdmmcRegs_t *sdmmc, SdmmcIrqState_t *irq,
                       const SdmmcCmd_t *cmd,
                       const SdmmcIdmaDesc_t *desc_first,
                       SdmmcBlockSize_t  block_size_log);

void SdmmcStartReadLL(volatile SdmmcRegs_t *sdmmc, SdmmcIrqState_t *irq,
                      const SdmmcCmd_t *cmd,
                      const SdmmcIdmaDesc_t *desc_first,
                      SdmmcBlockSize_t  block_size_log);

/* SDMMC1 / SDMMC2 / SDMMC3 独立中断入口, 编译时绑定, 由 do_irq → vector table 分发 */
void Sdmmc1IrqHandler(void);
void Sdmmc2IrqHandler(void);
void Sdmmc3IrqHandler(void);


/* ===== 寄存器辅助操作 (供上层使用, 避免直接访问寄存器) ===== */

/* 清除 ICR 所有标志位 */
void SdmmcClearFlags(volatile SdmmcRegs_t *sdmmc);

/* 设置数据超时计数器 (CK 周期数), 用于 R1b 命令等需要延长超时的场景 */
void SdmmcSetDataTimeout(volatile SdmmcRegs_t *sdmmc, uint32_t timeout);

/* 等待 BUSYD0 释放 (卡忙结束), timeout 为循环次数上限, 返回 SDMMC_OK 或 SDMMC_ERR_BUSY */
SdmmcErr_t SdmmcWaitBusy(volatile SdmmcRegs_t *sdmmc, uint32_t timeout);

/* 检查 IDMATE (IDMA 传输错误) 标志 */
static inline int SdmmcCheckIdmate(volatile SdmmcRegs_t *sdmmc)
{
    return (sdmmc->STAR & SDMMC_STA_IDMATE) != 0;
}

/* 设置 DDR 模式 (CLKCR.DDR), enable=1 使能 DDR, 0 关闭 */
void SdmmcSetDdrMode(volatile SdmmcRegs_t *sdmmc, int enable);

/* SELCLKRX 取值 (CLKCR bits 20-21): 读数据 RX 时钟源选择 */
#define SDMMC_CLKCR_SELCLKRX_CK     0U  /* CK (或 NEGEDGE 反向) */
#define SDMMC_CLKCR_SELCLKRX_INV_CK 1U  /* CK 延迟 */
#define SDMMC_CLKCR_SELCLKRX_DS     2U  /* DS (data strobe, HS400) */

/* 设置 DCTRL.DTMODE 字段 (00=普通, 01=SD电压切换) */
void SdmmcSetDtMode(volatile SdmmcRegs_t *sdmmc, uint32_t mode);

/* 等待 DATAEND (数据阶段完成) 或 DTIMEOUT, 超时返回 SDMMC_ERR_DTIMEOUT */
SdmmcErr_t SdmmcWaitDataEnd(volatile SdmmcRegs_t *sdmmc);

/* 关闭 IDMA (IDMACTRLR=0) */
void SdmmcIdmaDisable(volatile SdmmcRegs_t *sdmmc);

/* 屏蔽所有中断 (MASKR=0) */
void SdmmcIntrMaskAll(volatile SdmmcRegs_t *sdmmc);

/* 脉冲 FIFORST: 复位 FIFO 和数据通路状态机 (DPSM), 用于错误恢复 */
void SdmmcFifoReset(volatile SdmmcRegs_t *sdmmc);


#endif /* STM32MP1XX_SDMMC_H_ */
