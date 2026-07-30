#include "stm32mp1xx_ltdc.h"

volatile LtdcRegs_t *const LTDC = (void *)0x5A001000;

/**
 * @brief  根据像素格式返回每像素字节数
 * @param  pf  像素格式 (LTDC_PF_*)
 * @return 每像素字节数, 无效格式返回 0
 */
static int LtdcBpp(uint32_t pf)
{
    switch (pf) {
    case LTDC_PF_ARGB8888: return 4;
    case LTDC_PF_RGB888:   return 3;
    case LTDC_PF_RGB565:
    case LTDC_PF_ARGB1555:
    case LTDC_PF_ARGB4444:
    case LTDC_PF_AL88:     return 2;
    case LTDC_PF_L8:
    case LTDC_PF_AL44:     return 1;
    default:               return 0;
    }
}

/**
 * @brief  初始化 LTDC 时序与全局配置
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  tim   时序配置, 含极性/背景色/抖动
 * @return 0 成功, -1 参数错误
 *
 * 配置 SSCR/BPCR/AWCR/TWCR 四个时序寄存器及 GCR(极性/抖动)/BCCR。
 * 初始配置不会使能 LTDC, 需后续调用 LtdcEnable。
 */
int LtdcInit(LtdcRegs_t *ltdc, const LtdcTimingConfig_t *tim)
{
    uint32_t hsw, hbp, hfp, aw, vsw, vbp, vfp, ah;
    uint32_t gcr;

    if (!ltdc || !tim)
        return -1;
    if (tim->hsw == 0 || tim->vsw == 0 ||
        tim->active_w == 0 || tim->active_h == 0)
        return -1;

    hsw = tim->hsw;
    hbp = tim->hbp;
    hfp = tim->hfp;
    aw  = tim->active_w;
    vsw = tim->vsw;
    vbp = tim->vbp;
    vfp = tim->vfp;
    ah  = tim->active_h;

    ltdc->SSCR = ((hsw - 1) & 0xFFF) << 16 | ((vsw - 1) & 0xFFF);
    ltdc->BPCR = ((hsw + hbp - 1) & 0xFFF) << 16 | ((vsw + vbp - 1) & 0xFFF);
    ltdc->AWCR = ((hsw + hbp + aw - 1) & 0xFFF) << 16
               | ((vsw + vbp + ah - 1) & 0xFFF);
    ltdc->TWCR = ((hsw + hbp + aw + hfp - 1) & 0xFFF) << 16
               | ((vsw + vbp + ah + vfp - 1) & 0xFFF);

    gcr = 0;
    if (tim->hspol)    gcr |= LTDC_GCR_HSPOL;
    if (tim->vspol)    gcr |= LTDC_GCR_VSPOL;
    if (tim->depol)    gcr |= LTDC_GCR_DEPOL;
    if (tim->pcpol)    gcr |= LTDC_GCR_PCPOL;
    if (tim->dither_en) gcr |= LTDC_GCR_DEN;
    ltdc->GCR = gcr;

    ltdc->BCCR = tim->bg_rgb888;

    return 0;
}

/**
 * @brief  使能 LTDC 控制器
 * @param  ltdc  LTDC 寄存器基址指针
 * @note   配置完图层并触发重载后再调用此函数
 */
void LtdcEnable(LtdcRegs_t *ltdc)
{
    if (ltdc) ltdc->GCR |= LTDC_GCR_LTDCEN;
}

/**
 * @brief  关闭 LTDC 控制器
 * @param  ltdc  LTDC 寄存器基址指针
 * @note   关闭后时序复位到最后像素, FIFO 刷空, 输出消隐信号
 */
void LtdcDisable(LtdcRegs_t *ltdc)
{
    if (ltdc) ltdc->GCR &= ~LTDC_GCR_LTDCEN;
}

/**
 * @brief  立即重载影子寄存器
 * @param  ltdc  LTDC 寄存器基址指针
 * @note   所有影子寄存器修改完成后触发, 硬件自动清除此位
 */
void LtdcReloadImmediate(LtdcRegs_t *ltdc)
{
    if (ltdc) ltdc->SRCR |= LTDC_SRCR_IMR;
}

/**
 * @brief  垂直消隐期重载影子寄存器
 * @param  ltdc  LTDC 寄存器基址指针
 * @note   在每帧消隐期开始时刻重载, 可防止画面撕裂
 */
void LtdcReloadVBlank(LtdcRegs_t *ltdc)
{
    if (ltdc) ltdc->SRCR |= LTDC_SRCR_VBR;
}

/**
 * @brief  配置图层参数
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 * @param  cfg    图层配置
 * @return 0 成功, -1 参数错误
 *
 * 配置窗口位置/像素格式/帧缓冲地址/alpha/混合因子/颜色键等。
 * 写入后需调用 LtdcReloadImmediate 或 LtdcReloadVBlank 使配置生效。
 */
int LtdcLayerConfig(LtdcRegs_t *ltdc, int layer,
                    const LtdcLayerConfig_t *cfg)
{
    volatile uint32_t *cr, *whpcr, *wvpcr, *ckcr, *pfcr;
    volatile uint32_t *cacr, *dccr, *bfcr, *cfbar, *cfblr, *cfblnr;
    uint32_t ahbp, avbp, whstpos, whsppos, wvstpos, wvsppos;
    uint32_t cfbl, cfbp, bpp;

    if (!ltdc || !cfg)
        return -1;
    if (layer != 1 && layer != 2)
        return -1;
    if (cfg->win_x2 < cfg->win_x1 || cfg->win_y2 < cfg->win_y1)
        return -1;

    if (layer == 1) {
        cr     = &ltdc->L1CR;
        whpcr  = &ltdc->L1WHPCR;
        wvpcr  = &ltdc->L1WVPCR;
        ckcr   = &ltdc->L1CKCR;
        pfcr   = &ltdc->L1PFCR;
        cacr   = &ltdc->L1CACR;
        dccr   = &ltdc->L1DCCR;
        bfcr   = &ltdc->L1BFCR;
        cfbar  = &ltdc->L1CFBAR;
        cfblr  = &ltdc->L1CFBLR;
        cfblnr = &ltdc->L1CFBLNR;
    } else {
        cr     = &ltdc->L2CR;
        whpcr  = &ltdc->L2WHPCR;
        wvpcr  = &ltdc->L2WVPCR;
        ckcr   = &ltdc->L2CKCR;
        pfcr   = &ltdc->L2PFCR;
        cacr   = &ltdc->L2CACR;
        dccr   = &ltdc->L2DCCR;
        bfcr   = &ltdc->L2BFCR;
        cfbar  = &ltdc->L2CFBAR;
        cfblr  = &ltdc->L2CFBLR;
        cfblnr = &ltdc->L2CFBLNR;
    }

    /* 从已配好的时序寄存器读取累积值, 计算窗口绝对坐标 */
    ahbp = (ltdc->BPCR >> 16) & 0xFFF;  /* AHBP = hsw + hbp - 1 */
    avbp = ltdc->BPCR & 0xFFF;          /* AVBP = vsw + vbp - 1 */

    whstpos = ahbp + 1 + cfg->win_x1;
    whsppos = ahbp + 1 + cfg->win_x2;
    wvstpos = avbp + 1 + cfg->win_y1;
    wvsppos = avbp + 1 + cfg->win_y2;

    *whpcr = (whsppos & 0xFFF) << 16 | (whstpos & 0xFFF);
    *wvpcr = (wvsppos & 0xFFF) << 16 | (wvstpos & 0xFFF);

    *pfcr = cfg->pixel_fmt & 7;

    *cfbar = cfg->fb_addr;

    bpp = LtdcBpp(cfg->pixel_fmt);
    cfbl = cfg->width * bpp + 7;
    cfbp = cfg->pitch;
    *cfblr = (cfbp & 0x3FFF) << 16 | (cfbl & 0x3FFF);
    *cfblnr = cfg->height & 0xFFF;

    *cr &= ~LTDC_LxCR_CLUTEN;
    if (cfg->clut_en)
        *cr |= LTDC_LxCR_CLUTEN;

    *cacr = cfg->const_alpha & 0xFF;
    *dccr = cfg->def_color;
    *bfcr = (cfg->blend_f1 & (7U << 8)) | (cfg->blend_f2 & 7);

    *cr &= ~LTDC_LxCR_COLKEN;
    *ckcr = cfg->color_key;
    if (cfg->color_key_en)
        *cr |= LTDC_LxCR_COLKEN;

    *cr &= ~LTDC_LxCR_LEN;
    if (cfg->enable)
        *cr |= LTDC_LxCR_LEN;

    return 0;
}

/**
 * @brief  使能指定图层
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 */
void LtdcLayerEnable(LtdcRegs_t *ltdc, int layer)
{
    if (!ltdc) return;
    if (layer == 1)      ltdc->L1CR |= LTDC_LxCR_LEN;
    else if (layer == 2) ltdc->L2CR |= LTDC_LxCR_LEN;
}

/**
 * @brief  关闭指定图层
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 */
void LtdcLayerDisable(LtdcRegs_t *ltdc, int layer)
{
    if (!ltdc) return;
    if (layer == 1)      ltdc->L1CR &= ~LTDC_LxCR_LEN;
    else if (layer == 2) ltdc->L2CR &= ~LTDC_LxCR_LEN;
}

/**
 * @brief  加载 CLUT 颜色查找表
 * @param  ltdc   LTDC 寄存器基址指针
 * @param  layer  图层编号 (1 或 2)
 * @param  rgb    RGB 颜色数组, 每项 [R,G,B] 各 8bit
 * @param  count  颜色项数 (最大 256)
 * @note   须在消隐期或图层禁用时调用
 */
void LtdcLayerClutLoad(LtdcRegs_t *ltdc, int layer,
                       const uint8_t rgb[][3], int count)
{
    volatile uint32_t *clutwr;
    int i;

    if (!ltdc || !rgb) return;
    if (layer == 1)      clutwr = &ltdc->L1CLUTWR;
    else if (layer == 2) clutwr = &ltdc->L2CLUTWR;
    else                 return;

    for (i = 0; i < count && i < 256; i++)
        *clutwr = (i << 24) | (rgb[i][0] << 16) | (rgb[i][1] << 8) | rgb[i][2];
}

/**
 * @brief  使能中断
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  mask  中断掩码 (LTDC_INT_*_IE 组合)
 */
void LtdcIntEnable(LtdcRegs_t *ltdc, uint32_t mask)
{
    if (ltdc) ltdc->IER |= (mask & 0x0F);
}

/**
 * @brief  关闭中断
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  mask  中断掩码 (LTDC_INT_*_IE 组合)
 */
void LtdcIntDisable(LtdcRegs_t *ltdc, uint32_t mask)
{
    if (ltdc) ltdc->IER &= ~(mask & 0x0F);
}

/**
 * @brief  读取中断状态
 * @param  ltdc  LTDC 寄存器基址指针
 * @return 中断标志位 (LTDC_INT_*_IF 组合)
 */
uint32_t LtdcIntStatus(LtdcRegs_t *ltdc)
{
    return ltdc ? ltdc->ISR & 0x0F : 0;
}

/**
 * @brief  清除中断标志
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  mask  要清除的标志 (LTDC_INT_C* 组合)
 */
void LtdcIntClear(LtdcRegs_t *ltdc, uint32_t mask)
{
    if (ltdc) ltdc->ICR = mask & 0x0F;
}

/**
 * @brief  设置行中断位置
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  line  行号 (0-4095)
 */
void LtdcLineIntSetPos(LtdcRegs_t *ltdc, uint32_t line)
{
    if (ltdc) ltdc->LIPCR = line & 0xFFF;
}

/**
 * @brief  读取当前扫描位置
 * @param  ltdc  LTDC 寄存器基址指针
 * @param  x     输出 X 坐标, 可 NULL
 * @param  y     输出 Y 坐标, 可 NULL
 */
void LtdcGetPosition(LtdcRegs_t *ltdc, uint32_t *x, uint32_t *y)
{
    if (!ltdc) return;
    if (x) *x = (ltdc->CPSR >> 16) & 0xFFFF;
    if (y) *y = ltdc->CPSR & 0xFFFF;
}

/**
 * @brief  读取当前显示阶段状态
 * @param  ltdc  LTDC 寄存器基址指针
 * @return CDSR 低 4 位, 指示当前处于哪个显示阶段
 * @retval LTDC_CDSR_HSYNCS 水平同步阶段
 * @retval LTDC_CDSR_VSYNCS 垂直同步阶段
 * @retval LTDC_CDSR_HDES   水平有效数据阶段
 * @retval LTDC_CDSR_VDES   垂直有效数据阶段
 */
uint32_t LtdcGetDisplayStatus(LtdcRegs_t *ltdc)
{
    return ltdc ? (ltdc->CDSR & 0x0F) : 0;
}
