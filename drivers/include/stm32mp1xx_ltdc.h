#ifndef STM32MP1XX_LTDC_H_
#define STM32MP1XX_LTDC_H_

#include <stdint.h>

typedef struct {
    // 0x000
    volatile uint32_t IDR;          /* 标识寄存器 */
    // 0x004
    volatile uint32_t LCR;          /* 图层计数寄存器 */
    // 0x008
    volatile uint32_t SSCR;         /* 同步尺寸配置 */
    // 0x00C
    volatile uint32_t BPCR;         /* 后肩配置 */
    // 0x010
    volatile uint32_t AWCR;         /* 有效宽度配置 */
    // 0x014
    volatile uint32_t TWCR;         /* 总宽度配置 */
    // 0x018
    volatile uint32_t GCR;          /* 全局控制 */
    // 0x01C
    volatile uint32_t GC1R;         /* 全局配置 1 */
    // 0x020
    volatile uint32_t GC2R;         /* 全局配置 2 */
    // 0x024
    volatile uint32_t SRCR;         /* 影子寄存器重载控制 */
    // 0x028 - 0x02B
    volatile uint8_t  RSVD0[0x02C - 0x028];
    // 0x02C
    volatile uint32_t BCCR;         /* 背景色 */
    // 0x030 - 0x033
    volatile uint8_t  RSVD1[0x034 - 0x030];
    // 0x034
    volatile uint32_t IER;          /* 中断使能 */
    // 0x038
    volatile uint32_t ISR;          /* 中断状态 */
    // 0x03C
    volatile uint32_t ICR;          /* 中断清除 */
    // 0x040
    volatile uint32_t LIPCR;        /* 行中断位置 */
    // 0x044
    volatile uint32_t CPSR;         /* 当前位置状态 */
    // 0x048
    volatile uint32_t CDSR;         /* 当前显示状态 */
    // 0x04C - 0x083
    volatile uint8_t  RSVD2[0x084 - 0x04C];
    /* ===== Layer 1 (0x084 - 0x0C7) ===== */
    // 0x084
    volatile uint32_t L1CR;         /* 图层 1 控制 */
    // 0x088
    volatile uint32_t L1WHPCR;      /* 图层 1 窗口水平位置 */
    // 0x08C
    volatile uint32_t L1WVPCR;      /* 图层 1 窗口垂直位置 */
    // 0x090
    volatile uint32_t L1CKCR;       /* 图层 1 颜色键 */
    // 0x094
    volatile uint32_t L1PFCR;       /* 图层 1 像素格式 */
    // 0x098
    volatile uint32_t L1CACR;       /* 图层 1 常量 alpha */
    // 0x09C
    volatile uint32_t L1DCCR;       /* 图层 1 默认色 */
    // 0x0A0
    volatile uint32_t L1BFCR;       /* 图层 1 混合因子 */
    // 0x0A4 - 0x0AB
    volatile uint8_t  RSVD3[0x0AC - 0x0A4];
    // 0x0AC
    volatile uint32_t L1CFBAR;      /* 图层 1 帧缓冲地址 */
    // 0x0B0
    volatile uint32_t L1CFBLR;      /* 图层 1 帧缓冲行长 */
    // 0x0B4
    volatile uint32_t L1CFBLNR;     /* 图层 1 帧缓冲行数 */
    // 0x0B8 - 0x0C3
    volatile uint8_t  RSVD4[0x0C4 - 0x0B8];
    // 0x0C4
    volatile uint32_t L1CLUTWR;     /* 图层 1 CLUT 写入 */
    // 0x0C8 - 0x103
    volatile uint8_t  RSVD5[0x104 - 0x0C8];
    /* ===== Layer 2 (0x104 - 0x147) ===== */
    // 0x104
    volatile uint32_t L2CR;         /* 图层 2 控制 */
    // 0x108
    volatile uint32_t L2WHPCR;      /* 图层 2 窗口水平位置 */
    // 0x10C
    volatile uint32_t L2WVPCR;      /* 图层 2 窗口垂直位置 */
    // 0x110
    volatile uint32_t L2CKCR;       /* 图层 2 颜色键 */
    // 0x114
    volatile uint32_t L2PFCR;       /* 图层 2 像素格式 */
    // 0x118
    volatile uint32_t L2CACR;       /* 图层 2 常量 alpha */
    // 0x11C
    volatile uint32_t L2DCCR;       /* 图层 2 默认色 */
    // 0x120
    volatile uint32_t L2BFCR;       /* 图层 2 混合因子 */
    // 0x124 - 0x12B
    volatile uint8_t  RSVD6[0x12C - 0x124];
    // 0x12C
    volatile uint32_t L2CFBAR;      /* 图层 2 帧缓冲地址 */
    // 0x130
    volatile uint32_t L2CFBLR;      /* 图层 2 帧缓冲行长 */
    // 0x134
    volatile uint32_t L2CFBLNR;     /* 图层 2 帧缓冲行数 */
    // 0x138 - 0x143
    volatile uint8_t  RSVD7[0x144 - 0x138];
    // 0x144
    volatile uint32_t L2CLUTWR;     /* 图层 2 CLUT 写入 */
} LtdcRegs_t;

/* =================== GCR 位定义 =================== */
#define LTDC_GCR_HSPOL      (1U << 31)  /* HSYNC 极性: 1=高有效 */
#define LTDC_GCR_VSPOL      (1U << 30)  /* VSYNC 极性: 1=高有效 */
#define LTDC_GCR_DEPOL      (1U << 29)  /* Not DE 极性 (RM0436): 1=Not DE 高有效, 即 DE 低有效 */
#define LTDC_GCR_PCPOL      (1U << 28)  /* 像素时钟极性: 1=高有效 */
#define LTDC_GCR_DEN        (1U << 16)  /* 抖动使能 */
#define LTDC_GCR_DRW_POS    12          /* 抖动红色宽度位偏移 (只读) */
#define LTDC_GCR_DGW_POS    8           /* 抖动绿色宽度位偏移 (只读) */
#define LTDC_GCR_DBW_POS    4           /* 抖动蓝色宽度位偏移 (只读) */
#define LTDC_GCR_LTDCEN     (1U << 0)   /* LTDC 使能 */

/* =================== SRCR 位定义 =================== */
#define LTDC_SRCR_VBR       (1U << 1)   /* 垂直消隐期重载 */
#define LTDC_SRCR_IMR       (1U << 0)   /* 立即重载 */

/* =================== IER / ISR / ICR 位定义 =================== */
#define LTDC_INT_RRIE       (1U << 3)   /* 重载中断使能 */
#define LTDC_INT_TERRIE     (1U << 2)   /* 传输错误中断使能 */
#define LTDC_INT_FUIE       (1U << 1)   /* FIFO 欠载中断使能 */
#define LTDC_INT_LIE        (1U << 0)   /* 行中断使能 */

#define LTDC_INT_RRIF       (1U << 3)   /* 重载中断标志 */
#define LTDC_INT_TERRIF     (1U << 2)   /* 传输错误中断标志 */
#define LTDC_INT_FUIF       (1U << 1)   /* FIFO 欠载中断标志 */
#define LTDC_INT_LIF        (1U << 0)   /* 行中断标志 */

#define LTDC_INT_CRRIF      (1U << 3)   /* 清除重载中断标志 */
#define LTDC_INT_CTERRIF    (1U << 2)   /* 清除传输错误中断标志 */
#define LTDC_INT_CFUIF      (1U << 1)   /* 清除 FIFO 欠载中断标志 */
#define LTDC_INT_CLIF       (1U << 0)   /* 清除行中断标志 */

/* =================== LxCR 位定义 =================== */
#define LTDC_LxCR_CLUTEN    (1U << 4)   /* CLUT 使能 */
#define LTDC_LxCR_COLKEN    (1U << 1)   /* 颜色键使能 */
#define LTDC_LxCR_LEN       (1U << 0)   /* 图层使能 */

/* =================== LxPFCR 像素格式 =================== */
#define LTDC_PF_ARGB8888    0
#define LTDC_PF_RGB888      1
#define LTDC_PF_RGB565      2
#define LTDC_PF_ARGB1555    3
#define LTDC_PF_ARGB4444    4
#define LTDC_PF_L8          5
#define LTDC_PF_AL44        6
#define LTDC_PF_AL88        7

/* =================== LxBFCR 混合因子 =================== */
#define LTDC_BF1_CONST_ALPHA    (4U << 8)   /* BF1 = 常量 alpha */
#define LTDC_BF1_PIXALPHA_CONST (6U << 8)   /* BF1 = 逐像素 alpha × 常量 alpha */
#define LTDC_BF2_1_MINUS_CONST  (5U << 0)   /* BF2 = 1 - 常量 alpha */
#define LTDC_BF2_1_MINUS_PIXA   (7U << 0)   /* BF2 = 1 - (逐像素 × 常量) */

/* =================== CDSR 显示状态 =================== */
#define LTDC_CDSR_HSYNCS    (1U << 3)   /* HSYNC 状态 */
#define LTDC_CDSR_VSYNCS    (1U << 2)   /* VSYNC 状态 */
#define LTDC_CDSR_HDES      (1U << 1)   /* 水平 DE 状态 */
#define LTDC_CDSR_VDES      (1U << 0)   /* 垂直 DE 状态 */

/* =================== 配置结构体 =================== */

/**
 * @brief  LTDC 时序与全局配置
 */
typedef struct {
    uint32_t hsw;           /* HSYNC 脉宽 (像素时钟周期数) */
    uint32_t hbp;           /* 水平后肩 (像素数) */
    uint32_t hfp;           /* 水平前肩 (像素数) */
    uint32_t active_w;      /* 有效宽度 (像素数) */
    uint32_t vsw;           /* VSYNC 脉宽 (行数) */
    uint32_t vbp;           /* 垂直后肩 (行数) */
    uint32_t vfp;           /* 垂直前肩 (行数) */
    uint32_t active_h;      /* 有效高度 (行数) */
    int      hspol;         /* HSYNC 极性: 0=低有效, 1=高有效 */
    int      vspol;         /* VSYNC 极性 */
    int      depol;         /* DE 极性 */
    int      pcpol;         /* 像素时钟极性 */
    uint32_t bg_rgb888;     /* 背景色 RGB888 */
    int      dither_en;     /* 抖动使能 */
} LtdcTimingConfig_t;

/**
 * @brief  图层配置
 */
typedef struct {
    int      enable;        /* 启用该层 */
    uint32_t win_x1;        /* 窗口左上角 X (有效显示区坐标) */
    uint32_t win_y1;        /* 窗口左上角 Y */
    uint32_t win_x2;        /* 窗口右下角 X (含) */
    uint32_t win_y2;        /* 窗口右下角 Y (含) */
    uint32_t pixel_fmt;     /* 像素格式 (LTDC_PF_*) */
    uint32_t fb_addr;       /* 帧缓冲 DDR 地址 */
    uint32_t pitch;         /* 行跨度 (字节) */
    uint32_t width;         /* 帧缓冲宽度 (像素) */
    uint32_t height;        /* 帧缓冲行数 */
    int      clut_en;       /* CLUT 使能 */
    uint32_t const_alpha;   /* 常量 alpha (0-255) */
    uint32_t def_color;     /* 默认色 ARGB8888 */
    uint32_t blend_f1;      /* 混合因子 1 (LTDC_BF1_*) */
    uint32_t blend_f2;      /* 混合因子 2 (LTDC_BF2_*) */
    int      color_key_en;  /* 颜色键使能 */
    uint32_t color_key;     /* 透明色 RGB888 */
} LtdcLayerConfig_t;

/* =================== 函数声明 =================== */

/**
 * @brief  初始化 LTDC 时序与全局配置
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  tim   时序配置, 含极性/背景色/抖动
 * @return 0 成功, -1 参数错误
 */
int  LtdcInit(volatile LtdcRegs_t *ltdc, const LtdcTimingConfig_t *tim);

/**
 * @brief  使能 LTDC 控制器
 * @param  ltdc  LTDC 寄存器基址指针
 */
void LtdcEnable(volatile LtdcRegs_t *ltdc);

/**
 * @brief  关闭 LTDC 控制器
 * @param  ltdc  LTDC 寄存器基址指针
 */
void LtdcDisable(volatile LtdcRegs_t *ltdc);

/**
 * @brief  立即重载影子寄存器
 * @param  ltdc  LTDC 寄存器基址指针
 */
void LtdcReloadImmediate(volatile LtdcRegs_t *ltdc);

/**
 * @brief  垂直消隐期重载影子寄存器 (防撕裂)
 * @param  ltdc  LTDC 寄存器基址指针
 */
void LtdcReloadVBlank(volatile LtdcRegs_t *ltdc);

/**
 * @brief  配置图层参数
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 * @param  cfg    图层配置
 * @return 0 成功, -1 参数错误
 */
int  LtdcLayerConfig(volatile LtdcRegs_t *ltdc, int layer,
                     const LtdcLayerConfig_t *cfg);

/**
 * @brief  使能指定图层
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 */
void LtdcLayerEnable(volatile LtdcRegs_t *ltdc, int layer);

/**
 * @brief  关闭指定图层
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 */
void LtdcLayerDisable(volatile LtdcRegs_t *ltdc, int layer);

/**
 * @brief  加载 CLUT 颜色查找表
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 * @param  rgb    RGB 颜色数组, 每项 3 字节
 * @param  count  颜色项数 (最大 256)
 */
void LtdcLayerClutLoad(volatile LtdcRegs_t *ltdc, int layer,
                       const uint8_t rgb[][3], int count);

/**
 * @brief  使能中断
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  mask  中断掩码 (LTDC_INT_*_IE 组合)
 */
void LtdcIntEnable(volatile LtdcRegs_t *ltdc, uint32_t mask);

/**
 * @brief  关闭中断
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  mask  中断掩码 (LTDC_INT_*_IE 组合)
 */
void LtdcIntDisable(volatile LtdcRegs_t *ltdc, uint32_t mask);

/**
 * @brief  读取中断状态
 * @param  ltdc  LTDC 寄存器基址指针
 * @return 中断标志位 (LTDC_INT_*_IF 组合)
 */
uint32_t LtdcIntStatus(volatile LtdcRegs_t *ltdc);

/**
 * @brief  清除中断标志
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  mask  要清除的标志 (LTDC_INT_C* 组合)
 */
void LtdcIntClear(volatile LtdcRegs_t *ltdc, uint32_t mask);

/**
 * @brief  设置行中断位置
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  line  行号 (0-4095)
 */
void LtdcLineIntSetPos(volatile LtdcRegs_t *ltdc, uint32_t line);

/**
 * @brief  读取当前扫描位置
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  x     输出 X 坐标, 可 NULL
 * @param  y     输出 Y 坐标, 可 NULL
 */
void LtdcGetPosition(volatile LtdcRegs_t *ltdc, uint32_t *x, uint32_t *y);

/**
 * @brief  读取当前显示阶段状态
 * @param  ltdc  LTDC 寄存器基址指针
 * @return CDSR 寄存器低 4 位 (LTDC_CDSR_*)
 */
uint32_t LtdcGetDisplayStatus(volatile LtdcRegs_t *ltdc);

extern volatile LtdcRegs_t *const LTDC;

#endif /* STM32MP1XX_LTDC_H_ */
