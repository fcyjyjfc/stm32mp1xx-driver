#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_adc.h"
#include "stm32mp1xx_btimer.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_rcc.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

/* 内部通道（仅 ADC2）*/
static const uint32_t g_chans[] = {
    ADC2_CH_VSENSE,
    ADC2_CH_VREFINT,
    ADC2_CH_VDDCORE,
    ADC2_CH_VBAT,
    ADC2_CH_DAC1,
    ADC2_CH_DAC2,
};
static const char *g_names[] = {
    "VSENSE ",
    "VREFINT",
    "VDDCORE",
    "VBAT/4 ",
    "DAC1   ",
    "DAC2   ",
};
#define CHAN_N  (sizeof(g_chans) / sizeof(g_chans[0]))

/* ========================================================================
 *  辅助
 * ======================================================================== */

static void Tim6Init(void)
{
    RCC->MP_APB1ENSETR |= 1 << 4;

    BasicTimerCfg_t cfg;
    cfg.tim_psc  = 15999;
    cfg.tim_arr  = 15999;                             /* ~1Hz @64MHz */
    cfg.tim_arpe = 1;
    cfg.tim_opm  = 0;
    cfg.tim_urs  = 0;
    cfg.tim_udis = 0;
    cfg.tim_ude  = 0;
    cfg.tim_uie  = 0;
    BasicTimerCfg(TIM6, &cfg);
    BasicTimerSetTrgo(TIM6, BTIM_TRGO_UPDATE);                      /* MMS=010: 更新事件 → TRGO */
}

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

static void PrintStr(const char *s)
{
    UsartWrite(USART4, (void *)s, strlen(s));
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

/* ========================================================================
 *  打印一轮全部通道结果
 * ======================================================================== */

static void PrintAllResults(void)
{
    for (uint32_t i = 0; i < CHAN_N; i++)
    {
        AdcWaitEoc(ADC_IDX2, 1000000);
        PrintU32(g_names[i], AdcRead(ADC_IDX2));
    }
}

/* ========================================================================
 *  测试 1：单次模式（TIM6 每触发一次转一轮全部通道）
 * ======================================================================== */

static void TestSingleSeq(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 0);
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, ADC_EXTSEL_TIM6_TRGO, ADC_TRIG_RISING);    /* TIM6_TRGO 上升沿触发 */
    AdcEnable(ADC_IDX2);
    Tim6Init();
    BasicTimerStart(TIM6);
    AdcStart(ADC_IDX2);                              /* 等待硬件触发 */

    PrintStr("\r\n--- Test 1: Single CONT=0 AUTDLY=0 EXT=13(TRIG_RISING) TIM6_TRGO ---\r\n");

    uint32_t ch_idx = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            PrintU32(g_names[ch_idx], val);
            if (++ch_idx >= CHAN_N)
                ch_idx = 0;
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            PrintStr("--- EOS ---\r\n");
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 2：连续模式（软件触发一次，连续转换）
 * ======================================================================== */

static void TestContinuous(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_CONTINUOUS);
    AdcSetAutoDelay(ADC_IDX2, 0);
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);   /* 软件触发 */
    AdcEnable(ADC_IDX2);
    AdcStart(ADC_IDX2);                              /* 触发一次，连续转换 */

    PrintStr("\r\n--- Test 2: Continuous CONT=1 AUTDLY=0 EXT=0(SW) OVR risk ---\r\n");

    uint32_t ch_idx = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            PrintU32(g_names[ch_idx], val);
            if (++ch_idx >= CHAN_N)
                ch_idx = 0;
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            PrintStr("--- EOS ---\r\n");
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 3：单次模式（软件触发一次，跑一轮停）
 * ======================================================================== */

static void TestSingleSw(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 0);
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);   /* 软件触发 */
    AdcEnable(ADC_IDX2);
    AdcStart(ADC_IDX2);                              /* 触发一次，跑一轮 */

    PrintStr("\r\n--- Test 3: Single CONT=0 AUTDLY=0 EXT=0(SW) one shot ---\r\n");

    uint32_t ch_idx = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            PrintU32(g_names[ch_idx], val);
            ++ch_idx;
        }

        if ((ch_idx == CHAN_N) && AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            PrintStr("--- EOS (done) ---\r\n");
            break;
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 4：不连续模式（DISCEN=1, DISCNUM=1）
 *  每次触发转 1 个通道，需要 6 次触发走完一轮。
 * ======================================================================== */

static void TestDiscontinuous(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetDiscMode(ADC_IDX2, 2);                     /* DISCEN=1, DISCNUM=2：每次触发转 2 个通道 */
    AdcSetAutoDelay(ADC_IDX2, 0);
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, ADC_EXTSEL_TIM6_TRGO, ADC_TRIG_RISING);    /* TIM6_TRGO 上升沿触发 */
    AdcEnable(ADC_IDX2);
    Tim6Init();
    BasicTimerStart(TIM6);
    AdcStart(ADC_IDX2);

    PrintStr("\r\n--- Test 4: Discontinuous CONT=0 AUTDLY=0 DISC=2 EXT=13(TIM6_TRGO) ---\r\n");

    uint32_t seq_cnt = 0;
    uint32_t ch_pos  = 0;                            /* 当前在序列中的位置 */
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            char buf[48];
            int p = 0;
            const char *s = g_names[ch_pos];
            while (*s) buf[p++] = *s++;
            buf[p++] = ':';
            buf[p++] = ' ';
            NumToStr(buf + p, val);
            while (buf[p]) p++;
            buf[p++] = '\r';
            buf[p++] = '\n';
            UsartWrite(USART4, (void *)buf, p);

            ch_pos++;
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            ch_pos = 0;
            seq_cnt++;
            char buf[16];
            int p = 0;
            PrintStr("  --- EOS #");
            NumToStr(buf, seq_cnt);
            while (buf[p]) p++;
            buf[p++] = '\r';
            buf[p++] = '\n';
            UsartWrite(USART4, (void *)buf, p);
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 5：溢出阻塞演示（OVR MODE = PRESERVE）
 *  不读 DR，让溢出阻塞序列，观察 OVR 标志。
 * ======================================================================== */

static void TestOvrBlock(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 0);
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);

    /* 使用默认 OVR MODE = PRESERVE（阻塞），不调用 AdcSetOvrMode */
    AdcEnable(ADC_IDX2);
    AdcStart(ADC_IDX2);

    PrintStr("\r\n--- Test 5: OVR block CONT=0 AUTDLY=0 OVRMODE=PRESERVE no DR read ---\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            PrintStr("--- EOS (sequence complete) ---\r\n");
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_OVR))
        {
            PrintStr("OVR detected! Sequence blocked (EOS never comes).\r\n");
            PrintStr("Press any key to exit.\r\n");
            break;
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 6：连续模式 + 自动延迟（AUTDLY=1，每转换等 DR 读取，防溢出）
 * ======================================================================== */

static void TestContinuousAutoDelay(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_12BIT);
    AdcSetContMode(ADC_IDX2, ADC_CONTINUOUS);
    AdcSetAutoDelay(ADC_IDX2, 1);                        /* 每转换完等待 DR 被读，防 OVR */
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);
    AdcEnable(ADC_IDX2);
    AdcStart(ADC_IDX2);

    PrintStr("\r\n--- Test 6: Continuous+AUTDLY CONT=1 AUTDLY=1 EXT=0(SW) no OVR ---\r\n");

    uint32_t ch_idx = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            PrintU32(g_names[ch_idx], val);
            if (++ch_idx >= CHAN_N)
                ch_idx = 0;
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            PrintStr("--- EOS ---\r\n");
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  二级菜单入口
 * ======================================================================== */

void AdcTest(void)
{
    char buf[8];
    uint32_t exit = 0;

    while (!exit)
    {
        IwdgKickDog(IWDG2);
        PrintStr("\r\n===== ADC Test Menu =====\r\n");
        PrintStr("1. Single (TIM6 TRGO, each trigger converts all channels)\r\n");
        PrintStr("2. Continuous (software trigger, no delay, OVR possible)\r\n");
        PrintStr("3. Single (software trigger, one shot, auto stop)\r\n");
        PrintStr("4. Discontinuous (DISCEN=2, TIM6 TRGO, 2 ch per trigger)\r\n");
        PrintStr("5. OVR block demo (no DR read, PRESERVE mode)\r\n");
        PrintStr("6. Continuous + AUTDLY (wait DR read, no OVR)\r\n");
        PrintStr("0. Back to main menu\r\n");
        PrintStr("Select: ");

        ReadLine(buf, sizeof(buf));

        if (strcmp(buf, "0") == 0)
            exit = 1;
        else if (strcmp(buf, "1") == 0)
            TestSingleSeq();
        else if (strcmp(buf, "2") == 0)
            TestContinuous();
        else if (strcmp(buf, "3") == 0)
            TestSingleSw();
        else if (strcmp(buf, "4") == 0)
            TestDiscontinuous();
        else if (strcmp(buf, "5") == 0)
            TestOvrBlock();
        else if (strcmp(buf, "6") == 0)
            TestContinuousAutoDelay();
        else
            PrintStr("Invalid selection.\r\n");
    }
}
