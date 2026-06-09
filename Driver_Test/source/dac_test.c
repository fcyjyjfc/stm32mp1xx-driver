#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_dac.h"
#include "stm32mp1xx_adc.h"
#include "stm32mp1xx_btimer.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_rcc.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

/* ========================================================================
 *  辅助
 * ======================================================================== */

static void NumToStr(char *buf, uint32_t val)
{
    char rev[12];
    int i = 0;
    do {
        rev[i++] = '0' + val % 10;
        val /= 10;
    } while (val);
    while (i > 0)
        *buf++ = rev[--i];
    *buf = '\0';
}

static void PrintU32(const char *label, uint32_t val)
{
    char buf[48];
    int p = 0;
    while (*label) buf[p++] = *label++;
    buf[p++] = ':';
    buf[p++] = ' ';
    NumToStr(buf + p, val);
    while (buf[p]) p++;
    buf[p++] = '\r';
    buf[p++] = '\n';
    UsartWrite(USART4, (void *)buf, p);
}

static void PrintStr(const char *s)
{
    UsartWrite(USART4, (void *)s, strlen(s));
}

static int ReadLine(char *buf, int max_len)
{
    int pos = 0;
    char ch;
    while (pos < max_len - 1)
    {
        IwdgKickDog(IWDG2);
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 0)
            continue;
        if (ch == 'S' || ch == 's')
        {
            UsartWrite(USART4, (void *)"\r\n", 2);
            break;
        }
        UsartWrite(USART4, &ch, 1);
        buf[pos++] = ch;
    }
    buf[pos] = '\0';
    return pos;
}

/* ========================================================================
 *  DAC 写值后用 ADC2 内部通道回读验证
 * ======================================================================== */

static void TestBasicOutput(void)
{
    RCC->MP_APB1ENSETR |= 1 << 29;              /* DAC1 时钟使能 */
    RCC->MP_AHB2ENSETR |= 1 << 5;               /* ADC2 时钟使能 */

    /* ---- 配置 DAC1 ch1 ---- */
    DacCfg_t cfg;
    cfg.dac_ch2_cen        = 0;
    cfg.dac_ch2_dmaudrie   = 0;
    cfg.dac_ch2_dma_en     = 0;
    cfg.dac_ch2_mamp       = 0;
    cfg.dac_ch2_wave       = 0;
    cfg.dac_ch2_tsel       = 0;
    cfg.dac_ch2_tr_en      = 0;
    cfg.dac_ch2_en         = 0;
    cfg.dac_ch1_cen        = 0;
    cfg.dac_ch1_dmaudrie   = 0;
    cfg.dac_ch1_dma_en     = 0;
    cfg.dac_ch1_mamp       = 0;
    cfg.dac_ch1_wave       = 0;
    cfg.dac_ch1_tsel       = 0;
    cfg.dac_ch1_tr_en      = 0;
    cfg.dac_ch1_en         = 1;
    cfg.dac_pclk_hi_80     = 0;
    cfg.dac_ch2_mode       = 0;
    cfg.dac_ch1_mode       = DAC_CH_NORMAL_PER_NO_BUFFER;
    DacCfg(DAC1, &cfg);

    /* 校准 */
    DacCalibrate(DAC1, 1);

    /* ---- 配置 ADC2 读内部 DAC1 通道 ---- */
    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 1);
    AdcSetRegularSeq(ADC_IDX2, 1, (uint32_t[]){ ADC2_CH_DAC1 });
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);
    AdcEnable(ADC_IDX2);

    PrintStr("\r\n--- DAC Basic Output Test (SW trigger, ADC2 internal readback) ---\r\n");

    /* 输出并回读 */
    const uint16_t vals[] = { 0, 512, 1024, 2048, 3072, 4095 };

    for (int i = 0; i < 6; i++)
    {
        DacSoftTrig(DAC1, 1, vals[i]);          /* 写 DAC */

        for (volatile int d = 0; d < 2000; d++); /* 等待输出电压稳定 */

        AdcStart(ADC_IDX2);                      /* 启动 ADC 转换 */
        AdcWaitEoc(ADC_IDX2, 1000000);
        uint32_t adc_val = AdcRead(ADC_IDX2);    /* ADC 回读值 */

        char buf[32];
        int p = 0;
        NumToStr(buf, vals[i]);
        while (buf[p]) p++;
        UsartWrite(USART4, (void *)"DAC write: ", 11);
        UsartWrite(USART4, (void *)buf, p);
        UsartWrite(USART4, (void *)"  ADC read: ", 12);

        p = 0;
        NumToStr(buf, adc_val);
        while (buf[p]) p++;
        UsartWrite(USART4, (void *)buf, p);

        /* 估算偏差 */
        int32_t diff = (int32_t)adc_val - (int32_t)vals[i];
        if (diff < 0) diff = -diff;
        UsartWrite(USART4, (void *)"  diff: ", 8);
        p = 0;
        NumToStr(buf, (uint32_t)diff);
        while (buf[p]) p++;
        UsartWrite(USART4, (void *)buf, p);
        UsartWrite(USART4, (void *)"\r\n", 2);
    }

    DacCalibrate(DAC1, 1);

    PrintStr("--- Repeat after re-calibration ---\r\n");

    for (int i = 0; i < 6; i++)
    {
        DacSoftTrig(DAC1, 1, vals[i]);
        for (volatile int d = 0; d < 2000; d++);
        AdcStart(ADC_IDX2);
        AdcWaitEoc(ADC_IDX2, 1000000);
        uint32_t adc_val = AdcRead(ADC_IDX2);

        char buf[32];
        int p = 0;
        NumToStr(buf, vals[i]);
        while (buf[p]) p++;
        UsartWrite(USART4, (void *)"DAC write: ", 11);
        UsartWrite(USART4, (void *)buf, p);
        UsartWrite(USART4, (void *)"  ADC read: ", 12);
        p = 0;
        NumToStr(buf, adc_val);
        while (buf[p]) p++;
        UsartWrite(USART4, (void *)buf, p);

        int32_t diff = (int32_t)adc_val - (int32_t)vals[i];
        if (diff < 0) diff = -diff;
        UsartWrite(USART4, (void *)"  diff: ", 8);
        p = 0;
        NumToStr(buf, (uint32_t)diff);
        while (buf[p]) p++;
        UsartWrite(USART4, (void *)buf, p);
        UsartWrite(USART4, (void *)"\r\n", 2);
    }

    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
    DAC1->CR &= ~1;                              /* 关 DAC ch1 */
    PrintStr("--- Test done ---\r\n");
}

/* ========================================================================
 *  测试 2：通道2 三角波，基准 1000，幅值 2047，TIM6 触发，4 个周期
 * ======================================================================== */

static void TestTriangle(void)
{
    RCC->MP_APB1ENSETR |= 1 << 4;                /* TIM6 时钟使能 */
    RCC->MP_APB1ENSETR |= 1 << 29;               /* DAC1 时钟使能 */
    RCC->MP_AHB2ENSETR |= 1 << 5;                /* ADC2 时钟使能 */

    /* ---- 配置 DAC1 ch2 三角波 ---- */
    DacCfg_t cfg;
    cfg.dac_ch2_cen        = 0;
    cfg.dac_ch2_dmaudrie   = 0;
    cfg.dac_ch2_dma_en     = 0;
    cfg.dac_ch2_mamp       = UNMASK_BIT1_0_LFSR_AMP_2047;
    cfg.dac_ch2_wave       = DAC_WAVE_TRIANGLE;
    cfg.dac_ch2_tsel       = DAC_CH_TRG_TIM6_TRGO;
    cfg.dac_ch2_tr_en      = 1;
    cfg.dac_ch2_en         = 1;
    cfg.dac_ch1_cen        = 0;
    cfg.dac_ch1_dmaudrie   = 0;
    cfg.dac_ch1_dma_en     = 0;
    cfg.dac_ch1_mamp       = 0;
    cfg.dac_ch1_wave       = 0;
    cfg.dac_ch1_tsel       = 0;
    cfg.dac_ch1_tr_en      = 0;
    cfg.dac_ch1_en         = 0;
    cfg.dac_pclk_hi_80     = 0;
    cfg.dac_ch2_mode       = DAC_CH_NORMAL_PER_NO_BUFFER;
    cfg.dac_ch1_mode       = 0;
    DacCfg(DAC1, &cfg);
    DacCalibrate(DAC1, 2);

    DAC1->DHR12R2 = 1000;                        /* 基准值 */

    /* ---- 配置 TIM6 做触发源 ---- */
    BasicTimerCfg_t tcfg;
    tcfg.tim_psc  = 99;
    tcfg.tim_arr  = 97;
    tcfg.tim_arpe = 1;
    tcfg.tim_opm  = 0;
    tcfg.tim_urs  = 0;
    tcfg.tim_udis = 0;
    tcfg.tim_ude  = 1;
    tcfg.tim_uie  = 0;
    BasicTimerCfg(TIM6, &tcfg);
    BasicTimerSetTrgo(TIM6, BTIM_TRGO_UPDATE);

    /* ---- 配置 ADC2 读内部 DAC2 通道 ---- */
    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 1);
    AdcSetRegularSeq(ADC_IDX2, 1, (uint32_t[]){ ADC2_CH_DAC2 });
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);
    AdcEnable(ADC_IDX2);

    PrintStr("\r\n--- Test 2: Triangle wave ch2 DHR=1000 MAMP=2047 TIM6 trigger 4 cycles ---\r\n");

    /* 启动 TIM6 */
    BasicTimerStart(TIM6);

    uint32_t step = 0;
    uint32_t total = (2047 * 2) * 4;              /* 4094 步/周期 × 4 = 16376 */

    PrintStr("\r\n");
    while (step < total)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (TIM6->SR & 1)
        {
            TIM6->SR = ~(uint32_t)1;
            step++;

            /* 每 ~256 步采样一次（每周期 16 点，4 周期共 64 点） */
            if ((step % 256) == 1)
            {
                AdcStart(ADC_IDX2);
                AdcWaitEoc(ADC_IDX2, 1000000);
                uint32_t adc_val = AdcRead(ADC_IDX2);

                char buf[32];
                int p = 0;
                NumToStr(buf, step);
                while (buf[p]) p++;
                UsartWrite(USART4, (void *)buf, p);
                UsartWrite(USART4, (void *)"  ", 2);
                p = 0;
                NumToStr(buf, adc_val);
                while (buf[p]) p++;
                UsartWrite(USART4, (void *)buf, p);
                UsartWrite(USART4, (void *)"\r\n", 2);
            }

            if (UsartReadOne(USART4, &ch))
                break;
        }
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
    DAC1->CR &= ~(1 << 16);
    PrintStr("\r\n--- Test done ---\r\n");
}

/* ========================================================================
 *  测试 3：通道2 LFSR 噪声，基准 1000，幅值 2047，TIM6 触发
 * ======================================================================== */

static void TestNoise(void)
{
    RCC->MP_APB1ENSETR |= 1 << 4;
    RCC->MP_APB1ENSETR |= 1 << 29;
    RCC->MP_AHB2ENSETR |= 1 << 5;

    DacCfg_t cfg;
    cfg.dac_ch2_cen        = 0;
    cfg.dac_ch2_dmaudrie   = 0;
    cfg.dac_ch2_dma_en     = 0;
    cfg.dac_ch2_mamp       = UNMASK_BIT1_0_LFSR_AMP_2047;
    cfg.dac_ch2_wave       = DAC_WAVE_NOISE;
    cfg.dac_ch2_tsel       = DAC_CH_TRG_TIM6_TRGO;
    cfg.dac_ch2_tr_en      = 1;
    cfg.dac_ch2_en         = 1;
    cfg.dac_ch1_cen        = 0;
    cfg.dac_ch1_dmaudrie   = 0;
    cfg.dac_ch1_dma_en     = 0;
    cfg.dac_ch1_mamp       = 0;
    cfg.dac_ch1_wave       = 0;
    cfg.dac_ch1_tsel       = 0;
    cfg.dac_ch1_tr_en      = 0;
    cfg.dac_ch1_en         = 0;
    cfg.dac_pclk_hi_80     = 0;
    cfg.dac_ch2_mode       = DAC_CH_NORMAL_PER_NO_BUFFER;
    cfg.dac_ch1_mode       = 0;
    DacCfg(DAC1, &cfg);
    DacCalibrate(DAC1, 2);

    DAC1->DHR12R2 = 1000;

    BasicTimerCfg_t tcfg;
    tcfg.tim_psc  = 99;
    tcfg.tim_arr  = 97;
    tcfg.tim_arpe = 1;
    tcfg.tim_opm  = 0;
    tcfg.tim_urs  = 0;
    tcfg.tim_udis = 0;
    tcfg.tim_ude  = 1;
    tcfg.tim_uie  = 0;
    BasicTimerCfg(TIM6, &tcfg);
    BasicTimerSetTrgo(TIM6, BTIM_TRGO_UPDATE);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 1);
    AdcSetRegularSeq(ADC_IDX2, 1, (uint32_t[]){ ADC2_CH_DAC2 });
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);
    AdcEnable(ADC_IDX2);

    PrintStr("\r\n--- Test 3: Noise wave ch2 DHR=1000 MAMP=2047 TIM6 trigger, any key to stop ---\r\n");

    BasicTimerStart(TIM6);

    uint32_t count = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (UsartReadOne(USART4, &ch))
            break;

        if (TIM6->SR & 1)
        {
            TIM6->SR = ~(uint32_t)1;
            count++;

            if ((count % 200) == 1)
            {
                AdcStart(ADC_IDX2);
                AdcWaitEoc(ADC_IDX2, 1000000);
                uint32_t adc_val = AdcRead(ADC_IDX2);

                char buf[32];
                int p = 0;
                NumToStr(buf, adc_val);
                while (buf[p]) p++;
                UsartWrite(USART4, (void *)buf, p);
                UsartWrite(USART4, (void *)" ", 1);
            }
        }
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
    DAC1->CR &= ~(1 << 16);
    PrintStr("\r\n--- Test done ---\r\n");
}

/* ========================================================================
 *  测试 4：通道2 采样保持，写 DHR 即触发采样，验证长期保持
 *         (S&H 模式下 DHR 写入立即触发新采样，不能改 DHR 不触发)
 * ======================================================================== */

static void TestSampleHold(void)
{
    RCC->MP_APB1ENSETR |= 1 << 29;               /* DAC1 时钟使能 */
    RCC->MP_AHB2ENSETR |= 1 << 5;                /* ADC2 时钟使能 */

    /* ---- 配置 DAC1 ch2 S&H 模式 ---- */
    DacCfg_t cfg;
    cfg.dac_ch2_cen        = 0;
    cfg.dac_ch2_dmaudrie   = 0;
    cfg.dac_ch2_dma_en     = 0;
    cfg.dac_ch2_mamp       = 0;
    cfg.dac_ch2_wave       = 0;
    cfg.dac_ch2_tsel       = DAC_CH_TRG_SW;
    cfg.dac_ch2_tr_en      = 1;
    cfg.dac_ch2_en         = 0;
    cfg.dac_ch1_cen        = 0;
    cfg.dac_ch1_dmaudrie   = 0;
    cfg.dac_ch1_dma_en     = 0;
    cfg.dac_ch1_mamp       = 0;
    cfg.dac_ch1_wave       = 0;
    cfg.dac_ch1_tsel       = 0;
    cfg.dac_ch1_tr_en      = 0;
    cfg.dac_ch1_en         = 0;
    cfg.dac_pclk_hi_80     = 0;
    cfg.dac_ch2_mode       = DAC_CH_SAM_HOLD_EXT_PER_NO_BUFFER;
    cfg.dac_ch1_mode       = 0;
    DacCfg(DAC1, &cfg);
    DacSampleHoldCfg(DAC1, 2, 100, 1, 30);         /* 采样~3ms, 保持~31us, 刷新~0.94ms */
    cfg.dac_ch2_en = 1;
    DacCfg(DAC1, &cfg);
    DacCalibrate(DAC1, 2);

    /* ---- 配置 ADC2 读内部 DAC2 ---- */
    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 1);
    AdcSetRegularSeq(ADC_IDX2, 1, (uint32_t[]){ ADC2_CH_DAC2 });
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);
    AdcEnable(ADC_IDX2);

    PrintStr("\r\n--- Test 4: Sample & Hold ch2, long-term hold verification ---\r\n");

    /* Step 1: DacSoftTrig 写 DHR=2000 + 触发采样 */
    DacSoftTrig(DAC1, 2, 2000);
    for (volatile int d = 0; d < 20000; d++);
    AdcStart(ADC_IDX2);
    AdcWaitEoc(ADC_IDX2, 1000000);
    uint32_t v1 = AdcRead(ADC_IDX2);
    PrintU32("DacSoftTrig DHR=2000   ", v1);

    /* Step 2: DacWriteDhr 改 DHR=0 不触发, 长期保持 ~10s */
    DacWriteDhr(DAC1, 2, 0);
    PrintStr("DacWriteDhr DHR=0 (no trigger). Sampling every ~500ms:\r\n");

    uint32_t v_min = 4095, v_max = 0;
    for (int t = 0; t < 20; t++)
    {
        for (volatile int d = 0; d < 250000; d++);
        IwdgKickDog(IWDG2);
        AdcStart(ADC_IDX2);                         /* 双采样避刷新瞬态 */
        AdcWaitEoc(ADC_IDX2, 1000000);
        AdcRead(ADC_IDX2);
        AdcStart(ADC_IDX2);
        AdcWaitEoc(ADC_IDX2, 1000000);
        uint32_t v = AdcRead(ADC_IDX2);

        if (v < v_min) v_min = v;
        if (v > v_max) v_max = v;

        if ((t % 5) == 0 || t == 19)
        {
            char buf[32];
            int p = 0;
            buf[p++] = '[';
            NumToStr(buf + p, t);
            while (buf[p]) p++;
            buf[p++] = ']';
            buf[p++] = ' ';
            NumToStr(buf + p, v);
            while (buf[p]) p++;
            buf[p++] = '\r';
            buf[p++] = '\n';
            UsartWrite(USART4, (void *)buf, p);
        }
    }
    PrintStr("...\r\n");

    /* Step 3: DacSoftTrig 触发新采样 (DHR 已是 0) */
    DacSoftTrig(DAC1, 2, 0);
    for (volatile int d = 0; d < 20000; d++);
    AdcStart(ADC_IDX2);
    AdcWaitEoc(ADC_IDX2, 1000000);
    uint32_t v2 = AdcRead(ADC_IDX2);
    PrintU32("DacSoftTrig DHR=0      ", v2);

    /* 判定: 20 次波动 < 100, 触发后值必须显著变化 (证明更新) */
    int held_ok = (v_max - v_min) < 100;
    int32_t update = (int32_t)v2 - (int32_t)((v_max + v_min) / 2);
    if (update < 0) update = -update;
    if (held_ok && update > 1500)
        PrintStr("S&H PASS: value held ~10s, updated on trigger\r\n");
    else
    {
        PrintStr("S&H FAIL: range=");
        char dbuf[16];
        NumToStr(dbuf, v_max - v_min);
        UsartWrite(USART4, (void *)dbuf, strlen(dbuf));
        PrintStr("\r\n");
    }

    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
    cfg.dac_ch2_en = 0;
    DacCfg(DAC1, &cfg);
    PrintStr("--- Test done ---\r\n");
}

/* ========================================================================
 *  测试 5：双通道同步输出，DHR12RD 一次写两路，ADC2 同时采两路回读
 * ======================================================================== */

static void TestDualChannel(void)
{
    RCC->MP_APB1ENSETR |= 1 << 29;               /* DAC1 时钟使能 */
    RCC->MP_AHB2ENSETR |= 1 << 5;                /* ADC2 时钟使能 */

    /* ---- 配置 DAC1 双通道 ---- */
    DacCfg_t cfg;
    cfg.dac_ch2_cen        = 0;
    cfg.dac_ch2_dmaudrie   = 0;
    cfg.dac_ch2_dma_en     = 0;
    cfg.dac_ch2_mamp       = 0;
    cfg.dac_ch2_wave       = 0;
    cfg.dac_ch2_tsel       = DAC_CH_TRG_SW;
    cfg.dac_ch2_tr_en      = 1;
    cfg.dac_ch2_en         = 1;
    cfg.dac_ch1_cen        = 0;
    cfg.dac_ch1_dmaudrie   = 0;
    cfg.dac_ch1_dma_en     = 0;
    cfg.dac_ch1_mamp       = 0;
    cfg.dac_ch1_wave       = 0;
    cfg.dac_ch1_tsel       = DAC_CH_TRG_SW;
    cfg.dac_ch1_tr_en      = 1;
    cfg.dac_ch1_en         = 1;
    cfg.dac_pclk_hi_80     = 0;
    cfg.dac_ch2_mode       = DAC_CH_NORMAL_PER_NO_BUFFER;
    cfg.dac_ch1_mode       = DAC_CH_NORMAL_PER_NO_BUFFER;
    DacCfg(DAC1, &cfg);
    DacCalibrate(DAC1, 1);
    DacCalibrate(DAC1, 2);

    /* ---- 配置 ADC2 双通道序列 ch16(DAC1), ch17(DAC2) ---- */
    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 1);
    AdcSetRegularSeq(ADC_IDX2, 2, (uint32_t[]){ ADC2_CH_DAC1, ADC2_CH_DAC2 });
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);
    AdcEnable(ADC_IDX2);

    PrintStr("\r\n--- Test 5: Dual channel sync via DHR12RD ---\r\n");

    /* 测试 4 组值对 */
    const uint16_t pairs[][2] = { {0, 4095}, {1024, 3072}, {2048, 2048}, {3072, 1024} };

    for (int i = 0; i < 4; i++)
    {
        DacDualSoftTrig(DAC1, pairs[i][0], pairs[i][1]);
        for (volatile int d = 0; d < 20000; d++);

        AdcStart(ADC_IDX2);
        AdcWaitEoc(ADC_IDX2, 1000000);
        uint32_t r1 = AdcRead(ADC_IDX2);          /* 第 1 次读 = ch16 (DAC1) */
        AdcStart(ADC_IDX2);
        AdcWaitEoc(ADC_IDX2, 1000000);
        uint32_t r2 = AdcRead(ADC_IDX2);          /* 第 2 次读 = ch17 (DAC2) */

        char buf[64];
        int p = 0;
        buf[p++] = 'C'; buf[p++] = 'H'; buf[p++] = '1'; buf[p++] = ':';
        NumToStr(buf + p, pairs[i][0]);
        while (buf[p]) p++;
        buf[p++] = ' '; buf[p++] = 'C'; buf[p++] = 'H'; buf[p++] = '2'; buf[p++] = ':';
        NumToStr(buf + p, pairs[i][1]);
        while (buf[p]) p++;
        buf[p++] = '\r'; buf[p++] = '\n';
        UsartWrite(USART4, (void *)buf, p);

        p = 0;
        buf[p++] = 'R'; buf[p++] = '1'; buf[p++] = ':';
        NumToStr(buf + p, r1);
        while (buf[p]) p++;
        buf[p++] = ' '; buf[p++] = 'R'; buf[p++] = '2'; buf[p++] = ':';
        NumToStr(buf + p, r2);
        while (buf[p]) p++;
        buf[p++] = ' '; buf[p++] = '(';

        int32_t diff;
        diff = (int32_t)r1 - (int32_t)pairs[i][0];
        if (diff < 0) diff = -diff;
        NumToStr(buf + p, diff);
        while (buf[p]) p++;
        buf[p++] = '/';

        diff = (int32_t)r2 - (int32_t)pairs[i][1];
        if (diff < 0) diff = -diff;
        NumToStr(buf + p, diff);
        while (buf[p]) p++;
        buf[p++] = ')';
        buf[p++] = '\r'; buf[p++] = '\n';
        UsartWrite(USART4, (void *)buf, p);
    }

    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
    cfg.dac_ch1_en = 0;
    cfg.dac_ch2_en = 0;
    DacCfg(DAC1, &cfg);
    PrintStr("--- Test done ---\r\n");
}

/* ========================================================================
 *  测试 6：双通道 sin/cos，DHR12RD 同步输出，TIM6 触发，2 周期幅值 4000
 * ======================================================================== */

static void TestSinCos(void)
{
    RCC->MP_APB1ENSETR |= 1 << 4;                /* TIM6 时钟使能 */
    RCC->MP_APB1ENSETR |= 1 << 29;               /* DAC1 时钟使能 */
    RCC->MP_AHB2ENSETR |= 1 << 5;                /* ADC2 时钟使能 */

    /* ---- 配置 DAC1 双通道 mode1 (缓冲使能) ---- */
    DacCfg_t cfg;
    cfg.dac_ch2_cen        = 0;
    cfg.dac_ch2_dmaudrie   = 0;
    cfg.dac_ch2_dma_en     = 0;
    cfg.dac_ch2_mamp       = 0;
    cfg.dac_ch2_wave       = 0;
    cfg.dac_ch2_tsel       = DAC_CH_TRG_TIM6_TRGO;
    cfg.dac_ch2_tr_en      = 1;
    cfg.dac_ch2_en         = 1;
    cfg.dac_ch1_cen        = 0;
    cfg.dac_ch1_dmaudrie   = 0;
    cfg.dac_ch1_dma_en     = 0;
    cfg.dac_ch1_mamp       = 0;
    cfg.dac_ch1_wave       = 0;
    cfg.dac_ch1_tsel       = DAC_CH_TRG_TIM6_TRGO;
    cfg.dac_ch1_tr_en      = 1;
    cfg.dac_ch1_en         = 1;
    cfg.dac_pclk_hi_80     = 0;
    cfg.dac_ch2_mode       = DAC_CH_NORMAL_EXT_PIN_PER_BUFFER;
    cfg.dac_ch1_mode       = DAC_CH_NORMAL_EXT_PIN_PER_BUFFER;
    DacCfg(DAC1, &cfg);
    DacCalibrate(DAC1, 1);
    DacCalibrate(DAC1, 2);

    /* ---- TIM6 触发源 ---- */
    BasicTimerCfg_t tcfg;
    tcfg.tim_psc  = 99;
    tcfg.tim_arr  = 97;
    tcfg.tim_arpe = 1;
    tcfg.tim_opm  = 0;
    tcfg.tim_urs  = 0;
    tcfg.tim_udis = 0;
    tcfg.tim_ude  = 1;
    tcfg.tim_uie  = 0;
    BasicTimerCfg(TIM6, &tcfg);
    BasicTimerSetTrgo(TIM6, BTIM_TRGO_UPDATE);

    /* ---- ADC2 双通道 ---- */
    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 1);
    AdcSetRegularSeq(ADC_IDX2, 2, (uint32_t[]){ ADC2_CH_DAC1, ADC2_CH_DAC2 });
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);
    AdcEnable(ADC_IDX2);

    /* 16 点/周期 × 2 周期，中心 2048，幅值 ±2000 */
#define STEPS   32
    uint16_t sin_tab[STEPS] = {
        2048, 2813, 3462, 3896, 4048, 3896, 3462, 2813,
        2048, 1283,  634,  200,   48,  200,  634, 1283,
        2048, 2813, 3462, 3896, 4048, 3896, 3462, 2813,
        2048, 1283,  634,  200,   48,  200,  634, 1283
    };
    uint16_t cos_tab[STEPS] = {
        4048, 3896, 3462, 2813, 2048, 1283,  634,  200,
          48,  200,  634, 1283, 2048, 2813, 3462, 3896,
        4048, 3896, 3462, 2813, 2048, 1283,  634,  200,
          48,  200,  634, 1283, 2048, 2813, 3462, 3896
    };

    PrintStr("\r\n--- Test 6: Sin/Ch1 Cos/Ch2, 2 cycles, amp=4000pp ---\r\n");

    DacWriteDualDhr(DAC1, sin_tab[0], cos_tab[0]);
    BasicTimerStart(TIM6);

    for (int step = 0; step < STEPS; step++)
    {
        IwdgKickDog(IWDG2);

        while (!(TIM6->SR & 1));
        TIM6->SR = ~(uint32_t)1;

        if ((step % 4) == 0)
        {
            for (volatile int d = 0; d < 1000; d++);
            AdcStart(ADC_IDX2);
            AdcWaitEoc(ADC_IDX2, 1000000);
            uint32_t r1 = AdcRead(ADC_IDX2);
            AdcStart(ADC_IDX2);
            AdcWaitEoc(ADC_IDX2, 1000000);
            uint32_t r2 = AdcRead(ADC_IDX2);

            char buf[80];
            int p = 0;
            NumToStr(buf + p, step);
            while (buf[p]) p++;
            buf[p++] = ':';
            buf[p++] = ' ';
            NumToStr(buf + p, r1);
            while (buf[p]) p++;
            buf[p++] = '/';
            NumToStr(buf + p, sin_tab[step]);
            while (buf[p]) p++;
            buf[p++] = ' ';
            NumToStr(buf + p, r2);
            while (buf[p]) p++;
            buf[p++] = '/';
            NumToStr(buf + p, cos_tab[step]);
            while (buf[p]) p++;
            buf[p++] = '\r';
            buf[p++] = '\n';
            UsartWrite(USART4, (void *)buf, p);
        }

        if (step < STEPS - 1)
            DacWriteDualDhr(DAC1, sin_tab[step + 1], cos_tab[step + 1]);
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
    cfg.dac_ch1_en = 0;
    cfg.dac_ch2_en = 0;
    DacCfg(DAC1, &cfg);
    PrintStr("--- Test done ---\r\n");

#undef STEPS
}

/* ========================================================================
 *  主菜单
 * ======================================================================== */

void DacTest(void)
{
    char buf[8];
    uint32_t exit = 0;

    while (!exit)
    {
        IwdgKickDog(IWDG2);
        PrintStr("\r\n===== DAC Test Menu =====\r\n");
        PrintStr("1. Basic output (SW trigger + ADC2 internal readback)\r\n");
        PrintStr("2. Triangle wave ch2 (DHR=1000 MAMP=2047 TIM6 trigger)\r\n");
        PrintStr("3. Noise wave ch2 (DHR=1000 MAMP=2047 TIM6 trigger)\r\n");
        PrintStr("4. Sample & Hold ch2 (long-term hold ~10s)\r\n");
        PrintStr("5. Dual channel sync (DHR12RD + ADC2 dual read)\r\n");
        PrintStr("6. Sin/Ch1 Cos/Ch2 (2 cycles, amp=4000pp)\r\n");
        PrintStr("0. Back to main menu\r\n");
        PrintStr("Select: ");

        ReadLine(buf, sizeof(buf));

        if (strcmp(buf, "0") == 0)
            exit = 1;
        else if (strcmp(buf, "1") == 0)
            TestBasicOutput();
        else if (strcmp(buf, "2") == 0)
            TestTriangle();
        else if (strcmp(buf, "3") == 0)
            TestNoise();
        else if (strcmp(buf, "4") == 0)
            TestSampleHold();
        else if (strcmp(buf, "5") == 0)
            TestDualChannel();
        else if (strcmp(buf, "6") == 0)
            TestSinCos();
        else
            PrintStr("Invalid selection.\r\n");
    }
}
