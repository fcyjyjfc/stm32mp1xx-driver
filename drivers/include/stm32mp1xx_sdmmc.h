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
#define SDMMC_DCTRL_DBLOCKSIZE_Pos 4
#define SDMMC_DCTRL_FIFORST     (1U << 13)

/* ---- IDMACTRLR 位域 ---- */
#define SDMMC_IDMA_IDMAEN       (1U << 0)
#define SDMMC_IDMA_IDMABMODE    (1U << 1)

/* ---- POWER 位域 ---- */
#define SDMMC_POWER_PWRCTRL_Pos 0
#define SDMMC_POWER_PWRCTRL_Msk (3U << 0)


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

typedef struct {
    uint8_t          index;
    uint32_t         arg;
    SdmmcRespType_t  resp_type;
    uint8_t          cmdtrans;
    uint8_t          cmdstop;
} SdmmcCmd_t;

typedef struct {
    uint32_t        buf_addr;
    uint32_t        len;
    uint8_t         block_size_log;
    uint8_t         dir_read;
} SdmmcData_t;


/* ===== 函数声明 ===== */

extern void SdmmcPowerOn(volatile SdmmcRegs_t *sdmmc, uint32_t ker_ck_hz);
extern void SdmmcPowerOff(volatile SdmmcRegs_t *sdmmc);
extern void SdmmcSetClock(volatile SdmmcRegs_t *sdmmc, uint32_t ker_ck_hz, uint32_t target_hz);
extern void SdmmcSetBusWidth(volatile SdmmcRegs_t *sdmmc, SdmmcBusWidth_t width);

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


#endif /* STM32MP1XX_SDMMC_H_ */
