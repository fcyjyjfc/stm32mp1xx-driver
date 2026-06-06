#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_adc.h"
#include "stm32mp1xx_btimer.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_exti.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"

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
    cfg.tim_arr  = 63999;                             /* ~1Hz @64MHz */
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
    AdcSetDiscMode(ADC_IDX2, 0);
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
 *  测试 7：注入转换（EXTI0/PA0 按键触发注入，过采样 1024x）
 * ======================================================================== */

static void exti0_inj_isr(void)
{
    ExtiClearFpr(0);
    AdcStartInjected(ADC_IDX2);
}

static void TestInjected(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_16BIT);

    /* 常规组：6 个内部通道，连续模式 */
    AdcSetContMode(ADC_IDX2, ADC_CONTINUOUS);
    AdcSetAutoDelay(ADC_IDX2, 1);
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, 0, ADC_TRIG_SOFTWARE);

    /* 注入组：通道 1（外部滑动变阻器），采样最大，过采样 1024x 右移 10 */
    AdcSetChanPreselect(ADC_IDX2, (1u << 1));
    AdcSetSampleTime(ADC_IDX2, 1, ADC_SMP_810P5);
    uint32_t inj_ch[] = { 1 };
    AdcSetInjectedSeq(ADC_IDX2, 1, inj_ch, 0, ADC_TRIG_SOFTWARE);
    AdcSetOverSample(ADC_IDX2, 1023, 10, 0, 1);

    AdcEnable(ADC_IDX2);
    AdcStart(ADC_IDX2);

    /* ---- EXTI0/PA0 按键中断 ---- */
    RCC->MP_AHB4ENSETR |= 1 << 0;
    GpioMode(GPIO_A, 0, GPIO_MODER_INPUT);
    GpioPullUpDown(GPIO_A, 0, GPIO_PUPDR_PULL_UP);
    ExtiSetGpio(0, EXTI_GPIO_PA);
    ExtiSetTrig(0, 2);
    ExtiEnableInt(1, 0);
    GicdSetGroup(GIC_EXTI0);
    GicdSetPriority(GIC_EXTI0, 5);
    GicdSetTarget(GIC_EXTI0, 1);
    GicdSetTrigMode(GIC_EXTI0, 0);
    GicRegisterIrq(GIC_EXTI0, exti0_inj_isr);
    GicdEnableInt(GIC_EXTI0);
    GicdInit();
    GiccInit(10, 2);
    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :: : "r0"
    );

    PrintStr("\r\n--- Test 7: Injected CONT=1 AUTDLY=1 EXT=0(SW) EXTI0 triggers inj ch1 OS=1024x ---\r\n");
    PrintStr("Press PA0 button to trigger injected conversion\r\n");

    uint32_t reg_idx = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            PrintU32(g_names[reg_idx], val);
            if (++reg_idx >= CHAN_N)
                reg_idx = 0;
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            PrintStr("--- EOS ---\r\n");
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_JEOS))
        {
        	PrintStr("*********************************************************\r\n\r\n\r\n\r\n");
            AdcClearJeos(ADC_IDX2);
            uint32_t val = AdcReadInjected(ADC_IDX2, 0);
            uint32_t mv = val * 3300 / 65536;
            char buf[64];
            int p = 0;
            const char *s = "  [INJ ch1: ";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, val);
            while (buf[p]) p++;
            s = " = ";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, mv);
            while (buf[p]) p++;
            s = " mV]\r\n";


            while (*s) buf[p++] = *s++;
            UsartWrite(USART4, (void *)buf, p);
            PrintStr("*********************************************************\r\n\r\n\r\n\r\n");
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    GicdDisableInt(GIC_EXTI0);
    ExtiDisableInt(1, 0);
    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "orr r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :: : "r0"
    );
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcSetOverSample(ADC_IDX2, 0, 0, 0, 0);
    AdcSetChanPreselect(ADC_IDX2, 0);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 8：自动注入（JAUTO=1，TIM6 触发常规组后自动启动注入）
 * ======================================================================== */

static void TestAutoInject(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_16BIT);

    /* 常规组：6 个内部通道，TIM6 触发单次 */
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 0);
    AdcSetRegularSeq(ADC_IDX2, CHAN_N, g_chans);
    AdcSetExtTrig(ADC_IDX2, ADC_EXTSEL_TIM6_TRGO, ADC_TRIG_RISING);

    /* 注入组：通道 1，过采样 1024x，JAUTO 自动触发（JEXTEN=0） */
    AdcSetChanPreselect(ADC_IDX2, (1u << 1));
    AdcSetSampleTime(ADC_IDX2, 1, ADC_SMP_810P5);
    uint32_t inj_ch[] = { 1 };
    AdcSetInjectedSeq(ADC_IDX2, 1, inj_ch, 0, ADC_TRIG_SOFTWARE);
    AdcSetOverSample(ADC_IDX2, 1023, 10, 0, 1);
    AdcSetAutoInject(ADC_IDX2, 1);                      /* JAUTO=1：EOS 后自动启动注入 */

    AdcEnable(ADC_IDX2);
    Tim6Init();
    BasicTimerStart(TIM6);
    AdcStart(ADC_IDX2);

    PrintStr("\r\n--- Test 8: AutoInject CONT=0 EXT=TIM6_TRGO JAUTO=1 inj OS=1024x ---\r\n");

    uint32_t reg_idx = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            PrintU32(g_names[reg_idx], val);
            if (++reg_idx >= CHAN_N)
                reg_idx = 0;
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOS))
        {
            AdcClearEos(ADC_IDX2);
            PrintStr("--- EOS (regular done, JAUTO starts injected) ---\r\n");
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_JEOS))
        {
            AdcClearJeos(ADC_IDX2);
            uint32_t val = AdcReadInjected(ADC_IDX2, 0);
            uint32_t mv = val * 3300 / 65536;
            char buf[64];
            int p = 0;
            const char *s = "  [INJ ch1: ";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, val);
            while (buf[p]) p++;
            s = " = ";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, mv);
            while (buf[p]) p++;
            s = " mV]\r\n";
            while (*s) buf[p++] = *s++;
            UsartWrite(USART4, (void *)buf, p);
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcSetAutoInject(ADC_IDX2, 0);
    AdcSetOverSample(ADC_IDX2, 0, 0, 0, 0);
    AdcSetChanPreselect(ADC_IDX2, 0);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 9：模拟看门狗（AWD1 + AWD2，监控通道 1，不同阈值）
 * ======================================================================== */

static void TestAwd1(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 5);

    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetPrescaler(1);
    AdcSetResolution(ADC_IDX2, ADC_RES_16BIT);

    /* 常规组：仅通道 1，TIM6 触发单次 */
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 0);
    uint32_t ch1[] = { 1 };
    AdcSetRegularSeq(ADC_IDX2, 1, ch1);
    AdcSetExtTrig(ADC_IDX2, ADC_EXTSEL_TIM6_TRGO, ADC_TRIG_RISING);

    /* 通道预选 + 采样时间 */
    AdcSetChanPreselect(ADC_IDX2, (1u << 1));
    AdcSetSampleTime(ADC_IDX2, 1, ADC_SMP_810P5);

    /* 看门狗 1：监控通道 1，LTR=1V，HTR=2V */
    uint32_t ltr1 = 65535u * 1000 / 3300;     /* 19859 = 1V */
    uint32_t htr1 = 65535u * 2000 / 3300;     /* 39718 = 2V */
    AdcSetAwd1(ADC_IDX2, 1, 0, 1, 1, ltr1, htr1);

    /* 看门狗 2：监控通道 1，LTR=1.2V，HTR=1.8V */
    uint32_t ltr2 = 65535u * 1200 / 3300;     /* 23831 = 1.2V */
    uint32_t htr2 = 65535u * 1800 / 3300;     /* 35746 = 1.8V */
    AdcSetAwd2(ADC_IDX2, (1u << 1), ltr2, htr2);

    AdcEnable(ADC_IDX2);
    Tim6Init();
    BasicTimerStart(TIM6);
    AdcStart(ADC_IDX2);

    PrintStr("\r\n--- Test 9: AWD1+2 CONT=0 EXT=TIM6_TRGO ch1 ---\r\n");
    PrintStr("  AWD1: 1V~2V   AWD2: 1.2V~1.8V\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val = AdcRead(ADC_IDX2);
            uint32_t mv = val * 3300 / 65536;
            PrintU32("ch1", val);
            char mv_str[12];
            int p = 0;
            NumToStr(mv_str, mv);
            PrintStr("  (");
            PrintStr(mv_str);
            PrintStr(" mV)\r\n");
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_AWD1))
        {
            AdcClearFlag(ADC_IDX2, ADC_FLAG_AWD1);
            PrintStr("*** AWD1: voltage OUT OF RANGE (1V~2V) ***\r\n");
        }

        if (AdcGetFlag(ADC_IDX2, ADC_FLAG_AWD2))
        {
            AdcClearFlag(ADC_IDX2, ADC_FLAG_AWD2);
            PrintStr("*** AWD2: voltage OUT OF RANGE (1.2V~1.8V) ***\r\n");
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX2);
    AdcSetAwd1(ADC_IDX2, 0, 0, 0, 0, 0, 0);
    AdcSetAwd2(ADC_IDX2, 0, 0, 0);
    AdcSetChanPreselect(ADC_IDX2, 0);
    AdcPowerDown(ADC_IDX2);
}

/* ========================================================================
 *  测试 10：双 ADC 同步模式（DUAL=REG_SIMULT，ADC1 ch1 + ADC2 VREFINT）
 * ======================================================================== */

static void TestDual(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 4) | (1u << 5);
    AdcSetVrefint(1);
    AdcSetTempSensor(1);
    AdcSetVbat(1);
    Adc2SetVddcore(1);
    AdcSetPrescaler(1);                         /* 和其他测试保持一致的时钟分频 */
    AdcSetCkMode(ADC_CK_ASYNC);

    /* === ADC1 配置（主，外部通道 1 滑动变阻器）=== */
    AdcPowerUp(ADC_IDX1);
    AdcCalibrate(ADC_IDX1, 0, 0);
    AdcSetResolution(ADC_IDX1, ADC_RES_16BIT);
    AdcSetContMode(ADC_IDX1, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX1, 0);
    uint32_t adc1_ch[] = { 1 };
    AdcSetRegularSeq(ADC_IDX1, 1, adc1_ch);
    AdcSetExtTrig(ADC_IDX1, ADC_EXTSEL_TIM6_TRGO, ADC_TRIG_RISING);
    AdcSetChanPreselect(ADC_IDX1, (1u << 1));
    AdcSetSampleTime(ADC_IDX1, 1, ADC_SMP_810P5);

    /* === ADC2 配置（从，内部 VREFINT）=== */
    AdcPowerUp(ADC_IDX2);
    AdcCalibrate(ADC_IDX2, 0, 0);
    AdcSetResolution(ADC_IDX2, ADC_RES_16BIT);
    AdcSetContMode(ADC_IDX2, ADC_SINGLE);
    AdcSetAutoDelay(ADC_IDX2, 0);
    AdcSetSampleTime(ADC_IDX2, ADC2_CH_VREFINT, ADC_SMP_810P5);
    uint32_t adc2_ch[] = { ADC2_CH_VREFINT };
    AdcSetRegularSeq(ADC_IDX2, 1, adc2_ch);

    /* === 双 ADC 模式：常规同步（必须在两个 ADC 都禁用时配置）=== */
    ADC->COMM.CCR = (ADC->COMM.CCR & ~0x1Fu) | ADC_DUAL_REG_SIMULT;

    /* === 使能双 ADC 前确认状态 === */
    if (!AdcEnable(ADC_IDX1))
        PrintStr("ADC1 enable FAIL\r\n");
    if (!AdcEnable(ADC_IDX2))
        PrintStr("ADC2 enable FAIL\r\n");

    Tim6Init();
    BasicTimerStart(TIM6);
    AdcStart(ADC_IDX1);                       /* 触发主 ADC1，ADC2 同步跟随 */

    {
        /* 打印 DUAL 模式值确认 */
        char dbg[40];
        int dp = 0;
        const char *ds = "DUAL=";
        while (*ds) dbg[dp++] = *ds++;
        uint32_t dual_val = ADC->COMM.CCR & 0x1Fu;
        NumToStr(dbg + dp, dual_val);
        while (dbg[dp]) dp++;
        ds = "\r\n";
        while (*ds) dbg[dp++] = *ds++;
        UsartWrite(USART4, (void *)dbg, dp);
    }

    PrintStr("--- Test 10: Dual REG_SIMULT ADC1 ch1(ext) + ADC2 VREFINT TIM6 trig ---\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;

        /* 等待两个 ADC 都完成转换 */
        if (AdcGetFlag(ADC_IDX1, ADC_FLAG_EOC) && AdcGetFlag(ADC_IDX2, ADC_FLAG_EOC))
        {
            uint32_t val1 = AdcRead(ADC_IDX1);
            uint32_t val2 = AdcRead(ADC_IDX2);
            uint32_t mv1 = val1 * 3300 / 65536;
            uint32_t mv2 = val2 * 3300 / 65536;

            char buf[128];
            int p = 0;
            const char *s = "ADC1 ch1: raw=";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, val1);
            while (buf[p]) p++;
            s = " (";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, mv1);
            while (buf[p]) p++;
            s = " mV)  ADC2 VREFINT: raw=";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, val2);
            while (buf[p]) p++;
            s = " (";
            while (*s) buf[p++] = *s++;
            NumToStr(buf + p, mv2);
            while (buf[p]) p++;
            s = " mV)\r\n";
            while (*s) buf[p++] = *s++;
            UsartWrite(USART4, (void *)buf, p);
        }

        if (UsartReadOne(USART4, &ch))
            break;
    }

    BasicTimerStop(TIM6);
    AdcStop(ADC_IDX1);
    AdcStop(ADC_IDX2);
    AdcDisable(ADC_IDX1);
    AdcDisable(ADC_IDX2);
    ADC->COMM.CCR &= ~0x1Fu;                 /* 恢复独立模式 */
    AdcSetChanPreselect(ADC_IDX1, 0);
    AdcPowerDown(ADC_IDX1);
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
        PrintStr("7. Injected (EXTI0/PA0 triggers inj ch1 OS=1024x)\r\n");
        PrintStr("8. AutoInject (JAUTO=1, TIM6 triggers reg, auto inj ch1 OS=1024x)\r\n");
        PrintStr("9. AWD1+2 (monitor ch1, AWD1=1V~2V, AWD2=1.2V~1.8V)\r\n");
        PrintStr("10. Dual REG_SIMULT (ADC1 ch1 potentiometer + ADC2 VREFINT)\r\n");
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
        else if (strcmp(buf, "7") == 0)
            TestInjected();
        else if (strcmp(buf, "8") == 0)
            TestAutoInject();
        else if (strcmp(buf, "9") == 0)
            TestAwd1();
        else if (strcmp(buf, "10") == 0)
            TestDual();
        else
            PrintStr("Invalid selection.\r\n");
    }
}
