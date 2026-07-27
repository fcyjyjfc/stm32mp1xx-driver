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
#include "stm32mp1xx_dma.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_usart.h"
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
/*  Happy Birthday 旋律生成 (8kHz 16-bit PCM)                         */
/* ================================================================== */

/* 64 点正弦表 Q15 */
static const int16_t g_sine64[64] = {
       0,  3212,  6393,  9512, 12540, 15447, 18205, 20788,
   23170, 25330, 27246, 28899, 30274, 31357, 32138, 32610,
   32767, 32610, 32138, 31357, 30274, 28899, 27246, 25330,
   23170, 20788, 18205, 15447, 12540,  9512,  6393,  3212,
       0, -3212, -6393, -9512, -12540, -15447, -18205, -20788,
  -23170, -25330, -27246, -28899, -30274, -31357, -32138, -32610,
  -32767, -32610, -32138, -31357, -30274, -28899, -27246, -25330,
  -23170, -20788, -18205, -15447, -12540, -9512, -6393, -3212,
};

/* 十二半音频率 C4(0) ~ G5(11) */
static const uint16_t g_note_freq[12] = {
    262, 294, 330, 349, 392, 440, 494, 523, 587, 659, 698, 784,
};

/* Happy Birthday 主旋律: (note_index, duration_ticks)
 * note_index: 0=休止, 1=C4 ... 11=G5;  1 tick = 125ms */
static const uint8_t g_happy_birthday[] = {
    /* G  G  A  G  C  B         "Happy birthday to you" */
    4,4,  4,2,  5,2,  4,2,  7,4,  6,8,
    /* G  G  A  G  D  C         "Happy birthday to you" */
    4,4,  4,2,  5,2,  4,2,  8,4,  7,8,
    /* G  G  G  E  C  B  A      "Happy birthday dear friend" */
    4,4,  4,2,  4,2,  9,2,  7,4,  6,2,  5,8,
    /* F  F  E  C  D  C         "Happy birthday to you" */
   10,4, 10,2,  9,2,  7,2,  8,4,  7,8,
};

#define TICK_SAMPLES  1000     /* 125ms @ 8kHz */

static void SaiTestPlayMelody(SaiRegs_t *sai)
{
    uint32_t total_notes = sizeof(g_happy_birthday) / 2;
    uint32_t i;
    for (i = 0; i < total_notes; i++)
    {
        uint8_t note   = g_happy_birthday[i * 2];
        uint32_t ticks = g_happy_birthday[i * 2 + 1];
        uint32_t n     = ticks * TICK_SAMPLES;
        uint32_t s;

        if (note == 0 || note > 11)
        {
            for (s = 0; s < n; s++)
            {
                uint32_t frame[2] = { 0, 0 };
                SaiSendBlocking(sai, SAI_BLOCK, frame, 2);
                if ((s & 0xFF) == 0) IwdgKickDog(IWDG2);
            }
        }
        else
        {
            /* phase_inc (Q16.16):  freq * table_size / sample_rate
             *                     = freq * 64 / 8000
             *                     = (freq << 22) / 8000        */
            uint32_t phase_inc = ((uint64_t)g_note_freq[note] << 22) / 8000;
            uint32_t phase = 0;

            for (s = 0; s < n; s++)
            {
                uint32_t idx = (phase >> 16) & 0x3F;
                int16_t sam = g_sine64[idx];
                uint32_t frame[2] = { (uint16_t)sam, (uint16_t)sam };
                SaiSendBlocking(sai, SAI_BLOCK, frame, 2);
                phase += phase_inc;
                if ((s & 0xFF) == 0) IwdgKickDog(IWDG2);
            }
        }
    }
}


/* ================================================================== */
/*  PCM 流播放 — DMA 乒乓缓冲                                          */
/* ================================================================== */

#define PCM_BUF_WORDS   2048
#define PCM_HALF_WORDS  (PCM_BUF_WORDS / 2)
#define PCM_RING_SIZE   65536
#define PCM_DMA_STREAM  2       /* DMA2 Stream 2, GIC_DMA2_STR2=90 */
#define PCM_BAUD_HI     460800
#define PCM_BAUD_LO     115200

static uint32_t g_pcm_dma_buf[PCM_BUF_WORDS] __attribute__((aligned(4)));
static uint8_t  g_pcm_ring[PCM_RING_SIZE];
static volatile uint32_t g_pcm_ring_wr;
static volatile uint32_t g_pcm_ring_rd;
static volatile uint32_t g_pcm_underrun;
static volatile uint32_t g_pcm_playing;


/* ---- 从环形缓冲区取 count 个 32-bit PCM 字填到 dst ------------ */
static void SaiPcmFillHalf(uint32_t *dst, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        uint32_t sample = 0;
        int b;

        if (g_pcm_ring_rd == g_pcm_ring_wr) {
            /* underrun: 剩余位置填静音 */
            g_pcm_underrun++;
            for (; i < count; i++)
                dst[i] = 0;
            return;
        }

        /* 每次从 ring 取 2 字节 (16-bit), 拼成 32-bit SAI_DR 字 */
        for (b = 0; b < 2; b++) {
            sample |= (uint32_t)g_pcm_ring[g_pcm_ring_rd] << (b * 8);
            g_pcm_ring_rd = (g_pcm_ring_rd + 1) % PCM_RING_SIZE;
        }
        dst[i] = sample;
    }
}


/* ---- DMA Stream 2 ISR: HTIF→填前半, TCIF→填后半 -------------- */
static void SaiPcmDmaIsr(void)
{
    uint32_t s    = PCM_DMA_STREAM % 4;
    uint32_t sh   = (s / 2) * 16 + (s % 2) * 6;
    uint32_t htif = (1u << 4) << sh;
    uint32_t tcif = (1u << 5) << sh;
    volatile uint32_t *sr = (PCM_DMA_STREAM < 4) ? &DMA2->LISR : &DMA2->HISR;

    if (*sr & htif) {
        DmaClearHtif(DMA2, PCM_DMA_STREAM);
        SaiPcmFillHalf(&g_pcm_dma_buf[0], PCM_HALF_WORDS);
    }
    if (*sr & tcif) {
        DmaClearTcif(DMA2, PCM_DMA_STREAM);
        SaiPcmFillHalf(&g_pcm_dma_buf[PCM_HALF_WORDS], PCM_HALF_WORDS);
    }
}


/* ---- PCM 数据写入环形缓冲区 (由 FramePoll 回调触发) ----------- */
static void SaiPcmOnFrame(const Frame_t *frame)
{
    uint32_t i;
    if (frame->cmd != FRAME_CMD_PCM_DATA)
        return;
    for (i = 0; i < frame->data_len; i++) {
        g_pcm_ring[g_pcm_ring_wr] = frame->data[i];
        g_pcm_ring_wr = (g_pcm_ring_wr + 1) % PCM_RING_SIZE;
    }
}


/* ---- 子测试 3: PCM Stream ---- */
static void TestSaiPcmStream(void)
{
    SaiRegs_t *sai = SAI_BUS;
    volatile SaiBlockRegs_t *r = &sai->Block[SAI_BLOCK];
    SaiCfg_t   cfg;
    Cs42l51_t  codec;
    DmaCfg_t   dma_cfg;
    Frame_t    fr;
    uint32_t   pre_len;
    uint32_t   loop_cnt;

    PRINT("\r\n===== PCM Stream Player =====\r\n");
    PRINT("  UART switched to 460800 baud\r\n");
    PRINT("  Run pcm_sender.py on PC to stream audio\r\n");
    PRINT("  Press 0 to stop\r\n\r\n");

    /* ---- 1. 硬件初始化 (GPIO + SAI + Codec) ---- */
    SaiTest_GpioInit();
    SaiTest_ClockCfg();
    I2cCfg(I2C5);

    /* SAI: 8kHz 16-bit mono (nbslot=1) I2S master TX */
    SaiCfgFillI2S(&cfg, 64000000U, SAMPLE_RATE, SAI_DS_16,
                  SAI_MODE_MASTER_TX, 1);
    /* 覆写为单声道: 1 slot, 32-bit slot 容纳 16-bit 数据 */
    cfg.nbslot  = 1;
    cfg.slotsz  = SAI_SLOTSZ_32;
    cfg.sloten  = (1u << 0);
    SaiInit(sai, SAI_BLOCK, &cfg);

    /* 使能 SAI DMA TX 请求 */
    SaiDmaEnable(sai, SAI_BLOCK);

    /* Codec */
    codec.i2c        = CODEC_I2C;
    codec.slave_addr = CODEC_ADDR;
    codec.rst_port   = GPIO_C;
    codec.rst_pin    = 0;
    Cs42l51Init(&codec);
    Cs42l51SetVolume(&codec, 8);
    /* PCMMIXA/B -48dB: 值 32 = (-96 + 128) & 0x7F = -48dB */
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXA_VOL, 32);
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXB_VOL, 32);

    /* ---- 2. DMA 乒乓缓冲配置 ---- */
    dma_cfg                 = DMA_CFG_DEFAULT;
    dma_cfg.dma_dir         = DMA_DIR_MEM_2_PER;
    dma_cfg.dma_per_addr    = (uint32_t)&r->DR;
    dma_cfg.dma_mem0_addr   = (uint32_t)g_pcm_dma_buf;
    dma_cfg.dma_msize       = DMA_DATA_SIZE_BIT32;
    dma_cfg.dma_psize       = DMA_DATA_SIZE_BIT32;
    dma_cfg.dma_circual_buf = 1;
    dma_cfg.dma_ndtr        = PCM_BUF_WORDS;
    dma_cfg.dma_tcie        = 1;
    dma_cfg.dma_htie        = 1;
    dma_cfg.dma_stream_num  = PCM_DMA_STREAM;
    dma_cfg.dma_stream_pri  = DMA_STREAM_PRIOR_VERY_HI;
    DmaCfg(DMA2, &dma_cfg);

    /* DMAMUX 路由 */
    DmaMuxRoute(DMAMUX1, PCM_DMA_STREAM + 8, DMAMUX_REQ_SAI2_A);

    /* GIC 中断 */
    GicdSetGroup(GIC_DMA2_STR2);
    GicdSetPriority(GIC_DMA2_STR2, 4);
    GicdSetTarget(GIC_DMA2_STR2, 1);
    GicdSetTrigMode(GIC_DMA2_STR2, 0);
    GicRegisterIrq(GIC_DMA2_STR2, SaiPcmDmaIsr);
    GicdEnableInt(GIC_DMA2_STR2);

    /* PCM ring 状态清零 */
    g_pcm_ring_wr = 0;
    g_pcm_ring_rd = 0;
    g_pcm_underrun = 0;
    g_pcm_playing = 0;

    /* 预填充 DMA 缓冲区前半 (静音) */
    {
        uint32_t k;
        for (k = 0; k < PCM_BUF_WORDS; k++)
            g_pcm_dma_buf[k] = 0;
    }

    /* ---- 3. 切换波特率到 460800 ---- */
    UsartSetBaud(USART4, 64000000, PCM_BAUD_HI);

    /* ---- 4. 预缓冲大约 1 秒数据 ---- */
    PRINT("Buffering...\r\n");
    pre_len = PCM_BAUD_HI / 800;     /* ~0.5s buffer first */
    while (g_pcm_ring_wr - g_pcm_ring_rd < pre_len)
    {
        while (FramePoll(&fr))
            SaiPcmOnFrame(&fr);
        IwdgKickDog(IWDG2);
    }

    /* ---- 5. 启动播放 ---- */
    SaiStart(sai, SAI_BLOCK);
    g_pcm_playing = 1;
    PRINT("Playing + streaming...\r\n");

    /* ---- 6. 交互循环 ---- */
    loop_cnt = 0;
    while (1)
    {
        while (FramePoll(&fr))
        {
            if (fr.cmd == FRAME_CMD_BACK
                || (fr.data_len >= 1 && fr.data[0] == '0'))
                goto exit;
            SaiPcmOnFrame(&fr);
        }

        IwdgKickDog(IWDG2);

        /* 每 256 次循环打印一次 underrun 统计 */
        loop_cnt++;
        if ((loop_cnt & 0xFF) == 0 && g_pcm_underrun > 0)
        {
            PRINT("U=");
            {
                char buf[8];
                NumToStr(buf, g_pcm_underrun);
                PRINT(buf);
            }
            PRINT(" ");
        }
    }

exit:
    /* ---- 7. 清理 ---- */
    g_pcm_playing = 0;

    GicdDisableInt(GIC_DMA2_STR2);
    DmaDisable(DMA2, PCM_DMA_STREAM);
    SaiStop(sai, SAI_BLOCK);

    /* 恢复 115200 波特率 */
    UsartSetBaud(USART4, 64000000, PCM_BAUD_LO);

    Cs42l51Deinit(&codec);
    PRINT("\r\nStream stopped. U=");
    {
        char buf[8];
        NumToStr(buf, g_pcm_underrun);
        PRINT(buf);
    }
    PRINT(" underruns\r\n");
}


/* ================================================================== */
/*  子测试 1: 交互式 I2S 播放 + 音量控制                                */
/* ================================================================== */

static void TestSaiInteractive(void)
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
    PRINT("  s     status          0   quit\r\n");
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

    /* PCMMIX 音量: 由菜单命令 pN 交互控制 */
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXA_VOL, pcmmix_vol);
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXB_VOL, pcmmix_vol);

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

            if (cmd[0] == '0')
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


/* ================================================================== */
/*  子测试 2: 播放 Happy Birthday 旋律 (~11 秒)                        */
/* ================================================================== */

static void TestSaiMelody(void)
{
    SaiRegs_t *sai = SAI_BUS;
    SaiCfg_t   cfg;
    Cs42l51_t  codec;

    PRINT("\r\n===== SAI Melody: Happy Birthday =====\r\n");
    PRINT("Playing via SAI2 I2S 16-bit 8kHz...\r\n");

    /* ---- 硬件初始化 ---- */
    SaiTest_GpioInit();
    SaiTest_ClockCfg();
    I2cCfg(I2C5);

    /* SAI I2S master TX */
    SaiCfgFillI2S(&cfg, 64000000U, SAMPLE_RATE, SAI_DS_16,
                  SAI_MODE_MASTER_TX, 1);
    SaiInit(sai, SAI_BLOCK, &cfg);
    SaiStart(sai, SAI_BLOCK);

    /* Codec 初始化 + 默认音量 */
    codec.i2c        = CODEC_I2C;
    codec.slave_addr = CODEC_ADDR;
    codec.rst_port   = GPIO_C;
    codec.rst_pin    = 0;
    Cs42l51Init(&codec);
    Cs42l51SetVolume(&codec, 8);              /* AOUT = +4dB */
    /* PCMMIXA/B = -48dB: 旋律音量适中 */
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXA_VOL, 32);
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXB_VOL, 32);

    /* ---- 播放旋律 ---- */
    SaiTestPlayMelody(sai);

    /* ---- 清理退出 ---- */
    SaiStop(sai, SAI_BLOCK);
    Cs42l51Deinit(&codec);
    PRINT("Melody done.\r\n");
}


/* ================================================================== */
/*  子测试 4: CS42L51 内部自回环 (ADC→DAC, 无需 Block B)              */
/* ================================================================== */

static void TestSaiRecordLoopback(void)
{
    SaiRegs_t *sai = SAI_BUS;
    SaiCfg_t   cfg;
    Cs42l51_t  codec;
    Frame_t    fr;
    uint8_t    aout_vol = 8;       /* +4dB */
    char       dbuf[20];
    char      *cmd;
    int        len;

    PRINT("\r\n===== CS42L51 Self-Loopback Test =====\r\n");
    PRINT("  DATA_SEL=ADC: MIC -> ADC -> DAC -> HP\r\n");
    PRINT("  Block A runs to provide clocks (SDIN ignored)\r\n");
    PRINT("  Headset mic -> HP (echo)\r\n");
    PRINT("  +/-  AOUT +/-2dB    0  quit\r\n\r\n");

    /* ---- GPIO + Clock + I2C ---- */
    SaiTest_GpioInit();
    SaiTest_ClockCfg();
    I2cCfg(I2C5);

    /* ---- SAI Block A: Master TX (仅用来产生 MCLK/SCK/FS) ---- */
    SaiCfgFillI2S(&cfg, 64000000U, SAMPLE_RATE, SAI_DS_16,
                  SAI_MODE_MASTER_TX, 1);
    cfg.nbslot = 1; cfg.slotsz = SAI_SLOTSZ_32; cfg.sloten = (1u << 0);
    SaiInit(sai, SAI_BLOCK_A, &cfg);
    {
        uint32_t z = 0;
        SaiStart(sai, SAI_BLOCK_A);
        SaiSendBlocking(sai, SAI_BLOCK_A, &z, 1);
    }

    /* ---- CS42L51: 初始化 + MIC 配置 + ADC 自回环 ---- */
    codec.i2c        = CODEC_I2C;
    codec.slave_addr = CODEC_ADDR;
    codec.rst_port   = GPIO_C;
    codec.rst_pin    = 0;
    Cs42l51Init(&codec);

    /* MIC 控制: MIC2 输入, +30dB 增益 */
    Cs42l51RegWrite(&codec, CS42L51_REG_MIC_CTL,
                    CS42L51_MIC_GAIN_30dB);

    /* ADC 输入选择: AIN A/B 均选 MIC2 (CKR90IS 耳麦) */
    Cs42l51RegWrite(&codec, CS42L51_REG_ADC_INPUT_SEL,
                    CS42L51_AINA_SEL(CS42L51_IN_SEL_MIC2)
                  | CS42L51_AINB_SEL(CS42L51_IN_SEL_MIC2));

    /* DAC 输出: HP 增益 1.0× (替代默认 0.6×) */
    Cs42l51RegWrite(&codec, CS42L51_REG_DAC_OUT_CTL,
                    CS42L51_HP_GAIN_1p0);

    /* DAC 控制: 数据源 = ADC (内部自回环, 忽略 SDIN) */
    Cs42l51RegWrite(&codec, CS42L51_REG_DAC_CTL,
                    CS42L51_DATA_SEL(CS42L51_DATA_SEL_ADC));
    Cs42l51SetVolume(&codec, aout_vol);  /* AOUT = +4dB */

    /* PCMMIXA/B = 0dB (MUTE=0, VOL=0): PCM 路径虽未用, 保持非静音 */
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXA_VOL, 0);
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXB_VOL, 0);

    PRINT("Loopback active. AOUT=+4dB  MIC=+30dB\r\n");

    /* ---- 交互循环 ---- */
    while (1)
    {
        while (FramePoll(&fr))
        {
            if (fr.cmd == FRAME_CMD_BACK
                || (fr.data_len >= 1 && fr.data[0] == '0'))
                goto exit_rec;

            len = fr.data_len;
            cmd = (char *)fr.data;
            cmd[len] = '\0';

            if (cmd[0] == '+')
            {
                if (aout_vol + VOL_STEP <= 0x18)
                    aout_vol += VOL_STEP;
                Cs42l51SetVolume(&codec, aout_vol);
                PRINT("AOUT="); PrintHex8(aout_vol);
                PrintDec(dbuf, aout_vol); PRINT(" hdB\r\n");
            }
            else if (cmd[0] == '-')
            {
                if (aout_vol >= VOL_STEP)
                    aout_vol -= VOL_STEP;
                Cs42l51SetVolume(&codec, aout_vol);
                PRINT("AOUT="); PrintHex8(aout_vol);
                PrintDec(dbuf, aout_vol); PRINT(" hdB\r\n");
            }
        }
        IwdgKickDog(IWDG2);
    }

exit_rec:
    Cs42l51Deinit(&codec);
    SaiStop(sai, SAI_BLOCK_A);
    PRINT("Loopback stopped.\r\n");
}


/* ================================================================== */
/*  子测试 5: SAI 数字回环 (Block A RX -> Block B TX)                  */
/* ================================================================== */

static void TestSaiDigitalLoopback(void)
{
    SaiRegs_t *sai = SAI_BUS;
    SaiCfg_t   cfg;
    Cs42l51_t  codec;
    Frame_t    fr;
    uint8_t    aout_vol = 8;
    char       dbuf[20];
    char      *cmd;
    int        len;

    PRINT("\r\n===== SAI Digital Loopback Test =====\r\n");
    PRINT("  Block A: Master RX (ADC from SDOUT)\r\n");
    PRINT("  Block B: Slave  TX (DAC to SDIN)\r\n");
    PRINT("  MCU reads A -> writes B\r\n");
    PRINT("  +/-  AOUT +/-2dB    0  quit\r\n\r\n");

    /* ---- GPIO + Clock + I2C ---- */
    SaiTest_GpioInit();
    SaiTest_ClockCfg();
    I2cCfg(I2C5);

    /* ---- Block A: Master RX ---- */
    SaiCfgFillI2S(&cfg, 64000000U, SAMPLE_RATE, SAI_DS_16,
                  SAI_MODE_MASTER_RX, 1);
    cfg.nbslot = 1; cfg.slotsz = SAI_SLOTSZ_32; cfg.sloten = (1u << 0);
    SaiInit(sai, SAI_BLOCK_A, &cfg);

    /* ---- Block B: Slave TX, 同步到 Block A ---- */
    SaiCfgFillI2S(&cfg, 64000000U, SAMPLE_RATE, SAI_DS_16,
                  SAI_MODE_SLAVE_TX, 0);
    cfg.nbslot = 1; cfg.slotsz = SAI_SLOTSZ_32; cfg.sloten = (1u << 0);
    cfg.syncen = 1;     /* 同步到 Block A (sync internally) */
    SaiInit(sai, SAI_BLOCK_B, &cfg);

    /* ---- CS42L51: SDIN -> DAC ---- */
    codec.i2c        = CODEC_I2C;
    codec.slave_addr = CODEC_ADDR;
    codec.rst_port   = GPIO_C;
    codec.rst_pin    = 0;
    Cs42l51Init(&codec);

    /* MIC 控制: MIC2 输入, +30dB 增益 (CKR90IS 耳麦) */
    Cs42l51RegWrite(&codec, CS42L51_REG_MIC_CTL,
                    CS42L51_MIC_GAIN_30dB);

    /* ADC 输入选择: AIN A/B 均选 MIC2 (ADC 直通到 SDOUT) */
    Cs42l51RegWrite(&codec, CS42L51_REG_ADC_INPUT_SEL,
                    CS42L51_AINA_SEL(CS42L51_IN_SEL_MIC2)
                  | CS42L51_AINB_SEL(CS42L51_IN_SEL_MIC2));

    /* DAC 输出: HP 增益 1.0× */
    Cs42l51RegWrite(&codec, CS42L51_REG_DAC_OUT_CTL,
                    CS42L51_HP_GAIN_1p0);

    /* DAC 控制: 数据源 = DSP (PCM→DAC, 使能 AOUT/PCMMIX 音量控制) */
    Cs42l51RegWrite(&codec, CS42L51_REG_DAC_CTL,
                    CS42L51_DATA_SEL(CS42L51_DATA_SEL_DSP));
    Cs42l51SetVolume(&codec, aout_vol);

    /* PCMMIXA/B = 0dB: PCM 输入未经衰减送 DAC */
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXA_VOL, 0);
    Cs42l51RegWrite(&codec, CS42L51_REG_PCMMIXB_VOL, 0);

    /* ADCMIXA/B = +12dB: ADC 混音增益, 提升 MIC 音量 */
    Cs42l51RegWrite(&codec, CS42L51_REG_ADCMIXA_VOL,
                    CS42L51_ADCMIX_VOL_12dB);
    Cs42l51RegWrite(&codec, CS42L51_REG_ADCMIXB_VOL,
                    CS42L51_ADCMIX_VOL_12dB);

    /* 先启动 B (从), 预填一帧; 再启动 A (主) */
    {
        uint32_t z = 0;
        SaiSendBlocking(sai, SAI_BLOCK_B, &z, 1);
    }
    SaiStart(sai, SAI_BLOCK_B);
    SaiStart(sai, SAI_BLOCK_A);

    PRINT("Loopback active.\r\n");

    /* ---- 主循环: Read A → Write B ---- */
    while (1)
    {
        uint32_t sample;
        SaiRecvBlocking(sai, SAI_BLOCK_A, &sample, 1);
        SaiSendBlocking(sai, SAI_BLOCK_B, &sample, 1);

        while (FramePoll(&fr))
        {
            if (fr.cmd == FRAME_CMD_BACK
                || (fr.data_len >= 1 && fr.data[0] == '0'))
                goto exit_rec;
            len = fr.data_len;
            cmd = (char *)fr.data;
            cmd[len] = '\0';
            if (cmd[0] == '+')
            {
                if (aout_vol + VOL_STEP <= 0x18) aout_vol += VOL_STEP;
                Cs42l51SetVolume(&codec, aout_vol);
                PRINT("AOUT="); PrintHex8(aout_vol);
                PrintDec(dbuf, aout_vol); PRINT(" hdB\r\n");
            }
            else if (cmd[0] == '-')
            {
                if (aout_vol >= VOL_STEP) aout_vol -= VOL_STEP;
                Cs42l51SetVolume(&codec, aout_vol);
                PRINT("AOUT="); PrintHex8(aout_vol);
                PrintDec(dbuf, aout_vol); PRINT(" hdB\r\n");
            }
        }
        IwdgKickDog(IWDG2);
    }

exit_rec:
    Cs42l51Deinit(&codec);
    SaiStop(sai, SAI_BLOCK_B);
    SaiStop(sai, SAI_BLOCK_A);
    PRINT("Loopback stopped.\r\n");
}


/* ================================================================== */
/*  子测试菜单 + 入口                                                   */
/* ================================================================== */

static const MenuEntry_t g_sai_menu[] = {
    { "1", "Interactive I2S play + volume control", TestSaiInteractive },
    { "2", "Happy Birthday melody",                  TestSaiMelody },
    { "3", "PCM Stream (DMA ping-pong 460800bd)",    TestSaiPcmStream },
    { "4", "Self-loopback (MIC->ADC->DAC->HP)",     TestSaiRecordLoopback },
    { "5", "Digital loopback (A RX -> B TX)",        TestSaiDigitalLoopback },
};

void SaiTest(void)
{
    RunSubMenu("SAI + CS42L51 Audio Test",
               g_sai_menu, sizeof(g_sai_menu) / sizeof(g_sai_menu[0]));
}
