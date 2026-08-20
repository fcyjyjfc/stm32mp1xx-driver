#include "sii9022a.h"

/* ======================== I2C 底层 ======================== */

/**
 * @brief  事务间稳定延时
 * @note   TMDS 输出开启后 TPI 变忙, 寄存器访问需留稳定时间,
 *         实测无延时写 SYS_CTRL 不生效 (回读仍是旧值)。
 */
static void SiiSettle(void)
{
    volatile int d;
    for (d = 0; d < 2000000; d++);
}

void SiiRegWrite(Sii9022a_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    I2cMstWrite(dev->i2c, dev->addr, buf, 2, I2C_BUS_START, I2C_BUS_STOP);
}

int SiiRegRead(Sii9022a_t *dev, uint8_t reg)
{
    uint8_t val;
    int ret;

    I2cMstWrite(dev->i2c, dev->addr, &reg, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
    ret = I2cPollFlag(dev->i2c, 1u << 6, 10000000);
    if (ret) return -1;
    I2cMstRead(dev->i2c, dev->addr, &val, 1, I2C_BUS_RESTART, I2C_BUS_STOP);
    return val;
}

void SiiRegWriteBurst(Sii9022a_t *dev, uint8_t reg, const uint8_t *data, int len)
{
    uint8_t buf[32];
    int i;

    if (len > 30) len = 30;
    buf[0] = reg;
    for (i = 0; i < len; i++)
        buf[1 + i] = data[i];

    I2cMstWrite(dev->i2c, dev->addr, buf, len + 1, I2C_BUS_START, I2C_BUS_STOP);
}

void SiiIdxWrite(Sii9022a_t *dev, uint8_t page, uint8_t offset, uint8_t val)
{
    SiiRegWrite(dev, SII_TPI_IDX_PAGE, page);
    SiiSettle();
    SiiRegWrite(dev, SII_TPI_IDX_ADDR, offset);
    SiiSettle();
    SiiRegWrite(dev, SII_TPI_IDX_DATA, val);
    SiiSettle();
}

int SiiIdxRead(Sii9022a_t *dev, uint8_t page, uint8_t offset)
{
    SiiRegWrite(dev, SII_TPI_IDX_PAGE, page);
    SiiSettle();
    SiiRegWrite(dev, SII_TPI_IDX_ADDR, offset);
    SiiSettle();
    return SiiRegRead(dev, SII_TPI_IDX_DATA);
}

/* ======================== 初始化和控制 ======================== */

int SiiDetect(Sii9022a_t *dev)
{
    int val;
    int i;

    SiiRegWrite(dev, SII_TPI_TPI_MODE, 0x00);

    for (i = 0; i < 20; i++) {
        val = SiiRegRead(dev, SII_TPI_DEVICE_ID);
        if (val == 0xB0) return 0;
        if (val < 0) return -1;
        {
            volatile int d;
            for (d = 0; d < 500000; d++);
        }
    }
    return -1;
}

int SiiInit(Sii9022a_t *dev)
{
    int val;

    SiiRegWrite(dev, SII_TPI_POWER_STATE, SII_POWER_D0);

    val = SiiIdxRead(dev, SII_IDX_PAGE0, SII_IDX_SRC_TERM);
    if (val < 0) return -1;
    SiiIdxWrite(dev, SII_IDX_PAGE0, SII_IDX_SRC_TERM, val | 1);

    SiiRegWrite(dev, SII_TPI_INPUT_BUS,
                SII_INPUT_BUS_TCLK_X1 | SII_INPUT_BUS_FULL |
                SII_INPUT_BUS_EDGE_RISE | SII_INPUT_BUS_PR_NONE);

    SiiRegWrite(dev, SII_TPI_INPUT_FMT, SII_INPUT_CS_RGB | SII_INPUT_DEPTH_8BIT);
    SiiRegWrite(dev, SII_TPI_OUTPUT_FMT, SII_OUTPUT_CS_RGB);
    SiiRegWrite(dev, SII_TPI_YC_MODE, 0x00);
    SiiRegWrite(dev, SII_TPI_SYNC_CTRL, SII_SYNC_EXTERNAL);

    return 0;
}

void SiiSetPowerState(Sii9022a_t *dev, uint8_t state)
{
    SiiRegWrite(dev, SII_TPI_POWER_STATE, state & 3);
}

void SiiTmdsOutput(Sii9022a_t *dev, int enable)
{
    int ctrl = SiiRegRead(dev, SII_TPI_SYS_CTRL);
    if (ctrl < 0) return;

    if (enable)
        ctrl &= ~SII_SYS_TMDS_PD;
    else
        ctrl |= SII_SYS_TMDS_PD;

    SiiRegWrite(dev, SII_TPI_SYS_CTRL, ctrl);
}

/* ======================== 视频 ======================== */

void SiiSetVideoMode(Sii9022a_t *dev, const SiiVideoMode_t *vm)
{
    uint16_t pixel_div = vm->pixel_clock / 10000;
    uint8_t data[8];

    data[0] = pixel_div & 0xFF;
    data[1] = (pixel_div >> 8) & 0xFF;
    data[2] = vm->vfreq & 0xFF;
    data[3] = (vm->vfreq >> 8) & 0xFF;
    data[4] = vm->htotal & 0xFF;
    data[5] = (vm->htotal >> 8) & 0xFF;
    data[6] = vm->vtotal & 0xFF;
    data[7] = (vm->vtotal >> 8) & 0xFF;

    SiiRegWriteBurst(dev, SII_TPI_PIXEL_CLOCK_L, data, 8);
}

void SiiSetColorFormat(Sii9022a_t *dev, uint8_t input_cs, uint8_t output_cs)
{
    SiiRegWrite(dev, SII_TPI_INPUT_FMT, input_cs | SII_INPUT_DEPTH_8BIT);
    SiiRegWrite(dev, SII_TPI_OUTPUT_FMT, output_cs);
}

void SiiSetAVIInfoFrame(Sii9022a_t *dev, const uint8_t *data, int len)
{
    uint8_t checksum;
    int i;

    checksum = 0x82 + 0x02 + 13;
    for (i = 0; i < len; i++)
        checksum += data[i];
    checksum = 0x100 - checksum;

    SiiRegWrite(dev, SII_TPI_AVI_DBYTE0, checksum);
    SiiSettle();
    SiiRegWriteBurst(dev, SII_TPI_AVI_DBYTE1, data, len);
}

void SiiSetSyncPolarity(Sii9022a_t *dev, int hsync_neg, int vsync_neg)
{
    uint8_t val = 0;

    if (hsync_neg) val |= SII_DE_HS_NEG;
    if (vsync_neg) val |= SII_DE_VS_NEG;

    SiiRegWrite(dev, SII_TPI_DE_CTRL, val);
}

/**
 * @brief  使能内部 DE 生成器, 由 HS/VS 合成 DE 信号
 * @param  dev      驱动上下文
 * @param  de_dly   HSYNC 宽度+HBP (像素), 10-bit
 * @param  de_top   VSYNC 宽度+VBP (行), 7-bit
 * @param  de_cnt   有效宽度 (像素), 12-bit
 * @param  de_lin   有效高度 (行), 11-bit
 * @return 0 成功, -1 参数错误
 * @note   外部 DE 通路异常时可借此排除 DE 路由问题。
 *         极性位保持正极性 (本工程 LTDC 输出正极性 HS/VS)。
 *         每笔写之间留稳定时间: 实测无间隔的 7 笔连写全部丢失 (回读 0),
 *         而带间隔的索引写 (SiiIdxWrite) 全部锁存。间隔具体是覆盖芯片
 *         内部忙窗口还是 I2C 控制器状态尚未定位, 先按已验证的模式处理。
 */
int SiiSetDeGenerator(Sii9022a_t *dev, uint16_t de_dly, uint16_t de_top,
                      uint16_t de_cnt, uint16_t de_lin)
{
    if (!dev) return -1;

    SiiSettle();
    SiiRegWrite(dev, SII_TPI_DE_DLY, de_dly & 0xFF);
    SiiSettle();
    SiiRegWrite(dev, SII_TPI_DE_TOP, de_top & 0x7F);
    SiiSettle();
    SiiRegWrite(dev, SII_TPI_DE_CNT, de_cnt & 0xFF);
    SiiSettle();
    SiiRegWrite(dev, 0x67, (de_cnt >> 8) & 0x0F);
    SiiSettle();
    SiiRegWrite(dev, SII_TPI_DE_LIN, de_lin & 0xFF);
    SiiSettle();
    SiiRegWrite(dev, 0x69, (de_lin >> 8) & 0x07);
    SiiSettle();
    SiiRegWrite(dev, SII_TPI_DE_CTRL, SII_DE_GEN_EN | ((de_dly >> 8) & 0x03));
    return 0;
}

/* ======================== 热插拔 & EDID ======================== */

int SiiIsConnected(Sii9022a_t *dev)
{
    int val = SiiRegRead(dev, SII_TPI_INT_STATUS);
    if (val < 0) return 0;
    return (val & SII_INT_HPD_STATE) ? 1 : 0;
}

void SiiClearInterrupt(Sii9022a_t *dev, uint8_t mask)
{
    SiiRegWrite(dev, SII_TPI_INT_STATUS, mask);
}

int SiiReadEDID(Sii9022a_t *dev, uint8_t *edid, int len)
{
    int ctrl;
    int i;

    ctrl = SiiRegRead(dev, SII_TPI_SYS_CTRL);
    if (ctrl < 0) return -1;
    SiiRegWrite(dev, SII_TPI_SYS_CTRL, ctrl | SII_SYS_DDC_REQ);
    SiiSettle();

    for (i = 0; i < 100; i++) {
        ctrl = SiiRegRead(dev, SII_TPI_SYS_CTRL);
        if (ctrl < 0) return -1;
        if (ctrl & (1 << 1))
            break;
        SiiSettle();
    }
    if (i >= 100) return -1;

    SiiRegWrite(dev, SII_TPI_SYS_CTRL, SII_SYS_DDC_REQ | (1 << 1));
    SiiSettle();

    {
        uint8_t addr = 0;
        int ret;

        I2cMstWrite(dev->i2c, 0x50, &addr, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
        ret = I2cPollFlag(dev->i2c, 1u << 6, 100000);
        if (ret) return -1;
        I2cMstRead(dev->i2c, 0x50, edid, len, I2C_BUS_RESTART, I2C_BUS_STOP);
    }

    SiiRegWrite(dev, SII_TPI_SYS_CTRL, 0x00);
    SiiSettle();

    for (i = 0; i < 100; i++) {
        ctrl = SiiRegRead(dev, SII_TPI_SYS_CTRL);
        if (ctrl < 0) return -1;
        if (!(ctrl & (SII_SYS_DDC_REQ | (1 << 1))))
            break;
        SiiSettle();
    }

    return 0;
}

/* ======================== 音频 ======================== */

void SiiAudioSpdifInit(Sii9022a_t *dev)
{
    SiiRegWrite(dev, SII_TPI_AUDIO_CTRL, SII_AUDIO_SPDIF | SII_AUDIO_MUTE);
    SiiRegWrite(dev, SII_TPI_AUDIO_FS, 0x03);
    SiiRegWrite(dev, SII_TPI_AUDIO_CTRL, SII_AUDIO_SPDIF);
}

void SiiAudioSpdifMute(Sii9022a_t *dev, int mute)
{
    int ctrl = SiiRegRead(dev, SII_TPI_AUDIO_CTRL);
    if (ctrl < 0) return;

    if (mute)
        ctrl |= SII_AUDIO_MUTE;
    else
        ctrl &= ~SII_AUDIO_MUTE;

    SiiRegWrite(dev, SII_TPI_AUDIO_CTRL, ctrl);
}
