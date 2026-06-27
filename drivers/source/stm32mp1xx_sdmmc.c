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


/**
 * @brief SDMMC 上电序列, 配置初始时钟 ≤ 400kHz (RM0436 58.6.1)
 * @param sdmmc     SDMMC 实例
 * @param ker_ck_hz 内核时钟频率 (HSI 64MHz)
 *
 * PWRCTRL: 10(power-cycle) → 00(power-off) → 11(power-on)
 * 每步延时让供电稳定. 最后设 CLKDIV 保证 CK ≤ 400kHz.
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
    uint32_t clkdiv = (ker_ck_hz + 2U * SDMMC_INIT_CLK_HZ - 1U) / (2U * SDMMC_INIT_CLK_HZ);
    if (clkdiv == 0)
        clkdiv = 1;

    /* 其余位全0: 1-bit宽度, SDR, 无流控, CK常开 */
    sdmmc->CLKCR = (clkdiv & SDMMC_CLKCR_CLKDIV_Msk);
}


/**
 * @brief SDMMC 关电
 */
void SdmmcPowerOff(volatile SdmmcRegs_t *sdmmc)
{
    sdmmc->POWER &= ~SDMMC_POWER_PWRCTRL_Msk;
}


/**
 * @brief 修改 SDMMC_CK 频率
 * @param target_hz 目标频率, 实际 CK ≤ target_hz (向上取整 CLKDIV)
 *
 * CK = ker_ck / (2 * CLKDIV), CLKDIV=0 等效于 1.
 * 必须在 CPSM/DPSM 空闲时调用.
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


/**
 * @brief 切换总线宽度, 4/8-bit 同时开启硬件流控 (HWFC_EN) 防止 FIFO 溢出/下溢
 * @param width SDMMC_BUS_1BIT / _4BIT / _8BIT
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


/**
 * @brief 命令阶段 (内部公共函数, 被 SendCmd/ReadData/WriteData 复用)
 * @return SDMMC_OK 或错误码
 *
 * 流程: 清ICR → 写ARG → 组装CMDR(触发CPSM) → 轮询STAR → 校验RESPCMDR → 读RESP.
 *
 * 结尾不清 ICR — 调用者可能还要轮询数据阶段标志 (DATAEND 等),
 * 过早清除会导致后续永远等不到. 由调用者全部完成后统一清.
 *
 * RESPCMDR 仅对 R1/R6/R7 (SHORT) 校验 — 卡回显命令索引.
 * R2(长响应) 和 R3(OCR) 固定 0x3F, 不做校验.
 * R3 无 CRC 字段, 硬件报 CCRCFAIL 不视为错误.
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


/**
 * @brief 发送命令 (无数据传输)
 */
SdmmcErr_t SdmmcSendCmd(volatile SdmmcRegs_t *sdmmc,
                        const SdmmcCmd_t *cmd,
                        uint32_t resp[4])
{
    SdmmcErr_t err = SdmmcCmdPhase(sdmmc, cmd, resp);
    sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
    return err;
}


/**
 * @brief 命令 + 读数据 (IDMA single buffer)
 * @param data buf_addr 需 4 字节对齐, len 需 2^block_size_log 整数倍
 *
 * CMDTRANS 触发 DPSM, IDMA 自动搬 FIFO 到 buf_addr.
 * 不设 DTEN — SD/eMMC 卡禁止手动启动 DPSM (RM0436: "DTEN must not be used with SD or eMMC cards").
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
    sdmmc->DTIMER = SDMMC_DTIMER_MAX;
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


/**
 * @brief 命令 + 写数据 (IDMA single buffer)
 *
 * 与读的区别: DTDIR=0, 错误检测 TXUNDERR 替代 RXOVERR.
 * 写后卡内部编程拉低 DAT0 (Busy), 轮询 STAR.BUSYD0 等释放.
 * DATAEND 后 DPSM 已 idle, 不能用 BUSYD0END 等待 (它需要 DPSM 活跃).
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
    sdmmc->DTIMER = SDMMC_DTIMER_MAX;
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


/* ===== IDMA 链表模式 (scatter-gather, 3-word 可变大小节点) =====
 *
 * 链表第一个节点由寄存器本身描述, 后续节点从 IDMABAR + IDMALA 加载.
 *
 * 节点格式 (SdmmcIdmaDesc_t, 12 字节):
 *   idmalar  → IDMALAR: ULA(31)|ULS(30)|ABR(29)|IDMALA[15:2]
 *   buf_addr → IDMABASER (word 对齐)
 *   buf_size → IDMABSIZER (字节, bit[4:0]保留, 32 倍数天然对齐)
 *
 * 流程: 传第1个buffer → 查ULA → 从IDMABAR+IDMALA加载下一节点 → 重复直到ULA=0.
 *
 * 典型用法 (3 buffer 读):
 *   desc[0].idmalar = ULA|ULS|ABR|12;   // IDMALA → desc[1]
 *   desc[1].idmalar = ULA|ULS|ABR|24;   // IDMALA → desc[2]
 *   desc[2].idmalar = ULS|ABR;          // ULA=0 最后
 */


#define IDMA_MAX_NODES  256   /* 软件硬限, 防死循环; 硬件上限由 IDMALA[13:0] 决定 ≈ 5461 */


/**
 * @brief  校验 IDMA 描述符链, 返回总字节数
 * @param  desc       描述符数组首地址
 * @param  block_size 块大小 (字节)
 * @return 总字节数 (成功), 0 (失败: word 未对齐 / size 非 32B 倍数 / 环形链 / 超 32MB)
 */
static uint32_t LlValidate(const SdmmcIdmaDesc_t *desc, uint32_t block_size)
{
    const SdmmcIdmaDesc_t *d = desc;
    uint32_t total = 0;
    int count = 0;

    while (1)
    {
        if (count++ >= IDMA_MAX_NODES)      /* 软件约束: 防环形链无限循环 */
            return 0;
        if (d->buf_addr & 0x3)              /* IDMABASER 要求 word 对齐 */
            return 0;
        if (d->buf_size == 0 || (d->buf_size & 31))
            return 0;                       /* IDMABSIZER bit[4:0] 保留, 非 32B 倍数
                                               → 硬件截断丢数据, 拒绝 */

        total += d->buf_size;
        if (total > 0x1FFFFFFU)             /* DLENR DATALENGTH[24:0] 上限 */
            return 0;

        if (!(d->idmalar & SDMMC_IDMALAR_ULA))  /* ULA=0: 最后一个 buffer */
            break;

        /* IDMALA 在 bit[15:2], 低 2 位恒为 0 (4 字节对齐)
           掩码 & 0x0000FFFC 提取 14-bit 字节偏移 */
        d = (const SdmmcIdmaDesc_t *)((const uint8_t *)desc
               + (d->idmalar & 0x0000FFFCU));
    }

    if (total & (block_size - 1))           /* Block 模式: DATALENGTH 必须
                                               是 DBLOCKSIZE 整数倍 (RM 58.6.8) */
        return 0;

    return total;
}


/**
 * @brief  命令 + 多段读 (IDMA linked-list)
 * @param  desc_first      描述符链首节点
 * @param  block_size_log  log2(块大小), SD 卡固定 9 (512B)
 * @param  resp            命令响应 (4×32-bit, 可为 NULL)
 * @return SDMMC_OK 或错误码
 */
SdmmcErr_t SdmmcReadDataLL(volatile SdmmcRegs_t *sdmmc,
                            const SdmmcCmd_t *cmd,
                            const SdmmcIdmaDesc_t *desc_first,
                            SdmmcBlockSize_t block_size_log,
                            uint32_t resp[4])
{
    uint32_t block_size = 1U << block_size_log;
    uint32_t total_len = LlValidate(desc_first, block_size);
    if (total_len == 0)
        return SDMMC_ERR_PARAM;

    sdmmc->DTIMER = SDMMC_DTIMER_MAX;                /* 用最大超时, 避免长传输误触发 DTIMEOUT */
    sdmmc->DLENR = total_len;                   /* 总字节数, 必须是 DBLOCKSIZE 整数倍 */

    /* 第一个节点 = 寄存器本身 (IDMABASER/IDMABSIZER/IDMALAR)
       传完第 1 个 buffer 后硬件检查 IDMALAR.ULA → 1=从 IDMABAR+IDMALA 加载下一节点 */
    sdmmc->IDMABASER = desc_first->buf_addr;    /* 第 1 个 buffer 地址 (word 对齐) */
    sdmmc->IDMABSIZER = desc_first->buf_size;   /* 第 1 个 buffer 大小 (bit[4:0] 保留, 32B 倍数) */
    sdmmc->IDMABAR = (uint32_t)(uintptr_t)desc_first;  /* 描述符表基地址 */
    sdmmc->IDMALAR = desc_first->idmalar;       /* ULA(31)=继续, ULS(30)=3-word,
                                                   ABR(29)=就绪, IDMALA[15:2]=下一节点偏移 */
    sdmmc->IDMACTRLR = SDMMC_IDMA_IDMAEN    /* bit0: 使能 IDMA */
                     | SDMMC_IDMA_IDMABMODE; /* bit1: 链表模式 (非 single buffer) */

    /* DCTRL: DTDIR=1 读方向, DBLOCKSIZE=block_size_log */
    sdmmc->DCTRL = ((uint32_t)block_size_log << SDMMC_DCTRL_DBLOCKSIZE_Pos)
                 | SDMMC_DCTRL_DTDIR;

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


/**
 * @brief  命令 + 多段写 (IDMA linked-list)
 * @param  desc_first      描述符链首节点
 * @param  block_size_log  log2(块大小), SD 卡固定 9 (512B)
 * @param  resp            命令响应 (4×32-bit, 可为 NULL)
 * @return SDMMC_OK 或错误码
 */
SdmmcErr_t SdmmcWriteDataLL(volatile SdmmcRegs_t *sdmmc,
                             const SdmmcCmd_t *cmd,
                             const SdmmcIdmaDesc_t *desc_first,
                             SdmmcBlockSize_t block_size_log,
                             uint32_t resp[4])
{
    uint32_t block_size = 1U << block_size_log;
    uint32_t total_len = LlValidate(desc_first, block_size);
    if (total_len == 0)
        return SDMMC_ERR_PARAM;

    sdmmc->DTIMER = SDMMC_DTIMER_MAX;                /* 用最大超时, 避免长传输误触发 DTIMEOUT */
    sdmmc->DLENR = total_len;                   /* 总字节数, 必须是 DBLOCKSIZE 整数倍 */

    /* 第一个节点 = 寄存器本身 (IDMABASER/IDMABSIZER/IDMALAR)
       传完第 1 个 buffer 后硬件检查 IDMALAR.ULA → 1=从 IDMABAR+IDMALA 加载下一节点 */
    sdmmc->IDMABASER = desc_first->buf_addr;    /* 第 1 个 buffer 地址 (word 对齐) */
    sdmmc->IDMABSIZER = desc_first->buf_size;   /* 第 1 个 buffer 大小 (bit[4:0] 保留, 32B 倍数) */
    sdmmc->IDMABAR = (uint32_t)(uintptr_t)desc_first;  /* 描述符表基地址 */
    sdmmc->IDMALAR = desc_first->idmalar;       /* ULA(31)=继续, ULS(30)=3-word,
                                                   ABR(29)=就绪, IDMALA[15:2]=下一节点偏移 */
    sdmmc->IDMACTRLR = SDMMC_IDMA_IDMAEN    /* bit0: 使能 IDMA */
                     | SDMMC_IDMA_IDMABMODE; /* bit1: 链表模式 (非 single buffer) */

    /* DCTRL: DTDIR=0 写方向 (不置位), DBLOCKSIZE=block_size_log */
    sdmmc->DCTRL = ((uint32_t)block_size_log << SDMMC_DCTRL_DBLOCKSIZE_Pos);

    SdmmcErr_t err = SdmmcCmdPhase(sdmmc, cmd, resp);
    if (err != SDMMC_OK)
    {
        sdmmc->IDMACTRLR = 0;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        return err;
    }

    /* 数据阶段: 等 DATAEND 或错误 (写方向不检测 RXOVERR, 而是 TXUNDERR) */
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

    /* 写后卡进入 Busy 状态, 拉低 D0; 等 BUSYD0 释放后再清理 */
    do {
        sta = sdmmc->STAR;
    } while (sta & SDMMC_STA_BUSYD0);

    sdmmc->IDMACTRLR = 0;
    sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
    return SDMMC_OK;
}


/* ===== 寄存器辅助操作 ===== */

/**
 * @brief 清除 ICR 所有标志位
 */
void SdmmcClearFlags(volatile SdmmcRegs_t *sdmmc)
{
    sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
}

/**
 * @brief 设置数据超时计数器 (CK 周期数)
 * @param timeout DTIMER 值, 通常用 0xFFFFFFFF (最大超时)
 *
 * R1b 命令 (如 CMD38 擦除) 没有数据传输, 但硬件用 DTIMER 计时 BUSYD0 超时.
 * 设在命令发出之前, 防止卡忙期间硬件误报 DTIMEOUT.
 */
void SdmmcSetDataTimeout(volatile SdmmcRegs_t *sdmmc, uint32_t timeout)
{
    sdmmc->DTIMER = timeout;
}

/**
 * @brief 等待 BUSYD0 释放 (卡忙结束)
 * @param timeout 循环次数上限
 * @return SDMMC_OK (释放) 或 SDMMC_ERR_BUSY (超时)
 *
 * 用于 R1b 命令后等卡内部操作完成 (擦除/编程).
 * 轮询 STAR.BUSYD0, BUSYD0=1 卡忙, BUSYD0=0 卡空闲.
 */
SdmmcErr_t SdmmcWaitBusy(volatile SdmmcRegs_t *sdmmc, uint32_t timeout)
{
    uint32_t sta;
    uint32_t to = 0;
    do {
        sta = sdmmc->STAR;
        if (++to > timeout)
            return SDMMC_ERR_BUSY;
    } while (sta & SDMMC_STA_BUSYD0);
    return SDMMC_OK;
}

/**
 * @brief 设置 DCTRL.DTMODE 字段
 * @param mode 00=普通传输, 01=SD电压切换模式
 */
void SdmmcSetDtMode(volatile SdmmcRegs_t *sdmmc, uint32_t mode)
{
    sdmmc->DCTRL = (sdmmc->DCTRL & ~SDMMC_DCTRL_DTMODE_Msk)
                 | ((mode << SDMMC_DCTRL_DTMODE_Pos) & SDMMC_DCTRL_DTMODE_Msk);
}

/**
 * @brief 等待 DATAEND (数据阶段完成) 或 DTIMEOUT
 * @return SDMMC_OK (DATAEND) 或 SDMMC_ERR_DTIMEOUT
 *
 * 用于 SD 电压切换等非标准数据传输, 轮询 DATAEND/DTIMEOUT.
 */
SdmmcErr_t SdmmcWaitDataEnd(volatile SdmmcRegs_t *sdmmc)
{
    uint32_t sta;
    do {
        sta = sdmmc->STAR;
    } while (!(sta & (SDMMC_STA_DATAEND | SDMMC_STA_DTIMEOUT)));

    if (sta & SDMMC_STA_DTIMEOUT)
        return SDMMC_ERR_DTIMEOUT;
    return SDMMC_OK;
}

/**
 * @brief 关闭 IDMA (IDMACTRLR=0)
 */
void SdmmcIdmaDisable(volatile SdmmcRegs_t *sdmmc)
{
    sdmmc->IDMACTRLR = 0;
}

/**
 * @brief 屏蔽所有中断 (MASKR=0)
 */
void SdmmcIntrMaskAll(volatile SdmmcRegs_t *sdmmc)
{
    sdmmc->MASKR = 0;
}

/**
 * @brief 脉冲 FIFORST: 复位 FIFO 和数据通路状态机 (DPSM)
 *
 * 用于 IDMATE 等错误后的恢复, 清除 DPSM 滞留状态.
 * FIFORST 置位后保持至少 2 CK 周期再清零, NOP 延时即可.
 */
void SdmmcFifoReset(volatile SdmmcRegs_t *sdmmc)
{
    sdmmc->DCTRL |= SDMMC_DCTRL_FIFORST;
    __asm__ volatile("nop\n\tnop\n\tnop\n\tnop");
    sdmmc->DCTRL &= ~SDMMC_DCTRL_FIFORST;
}


/* ===== 中断模式 (非阻塞) =====
 *
 * MASKR 一次使能所有中断, ISR 按 STA 标志区分当前阶段.
 * 写后卡内部编程产生的 BUSYD0 由下次 SdmmcStartLL 入口检查 (不阻塞 ISR).
 * IDMATE 无中断使能位 (实测), 仅 STAR 标志 + ICR 清除.
 */

volatile uint32_t   g_sdmmc1_it_done;
volatile SdmmcErr_t g_sdmmc1_it_result;
volatile uint32_t   g_sdmmc1_btc_cnt;

/* 调试跟踪: IDMABTC ISR→1, 主循环置BAR→2, TXUNDERR ISR→3, IDMATE ISR→4 */
volatile uint32_t g_sdmmc1_dbg_trace[SDMMC_DBG_TRACE_SIZE];
volatile uint32_t g_sdmmc1_dbg_idx;

/* BUSYD0END 信号量: ISR 置 1, 前台轮询 */
volatile uint32_t g_sdmmc1_busy0end;

volatile SdmmcRegs_t *g_it_sdmmc;   /* 当前操作的 SDMMC 实例 */
static uint8_t                g_it_is_write; /* 1=写, 0=读 */


/**
 * @brief 启动 IDMA 链表传输 (不阻塞), 后续由 ISR 接管
 * @param is_write 1=写, 0=读
 *
 * 入口检查 BUSYD0 防上次写未完成的卡内部编程冲突.
 * 寄存器配置同阻塞版 Ll 函数, 最后用 MASKR 替代轮询.
 */
static void SdmmcStartLL(volatile SdmmcRegs_t *sdmmc,
                         const SdmmcCmd_t *cmd,
                         const SdmmcIdmaDesc_t *desc_first,
                         SdmmcBlockSize_t  block_size_log,
                         uint8_t  is_write)
{
    uint32_t total_len = LlValidate(desc_first, 1U << block_size_log);
    if (total_len == 0)
    {
        g_sdmmc1_it_result = SDMMC_ERR_PARAM;
        g_sdmmc1_it_done = 1;
        return;
    }

    g_it_sdmmc    = sdmmc;
    g_it_is_write = is_write;
    g_sdmmc1_it_done   = 0;
    g_sdmmc1_it_result = SDMMC_OK;
    g_sdmmc1_btc_cnt   = 0;

    /* 上次写入的卡内部编程可能未完成 (BUSYD0=1), 忙则拒绝而非死等 */
    if (sdmmc->STAR & SDMMC_STA_BUSYD0)
    {
        g_sdmmc1_it_result = SDMMC_ERR_BUSY;
        g_sdmmc1_it_done = 1;
        return;
    }

    sdmmc->DTIMER    = SDMMC_DTIMER_MAX;
    sdmmc->DLENR     = total_len;
    sdmmc->IDMABASER = desc_first->buf_addr;
    sdmmc->IDMABSIZER = desc_first->buf_size;
    sdmmc->IDMABAR   = (uint32_t)(uintptr_t)desc_first;
    sdmmc->IDMALAR   = desc_first->idmalar;
    sdmmc->IDMACTRLR = SDMMC_IDMA_IDMAEN
                     | SDMMC_IDMA_IDMABMODE;

    sdmmc->DCTRL = ((uint32_t)block_size_log << SDMMC_DCTRL_DBLOCKSIZE_Pos);
    if (!is_write)
        sdmmc->DCTRL |= SDMMC_DCTRL_DTDIR;   /* 读方向 */

    /* 一次使能所有可能的中断: 命令阶段 + 数据阶段 + 写后Busy */
    /* IDMATE 无中断 (实测不触发), 仅 STAR 标志 + ICR 清除 */
    uint32_t mask = SDMMC_STA_CMDREND | SDMMC_STA_CTIMEOUT | SDMMC_STA_CCRCFAIL
                  | SDMMC_STA_DATAEND | SDMMC_STA_DTIMEOUT | SDMMC_STA_DCRCFAIL
                  | SDMMC_STA_IDMABTC | SDMMC_STA_BUSYD0END;
    if (is_write)
    {
        mask |= SDMMC_STA_TXUNDERR;
        /* 写后卡内部编程产生的 BUSYD0 由下次传输启动前轮询处理,
           BUSYD0END 是 R1b 响应用的, 与数据写无关 */
    }
    else
    {
        mask |= SDMMC_STA_RXOVERR;
    }
    sdmmc->MASKR = mask;

    /* 写 ARGR + CMDR (CPSMEN=1), 触发硬件状态机 → ISR 接管后续 */
    sdmmc->ICR  = SDMMC_ICR_ALLFLAGS;
    sdmmc->ARGR = cmd->arg;
    sdmmc->CMDR = ((uint32_t)(cmd->index & 0x3F) << SDMMC_CMDR_CMDINDEX_Pos)
                | ((uint32_t)cmd->resp_type << SDMMC_CMDR_WAITRESP_Pos)
                | SDMMC_CMDR_CPSMEN
                | (cmd->cmdtrans ? SDMMC_CMDR_CMDTRANS : 0)
                | (cmd->cmdstop  ? SDMMC_CMDR_CMDSTOP  : 0);
}


/**
 * @brief 中断模式启动 IDMA 链表写 (CMD23 轮询, CMD25 中断)
 */
void SdmmcStartWriteLL(volatile SdmmcRegs_t *sdmmc,
                       const SdmmcCmd_t *cmd,
                       const SdmmcIdmaDesc_t *desc_first,
                       SdmmcBlockSize_t  block_size_log)
{
    SdmmcStartLL(sdmmc, cmd, desc_first, block_size_log, 1);
}


/**
 * @brief 中断模式启动 IDMA 链表读 (CMD23 轮询, CMD18 中断)
 */
void SdmmcStartReadLL(volatile SdmmcRegs_t *sdmmc,
                      const SdmmcCmd_t *cmd,
                      const SdmmcIdmaDesc_t *desc_first,
                      SdmmcBlockSize_t  block_size_log)
{
    SdmmcStartLL(sdmmc, cmd, desc_first, block_size_log, 0);
}


/**
 * @brief SDMMC1 中断处理入口 (由 do_irq 根据 GIC ID 81 分发)
 *
 * MASKR 使能全部中断, ISR 一次检查所有标志:
 *   错误 (CTIMEOUT/CCRCFAIL/DTIMEOUT/DCRCFAIL/TXUNDERR/RXOVERR/IDMATE) → 关 MASKR+IDMA, 设 result 并 done
 *   CMDREND → 仅清该位, 保留数据阶段标志
 *   IDMABTC → 递增计数 (前台轮询用)
 *   BUSYD0END → 置信号量 (前台等)
 *   DATAEND  → 传输完成, 关 MASKR+IDMA, done
 */
void Sdmmc1IrqHandler(void)
{
    volatile SdmmcRegs_t *sdmmc = g_it_sdmmc;
    uint32_t sta = sdmmc->STAR;

    /* ---- 错误: 命令阶段 ---- */
    if (sta & SDMMC_STA_CTIMEOUT)
    {
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_ERR_CTIMEOUT;
        g_sdmmc1_it_done = 1;
        return;
    }
    if (sta & SDMMC_STA_CCRCFAIL)
    {
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_ERR_CCRCFAIL;
        g_sdmmc1_it_done = 1;
        return;
    }

    /* ---- 命令完成 ---- */
    if (sta & SDMMC_STA_CMDREND)
    {
        sdmmc->ICR = SDMMC_STA_CMDREND;  /* 只清 CMDREND, 保留数据阶段标志 */
    }

    /* ---- 错误: 数据阶段 ---- */
    if (sta & SDMMC_STA_DTIMEOUT)
    {
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_ERR_DTIMEOUT;
        g_sdmmc1_it_done = 1;
        return;
    }
    if (sta & SDMMC_STA_DCRCFAIL)
    {
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_ERR_DCRCFAIL;
        g_sdmmc1_it_done = 1;
        return;
    }
    if (sta & SDMMC_STA_TXUNDERR)
    {
        if (g_sdmmc1_dbg_idx < SDMMC_DBG_TRACE_SIZE)
            g_sdmmc1_dbg_trace[g_sdmmc1_dbg_idx++] = 3;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_ERR_TXUNDERR;
        g_sdmmc1_it_done = 1;
        return;
    }
    if (sta & SDMMC_STA_RXOVERR)
    {
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_ERR_RXOVERR;
        g_sdmmc1_it_done = 1;
        return;
    }

    /* ---- IDMA 链表错误 ---- */
    if (sta & SDMMC_STA_IDMATE)
    {
        if (g_sdmmc1_dbg_idx < SDMMC_DBG_TRACE_SIZE)
            g_sdmmc1_dbg_trace[g_sdmmc1_dbg_idx++] = 4;
        sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_ERR_PARAM;  /* IDMA 链表错误: ABR=0/保留地址等 */
        g_sdmmc1_it_done = 1;
        return;
    }

    /* ---- IDMA buffer 传输完成 (所有节点都触发, 含最后一个 ULA=0) ---- */
    if (sta & SDMMC_STA_IDMABTC)
    {
        sdmmc->ICR = SDMMC_STA_IDMABTC;
        g_sdmmc1_btc_cnt++;
        if (g_sdmmc1_dbg_idx < SDMMC_DBG_TRACE_SIZE)
            g_sdmmc1_dbg_trace[g_sdmmc1_dbg_idx++] = 1;
    }

    /* ---- 卡忙结束 (R1b: CMD38 擦除等), 仅置标志 ---- */
    if (sta & SDMMC_STA_BUSYD0END)
    {
        sdmmc->ICR = SDMMC_STA_BUSYD0END;
        g_sdmmc1_busy0end = 1;
    }

    /* ---- 数据传输完成 ----
     * DATAEND 后 SDMMC 通信已结束. 写操作后卡内部 Flash 编程产生的 BUSYD0
     * 在下次传输启动前轮询处理, 不在此处等待. */
    if (sta & SDMMC_STA_DATAEND)
    {
        sdmmc->ICR = SDMMC_STA_DATAEND;
        sdmmc->MASKR = 0;
        sdmmc->IDMACTRLR = 0;
        g_sdmmc1_it_result = SDMMC_OK;
        g_sdmmc1_it_done = 1;
    }
}
