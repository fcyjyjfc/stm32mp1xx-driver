#include "sii9022a.h"

/* ======================== I2C 底层 ======================== */

void SiiRegWrite(Sii9022a_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    I2cMstWrite(dev->i2c, dev->addr, buf, 2, I2C_BUS_START, I2C_BUS_STOP);
}

uint8_t SiiRegRead(Sii9022a_t *dev, uint8_t reg)
{
    uint8_t val;
    I2cMstWrite(dev->i2c, dev->addr, &reg, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
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
    SiiRegWrite(dev, SII_TPI_IDX_ADDR, offset);
    SiiRegWrite(dev, SII_TPI_IDX_DATA, val);
}

uint8_t SiiIdxRead(Sii9022a_t *dev, uint8_t page, uint8_t offset)
{
    SiiRegWrite(dev, SII_TPI_IDX_PAGE, page);
    SiiRegWrite(dev, SII_TPI_IDX_ADDR, offset);
    return SiiRegRead(dev, SII_TPI_IDX_DATA);
}

/* ======================== 初始化和控制 ======================== */

int SiiDetect(Sii9022a_t *dev)
{
    return (SiiRegRead(dev, SII_TPI_DEVICE_ID) == 0xB0) ? 0 : -1;
}

int SiiInit(Sii9022a_t *dev)
{
    uint8_t val;

    SiiRegWrite(dev, SII_TPI_TPI_MODE, 0x00);

    if (SiiRegRead(dev, SII_TPI_DEVICE_ID) != 0xB0)
        return -1;

    SiiRegWrite(dev, SII_TPI_POWER_STATE, SII_POWER_D0);

    val = SiiIdxRead(dev, SII_IDX_PAGE0, SII_IDX_SRC_TERM);
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
    uint8_t ctrl = SiiRegRead(dev, SII_TPI_SYS_CTRL);

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
    SiiRegWriteBurst(dev, SII_TPI_AVI_DBYTE1, data, len);
}

void SiiSetSyncPolarity(Sii9022a_t *dev, int hsync_neg, int vsync_neg)
{
    uint8_t val = 0;

    if (hsync_neg) val |= SII_DE_HS_NEG;
    if (vsync_neg) val |= SII_DE_VS_NEG;

    SiiRegWrite(dev, SII_TPI_DE_CTRL, val);
}

/* ======================== 热插拔 & EDID ======================== */

int SiiIsConnected(Sii9022a_t *dev)
{
    return (SiiRegRead(dev, SII_TPI_INT_STATUS) & SII_INT_HPD_STATE) ? 1 : 0;
}

void SiiClearInterrupt(Sii9022a_t *dev, uint8_t mask)
{
    SiiRegWrite(dev, SII_TPI_INT_STATUS, mask);
}

int SiiReadEDID(Sii9022a_t *dev, uint8_t *edid, int len)
{
    uint8_t ctrl;
    int i;

    ctrl = SiiRegRead(dev, SII_TPI_SYS_CTRL);
    SiiRegWrite(dev, SII_TPI_SYS_CTRL, ctrl | SII_SYS_DDC_REQ);

    for (i = 0; i < 100; i++) {
        if (SiiRegRead(dev, SII_TPI_SYS_CTRL) & (1 << 1))
            break;
    }
    if (i >= 100) return -1;

    SiiRegWrite(dev, SII_TPI_SYS_CTRL, SII_SYS_DDC_REQ | (1 << 1));

    {
        uint8_t addr = 0;
        I2cMstWrite(dev->i2c, 0x50, &addr, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
        I2cMstRead(dev->i2c, 0x50, edid, len, I2C_BUS_RESTART, I2C_BUS_STOP);
    }

    SiiRegWrite(dev, SII_TPI_SYS_CTRL, 0x00);

    for (i = 0; i < 100; i++) {
        ctrl = SiiRegRead(dev, SII_TPI_SYS_CTRL);
        if (!(ctrl & (SII_SYS_DDC_REQ | (1 << 1))))
            break;
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
    uint8_t ctrl = SiiRegRead(dev, SII_TPI_AUDIO_CTRL);

    if (mute)
        ctrl |= SII_AUDIO_MUTE;
    else
        ctrl &= ~SII_AUDIO_MUTE;

    SiiRegWrite(dev, SII_TPI_AUDIO_CTRL, ctrl);
}
