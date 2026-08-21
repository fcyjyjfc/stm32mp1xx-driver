/*
 * display_test.c — LTDC + SiI9022A HDMI 显示测试
 */

#include "stm32mp1xx_ltdc.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_i2c.h"
#include "stm32mp1xx_iwdg.h"
#include "stm32mp1xx_btimer.h"
#include "font_gbk16.h"
#include "sii9022a.h"
#include "common_test.h"
#include <string.h>


/* ================================================================== */
/*  硬件配置                                                          */
/* ================================================================== */

#define DISPLAY_I2C        I2C5
#define SII9022A_ADDR      0x72

/* 分辨率 720p60 */
#define TIM_HSW            40
#define TIM_HBP            220
#define TIM_HFP            110
#define TIM_ACTIVE_W       1280
#define TIM_VSW            5
#define TIM_VBP            20
#define TIM_VFP            5
#define TIM_ACTIVE_H       720


/* ================================================================== */
/*  帧缓冲 (RGB565, ~1.8MB)                                           */
/* ================================================================== */

static uint16_t g_fb[TIM_ACTIVE_W * TIM_ACTIVE_H] __attribute__((aligned(32)));

/* ================================================================== */
/*  图片流帧缓冲 (RGB888, ~2.7MB)                                     */
/* ================================================================== */

static uint8_t g_img_fb[TIM_ACTIVE_W * TIM_ACTIVE_H * 3] __attribute__((aligned(32)));

/* ================================================================== */
/*  板载轮播帧缓冲 (6 张 RGB888, 共 ~16.5MB)                          */
/* ================================================================== */

#define SLIDE_COUNT  6

static uint8_t g_slide_fb[SLIDE_COUNT][TIM_ACTIVE_W * TIM_ACTIVE_H * 3]
    __attribute__((aligned(32)));


/* ================================================================== */
/*  时钟 & GPIO                                                        */
/* ================================================================== */

static void DisplayTest_ClockCfg(void)
{
    RccPll34Cfg_t pll4;

    /* PLL4 source: HSE */
    RccPll4SrcSelect(RCC_PLLSRC_HSE);

    /* PLL4: 24MHz HSE → 74.25MHz (720p60 pixel clock)
     *   Fref = 24 / (5+1) = 4MHz
     *   DIVN=147, fracv=4096 → ×148.5 → VCO=594MHz (规格 400-800MHz 内)
     *   DIVQ=7 → /8 → PLL4_Q = 74.25MHz
     *   LTDC ker_ck 来自 PLL4_Q (RM0436 时钟树), 不是 PLL4_P */
    pll4.divm   = 5;
    pll4.divn   = 147;
    pll4.fracv  = 4096;
    pll4.divp   = 7;
    pll4.en_p   = 0;
    pll4.divq   = 7;
    pll4.en_q   = 1;
    pll4.divr   = 0;
    pll4.en_r   = 0;
    pll4.ifrge  = 0;
    RccPll4Init(&pll4);

    RCC->APB4RSTCLRR = (1 << 0);
    /* LTDC 时钟 + 复位释放 */
    RCC->MP_APB4ENSETR |= (1 << 0);
    
    // i2c5时钟使能
    RCC->MP_APB1ENSETR |= (1 << 24);
}


static void GpioCfgAf(volatile GpioRegs_t *const gpio_reg, uint32_t pin, uint32_t af, uint32_t speed)
{
    GpioMode(gpio_reg, pin, GPIO_MODER_AF);
    GpioAf(gpio_reg, pin, af);
    GpioOtype(gpio_reg, pin, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(gpio_reg, pin, speed);
}

static void DisplayTest_GpioInit(void)
{
    uint32_t i;

    /* GPIO 时钟使能 */
    RCC->MP_AHB4ENSETR |= (1 << 0)  /* GPIOA (I2C5, HDMI control) */
                        | (1 << 8)  /* GPIOI (sync, clock, R0) */
                        | (1 << 9)  /* GPIOJ (R/G/B data) */
                        | (1 << 10);/* GPIOK (G/B data, DEN) */

    /* ===== LTDC 引脚 (全部 AF14) ===== */

    /* LCD_R[7:1]: PJ[6:0] */
    for (i = 0; i <= 6; i++)
        GpioCfgAf(GPIO_J, i, 14, GPIO_OSPEED_HI);
    /* LCD_R0: PI15 */
    GpioCfgAf(GPIO_I, 15, 14, GPIO_OSPEED_HI);

    /* LCD_G[4:0]: PJ[11:7] */
    for (i = 7; i <= 11; i++)
        GpioCfgAf(GPIO_J, i, 14, GPIO_OSPEED_HI);
    /* LCD_G[7:5]: PK[2:0] */
    for (i = 0; i <= 2; i++)
        GpioCfgAf(GPIO_K, i, 14, GPIO_OSPEED_HI);

    /* LCD_B[3:0]: PJ[15:12] */
    for (i = 12; i <= 15; i++)
        GpioCfgAf(GPIO_J, i, 14, GPIO_OSPEED_HI);
    /* LCD_B[7:4]: PK[6:3] */
    for (i = 3; i <= 6; i++)
        GpioCfgAf(GPIO_K, i, 14, GPIO_OSPEED_HI);

    /* 同步信号 */
    GpioCfgAf(GPIO_I, 12, 14, GPIO_OSPEED_HI); /* LCD_HSYNC */
    GpioCfgAf(GPIO_I, 13, 14, GPIO_OSPEED_HI); /* LCD_VSYNC */
    GpioCfgAf(GPIO_I, 14, 14, GPIO_OSPEED_HI); /* LCD_PCLK */

    /* LCD_DEN: PK7 */
    GpioCfgAf(GPIO_K, 7, 14, GPIO_OSPEED_HI);

    /* ===== I2C5 (CS42L51 + SiI9022A) ===== */
    /* PA11=SCL(AF4), PA12=SDA(AF4), open-drain */
    GpioMode(GPIO_A, 11, GPIO_MODER_AF);
    GpioAf(GPIO_A, 11, 4);
    GpioOtype(GPIO_A, 11, GPIO_OTYPE_OD);
    GpioOspeed(GPIO_A, 11, GPIO_OSPEED_LOW);

    GpioMode(GPIO_A, 12, GPIO_MODER_AF);
    GpioAf(GPIO_A, 12, 4);
    GpioOtype(GPIO_A, 12, GPIO_OTYPE_OD);
    GpioOspeed(GPIO_A, 12, GPIO_OSPEED_LOW);

    /* ===== HDMI 控制引脚 ===== */
    /* HDMI_RST: PA13, 输出低 (复位有效) */
    GpioMode(GPIO_A, 13, GPIO_MODER_OUTPUT);
    GpioOtype(GPIO_A, 13, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_A, 13, GPIO_OSPEED_LOW);
    GpioOutLow(GPIO_A, 13);

    /* HDMI_INT: PA14, 输入 (热插拔检测) */
    GpioMode(GPIO_A, 14, GPIO_MODER_INPUT);
    GpioPullUpDown(GPIO_A, 14, GPIO_PUPDR_PULL_DOWN);

    /* HDMI_CEC: PA15, AF4 (可选) */
    GpioMode(GPIO_A, 15, GPIO_MODER_AF);
    GpioAf(GPIO_A, 15, 4);
    GpioOtype(GPIO_A, 15, GPIO_OTYPE_OD);
    GpioOspeed(GPIO_A, 15, GPIO_OSPEED_LOW);
}


/* ---- 公共初始化: 时钟 + GPIO + I2C + SiI9022A + LTDC 时序 ----
 * 返回 0=成功; 非 0=SiI9022A 未检测到 */
static int DisplayHdmiInit(LtdcTimingConfig_t *tim, Sii9022a_t *sii)
{
    uint32_t i;
    int      ret;

    PRINT("Clock init...\r\n");
    DisplayTest_ClockCfg();

    PRINT("GPIO init...\r\n");
    DisplayTest_GpioInit();

    /* ---- I2C5 初始化 (与 CS42L51 共用总线) ---- */
    I2cCfg(I2C5);

    /* ---- SiI9022A 硬件复位 (PA13, 低有效) ---- */
    PRINT("SiI9022A reset...\r\n");
    GpioOutLow(GPIO_A, 13);
    for (i = 0; i < 500000; i++);
    GpioOutHi(GPIO_A, 13);
    for (i = 0; i < 500000; i++);

    /* ---- SiI9022A 初始化 ---- */
    PRINT("SiI9022A init...\r\n");
    sii->i2c  = DISPLAY_I2C;
    sii->addr = SII9022A_ADDR;

    ret = SiiDetect(sii);
    if (ret != 0) {
        PRINT("  SiI9022A not detected!\r\n");
        return -1;
    }
    PRINT("  SiI9022A detected.\r\n");

    ret = SiiInit(sii);
    if (ret != 0)
        PRINT("  SiiInit failed!\r\n");

    /* ---- LTDC 时序配置 ---- */
    PRINT("LTDC init...\r\n");
    tim->hsw        = TIM_HSW;
    tim->hbp        = TIM_HBP;
    tim->hfp        = TIM_HFP;
    tim->active_w   = TIM_ACTIVE_W;
    tim->vsw        = TIM_VSW;
    tim->vbp        = TIM_VBP;
    tim->vfp        = TIM_VFP;
    tim->active_h   = TIM_ACTIVE_H;
    /* 720p60: HSYNC+、VSYNC+、DE+、PCPOL=1 (RM0436: active high =
     * 上升沿有效), 与 SiI9022A 上升沿锁存 (0x08[4]=1, 默认) 匹配。
     * 注意 DEPOL 语义: RM0436 该位是 "Not data enable polarity",
     * 0 = Not DE 低有效 (即 DE 高有效), 1 = DE 被反相! */
    tim->hspol      = 1;
    tim->vspol      = 1;
    tim->depol      = 0;
    tim->pcpol      = 1;
    tim->bg_rgb888  = 0x000000;
    tim->dither_en  = 0;

    LtdcInit(LTDC, tim);

    return 0;
}


/* ---- 视频模式启动: 模式时序 + 同步极性 + AVI + D0 + TMDS 使能 ----
 * 各显示测试共用 (显示时序已由 DisplayHdmiInit 配置) */
static void DisplayHdmiVideoStart(Sii9022a_t *sii, const SiiVideoMode_t *vm)
{
    SiiSetVideoMode(sii, vm);

    /* video mode 写后留空闲时间, 保证随后第一笔写 (SyncPolarity)
     * 落在空闲窗口里 (后续每笔写之间由设备层自行 settle)。 */
    {
        uint32_t t;
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
    }

    SiiSetSyncPolarity(sii, 0, 0);

    /* AVI InfoFrame */
    {
        uint8_t avi[13] = { 0 };
        avi[0] = 0x00;  /* Y=RGB, A0=active */
        avi[1] = 0x00;  /* C=No data, M=No data */
        avi[2] = 0x00;  /* ITC=0 */
        avi[3] = 4;     /* VIC=4 = 720p60 */
        SiiSetAVIInfoFrame(sii, avi, 13);
    }

    /* ---- 清除 HPD 事件 + 重写 D0 + 使能 TMDS + HDMI 模式 ----
     * 注意: TPI 忙窗口内背靠背写会被静默丢弃 (回读旧值)。实测紧跟 D0
     * 的 SYS_CTRL 写不生效, 需留空闲窗口再写并回读确认。 */
    SiiClearInterrupt(sii, 0x03);
    SiiRegWrite(sii, SII_TPI_POWER_STATE, SII_POWER_D0);

    /* D0 切换后留空闲窗口, 再写 SYS_CTRL 使能 TMDS 输出 */
    {
        uint32_t t;

        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        SiiRegWrite(sii, SII_TPI_SYS_CTRL, 0x01);
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        if (SiiRegRead(sii, SII_TPI_SYS_CTRL) != 0x01)
            PRINT("  SYS_CTRL TMDS enable failed (dropped write)\r\n");
    }
}


/* ================================================================== */
/*  测试主入口                                                         */
/* ================================================================== */

static void TestDisplayHdmi(void)
{
    LtdcTimingConfig_t tim;
    LtdcLayerConfig_t  lay;
    Sii9022a_t         sii;
    SiiVideoMode_t     vm;

    PRINT("\r\n===== LTDC + SiI9022A HDMI Display Test =====\r\n");

    if (DisplayHdmiInit(&tim, &sii) != 0)
        return;

    /* ---- 5. 填充帧缓冲: 彩虹 7 色竖条 + 白 ---- */
    {
        uint32_t bar_w = TIM_ACTIVE_W / 8;
        uint32_t x, y;
        static const uint16_t colors[8] = {
            0xF800, /* 赤 */
            0xFD20, /* 橙 */
            0xFFE0, /* 黄 */
            0x07E0, /* 绿 */
            0x07FF, /* 青 */
            0x001F, /* 蓝 */
            0xF81F, /* 紫 */
            0xFFFF, /* 白 */
        };
        for (y = 0; y < TIM_ACTIVE_H; y++) {
            for (x = 0; x < TIM_ACTIVE_W; x++) {
                uint32_t bar = x / bar_w;
                if (bar > 7) bar = 7;
                g_fb[y * TIM_ACTIVE_W + x] = colors[bar];
            }
        }
    }

    /* ---- 6. LTDC 图层配置 ---- */
    lay.enable       = 1;
    lay.win_x1       = 0;
    lay.win_y1       = 0;
    lay.win_x2       = TIM_ACTIVE_W - 1;
    lay.win_y2       = TIM_ACTIVE_H - 1;
    lay.pixel_fmt    = LTDC_PF_RGB565;
    lay.fb_addr      = (uint32_t)g_fb;
    lay.pitch        = TIM_ACTIVE_W * 2;
    lay.width        = TIM_ACTIVE_W;
    lay.height       = TIM_ACTIVE_H;
    lay.clut_en      = 0;
    lay.const_alpha  = 255;
    lay.def_color    = 0;
    lay.blend_f1     = LTDC_BF1_CONST_ALPHA;
    lay.blend_f2     = LTDC_BF2_1_MINUS_CONST;
    lay.color_key_en = 0;
    lay.color_key    = 0;

    LtdcLayerConfig(LTDC, 1, &lay);
    LtdcReloadImmediate(LTDC);
    LtdcEnable(LTDC);

    /* ---- 7. SiI9022A 视频模式 ---- */
    PRINT("SiI9022A video mode...\r\n");
    vm.pixel_clock = 74250000;
    vm.htotal      = TIM_HSW + TIM_HBP + TIM_ACTIVE_W + TIM_HFP;
    vm.vtotal      = TIM_VSW + TIM_VBP + TIM_ACTIVE_H + TIM_VFP;
    vm.vfreq       = 60;

    SiiSetVideoMode(&sii, &vm);

    /* video mode 写后留空闲时间, 保证随后第一笔写 (SyncPolarity)
     * 落在空闲窗口里 (后续每笔写之间由设备层自行 settle)。 */
    {
        uint32_t t;
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
    }

    SiiSetSyncPolarity(&sii, 0, 0);

    /* 外部 DE (PK7) 直连, 不使用内部 DE 生成器:
     * 实测画面正常 — 之前"外部 DE 故障"的根因是 LTDC GCR DEPOL=1
     * 把 DE 反相 (RM0436: DEPOL 是 "Not data enable polarity"),
     * depol 改为 0 后外部 DE 通路完好, DE gen 方案废弃。 */

    /* AVI InfoFrame */
    {
        uint8_t avi[13] = { 0 };
        avi[0] = 0x00;  /* Y=RGB, A0=active */
        avi[1] = 0x00;  /* C=No data, M=No data */
        avi[2] = 0x00;  /* ITC=0 */
        avi[3] = 4;     /* VIC=4 = 720p60 */
        SiiSetAVIInfoFrame(&sii, avi, 13);
    }

    /* ---- 8. 清除 HPD 事件 + 重写 D0 + 使能 TMDS + HDMI 模式 ----
     * 注意: TPI 忙窗口内背靠背写会被静默丢弃 (回读旧值)。实测紧跟 D0
     * 的 SYS_CTRL 写不生效, 需留空闲窗口再写并回读确认。 */
    SiiClearInterrupt(&sii, 0x03);
    SiiRegWrite(&sii, SII_TPI_POWER_STATE, SII_POWER_D0);

    /* D0 切换后留空闲窗口, 再写 SYS_CTRL 使能 TMDS 输出 */
    {
        uint32_t t;

        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        SiiRegWrite(&sii, SII_TPI_SYS_CTRL, 0x01);
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        if (SiiRegRead(&sii, SII_TPI_SYS_CTRL) != 0x01)
            PRINT("  SYS_CTRL TMDS enable failed (dropped write)\r\n");
    }

    /* 注: EDID 读取未实现 — DDC 授权成功后 0x50 事务会卡在 I2C 驱动的
     * 无超时等待 (stm32mp1xx_i2c.c), 10 秒后 IWDG2 看门狗复位整板 */

    PRINT("Display active.\r\n");
    PRINT("Press any key to stop...\r\n");

    /* ---- 9. 等待按键退出 ---- */
    {
        char buf[4];
        ReadLine(buf, sizeof(buf));
    }

    /* ---- 10. 清理 ---- */
    SiiTmdsOutput(&sii, 0);
    LtdcDisable(LTDC);
    PRINT("Display stopped.\r\n");
}


/* ================================================================== */
/*  UART 图片流 — PC 发 BMP24 原始字节, LTDC RGB888 直显 (零转换)     */
/*                                                                    */
/*   使用方法 (PC 端):                                               */
/*     python tools/img_sender.py <图片> COMx [--wait]               */
/*     图片任意格式 (需 Pillow, 脚本自动转 BGR; 无 Pillow 仅 BMP24)  */
/*     非 1280x720 自动缩放                                         */
/*     LTDC RGB888 内存序 BGR, 固件零转换                            */
/*                                                                    */
/*   命令 (帧 data 字段):                                            */
/*     0 / BACK  — 停止接收, 返回菜单                                */
/*                                                                    */
/*   注意:                                                            */
/*     --wait 默认 3s: 本测试初始化 (SiI9022A + LTDC) 期间 DMA RX    */
/*     256B 环形缓冲会溢出, 必须等初始化完成后才开始发数据           */
/* ================================================================== */

#define IMG_BAUD        460800

static void TestDisplayUartImage(void)
{
    LtdcTimingConfig_t tim;
    LtdcLayerConfig_t  lay;
    Sii9022a_t         sii;
    SiiVideoMode_t     vm;
    Frame_t            fr;
    const uint32_t     img_size = TIM_ACTIVE_W * TIM_ACTIVE_H * 3;
    uint32_t           off = 0;

    PRINT("\r\n===== UART Image Display (RGB888) =====\r\n");
    PRINT("  UART switched to 460800 baud\r\n");
    PRINT("  Run img_sender.py on PC to stream image\r\n");
    PRINT("  Press 0 to stop\r\n\r\n");

    if (DisplayHdmiInit(&tim, &sii) != 0)
        return;

    /* ---- 图层 RGB888 全屏, 始终使能: 收多少显示多少 ----
     * 内存序 BGR (docs/ltdc_overview.md: RGB888 @+0=B @+1=G @+2=R),
     * PC 端脚本负责 BGR 字节序, 固件零转换 */
    lay.enable       = 1;
    lay.win_x1       = 0;
    lay.win_y1       = 0;
    lay.win_x2       = TIM_ACTIVE_W - 1;
    lay.win_y2       = TIM_ACTIVE_H - 1;
    lay.pixel_fmt    = LTDC_PF_RGB888;
    lay.fb_addr      = (uint32_t)g_img_fb;
    lay.pitch        = TIM_ACTIVE_W * 3;
    lay.width        = TIM_ACTIVE_W;
    lay.height       = TIM_ACTIVE_H;
    lay.clut_en      = 0;
    lay.const_alpha  = 255;
    lay.def_color    = 0;
    lay.blend_f1     = LTDC_BF1_CONST_ALPHA;
    lay.blend_f2     = LTDC_BF2_1_MINUS_CONST;
    lay.color_key_en = 0;
    lay.color_key    = 0;

    LtdcLayerConfig(LTDC, 1, &lay);
    LtdcReloadImmediate(LTDC);

    /* 黑屏打底: 图片随接收逐行显现, 方便观察刷新过程 */
    memset(g_img_fb, 0, sizeof(g_img_fb));

    LtdcEnable(LTDC);

    /* ---- 视频模式 + AVI + D0 + TMDS 使能 (同 HDMI 测试) ---- */
    vm.pixel_clock = 74250000;
    vm.htotal      = TIM_HSW + TIM_HBP + TIM_ACTIVE_W + TIM_HFP;
    vm.vtotal      = TIM_VSW + TIM_VBP + TIM_ACTIVE_H + TIM_VFP;
    vm.vfreq       = 60;

    SiiSetVideoMode(&sii, &vm);

    {
        uint32_t t;
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
    }

    SiiSetSyncPolarity(&sii, 0, 0);

    {
        uint8_t avi[13] = { 0 };
        avi[0] = 0x00;
        avi[1] = 0x00;
        avi[2] = 0x00;
        avi[3] = 4;     /* VIC=4 = 720p60 */
        SiiSetAVIInfoFrame(&sii, avi, 13);
    }

    SiiClearInterrupt(&sii, 0x03);
    SiiRegWrite(&sii, SII_TPI_POWER_STATE, SII_POWER_D0);

    {
        uint32_t t;
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        SiiRegWrite(&sii, SII_TPI_SYS_CTRL, 0x01);
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        if (SiiRegRead(&sii, SII_TPI_SYS_CTRL) != 0x01)
            PRINT("  SYS_CTRL TMDS enable failed (dropped write)\r\n");
    }

    /* ---- 切波特率, 开始接收 ---- */
    UsartSetBaud(USART4, 64000000, IMG_BAUD);

    PRINT("Receiving image...\r\n");
    while (1)
    {
        IwdgKickDog(IWDG2);
        if (!FramePoll(&fr))
            continue;

        if (fr.cmd == FRAME_CMD_IMG_DATA)
        {
            if (off + fr.data_len <= img_size)
            {
                memcpy(&g_img_fb[off], fr.data, fr.data_len);
                off += fr.data_len;
                if (off >= img_size)
                    PRINT("Image complete.\r\n");
            }
        }
        else if (fr.cmd == FRAME_CMD_BACK
                 || (fr.data_len >= 1 && fr.data[0] == '0'))
            break;
    }

    /* ---- 清理 ---- */
    SiiTmdsOutput(&sii, 0);
    LtdcDisable(LTDC);
    PRINT("Image display stopped.\r\n");
}


/* ================================================================== */
/*  板载图片轮播 — 固件生成 6 张图案, 500ms 交替显示 (模拟视频)      */
/*                                                                    */
/*   图案: 彩虹条 / 对角渐变 / 棋盘格 / 纯色 / 几何 / 同心圆环       */
/*   切换: VBlank 重载避免撕裂                                       */
/*   计时: TIM6 自由运行 (PSC=63999 → CNT 1kHz), 16 位差值 < 半周期  */
/*     (不用 STGEN: 系统定时器在安全世界, 非安全世界不可访问)        */
/*   命令 (帧 data 字段):                                            */
/*     0 / BACK  — 停止播放, 返回菜单                                */
/* ================================================================== */

#define SLIDE_INTERVAL_MS  500

/* 整数平方根 (避免依赖 libm) */
static uint32_t SlideIsqrt(uint32_t n)
{
    uint32_t r = 0;
    uint32_t b = 0x80000000;
    while (b > n) b >>= 2;
    while (b)
    {
        if (n >= r + b)
        {
            n -= r + b;
            r = (r >> 1) + b;
        }
        else
        {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

/* 彩虹 8 竖条 */
static void SlideGenBars(uint8_t *fb)
{
    static const uint32_t colors[8] = {
        0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00,
        0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF,
    };
    uint32_t bar_w = TIM_ACTIVE_W / 8;
    uint32_t x, y;

    for (y = 0; y < TIM_ACTIVE_H; y++)
    {
        /* 24MHz 低速下整图生成会超 IWDG2 10s 窗口, 每行喂狗 */
        IwdgKickDog(IWDG2);
        for (x = 0; x < TIM_ACTIVE_W; x++)
        {
            uint32_t bar = x / bar_w;
            uint32_t c;
            uint8_t *p;
            if (bar > 7) bar = 7;
            c = colors[bar];
            p = &fb[(y * TIM_ACTIVE_W + x) * 3];
            p[0] = (c >> 16) & 0xFF;  /* B */
            p[1] = (c >> 8)  & 0xFF;  /* G */
            p[2] = c & 0xFF;          /* R */
        }
    }
}

/* 对角渐变: R=255, G 随 y, B 随 x */
static void SlideGenGradient(uint8_t *fb)
{
    uint32_t x, y;

    for (y = 0; y < TIM_ACTIVE_H; y++)
    {
        /* 24MHz 低速下整图生成会超 IWDG2 10s 窗口, 每行喂狗 */
        IwdgKickDog(IWDG2);
        for (x = 0; x < TIM_ACTIVE_W; x++)
        {
            uint8_t *p = &fb[(y * TIM_ACTIVE_W + x) * 3];
            p[0] = (uint8_t)(x * 255 / (TIM_ACTIVE_W - 1));       /* B */
            p[1] = (uint8_t)(y * 255 / (TIM_ACTIVE_H - 1));       /* G */
            p[2] = 255;                                           /* R */
        }
    }
}

/* 黑白棋盘格 (80px 格) */
static void SlideGenChecker(uint8_t *fb)
{
    const uint32_t cell = 80;
    uint32_t x, y;

    for (y = 0; y < TIM_ACTIVE_H; y++)
    {
        /* 24MHz 低速下整图生成会超 IWDG2 10s 窗口, 每行喂狗 */
        IwdgKickDog(IWDG2);
        for (x = 0; x < TIM_ACTIVE_W; x++)
        {
            uint8_t v = (((x / cell) + (y / cell)) & 1) ? 255 : 0;
            uint8_t *p = &fb[(y * TIM_ACTIVE_W + x) * 3];
            p[0] = v;
            p[1] = v;
            p[2] = v;
        }
    }
}

/* 纯色 */
static void SlideGenSolid(uint8_t *fb, uint32_t rgb)
{
    uint8_t b = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8)  & 0xFF;
    uint8_t r = rgb & 0xFF;
    uint32_t i;

    for (i = 0; i < (uint32_t)TIM_ACTIVE_W * TIM_ACTIVE_H * 3; i += 3)
    {
        fb[i]     = b;
        fb[i + 1] = g;
        fb[i + 2] = r;
        /* 24MHz 低速下整图生成会超 IWDG2 10s 窗口, 每行喂狗 */
        if ((i % (TIM_ACTIVE_W * 3)) == 0)
            IwdgKickDog(IWDG2);
    }
}

/* 几何图形: 白底 + 黑边框 + 两条黑对角线 */
static void SlideGenGeometry(uint8_t *fb)
{
    uint32_t x, y;

    for (y = 0; y < TIM_ACTIVE_H; y++)
    {
        /* 24MHz 低速下整图生成会超 IWDG2 10s 窗口, 每行喂狗 */
        IwdgKickDog(IWDG2);
        for (x = 0; x < TIM_ACTIVE_W; x++)
        {
            int32_t d1 = (int32_t)y * TIM_ACTIVE_W - (int32_t)x * TIM_ACTIVE_H;
            int32_t d2 = ((int32_t)y - (int32_t)TIM_ACTIVE_H) * TIM_ACTIVE_W
                         + (int32_t)x * TIM_ACTIVE_H;
            uint8_t v = 255;
            uint8_t *p = &fb[(y * TIM_ACTIVE_W + x) * 3];

            /* 边框 2px */
            if (x < 2 || x >= TIM_ACTIVE_W - 2 || y < 2 || y >= TIM_ACTIVE_H - 2)
                v = 0;
            /* 两条对角线 (~1.5px 宽) */
            if ((d1 > -2500 && d1 < 2500) || (d2 > -2500 && d2 < 2500))
                v = 0;
            p[0] = v;
            p[1] = v;
            p[2] = v;
        }
    }
}

/* 同心圆环: 以屏幕中心为圆心, 黑白交替, 环宽 40px */
static void SlideGenRings(uint8_t *fb)
{
    const uint32_t ring_w = 40;
    int32_t cx = TIM_ACTIVE_W / 2;
    int32_t cy = TIM_ACTIVE_H / 2;
    uint32_t x, y;

    for (y = 0; y < TIM_ACTIVE_H; y++)
    {
        /* 24MHz 低速下整图生成会超 IWDG2 10s 窗口, 每行喂狗 */
        IwdgKickDog(IWDG2);
        for (x = 0; x < TIM_ACTIVE_W; x++)
        {
            int32_t dx = (int32_t)x - cx;
            int32_t dy = (int32_t)y - cy;
            uint32_t ring = SlideIsqrt((uint32_t)(dx * dx + dy * dy)) / ring_w;
            uint8_t v = (ring & 1) ? 255 : 0;
            uint8_t *p = &fb[(y * TIM_ACTIVE_W + x) * 3];
            p[0] = v;
            p[1] = v;
            p[2] = v;
        }
    }
}

/* 轮播播放: 预生成 6 张, VBlank 重载切换 fb 指针 */
static void TestDisplaySlideshow(void)
{
    LtdcTimingConfig_t tim;
    LtdcLayerConfig_t  lay;
    Sii9022a_t         sii;
    SiiVideoMode_t     vm;
    Frame_t            fr;
    uint32_t           cur = 0;

    PRINT("\r\n===== On-board Image Slideshow =====\r\n");
    PRINT("  6 patterns, 500ms each\r\n");
    PRINT("  Press 0 to stop\r\n\r\n");

    if (DisplayHdmiInit(&tim, &sii) != 0)
        return;

    /* ---- 预生成 6 张图案 ---- */
    SlideGenBars(&g_slide_fb[0][0]);
    IwdgKickDog(IWDG2);
    SlideGenGradient(&g_slide_fb[1][0]);
    IwdgKickDog(IWDG2);
    SlideGenChecker(&g_slide_fb[2][0]);
    IwdgKickDog(IWDG2);
    SlideGenSolid(&g_slide_fb[3][0], 0x0000FF);  /* 纯蓝 */
    IwdgKickDog(IWDG2);
    SlideGenGeometry(&g_slide_fb[4][0]);
    IwdgKickDog(IWDG2);
    SlideGenRings(&g_slide_fb[5][0]);
    IwdgKickDog(IWDG2);

    /* ---- 图层 RGB888 全屏 (fb 指针每张切换) ---- */
    lay.enable       = 1;
    lay.win_x1       = 0;
    lay.win_y1       = 0;
    lay.win_x2       = TIM_ACTIVE_W - 1;
    lay.win_y2       = TIM_ACTIVE_H - 1;
    lay.pixel_fmt    = LTDC_PF_RGB888;
    lay.fb_addr      = (uint32_t)&g_slide_fb[0][0];
    lay.pitch        = TIM_ACTIVE_W * 3;
    lay.width        = TIM_ACTIVE_W;
    lay.height       = TIM_ACTIVE_H;
    lay.clut_en      = 0;
    lay.const_alpha  = 255;
    lay.def_color    = 0;
    lay.blend_f1     = LTDC_BF1_CONST_ALPHA;
    lay.blend_f2     = LTDC_BF2_1_MINUS_CONST;
    lay.color_key_en = 0;
    lay.color_key    = 0;

    LtdcLayerConfig(LTDC, 1, &lay);
    LtdcReloadImmediate(LTDC);
    LtdcEnable(LTDC);

    /* ---- 视频模式 + AVI + D0 + TMDS 使能 (同 HDMI 测试) ---- */
    vm.pixel_clock = 74250000;
    vm.htotal      = TIM_HSW + TIM_HBP + TIM_ACTIVE_W + TIM_HFP;
    vm.vtotal      = TIM_VSW + TIM_VBP + TIM_ACTIVE_H + TIM_VFP;
    vm.vfreq       = 60;

    SiiSetVideoMode(&sii, &vm);

    {
        uint32_t t;
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
    }

    SiiSetSyncPolarity(&sii, 0, 0);

    {
        uint8_t avi[13] = { 0 };
        avi[0] = 0x00;
        avi[1] = 0x00;
        avi[2] = 0x00;
        avi[3] = 4;     /* VIC=4 = 720p60 */
        SiiSetAVIInfoFrame(&sii, avi, 13);
    }

    SiiClearInterrupt(&sii, 0x03);
    SiiRegWrite(&sii, SII_TPI_POWER_STATE, SII_POWER_D0);

    {
        uint32_t t;
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        SiiRegWrite(&sii, SII_TPI_SYS_CTRL, 0x01);
        for (t = 0; t < 5000000; t++) {
            if ((t & 0x7FFFF) == 0) IwdgKickDog(IWDG2);
        }
        if (SiiRegRead(&sii, SII_TPI_SYS_CTRL) != 0x01)
            PRINT("  SYS_CTRL TMDS enable failed (dropped write)\r\n");
    }

    /* ---- 轮播: TIM6 计时 500ms, VBlank 重载切下一张 ----
     * TIM6 自由运行 (PSC=63999 → CNT 时钟 1kHz), 500ms = 500 tick;
     * 16 位计数器回绕周期 65.5s, 500 差值远小于半周期, 相减安全 */
    {
        BasicTimerCfg_t tcfg;

        BasicTimerClkEnable(TIM6);
        tcfg.tim_psc  = 63999;
        tcfg.tim_arr  = 0xFFFF;
        tcfg.tim_arpe = 0;
        tcfg.tim_opm  = 0;
        tcfg.tim_urs  = 0;
        tcfg.tim_udis = 0;
        tcfg.tim_ude  = 0;
        tcfg.tim_uie  = 0;
        BasicTimerCfg(TIM6, &tcfg);
        BasicTimerStart(TIM6);
    }

    while (1)
    {
        uint16_t t_start;

        lay.fb_addr = (uint32_t)&g_slide_fb[cur][0];
        LtdcLayerConfig(LTDC, 1, &lay);
        LtdcReloadVBlank(LTDC);

        t_start = (uint16_t)BasicTimerCnt(TIM6);
        while ((uint16_t)(BasicTimerCnt(TIM6) - t_start) < SLIDE_INTERVAL_MS)
        {
            IwdgKickDog(IWDG2);
            if (FramePoll(&fr)
                && (fr.cmd == FRAME_CMD_BACK
                    || (fr.data_len >= 1 && fr.data[0] == '0')))
                goto exit;
        }
        cur = (cur + 1) % SLIDE_COUNT;
    }

exit:
    /* ---- 清理 ---- */
    SiiTmdsOutput(&sii, 0);
    LtdcDisable(LTDC);
    PRINT("Slideshow stopped.\r\n");
}


/* ================================================================== */
/*  串口打字器 — 串口输入文本, LTDC 逐字显示 (16x16 汉字 + 8x16 ASCII)*/
/*                                                                    */
/*   字库: font_gbk16.c (宋体, GB2312 全字库)                         */
/*   输入: PC 端 tools/typewriter.py 发 UTF-8→GB2312 帧 (命令 0x12)  */
/*   控制: 0x08 退格 / 0x0A 换行 / 0x0C 清屏                          */
/*   命令 (帧 data 字段): 0 / BACK — 停止返回菜单                     */
/* ================================================================== */

static uint8_t g_txt_fb[TIM_ACTIVE_W * TIM_ACTIVE_H * 3] __attribute__((aligned(32)));

#define TXT_CELL_W       8     /* 半宽列宽: ASCII 一列, 汉字两列 */
#define TXT_CELL_H       16
#define TXT_COLS         (TIM_ACTIVE_W / TXT_CELL_W)   /* 160 半宽列 */
#define TXT_ROWS         (TIM_ACTIVE_H / TXT_CELL_H)   /* 45 行 */

static uint16_t g_txt_x;        /* 光标 x, 半宽列单位 */
static uint16_t g_txt_y;        /* 光标 y, 像素 */
static uint8_t  g_txt_last_w;   /* 上一字符占的半宽列数 (退格用) */

/* 单色点阵 (w8 像素宽 x 16 行, 每行位图 MSB 为最左) 画到帧缓冲 */
static void TxtDrawGlyph(uint16_t x8, uint16_t y, const uint8_t *glyph, uint8_t w8)
{
    uint16_t row, col;
    uint32_t base = (uint32_t)y * TIM_ACTIVE_W * 3 + (uint32_t)x8 * 8 * 3;

    for (row = 0; row < TXT_CELL_H; row++)
    {
        for (col = 0; col < w8; col++)
        {
            uint8_t v = ((glyph[row * (w8 / 8) + (col / 8)] >> (7 - (col % 8))) & 1)
                        ? 255 : 0;
            uint8_t *p = &g_txt_fb[base + row * TIM_ACTIVE_W * 3 + col * 3];
            p[0] = v;
            p[1] = v;
            p[2] = v;
        }
    }
}

static void TxtNewline(void)
{
    g_txt_x = 0;
    g_txt_y += TXT_CELL_H;
    if (g_txt_y > (TXT_ROWS - 1) * TXT_CELL_H)
    {
        /* 整屏上滚一行: 帧缓冲上移 16 像素, 末行清黑 */
        memmove(g_txt_fb, &g_txt_fb[TIM_ACTIVE_W * 3 * TXT_CELL_H],
                (TIM_ACTIVE_H - TXT_CELL_H) * TIM_ACTIVE_W * 3);
        memset(&g_txt_fb[(TIM_ACTIVE_H - TXT_CELL_H) * TIM_ACTIVE_W * 3], 0,
               TXT_CELL_H * TIM_ACTIVE_W * 3);
        g_txt_y = (TXT_ROWS - 1) * TXT_CELL_H;
    }
}

static void TxtClear(void)
{
    memset(g_txt_fb, 0, sizeof(g_txt_fb));
    g_txt_x = 0;
    g_txt_y = 0;
    g_txt_last_w = 1;
}

static void TxtPutAscii(uint8_t ch)
{
    if (g_txt_x + 1 > TXT_COLS)
        TxtNewline();
    TxtDrawGlyph(g_txt_x, g_txt_y, &g_ascii8x16_font[ch * 16], 8);
    g_txt_x += 1;
    g_txt_last_w = 1;
}

static void TxtPutGb(uint8_t a, uint8_t p)
{
    if (g_txt_x + 2 > TXT_COLS)
        TxtNewline();
    TxtDrawGlyph(g_txt_x, g_txt_y, &g_gbk16_font[GBK16_INDEX(a, p)], 16);
    g_txt_x += 2;
    g_txt_last_w = 2;
}

static void TxtBackspace(void)
{
    uint16_t w = g_txt_last_w;
    uint16_t x = (g_txt_x > w) ? (g_txt_x - w) : 0;
    uint16_t row;

    if (g_txt_x == 0)
        return;
    /* 擦除该字格 (画黑) */
    for (row = 0; row < TXT_CELL_H; row++)
    {
        memset(&g_txt_fb[(uint32_t)(g_txt_y + row) * TIM_ACTIVE_W * 3
                        + (uint32_t)x * 8 * 3], 0, (uint32_t)w * 8 * 3);
    }
    g_txt_x = x;
    g_txt_last_w = 1;
}

/* 处理一帧文本数据 (GB2312 字节流, 含控制符) */
static void TxtFeed(const uint8_t *data, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < len; i++)
    {
        uint8_t b = data[i];

        if (b == 0x08)          /* 退格 */
        {
            TxtBackspace();
        }
        else if (b == 0x0A)     /* 换行 */
        {
            TxtNewline();
        }
        else if (b == 0x0C)     /* 清屏 */
        {
            TxtClear();
        }
        else if (b >= 0x20 && b <= 0x7E)
        {
            TxtPutAscii(b);
        }
        else if (b >= 0xA1 && i + 1 < len && data[i + 1] >= 0xA1)
        {
            TxtPutGb(b, data[i + 1]);
            i++;
        }
        /* 其他字节忽略 */
    }
}

static void TestDisplayTypewriter(void)
{
    LtdcTimingConfig_t tim;
    LtdcLayerConfig_t  lay;
    Sii9022a_t         sii;
    SiiVideoMode_t     vm;
    Frame_t            fr;

    PRINT("\r\n===== Serial Typewriter =====\r\n");
    PRINT("  Run tools/typewriter.py on PC, type + Enter\r\n");
    PRINT("  /clr clear  /quit exit  (or send 0)\r\n\r\n");

    if (DisplayHdmiInit(&tim, &sii) != 0)
        return;

    /* ---- 文本图层 RGB888 全屏 ---- */
    lay.enable       = 1;
    lay.win_x1       = 0;
    lay.win_y1       = 0;
    lay.win_x2       = TIM_ACTIVE_W - 1;
    lay.win_y2       = TIM_ACTIVE_H - 1;
    lay.pixel_fmt    = LTDC_PF_RGB888;
    lay.fb_addr      = (uint32_t)g_txt_fb;
    lay.pitch        = TIM_ACTIVE_W * 3;
    lay.width        = TIM_ACTIVE_W;
    lay.height       = TIM_ACTIVE_H;
    lay.clut_en      = 0;
    lay.const_alpha  = 255;
    lay.def_color    = 0;
    lay.blend_f1     = LTDC_BF1_CONST_ALPHA;
    lay.blend_f2     = LTDC_BF2_1_MINUS_CONST;
    lay.color_key_en = 0;
    lay.color_key    = 0;

    LtdcLayerConfig(LTDC, 1, &lay);
    LtdcReloadImmediate(LTDC);
    TxtClear();
    LtdcEnable(LTDC);

    /* ---- 视频模式 + AVI + D0 + TMDS (共用流程) ---- */
    vm.pixel_clock = 74250000;
    vm.htotal      = TIM_HSW + TIM_HBP + TIM_ACTIVE_W + TIM_HFP;
    vm.vtotal      = TIM_VSW + TIM_VBP + TIM_ACTIVE_H + TIM_VFP;
    vm.vfreq       = 60;

    DisplayHdmiVideoStart(&sii, &vm);

    /* ---- 切波特率, 接收文本 ---- */
    UsartSetBaud(USART4, 64000000, IMG_BAUD);

    PRINT("Typewriter active.\r\n");
    while (1)
    {
        IwdgKickDog(IWDG2);
        if (!FramePoll(&fr))
            continue;

        if (fr.cmd == FRAME_CMD_TXT_DATA)
            TxtFeed(fr.data, fr.data_len);
        else if (fr.cmd == FRAME_CMD_BACK
                 || (fr.data_len >= 1 && fr.data[0] == '0'))
            break;
    }

    /* ---- 清理 ---- */
    SiiTmdsOutput(&sii, 0);
    LtdcDisable(LTDC);
    PRINT("Typewriter stopped.\r\n");
}


/* ================================================================== */
/*  子测试菜单 + 入口                                                   */
/* ================================================================== */

static const MenuEntry_t g_display_menu[] = {
    { "1", "HDMI output (720p60 color bars)", TestDisplayHdmi },
    { "2", "Image via UART (RGB888 stream)",  TestDisplayUartImage },
    { "3", "Slideshow (on-board 6 images)",   TestDisplaySlideshow },
    { "4", "Typewriter (serial text input)",  TestDisplayTypewriter },
};

void DisplayTest(void)
{
    RunSubMenu("LTDC + SiI9022A HDMI Test",
               g_display_menu, sizeof(g_display_menu) / sizeof(g_display_menu[0]));
}
