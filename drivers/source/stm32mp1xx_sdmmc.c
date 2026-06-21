/*
 * stm32mp1xx_sdmmc.c — SDMMC 底层外设驱动 (物理协议层)
 *
 * 只负责: 上电、发命令、搬数据。
 * 不负责: SD卡初始化序列(CMD0/CMD8/ACMD41)、文件系统。
 *
 * 架构:
 *   SdmmcCmdPhase (static)  — 内部: 命令阶段, 不清尾部ICR
 *       ├── SdmmcSendCmd    — 纯命令 (CMD0/CMD7/ACMD41等)
 *       ├── SdmmcReadData   — 命令+读数据 (CMD17/CMD18)
 *       └── SdmmcWriteData  — 命令+写数据 (CMD24/CMD25)
 *
 *  Created on: 2025-6-21
 *      Author: gjsbr
 */

#include "stm32mp1xx_sdmmc.h"


/* 粗略软件延时, 不依赖 STGEN; CA7 跑几百MHz 时约 1ms/次 */
static void DelayMs(uint32_t ms)
{
    volatile uint32_t count;
    while (ms--)
    {
        count = 200000;
        while (count--)
            ;
    }
}


/*
 * 上电序列 (RM0436 58.6.1):
 *   PWRCTRL: 10(power-cycle) → 00(power-off) → 11(power-on)
 *   每步之间需要延时让供电稳定, 最后配置初始时钟 ≤ 400kHz (识别模式要求)
 */
void SdmmcPowerOn(volatile SdmmcRegs_t *sdmmc, uint32_t ker_ck_hz)
{
    /* Step1: Power-cycle — 对外部供电做一次完整复位 */
    sdmmc->POWER = (sdmmc->POWER & ~SDMMC_POWER_PWRCTRL_Msk) | (2U << SDMMC_POWER_PWRCTRL_Pos);
    DelayMs(2);

    /* Step2: Power-off */
    sdmmc->POWER &= ~SDMMC_POWER_PWRCTRL_Msk;
    DelayMs(2);

    /* Step3: Power-on — 之后 SDMMC_CK 开始输出 */
    sdmmc->POWER = (sdmmc->POWER & ~SDMMC_POWER_PWRCTRL_Msk) | (3U << SDMMC_POWER_PWRCTRL_Pos);
    DelayMs(2);

    /* 初始时钟: CK = ker_ck / (2 * CLKDIV), 保证 ≤ 400kHz */
    uint32_t clkdiv = (ker_ck_hz + 2U * 400000U - 1U) / (2U * 400000U);
    if (clkdiv == 0)
        clkdiv = 1;

    /* 其余位全0: 1-bit宽度, SDR, 无流控, CK常开 */
    sdmmc->CLKCR = (clkdiv & SDMMC_CLKCR_CLKDIV_Msk);
}


void SdmmcPowerOff(volatile SdmmcRegs_t *sdmmc)
{
    sdmmc->POWER &= ~SDMMC_POWER_PWRCTRL_Msk;
}


/*
 * 修改 SDMMC_CK 频率
 * 公式: CK = ker_ck / (2 * CLKDIV), CLKDIV=0 则直通(不支持DDR)
 * 使用向上取整确保 CK ≤ target_hz (SD 规范定义了各模式的时钟上限)
 * 注意: 必须在 CPSM/DPSM 空闲时调用
 */
void SdmmcSetClock(volatile SdmmcRegs_t *sdmmc, uint32_t ker_ck_hz, uint32_t target_hz)
{
    /* 向上取整, 保证实际 CK ≤ target_hz */
    uint32_t clkdiv = (ker_ck_hz + 2U * target_hz - 1U) / (2U * target_hz);
    if (clkdiv == 0)
        clkdiv = 1;

    uint32_t tmp = sdmmc->CLKCR;
    tmp &= ~SDMMC_CLKCR_CLKDIV_Msk;
    tmp |= (clkdiv & SDMMC_CLKCR_CLKDIV_Msk);
    sdmmc->CLKCR = tmp;
}


/*
 * 切换总线宽度, 枚举值直接对应 WIDBUS 字段
 * 4/8-bit 模式同时开启硬件流控 (HWFC_EN), 防止 FIFO 溢出/下溢
 */
void SdmmcSetBusWidth(volatile SdmmcRegs_t *sdmmc, SdmmcBusWidth_t width)
{
    uint32_t tmp = sdmmc->CLKCR;
    tmp &= ~SDMMC_CLKCR_WIDBUS_Msk;
    tmp |= ((uint32_t)width << SDMMC_CLKCR_WIDBUS_Pos);

    if (width >= SDMMC_BUS_4BIT)
        tmp |= SDMMC_CLKCR_HWFC_EN;
    else
        tmp &= ~SDMMC_CLKCR_HWFC_EN;

    sdmmc->CLKCR = tmp;
}


/*
 * 命令阶段 (内部公共函数, 被 SdmmcSendCmd / ReadData / WriteData 复用)
 *
 * 流程: 清ICR → 写ARG → 组装并写CMDR(触发CPSM) → 轮询STAR → 校验RESPCMDR → 读RESP
 *
 * 为什么结尾不清 ICR:
 *   ReadData/WriteData 在命令阶段后还要继续轮询 DATAEND,
 *   如果此处清 ICR 会把已置位的数据标志也清掉, 导致后续轮询永远等不到.
 *   由调用者在全部完成后统一清 ICR.
 *
 * RESPCMDR 校验:
 *   仅对 SDMMC_RESP_SHORT (R1/R6/R7) 做校验 — 卡回显命令索引, 不匹配说明通信异常.
 *   R2(长响应) 和 R3(OCR) 的 RESPCMDR 固定为 0x3F, 无校验意义.
 */
static SdmmcErr_t SdmmcCmdPhase(volatile SdmmcRegs_t *sdmmc,
                                const SdmmcCmd_t *cmd,
                                uint32_t resp[4])
{
    sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
    sdmmc->ARGR = cmd->arg;

    /* 组装 CMDR: 写入后 CPSM 立即开始发送命令帧 */
    uint32_t cmdr = (uint32_t)(cmd->index & 0x3F) << SDMMC_CMDR_CMDINDEX_Pos;
    cmdr |= ((uint32_t)cmd->resp_type << SDMMC_CMDR_WAITRESP_Pos);
    cmdr |= SDMMC_CMDR_CPSMEN;
    if (cmd->cmdtrans)
        cmdr |= SDMMC_CMDR_CMDTRANS;  /* 通知 DPSM: 命令完成后开始数据传输 */
    if (cmd->cmdstop)
        cmdr |= SDMMC_CMDR_CMDSTOP;   /* 通知 DPSM: 中止当前数据传输 (CMD12) */

    sdmmc->CMDR = cmdr;

    /* 轮询: 无响应等 CMDSENT, 有响应等 CMDREND 或错误 */
    uint32_t sta;
    if (cmd->resp_type == SDMMC_RESP_NONE)
    {
        do {
            sta = sdmmc->STAR;
        } while (!(sta & (SDMMC_STA_CMDSENT | SDMMC_STA_CTIMEOUT)));
    }
    else
    {
        do {
            sta = sdmmc->STAR;
        } while (!(sta & (SDMMC_STA_CMDREND | SDMMC_STA_CTIMEOUT | SDMMC_STA_CCRCFAIL)));
    }

    if (sta & SDMMC_STA_CTIMEOUT)
        return SDMMC_ERR_CTIMEOUT;

    /* R3(OCR) 响应无 CRC 字段, 硬件报 CCRCFAIL 是正常的, 不视为错误 */
    if ((sta & SDMMC_STA_CCRCFAIL) && (cmd->resp_type != SDMMC_RESP_SHORT_NOCRC))
        return SDMMC_ERR_CCRCFAIL;

    /* 校验回显的命令索引 */
    if (cmd->resp_type == SDMMC_RESP_SHORT)
    {
        if ((sdmmc->RESPCMDR & 0x3F) != cmd->index)
            return SDMMC_ERR_RESP_MISMATCH;
    }

    /* 读取响应数据, 上层根据命令类型自行解析 */
    if (resp)
    {
        resp[0] = sdmmc->RESP1R;
        resp[1] = sdmmc->RESP2R;
        resp[2] = sdmmc->RESP3R;
        resp[3] = sdmmc->RESP4R;
    }

    return SDMMC_OK;
}


/* 纯命令: 发命令 + 收响应, 不涉及数据传输 */
SdmmcErr_t SdmmcSendCmd(volatile SdmmcRegs_t *sdmmc,
                        const SdmmcCmd_t *cmd,
                        uint32_t resp[4])
{
    SdmmcErr_t err = SdmmcCmdPhase(sdmmc, cmd, resp);
    sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
    return err;
}


/*
 * 命令 + 读数据 (IDMA single buffer 模式)
 *
 * 时序 (RM0436 58.6.8):
 *   1. 配 DTIMER/DLENR/IDMA/DCTRL (不设 DTEN! SD/eMMC 禁止手动启动 DPSM)
 *   2. CmdPhase 发命令 (CMDTRANS=1), 响应收到后 CPSM 内部发 DataEnable 触发 DPSM
 *   3. IDMA 自动把 FIFO 数据搬到 buf_addr, 全部完成后置 DATAEND
 *
 * 易错点:
 *   - DTEN=1 会绕过 CPSM 直接启动 DPSM, 导致 CPSM 无法发送命令 (卡住)
 *     RM0436 明确说明: "DTEN must not be used with SD or eMMC cards"
 *
 * 参数约束:
 *   - len 必须是 2^block_size_log 的整数倍
 *   - buf_addr 必须 4 字节对齐 (IDMA 要求 word 对齐)
 *   - cmd->cmdtrans 应为 1
 */
SdmmcErr_t SdmmcReadData(volatile SdmmcRegs_t *sdmmc,
                         const SdmmcCmd_t *cmd,
                         const SdmmcData_t *data,
                         uint32_t resp[4])
{
    uint32_t block_size = 1U << data->block_size_log;
    if (data->len == 0 || (data->len & (block_size - 1)) || (data->buf_addr & 0x3))
        return SDMMC_ERR_PARAM;

    /* 配置数据路径: 超时用最大值, 方向=读(DTDIR=1)
     * 注意: 不设 DTEN — SD 卡数据传输由 CMDTRANS 触发, 不需要手动启动 DPSM
     * (RM0436: DTEN must not be used with SD or eMMC cards) */
    sdmmc->DTIMER = 0xFFFFFFFFU;
    sdmmc->DLENR = data->len;
    sdmmc->IDMABASER = data->buf_addr;
    sdmmc->IDMACTRLR = SDMMC_IDMA_IDMAEN;
    sdmmc->DCTRL = ((uint32_t)data->block_size_log << SDMMC_DCTRL_DBLOCKSIZE_Pos)
                 | SDMMC_DCTRL_DTDIR;

    /* 命令阶段: 发 CMD + 收响应 */
    SdmmcErr_t err = SdmmcCmdPhase(sdmmc, cmd, resp);
    if (err != SDMMC_OK)
    {
        sdmmc->IDMACTRLR = 0;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        return err;
    }

    /* 数据阶段: 等 DATAEND 或错误 */
    uint32_t sta;
    do {
        sta = sdmmc->STAR;
    } while (!(sta & (SDMMC_STA_DATAEND | SDMMC_STA_DTIMEOUT |
                      SDMMC_STA_DCRCFAIL | SDMMC_STA_RXOVERR)));

    sdmmc->IDMACTRLR = 0;
    sdmmc->ICR = SDMMC_ICR_ALLFLAGS;

    if (sta & SDMMC_STA_DTIMEOUT)
        return SDMMC_ERR_DTIMEOUT;
    if (sta & SDMMC_STA_DCRCFAIL)
        return SDMMC_ERR_DCRCFAIL;
    if (sta & SDMMC_STA_RXOVERR)
        return SDMMC_ERR_RXOVERR;

    return SDMMC_OK;
}


/*
 * 命令 + 写数据 (IDMA single buffer 模式)
 *
 * 与 ReadData 的区别:
 *   1. DCTRL 方向为写 (DTDIR=0, 不设该位)
 *   2. 数据发完后卡编程 Flash, DAT0 拉低=Busy, 轮询 BUSYD0 位等释放
 *   3. 错误检测: TXUNDERR (FIFO 空了 IDMA 来不及填) 替代 RXOVERR
 *
 * 易错点:
 *   - 不能用 BUSYD0END 等待写完成: DATAEND 后 DPSM 已回 idle, 无法检测跳变
 *     改为直接轮询 STAR.BUSYD0 电平 (=0 表示卡不忙)
 *   - 同 ReadData: 不设 DTEN
 */
SdmmcErr_t SdmmcWriteData(volatile SdmmcRegs_t *sdmmc,
                          const SdmmcCmd_t *cmd,
                          const SdmmcData_t *data,
                          uint32_t resp[4])
{
    uint32_t block_size = 1U << data->block_size_log;
    if (data->len == 0 || (data->len & (block_size - 1)) || (data->buf_addr & 0x3))
        return SDMMC_ERR_PARAM;

    /* 配置数据路径: 方向=写(DTDIR=0, 不设 DTDIR 位)
     * 不设 DTEN — 由 CMDTRANS 触发 (RM0436: DTEN must not be used with SD/eMMC) */
    sdmmc->DTIMER = 0xFFFFFFFFU;
    sdmmc->DLENR = data->len;
    sdmmc->IDMABASER = data->buf_addr;
    sdmmc->IDMACTRLR = SDMMC_IDMA_IDMAEN;
    sdmmc->DCTRL = ((uint32_t)data->block_size_log << SDMMC_DCTRL_DBLOCKSIZE_Pos);

    /* 命令阶段 */
    SdmmcErr_t err = SdmmcCmdPhase(sdmmc, cmd, resp);
    if (err != SDMMC_OK)
    {
        sdmmc->IDMACTRLR = 0;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        return err;
    }

    /* 数据阶段: 等主机侧数据发送完毕 */
    uint32_t sta;
    do {
        sta = sdmmc->STAR;
    } while (!(sta & (SDMMC_STA_DATAEND | SDMMC_STA_DTIMEOUT |
                      SDMMC_STA_DCRCFAIL | SDMMC_STA_TXUNDERR)));

    if (sta & SDMMC_STA_DTIMEOUT)
    {
        sdmmc->IDMACTRLR = 0;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        return SDMMC_ERR_DTIMEOUT;
    }
    if (sta & SDMMC_STA_DCRCFAIL)
    {
        sdmmc->IDMACTRLR = 0;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        return SDMMC_ERR_DCRCFAIL;
    }
    if (sta & SDMMC_STA_TXUNDERR)
    {
        sdmmc->IDMACTRLR = 0;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        return SDMMC_ERR_TXUNDERR;
    }

    /* 等卡编程完成: 卡在写 Flash 期间 DAT0 保持低电平
     * BUSYD0END 需要 DPSM 活跃才能检测跳变, 此处 DPSM 已 idle
     * 改为直接轮询 DAT0 电平: BUSYD0=1 表示忙, =0 表示完成 */
    do {
        sta = sdmmc->STAR;
    } while (sta & SDMMC_STA_BUSYD0);

    sdmmc->IDMACTRLR = 0;
    sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
    return SDMMC_OK;
}
