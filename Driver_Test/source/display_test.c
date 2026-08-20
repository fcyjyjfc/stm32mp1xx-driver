/*
 * display_test.c — LTDC + SiI9022A HDMI 显示测试
 */

#include "stm32mp1xx_ltdc.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_i2c.h"
#include "stm32mp1xx_iwdg.h"
#include "sii9022a.h"
#include "common_test.h"


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


/* ================================================================== */
/*  测试主入口                                                         */
/* ================================================================== */

static void TestDisplayHdmi(void)
{
    LtdcTimingConfig_t tim;
    LtdcLayerConfig_t  lay;
    Sii9022a_t         sii;
    SiiVideoMode_t     vm;
    uint32_t           i;
    int                ret;

    PRINT("\r\n===== LTDC + SiI9022A HDMI Display Test =====\r\n");

    /* ---- 1. 时钟 + GPIO ---- */
    PRINT("Clock init...\r\n");
    DisplayTest_ClockCfg();

    PRINT("GPIO init...\r\n");
    DisplayTest_GpioInit();

    /* ---- 2. I2C5 初始化 (与 CS42L51 共用总线) ---- */
    I2cCfg(I2C5);

    /* ---- 3. SiI9022A 硬件复位 (PA13, 低有效) ---- */
    PRINT("SiI9022A reset...\r\n");
    GpioOutLow(GPIO_A, 13);
    for (i = 0; i < 500000; i++);
    GpioOutHi(GPIO_A, 13);
    for (i = 0; i < 500000; i++);

    /* ---- 3. SiI9022A 初始化 ---- */
    PRINT("SiI9022A init...\r\n");
    sii.i2c  = DISPLAY_I2C;
    sii.addr = SII9022A_ADDR;

    ret = SiiDetect(&sii);
    if (ret != 0) {
        PRINT("  SiI9022A not detected!\r\n");
        return;
    }
    PRINT("  SiI9022A detected.\r\n");

    ret = SiiInit(&sii);
    if (ret != 0)
        PRINT("  SiiInit failed!\r\n");

    /* ---- 4. LTDC 时序配置 ---- */
    PRINT("LTDC init...\r\n");
    tim.hsw        = TIM_HSW;
    tim.hbp        = TIM_HBP;
    tim.hfp        = TIM_HFP;
    tim.active_w   = TIM_ACTIVE_W;
    tim.vsw        = TIM_VSW;
    tim.vbp        = TIM_VBP;
    tim.vfp        = TIM_VFP;
    tim.active_h   = TIM_ACTIVE_H;
    /* 720p60: HSYNC+、VSYNC+、DE+、PCPOL=1 (RM0436: active high =
     * 上升沿有效), 与 SiI9022A 上升沿锁存 (0x08[4]=1, 默认) 匹配。
     * 注意 DEPOL 语义: RM0436 该位是 "Not data enable polarity",
     * 0 = Not DE 低有效 (即 DE 高有效), 1 = DE 被反相! */
    tim.hspol      = 1;
    tim.vspol      = 1;
    tim.depol      = 0;
    tim.pcpol      = 1;
    tim.bg_rgb888  = 0x000000;
    tim.dither_en  = 0;

    LtdcInit(LTDC, &tim);

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
/*  子测试菜单 + 入口                                                   */
/* ================================================================== */

static const MenuEntry_t g_display_menu[] = {
    { "1", "HDMI output (720p60 color bars)", TestDisplayHdmi },
};

void DisplayTest(void)
{
    RunSubMenu("LTDC + SiI9022A HDMI Test",
               g_display_menu, sizeof(g_display_menu) / sizeof(g_display_menu[0]));
}
