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

uint32_t AdcRestoreCalib(AdcIdx_t idx, const AdcCalibResult_t *calib)
{
    /*
     * 恢复校准系数，用于 DEEPPWD/STANDBY 唤醒后快速恢复校准状态。
     * 前提：ADEN=1, ADSTART=0, JADSTART=0，否则跳出不写入。
     *
     * 步骤：
     *   1. 写 CALFACT（CALFACT_S + CALFACT_D 偏移系数）
     *   2. 通过 LINCALRDYW 握手依次写入 6 个线性系数
     */
    if (!(ADC->ADC[idx].CR & ADC_CR_ADEN))
        return 0;
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return 0;

    /* ---- 偏移系数（CALFACT_S + CALFACT_D） ---- */
    uint32_t calfact = ADC->ADC[idx].CALFACT;
    calfact = (calfact & ~0x7FFu) | (calib->calfact_s & 0x7FFu);
    calfact = (calfact & ~(0x7FFu << 16)) | ((calib->calfact_d & 0x7FFu) << 16);
    ADC->ADC[idx].CALFACT = calfact;

    /* ---- 线性系数（CALFACT2，需 LINCALRDYW 握手） ----
     *
     * 手册写入规程（adc_p1503_1509.txt:242-265）：
     *   步骤 3..20 依次处理 W6..W1:
     *     写 CALFACT2 → 置 LINCALRDYWx → 轮询该位 =1 确认生效
     *   W6 仅使用 CALFACT2[9:0]（bits[159:150]），[29:10] 为 0
     *   W5..W1 使用 CALFACT2[29:0]（各 30 位）
     *   每次只能操作一个 LINCALRDYWx 位
     */
    static const uint32_t lincalrdy[6] = {
        ADC_CR_LINCALRDYW6, ADC_CR_LINCALRDYW5, ADC_CR_LINCALRDYW4,
        ADC_CR_LINCALRDYW3, ADC_CR_LINCALRDYW2, ADC_CR_LINCALRDYW1,
    };

    for (int i = 0; i < 6; i++)
    {
        uint32_t val = calib->lincalfact[i];
        if (i == 0)
            val &= 0x3FFu;               /* W6: bits[159:150], only CALFACT2[9:0] valid */
        ADC->ADC[idx].CALFACT2 = val;
        ADC->ADC[idx].CR |= lincalrdy[i];

        uint32_t tout = 1000000;
        while (tout--)
        {
            if (ADC->ADC[idx].CR & lincalrdy[i])
                break;
        }
        if (!tout)
            return 0;
    }

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
     * 清所有 ISR 标志，防止残留状态影响下次使能。
     */
    ADC->ADC[idx].ISR = 0xFFFFFFFFu;
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
    return ADC->ADC[idx].JDR[ch];
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

void AdcSetRegularSeq(AdcIdx_t idx, uint32_t len, const uint32_t *channels)
{
    /*
     * 一键配置常规序列：长度 + 各位置通道号。
     *   len: 转换笔数 1~16
     *   channels[0..len-1]: 依次填入 SQ1~SQN 的通道号
     * 内部自动分配到 SQR[0]~SQR[3] 对应位域。
     * 约束：ADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        return;

    if (len > 16) len = 16;
    if (len == 0) return;

    /* 清空所有 SQR 寄存器 */
    ADC->ADC[idx].SQR[0] = 0;
    ADC->ADC[idx].SQR[1] = 0;
    ADC->ADC[idx].SQR[2] = 0;
    ADC->ADC[idx].SQR[3] = 0;

    /* 填写通道序列 */
    for (uint32_t i = 1; i <= len; i++)
    {
        uint32_t reg_idx = i / 5;
        uint32_t pos     = i % 5;
        uint32_t shift   = pos * 6;
        volatile uint32_t *reg = &ADC->ADC[idx].SQR[reg_idx];

        *reg = (*reg & ~(0x1Fu << shift)) | ((channels[i - 1] & 0x1Fu) << shift);
    }

    /* 设置序列长度 L[3:0] */
    ADC->ADC[idx].SQR[0] |= (len - 1) & 0xFu;
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

void AdcSetInjectedSeq(AdcIdx_t idx, uint32_t len, const uint32_t *channels,
                       uint32_t jextsel, AdcTrigEn_t jexten)
{
    /*
     * 一键配置注入序列：触发 + 长度 + 各位置通道号。
     *   len: 转换笔数 1~4
     *   channels[0..len-1]: 依次填入 JSQ1~JSQN 的通道号
     *   约束：JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_JADSTART)
        return;

    if (len > 4) len = 4;
    if (len == 0) return;

    uint32_t jsqr = 0;

    /* 触发选择 */
    jsqr |= (jextsel & 0x1Fu) << 2;
    jsqr |= (jexten & 3u) << 7;

    /* 序列长度 JL[1:0] */
    jsqr |= ((len - 1) & 3u);

    /* 填写通道序列 (JSQ1~JSQ4) */
    for (uint32_t i = 0; i < len; i++)
    {
        uint32_t shift = i * 6 + 9;
        jsqr |= (channels[i] & 0x1Fu) << shift;
    }

    ADC->ADC[idx].JSQR = jsqr;
}

void AdcSetJqConfig(AdcIdx_t idx, uint32_t disable, uint32_t mode, uint32_t jdiscen)
{
    /*
     * 注入通道配置（CFGR 相关位集中设置）：
     *   disable: JQDIS=1 关闭注入上下文队列（队列关闭会清空 JSQR）
     *   mode:    JQM=1 队列可空模式
     *   jdiscen: JDISCEN=1 注入不连续模式（每触发只转 1 个注入通道）
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     * 注意：JDISCEN 与 JAUTO 互斥（JAUTO=1 时 JDISCEN 必须为 0）。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    uint32_t cfgr = ADC->ADC[idx].CFGR;
    cfgr &= ~((1u << 31) | (1u << 21) | (1u << 20));  /* 清 JQDIS + JQM + JDISCEN */
    if (disable)
        cfgr |= (1u << 31);                            /* JQDIS=1 */
    if (mode)
        cfgr |= (1u << 21);                            /* JQM=1 */
    if (jdiscen)
        cfgr |= (1u << 20);                            /* JDISCEN=1 */
    ADC->ADC[idx].CFGR = cfgr;
}

void AdcSetChanPreselect(AdcIdx_t idx, uint32_t mask)
{
    /*
     * PCSEL[19:0]：通道预选，对应位写 1 使能该通道的 IO 连接。
     * 未预选的通道转换结果不可靠。
     * 约束：ADSTART=0 且 JADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    ADC->ADC[idx].PCSEL = mask & 0xFFFFFu;
}

void AdcSetDiffMode(AdcIdx_t idx, uint32_t mask)
{
    /*
     * DIFSEL[19:0]：通道差分模式选择，每个bit对应一个通道。
     *   对应位写 0 = 单端，写 1 = 差分。
     * 仅影响通道的模拟输入方式，不区分常规或注入。
     * 约束：ADC 禁用且非校准中（ADEN=0, ADCAL=0）时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADEN | ADC_CR_ADCAL))
        return;

    ADC->ADC[idx].DIFSEL = mask & 0xFFFFFu;
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

void AdcSetOverSample(AdcIdx_t idx, uint32_t ratio, uint32_t shift,
                      uint32_t enable_reg, uint32_t enable_inj)
{
    /*
     * 过采样配置（CFGR2）。
     *   ratio:   0=1x, 1=2x, ..., 1023=1024x
     *   shift:   过采样右移位 0~15（补偿累加后的位宽扩展）
     *   enable_reg:  常规过采样使能
     *   enable_inj: 注入过采样使能
     * 约束：ADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        return;

    uint32_t cfgr2 = ADC->ADC[idx].CFGR2;
    cfgr2 &= ~((0x3FFu << 16) | (0xFu << 5) | (1u << 1) | (1u << 0));
    cfgr2 |= (ratio & 0x3FFu) << 16;        /* OSVR */
    cfgr2 |= (shift & 0xFu) << 5;            /* OVSS */
    if (enable_reg)
        cfgr2 |= (1u << 0);                  /* ROVSE */
    if (enable_inj)
        cfgr2 |= (1u << 1);                  /* JOVSE */
    ADC->ADC[idx].CFGR2 = cfgr2;
}

void AdcSetOverSampleMode(AdcIdx_t idx, uint32_t rovs_mode, uint32_t trovs)
{
    /*
     * 过采样行为模式。
     *   rovs_mode: 0=继续模式（注入时不中断过采样，注完后继续）
     *              1=重启模式（注入时中止过采样，注完后重头开始）
     *   trovs:     0=连续过采样（触发一次采完全部）
     *              1=触发过采样（每次触发采一个样）
     * 约束：ADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        return;

    uint32_t cfgr2 = ADC->ADC[idx].CFGR2;
    cfgr2 &= ~((1u << 10) | (1u << 9));
    if (rovs_mode)
        cfgr2 |= (1u << 10);                 /* ROVSM */
    if (trovs)
        cfgr2 |= (1u << 9);                  /* TROVS */
    ADC->ADC[idx].CFGR2 = cfgr2;
}

void AdcSetLeftShift(AdcIdx_t idx, uint32_t shift)
{
    /*
     * LSHIFT：最终结果左移 0~15 位。
     * 右对齐 16 位数据时可配合左移实现 32 位对齐，方便软件读取。
     * 约束：ADSTART=0 时才能写，否则跳过。
     */
    if (ADC->ADC[idx].CR & ADC_CR_ADSTART)
        return;

    ADC->ADC[idx].CFGR2 = (ADC->ADC[idx].CFGR2 & ~(0xFu << 28))
                        | ((shift & 0xFu) << 28);
}

void AdcSetOffset(AdcIdx_t idx, uint32_t ofr_num, uint32_t ch,
                   uint32_t offset, uint32_t ssate)
{
    /*
     * OFR1~OFR4：偏移校正寄存器。
     *   OFFSET[25:0]   —— 校正值（转换结果减去该值）
     *   OFFSET_CH[4:0]  —— 偏移作用于哪个通道
     *   SSATE           —— 1=单端模式生效，0=差分模式生效
     * 约束：ADSTART=0 且 JADSTART=0 时才能写。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;
    if (ofr_num > 3)
        return;

    uint32_t reg = (offset & 0x3FFFFFFu)
                | ((ch & 0x1Fu) << 26)
                | (ssate ? ADC_OFR_SSATE : 0);

    ADC->ADC[idx].OFR[ofr_num] = reg;
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

void AdcSetVrefint(uint32_t enable)
{
    /*
     * VREFEN（CCR bit 22）：使能内部参考电压通道。
     * ADC1 通道 17 / ADC2 通道 13。
     * 约束：所有 ADC 均处于禁用状态时才能写。
     */
    if (enable)
        ADC->COMM.CCR |= ADC_CCR_VREFEN;
    else
        ADC->COMM.CCR &= ~ADC_CCR_VREFEN;
}

void AdcSetTempSensor(uint32_t enable)
{
    /*
     * TSEN（CCR bit 23）：使能温度传感器。
     * ADC1 通道 16 / ADC2 通道 12。
     * 约束：所有 ADC 均处于禁用状态时才能写。
     */
    if (enable)
        ADC->COMM.CCR |= ADC_CCR_TSEN;
    else
        ADC->COMM.CCR &= ~ADC_CCR_TSEN;
}

void AdcSetVbat(uint32_t enable)
{
    /*
     * VBATEN（CCR bit 24）：使能 VBAT 监测。
     * VBAT 经内部 /4 分压后接入 ADC1 通道 18 / ADC2 通道 15。
     * 约束：所有 ADC 均处于禁用状态时才能写。
     */
    if (enable)
        ADC->COMM.CCR |= ADC_CCR_VBATEN;
    else
        ADC->COMM.CCR &= ~ADC_CCR_VBATEN;
}

void Adc2SetVddcore(uint32_t enable)
{
    /*
     * VDDCOREEN（ADC2_OR bit 0）：使能 VDDCORE 监测。
     * 仅 ADC2 通道 14。
     * 约束：ADC2 禁用时才能写。
     */
    if (enable)
        ADC->ADC[1].OR |= ADC2_OR_VDDCOREEN;
    else
        ADC->ADC[1].OR &= ~ADC2_OR_VDDCOREEN;
}

void AdcSetAwd1(AdcIdx_t idx, uint32_t en_reg, uint32_t en_inj,
                 uint32_t single, uint32_t ch,
                 uint32_t ltr, uint32_t htr)
{
    /*
     * AWD1 配置（CFGR 相关位）：
     *   AWD1EN(23)  —— 常规通道使能
     *   JAWD1EN(24) —— 注入通道使能
     *   AWD1SGL(22) —— 0=所有通道，1=指定通道
     *   AWD1CH(30:26)—— 指定通道号
     *   LTR1/HTR1   —— 低/高阈值
     * 约束：ADSTART=0 且 JADSTART=0。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    uint32_t cfgr = ADC->ADC[idx].CFGR;
    cfgr &= ~((1u << 23) | (1u << 24) | (1u << 22) | (0x1Fu << 26));
    cfgr |= (en_reg  ? (1u << 23) : 0)
          | (en_inj  ? (1u << 24) : 0)
          | (single  ? (1u << 22) : 0)
          | ((ch & 0x1Fu) << 26);
    ADC->ADC[idx].CFGR = cfgr;

    ADC->ADC[idx].LTR1 = ltr & 0x3FFFFFFu;
    ADC->ADC[idx].HTR1 = htr & 0x3FFFFFFu;
}

void AdcSetAwd2(AdcIdx_t idx, uint32_t ch_mask,
                 uint32_t ltr, uint32_t htr)
{
    /*
     * AWD2 配置：
     *   AWD2CR[19:0] —— 通道位掩码（全 0=禁用）
     *   LTR2/HTR2    —— 低/高阈值
     * 约束：ADSTART=0 且 JADSTART=0。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    ADC->ADC[idx].AWD2CR = ch_mask & 0xFFFFFu;
    ADC->ADC[idx].LTR2   = ltr & 0x3FFFFFFu;
    ADC->ADC[idx].HTR2   = htr & 0x3FFFFFFu;
}

void AdcSetAwd3(AdcIdx_t idx, uint32_t ch_mask,
                 uint32_t ltr, uint32_t htr)
{
    /*
     * AWD3 配置：
     *   AWD3CR[19:0] —— 通道位掩码
     *   LTR3/HTR3    —— 低/高阈值
     * 约束：ADSTART=0 且 JADSTART=0。
     */
    if (ADC->ADC[idx].CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))
        return;

    ADC->ADC[idx].AWD3CR = ch_mask & 0xFFFFFu;
    ADC->ADC[idx].LTR3   = ltr & 0x3FFFFFFu;
    ADC->ADC[idx].HTR3   = htr & 0x3FFFFFFu;
}

