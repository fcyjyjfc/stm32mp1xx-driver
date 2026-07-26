/*
 * sai_test.c — SAI + CS42L51 音频交互测试
 *
 *   命令 (帧 data 字段):
 *     ENTER / q  — 停止播放, 返回菜单
 *     + / -      — AOUT +/-2dB (快速)
 *     m          — 静音切换
 *     v          — 设 AOUT 增益 (half-db, 如 v24=+12dB)
 *     p          — 设 PCMMIX 音量 (half-db, 如 p-96=-48dB)
 *     s          — 显示当前状态
 */

#include "stm32mp1xx_sai.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_i2c.h"
#include "stm32mp1xx_iwdg.h"
#include "cs42l51.h"
#include "common_test.h"


/* ================================================================== */
/*  硬件配置                                                          */
/* ================================================================== */

extern SaiRegs_t *const SAI2;

#define SAI_BUS           SAI2
#define SAI_BLOCK         SAI_BLOCK_A
#define SAMPLE_RATE       8000U

#define CODEC_I2C         I2C5
#define CODEC_ADDR        CS42L51_ADDR(0)

/* +/- 快速步进 (2 = 1dB) */
#define VOL_STEP          4U


/* ================================================================== */
/*  1kHz 正弦波表 (16 点, 16-bit, 1/4 满幅)                           */
/* ================================================================== */

static const uint16_t g_sine16[] = {
    0x0000, 0x0C3E, 0x16A0, 0x1D90,
    0x1FFF, 0x1D90, 0x16A0, 0x0C3E,
    0x0000, 0xF3C1, 0xE95F, 0xE26F,
    0xE000, 0xE26F, 0xE95F, 0xF3C1,
};

#define SINE_STEP  2   /* 1kHz @ 8kHz: 1000 * 16 / 8000 = 2 */


/* ================================================================== */
/*  时钟 & GPIO                                                        */
/* ================================================================== */

static void SaiTest_ClockCfg(void)
{
    RCC->CPERCKSELR = 0;
    RCC->MP_APB1ENSETR |= (1 << 24);
    RCC->MP_APB2ENSETR |= (1 << 17);
    RCC->SAI2CKSELR = 0x3;
}


static void SaiTest_GpioInit(void)
{
    RCC->MP_AHB4ENSETR |= (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 8);

    /* I2C5: PA11=SCL(AF4), PA12=SDA(AF4), open-drain */
    GpioMode(GPIO_A, 11, GPIO_MODER_AF);
    GpioAf(GPIO_A, 11, 4);
    GpioOtype(GPIO_A, 11, GPIO_OTYPE_OD);
    GpioOspeed(GPIO_A, 11, GPIO_OSPEED_LOW);

    GpioMode(GPIO_A, 12, GPIO_MODER_AF);
    GpioAf(GPIO_A, 12, 4);
    GpioOtype(GPIO_A, 12, GPIO_OTYPE_OD);
    GpioOspeed(GPIO_A, 12, GPIO_OSPEED_LOW);

    /* SAI2 Block A: PE0=MCLK(AF10), PI7=FS(AF10), PI5=SCK(AF10), PI6=SD(AF10) */
    GpioMode(GPIO_E, 0, GPIO_MODER_AF);
    GpioAf(GPIO_E, 0, 10);
    GpioOtype(GPIO_E, 0, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_E, 0, GPIO_OSPEED_HI);

    GpioMode(GPIO_I, 7, GPIO_MODER_AF);
    GpioAf(GPIO_I, 7, 10);
    GpioOtype(GPIO_I, 7, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_I, 7, GPIO_OSPEED_HI);

    GpioMode(GPIO_I, 5, GPIO_MODER_AF);
    GpioAf(GPIO_I, 5, 10);
    GpioOtype(GPIO_I, 5, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_I, 5, GPIO_OSPEED_HI);

    GpioMode(GPIO_I, 6, GPIO_MODER_AF);
    GpioAf(GPIO_I, 6, 10);
    GpioOtype(GPIO_I, 6, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_I, 6, GPIO_OSPEED_HI);

    /* CS42L51 复位: PC0=输出, 默认低 */
    GpioMode(GPIO_C, 0, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_C, 0, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_C, 0, GPIO_OSPEED_LOW);
    GpioOutLow(GPIO_C, 0);

    /* SAI2 Block B (预留): PF11=SD(AF10) */
    GpioMode(GPIO_F, 11, GPIO_MODER_AF);
    GpioAf(GPIO_F, 11, 10);
    GpioOtype(GPIO_F, 11, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_F, 11, GPIO_OSPEED_LOW);
}


/* ---- 解析有符号十进制整数 ---------------------------------------- */
static int ParseDec(const char *s, int len)
{
    int val = 0, neg = 0, i = 0;
    if (i < len && s[i] == '-') { neg = 1; i++; }
    else if (i < len && s[i] == '+') { i++; }
    while (i < len && s[i] >= '0' && s[i] <= '9')
        val = val * 10 + (s[i++] - '0');
    return neg ? -val : val;
}


/* ================================================================== */
/*  SAI 测试主函数                                                     */
/* ================================================================== */

void SaiTest(void)
{
    SaiRegs_t *sai = SAI_BUS;
    SaiCfg_t   cfg;
    Cs42l51_t  codec;
    uint32_t   phase = 0;
    Frame_t    fr;
    char      *cmd;
    uint8_t    aout_vol   = 8;       /* +4dB */
    uint8_t    pcmmix_vol = 32;      /* -48dB */
    char       dbuf[20];
    uint8_t    muted      = 0;
    int        len;

    PRINT("\r\n===== SAI + CS42L51 Audio Test =====\r\n");
    PRINT("  +/-   AOUT +/-2dB     m   mute\r\n");
    PRINT("  vN    AOUT gain        pN  PCMMIX vol\r\n");
    PRINT("        (half-db: v24=+12dB)   (half-db: p-96=-48dB)\r\n");
    PRINT("  s     status          q   quit\r\n");
    PRINT("AOUT: 0~24 = 0~+12dB    (增益, >24截断)\r\n");
    PRINT("PCMMIX: -102~+24 = -51~+12dB  (负=衰减, 正=增益)\r\n");
    PRINT("Initial: AOUT=+4dB  PCMMIX=-48dB  Sine=1/4  8kHz\r\n\r\n");

    /* ---- 1. GPIO + 时钟 + I2C ---- */
    SaiTest_GpioInit();
    SaiTest_ClockCfg();
    I2cCfg(I2C5);

    /* ---- 2. SAI I2S 主机 ---- */
    SaiCfgFillI2S(&cfg, 64000000U, SAMPLE_RATE, SAI_DS_16,
                  SAI_MODE_MASTER_TX, 1);
    SaiInit(sai, SAI_BLOCK, &cfg);
    SaiStart(sai, SAI_BLOCK);

    /* ---- 3. CODEC 初始化 ---- */
    codec.i2c        = CODEC_I2C;
    codec.slave_addr = CODEC_ADDR;
    codec.rst_port   = GPIO_C;
    codec.rst_pin    = 0;
    Cs42l51Init(&codec);
    Cs42l51SetVolume(&codec, aout_vol);

    /* ---- 手动覆写 PCMMIX (init 里写的是 0x10) ---- */
    Cs42l51RegWrite(&codec, 0x10, pcmmix_vol);
    Cs42l51RegWrite(&codec, 0x11, pcmmix_vol);

    /* ==================== 交互循环 ==================== */
    while (1)
    {
        uint32_t sample = g_sine16[phase];
        uint32_t frame[2] = { sample, sample };
        SaiSendBlocking(sai, SAI_BLOCK, frame, 2);

        phase += SINE_STEP;
        if (phase >= 16)
            phase -= 16;

        while (FramePoll(&fr))
        {
            len = (int)fr.data_len;
            if (len == 0)
                continue;
            cmd = (char *)fr.data;
            cmd[len] = '\0';

            if (cmd[0] == 'q' || fr.cmd == FRAME_CMD_BACK)
                goto exit_test;

            if (cmd[0] == '+')
            {
                if (aout_vol + VOL_STEP <= 0x18)
                    aout_vol += VOL_STEP;
                Cs42l51SetVolume(&codec, muted ? 0x80 : aout_vol);
                PRINT("AOUT="); PrintHex8(aout_vol);
                PrintDec(dbuf, aout_vol); PRINT(" hdB\r\n");
            }
            else if (cmd[0] == '-')
            {
                if (aout_vol >= VOL_STEP)
                    aout_vol -= VOL_STEP;
                Cs42l51SetVolume(&codec, muted ? 0x80 : aout_vol);
                PRINT("AOUT="); PrintHex8(aout_vol);
                PrintDec(dbuf, aout_vol); PRINT(" hdB\r\n");
            }
            else if (cmd[0] == 'm')
            {
                muted = !muted;
                Cs42l51SetVolume(&codec, muted ? 0x80 : aout_vol);
                PRINT(muted ? "MUTED\r\n" : "UNMUTED\r\n");
            }
            else if (cmd[0] == 'v' && len >= 2)
            {
                int half = ParseDec(cmd + 1, len - 1);
                aout_vol = Cs42l51AoutHalfDbToReg(half);
                Cs42l51SetVolume(&codec, muted ? 0x80 : aout_vol);
                PRINT("AOUT="); PrintHex8(aout_vol); PRINT(" ");
                PrintDec(dbuf, half); PRINT(" hdB\r\n");
            }
            else if (cmd[0] == 'p' && len >= 2)
            {
                int half = ParseDec(cmd + 1, len - 1);
                pcmmix_vol = Cs42l51PcmmixHalfDbToReg(half);
                Cs42l51RegWrite(&codec, 0x10, pcmmix_vol);
                Cs42l51RegWrite(&codec, 0x11, pcmmix_vol);
                PRINT("PCMMIX="); PrintHex8(pcmmix_vol); PRINT(" ");
                PrintDec(dbuf, half); PRINT(" hdB\r\n");
            }
            else if (cmd[0] == 's')
            {
                PRINT("AOUT=");   PrintHex8(Cs42l51RegRead(&codec, 0x16));
                PRINT(" PCMMIX="); PrintHex8(Cs42l51RegRead(&codec, 0x10));
                PRINT(" 09h=");   PrintHex8(Cs42l51RegRead(&codec, 0x09));
                PRINT("\r\n");
            }
        }

        IwdgKickDog(IWDG2);
    }

exit_test:
    SaiStop(sai, SAI_BLOCK);
    Cs42l51Deinit(&codec);
    PRINT("Audio stopped.\r\n");
}
