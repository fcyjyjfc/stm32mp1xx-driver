/*
 * stm32mp1xx_adc.c
 *
 *  Created on: 2026-6-4
 *      Author: gjsbr
 *
 *  ADC 寄存器地址指针定义
 *  RM0436 section 29
 */

#include "stm32mp1xx_adc.h"


volatile AdcRegs_t *const ADC = (void *)ADC_BASE;


/* 校准系数保存（掉电后可恢复，省重新校准） */
static AdcCalibResult_t calib_result;


/* ========================================================================
 *  电源与启动
 *
 *  上电流程（手册 29.4.6 + 29.4.9）：
 *    AdcPowerUp → [AdcCalibrate] → AdcEnable
 *
 *  复位后 ADC 处于深度掉电（DEEPPWD=1），内部 LDO 关闭。
 *  必须先退掉电→开 LDO→等稳定，才能使能 ADC。
 * ======================================================================== */

void AdcPowerUp(AdcIdx_t idx)
{
    /*
     * DEEPPWD=0:  退出深度掉电模式（复位后默认=1）
     * ADVREGEN=1: 使能内部 LDO 稳压器
     * 必须先退掉电再开 LDO（DEEPPWD 会钳位 ADVREGEN）
     */
    ADC->ADC[idx].CR &= ~ADC_CR_DEEPPWD;
    ADC->ADC[idx].CR |= ADC_CR_ADVREGEN;

    /*
     * 等待 LDO 输出就绪。手册要求 TADCVREG_STUP 时间。
     * LDORDY=1 表示内部稳压器已稳定，可以继续后续操作。
     */
    uint32_t tout = 1000000;
    while (!(ADC->ADC[idx].ISR & ADC_FLAG_LDORDY) && tout--)
        ;
}

void AdcPowerDown(AdcIdx_t idx)
{
    /*
     * 掉电前先确保 ADC 已禁用（ADEN=0），否则直接写 DEEPPWD 无效。
     * 注意：进深度掉电后校准系数会丢失，需重新校准。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADEN)
        AdcDisable(idx);

    ADC->ADC[idx].CR &= ~ADC_CR_ADVREGEN;
    ADC->ADC[idx].CR |= ADC_CR_DEEPPWD;
}

uint32_t AdcCalibrate(AdcIdx_t idx, uint32_t adcaldif, uint32_t adcallin)
{
    /*
     * 校准前提：CR 必须完全等于 ADC_CR_ADVREGEN（ADEN=0，ADCAL=0，其余全零）。
     * ADEN=1 时设 ADCAL 会被硬件忽略。
     */
    if (ADC->ADC[idx].CR != ADC_CR_ADVREGEN)
        return 0;

    /*
     * ADCALDIF：0=单端校准，1=差分校准
     * ADCALLIN：1=同时做线性校准（160 位），0=只做偏移校准（11 位）
     * 线性校准只需做一次，单端/差分共用。
     */
    uint32_t cr = ADC->ADC[idx].CR & ~(ADC_CR_ADCALDIF | ADC_CR_ADCALLIN);
    if (adcaldif)
        cr |= ADC_CR_ADCALDIF;
    if (adcallin)
        cr |= ADC_CR_ADCALLIN;
    cr |= ADC_CR_ADCAL;
    ADC->ADC[idx].CR = cr;

    /*
     * 校准启动后 ADCAL 由硬件保持为 1，完成后自动清 0。
     * 注意：ADCAL 是 CR 中的位，不是 ISR 标志，不能用 AdcWaitFlagClr。
     */
    uint32_t tout = 1000000;
    while (tout--)
    {
        if (!(ADC->ADC[idx].CR & ADC_CR_ADCAL))
            break;
    }
    if (!tout)
        return 0;

    /* ---- 保存偏移校准系数（单端或差分） ---- */
    if (adcaldif)
        calib_result.calfact_d = (ADC->ADC[idx].CALFACT >> 16) & 0x7FFu;
    else
        calib_result.calfact_s = ADC->ADC[idx].CALFACT & 0x7FFu;

    /* ---- 线性系数读出（需临时使能 ADC） ---- */
    if (!adcallin)
        return 1;

    /* 使能 ADC 以访问 CALFACT2 */
    AdcClearFlag(idx, ADC_FLAG_ADRDY);
    ADC->ADC[idx].CR |= ADC_CR_ADEN;
    if (!AdcWaitFlagSet(idx, ADC_FLAG_ADRDY, 1000000))
        return 0;

    /*
     * 校准完成后 LINCALRDYW1~6 均为 1。读规程：
     *   清位 → 启动数据传输 → 轮询直到硬件清 0 → 读 CALFACT2
     * 必须依次操作，一次只动一个位。从高字 W6 读起（手册第 3 步）。
     */
    static const uint32_t lincalrdy[6] = {
        ADC_CR_LINCALRDYW6, ADC_CR_LINCALRDYW5, ADC_CR_LINCALRDYW4,
        ADC_CR_LINCALRDYW3, ADC_CR_LINCALRDYW2, ADC_CR_LINCALRDYW1,
    };

    for (int i = 0; i < 6; i++)
    {
        ADC->ADC[idx].CR &= ~lincalrdy[i];
        tout = 1000000;
        while (tout--)
        {
            if (!(ADC->ADC[idx].CR & lincalrdy[i]))
                break;
        }
        if (!tout)
            return 0;

        calib_result.lincalfact[i] = ADC->ADC[idx].CALFACT2;
    }

    /* 读完线性系数，关闭 ADC 回到校准后状态 */
    AdcDisable(idx);

    return 1;
}

uint32_t AdcEnable(AdcIdx_t idx)
{
    /*
     * ADEN=1 的前提：CR 必须完全等于 ADC_CR_ADVREGEN（仅该位为 1）。
     * 调用者应在上电并校准后，CR 处于干净状态时调用此函数。
     */
    if (ADC->ADC[idx].CR != ADC_CR_ADVREGEN)
        return 0;

    AdcClearFlag(idx, ADC_FLAG_ADRDY);
    ADC->ADC[idx].CR |= ADC_CR_ADEN;

    /*
     * ADRDY=1 表示 ADC 已稳定，可以接受转换请求（设 ADSTART/JADSTART）。
     */
    return AdcWaitFlagSet(idx, ADC_FLAG_ADRDY, 1000000);
}

void AdcDisable(AdcIdx_t idx)
{
    /*
     * 禁用前必须先停止正在进行的转换。
     * 设 ADSTP/JADSTP，然后等待 ADSTART/JADSTART 硬件清 0。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        AdcStop(idx);
    if (ADC->ADC[idx].CR & ADC_CR_JADSTART)
        AdcStopInjected(idx);

    /*
     * ADDIS=1 请求禁用 ADC，硬件完成内部下电后自动清 ADEN 和 ADDIS。
     */
    ADC->ADC[idx].CR |= ADC_CR_ADDIS;
    while (ADC->ADC[idx].CR & ADC_CR_ADEN)
        ;
}


/* ========================================================================
 *  转换控制
 *
 *  ADSTART/JADSTART 既是控制位也是状态位：
 *    写 1 启动（软件触发模式立即转换，硬件触发模式等待触发）
 *    硬件在序列结束或停止后自动清 0
 * ======================================================================== */

void AdcStart(AdcIdx_t idx)
{
    ADC->ADC[idx].CR |= ADC_CR_ADSTART;
}

void AdcStartInjected(AdcIdx_t idx)
{
    ADC->ADC[idx].CR |= ADC_CR_JADSTART;
}

void AdcStop(AdcIdx_t idx)
{
    /*
     * ADSTP 中止当前常规转换，部分转换结果丢弃（DR 不更新）。
     * 必须轮询 ADSTART=0 确认停止完成。
     */
    ADC->ADC[idx].CR |= ADC_CR_ADSTP;
    while (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        ;
}

void AdcStopInjected(AdcIdx_t idx)
{
    /*
     * JADSTP 中止当前注入转换，序列重置，下次从头开始。
     * 注意：自动注入模式（JAUTO=1）时禁止使用 JADSTP，用 ADSTP 同时停。
     */
    ADC->ADC[idx].CR |= ADC_CR_JADSTP;
    while (ADC->ADC[idx].CR & ADC_CR_JADSTART)
        ;
}


/* ========================================================================
 *  数据读取
 *
 *  读 DR 会自动清除 EOC 标志，读 JDRy 会自动清除 JEOC 标志。
 *  如果不用自动清除，也可以写 1 到 ISR 对应位手动清除。
 * ======================================================================== */

uint32_t AdcRead(AdcIdx_t idx)
{
    /*
     * 读 DR 会硬件自动清 EOC，所以先检查 EOC 再读通常不需要额外清标志。
     */
    return ADC->ADC[idx].DR;
}

uint32_t AdcReadInjected(AdcIdx_t idx, uint32_t ch)
{
    /*
     * ch: 0~3 对应 JDR1~JDR4
     * 读 JDRy 会自动清除对应通道的 JEOC 标志。
     */
    return ((uint32_t *)&ADC->ADC[idx].JDR1)[ch];
}


/* ========================================================================
 *  配置函数
 *
 *  注意写入时机约束（手册 29.4.10）：
 *    - 常规配置（CFGR, SQR, SMPR, 触发等）：需 ADSTART=0
 *    - 注入配置（JSQR 的 JEXTEN/JEXTSEL/序列）：需 JADSTART=0
 *    - DIFSEL、CCR、校准：需 ADEN=0（ADC 禁用）
 *    违反时硬件不报错，行为异常。
 * ======================================================================== */

void AdcSetResolution(AdcIdx_t idx, AdcRes_t res)
{
    /*
     * 降低分辨率可缩短转换时间（TSAR 从 8.5 降到 4.5 周期）。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    ADC->ADC[idx].CFGR = (ADC->ADC[idx].CFGR & ~(7u << 2)) | ((res & 7u) << 2);
}

void AdcSetContMode(AdcIdx_t idx, AdcContMode_t cont)
{
    /*
     * CONT=1：连续模式，触发一次后自动循环转换
     * CONT=0：单次模式，每触发执行一次序列
     * 连续模式会覆盖 EOS 事件（不清 ADSTART），序列自动重启。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    if (cont)
        ADC->ADC[idx].CFGR |= (1u << 13);
    else
        ADC->ADC[idx].CFGR &= ~(1u << 13);
}

void AdcSetOvrMode(AdcIdx_t idx, AdcOvrMode_t mode)
{
    /*
     * OVRMOD：常规通道溢出时 DR 的行为。
     *   PRESERVE  = 保留旧数据，丢弃新结果
     *   OVERWRITE = 新数据覆盖 DR，旧数据丢失
     * 约束：ADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        return;

    if (mode)
        ADC->ADC[idx].CFGR |= (1u << 12);
    else
        ADC->ADC[idx].CFGR &= ~(1u << 12);
}

void AdcSetAutoDelay(AdcIdx_t idx, uint32_t enable)
{
    /*
     * AUTDLY：自动延迟转换模式。
     * 使能后每个常规转换等待 DR 被读取后才启动下一个，
     * 避免溢出（OVR），但转换间隔受软件读取延迟影响。
     * 双 ADC 模式下从机跟随主机。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    if (enable)
        ADC->ADC[idx].CFGR |= (1u << 14);
    else
        ADC->ADC[idx].CFGR &= ~(1u << 14);
}

void AdcSetAutoInject(AdcIdx_t idx, uint32_t enable)
{
    /*
     * JAUTO：常规序列结束后自动启动注入序列。
     * 不能同时使用不连续模式（DISCEN=1 或 JDISCEN=1）。
     * 双 ADC 模式下从机跟随主机。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    if (enable)
        ADC->ADC[idx].CFGR |= (1u << 25);
    else
        ADC->ADC[idx].CFGR &= ~(1u << 25);
}

void AdcSetDmaMode(AdcIdx_t idx, AdcDmaMode_t dmngt)
{
    /*
     * dmngt: 无 DMA / 单次 / DFSDM / 循环
     * 双 ADC 模式下此位无效，改用 CCR.DAMDF。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    ADC->ADC[idx].CFGR = (ADC->ADC[idx].CFGR & ~(3u << 0)) | ((dmngt & 3u) << 0);
}

void AdcSetDiscMode(AdcIdx_t idx, uint32_t discnum)
{
    /*
     * 不连续模式：每触发一次只转 discnum 个通道，挂起序列等待下次触发继续。
     * discnum=0 关闭，1~8 使能并指定每触发通道数。
     * 不能与连续模式（CONT=1）或自动注入（JAUTO=1）同时使用。
     * 约束：ADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        return;

    if (discnum == 0)
        ADC->ADC[idx].CFGR &= ~(1u << 16);          /* DISCEN=0 */
    else
    {
        uint32_t dn = (discnum - 1) & 7u;
        ADC->ADC[idx].CFGR = (ADC->ADC[idx].CFGR & ~(7u << 17))
                           | (dn << 17);             /* DISCNUM */
        ADC->ADC[idx].CFGR |= (1u << 16);            /* DISCEN=1 */
    }
}

void AdcSetChanSeq(AdcIdx_t idx, uint32_t sqr, uint32_t pos, uint32_t ch)
{
    /*
     * SQR1~SQR4 中每个 SQx 字段占 5 位，通道号 0~19。
     * 寄存器中每个字段之间有保留位间隔（5 位 + 1 位保留 = 6 位一组）。
     * sqr: 1~4, pos: 该寄存器内第几个 SQx（0 起始）
     * 例：AdcSetChanSeq(adc, 1, 0, 3) → SQR1.SQ1 = 3（第1笔转换通道3）
     */
    uint32_t shift = pos * 6 + 6;
    volatile uint32_t *reg = &ADC->ADC[idx].SQR1 + (sqr - 1);
    *reg = (*reg & ~(0x1Fu << shift)) | ((ch & 0x1Fu) << shift);
}

void AdcSetSeqLen(AdcIdx_t idx, uint32_t len)
{
    /*
     * SQR1.L[3:0]: 常规序列长度。
     * 0=1笔, 1=2笔, ..., 15=16笔。
     * 序列长度必须 ≥ 实际填入的 SQx 数量。
     */
    ADC->ADC[idx].SQR1 = (ADC->ADC[idx].SQR1 & ~0xFu) | (len & 0xFu);
}

void AdcSetExtTrig(AdcIdx_t idx, uint32_t extsel, AdcTrigEn_t exten)
{
    /*
     * extsel[4:0]: 触发源选择（参见手册触发源表）
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    ADC->ADC[idx].CFGR = (ADC->ADC[idx].CFGR & ~(0x1Fu << 5)) | ((extsel & 0x1Fu) << 5);
    ADC->ADC[idx].CFGR = (ADC->ADC[idx].CFGR & ~(3u << 10)) | ((exten & 3u) << 10);
}

void AdcSetJExtTrig(AdcIdx_t idx, uint32_t jextsel, AdcTrigEn_t jexten)
{
    /*
     * 注入触发配置在 JSQR 中（不是 CFGR），
     * 可以利用上下文队列在运行时切换（JQDIS=0 时）。
     * 约束：JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_JADSTART)
        return;

    ADC->ADC[idx].JSQR = (ADC->ADC[idx].JSQR & ~(0x1Fu << 2)) | ((jextsel & 0x1Fu) << 2);
    ADC->ADC[idx].JSQR = (ADC->ADC[idx].JSQR & ~(3u << 7)) | ((jexten & 3u) << 7);
}

void AdcSetJqConfig(AdcIdx_t idx, uint32_t disable, uint32_t mode)
{
    /*
     * disable=0 使能队列，disable=1 关闭队列（队列关闭会清空 JSQR）。
     * mode：   0=队列永不空（保留上次配置），1=队列可空（空时关闭注入触发）。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    uint32_t cfgr = ADC->ADC[idx].CFGR;
    cfgr &= ~((1u << 31) | (1u << 21));     /* 清 JQDIS + JQM */
    if (disable)
        cfgr |= (1u << 31);                  /* JQDIS=1 */
    if (mode)
        cfgr |= (1u << 21);                  /* JQM=1 */
    ADC->ADC[idx].CFGR = cfgr;
}

void AdcSetSampleTime(AdcIdx_t idx, uint32_t ch, AdcSmp_t smp)
{
    /*
     * SMPR1 覆盖通道 0~9，SMPR2 覆盖通道 10~19。
     * 每通道 3 位，采样时间范围 1.5~810.5 个 ADC 时钟周期。
     * 快速通道（VINP0~5）和慢速通道（VINP6~19）有不同最小采样时间。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    volatile uint32_t *reg = (ch < 10) ? &ADC->ADC[idx].SMPR1 : &ADC->ADC[idx].SMPR2;
    uint32_t shift = (ch % 10) * 3;
    *reg = (*reg & ~(7u << shift)) | ((smp & 7u) << shift);
}

void AdcSetPrescaler(uint32_t presc)
{
    /*
     * PRESC[3:0] 仅异步时钟模式（CKMODE=00）有效。
     * 0=不分频, 1=/2, 2=/4, ..., 11=/256。
     * 两个 ADC 共用同一个预分频器。
     */
    ADC->COMM.CCR = (ADC->COMM.CCR & ~(0xFu << 18)) | ((presc & 0xFu) << 18);
}

void AdcSetCkMode(AdcCkMode_t ckmode)
{
    /*
     * CKMODE[1:0]: ADC 时钟模式
     *   异步 = 独立时钟（adc_ker_ck，经 PRESC 分频）
     *   HCLK/1/2/4 = 同步模式，无触发抖动，适合定时器触发
     * 约束：两个 ADC 均处于禁用状态（ADEN=0）时才能写。
     */
    ADC->COMM.CCR = (ADC->COMM.CCR & ~(3u << 16)) | ((ckmode & 3u) << 16);
}
