#ifndef __SII9022A_H_
#define __SII9022A_H_

#include <stdint.h>
#include "stm32mp1xx_i2c.h"

/* =================== TPI 寄存器偏移 =================== */
#define SII_TPI_PIXEL_CLOCK_L   (0x00)
#define SII_TPI_PIXEL_CLOCK_H   (0x01)
#define SII_TPI_VFREQ_L         (0x02)
#define SII_TPI_VFREQ_H         (0x03)
#define SII_TPI_PIXELS_L        (0x04)
#define SII_TPI_PIXELS_H        (0x05)
#define SII_TPI_LINES_L         (0x06)
#define SII_TPI_LINES_H         (0x07)
#define SII_TPI_INPUT_BUS       (0x08)
#define SII_TPI_INPUT_FMT       (0x09)
#define SII_TPI_OUTPUT_FMT      (0x0A)
#define SII_TPI_YC_MODE         (0x0B)
#define SII_TPI_AVI_DBYTE0      (0x0C)
#define SII_TPI_AVI_DBYTE1      (0x0D)
#define SII_TPI_SYS_CTRL        (0x1A)
#define SII_TPI_DEVICE_ID       (0x1B)
#define SII_TPI_POWER_STATE     (0x1E)
#define SII_TPI_AUDIO_CTRL      (0x26)
#define SII_TPI_AUDIO_FS        (0x27)
#define SII_TPI_INT_ENABLE      (0x3C)
#define SII_TPI_INT_STATUS      (0x3D)
#define SII_TPI_SYNC_CTRL       (0x60)
#define SII_TPI_DE_CTRL         (0x63)
#define SII_TPI_DE_DLY          (0x62)
#define SII_TPI_DE_TOP          (0x64)
#define SII_TPI_DE_CNT          (0x66)
#define SII_TPI_DE_LIN          (0x68)
#define SII_TPI_H_RES           (0x6A)
#define SII_TPI_V_RES           (0x6C)
#define SII_TPI_IDX_PAGE        (0xBC)
#define SII_TPI_IDX_ADDR        (0xBD)
#define SII_TPI_IDX_DATA        (0xBE)
#define SII_TPI_TPI_MODE        (0xC7)

/* =================== 位定义 =================== */

/* 0x08 - InputBus */
#define SII_INPUT_BUS_TCLK(x)   (((x) & 3) << 6)
#define SII_INPUT_BUS_TCLK_X1   (1 << 6)
#define SII_INPUT_BUS_FULL      (1 << 5)
#define SII_INPUT_BUS_EDGE_RISE (1 << 4)
#define SII_INPUT_BUS_PR_NONE   (0)

/* 0x09 - InputFormat */
#define SII_INPUT_DEPTH_8BIT    (0)
#define SII_INPUT_CS_RGB        (0)

/* 0x0A - OutputFormat */
#define SII_OUTPUT_CS_RGB       (0)

/* 0x1A - SysCtrl */
#define SII_SYS_AVMUTE          (1 << 3)
#define SII_SYS_TMDS_PD         (1 << 4)
#define SII_SYS_DDC_REQ         (1 << 2)
#define SII_SYS_HDMI_MODE       (1 << 0)

/* 0x1E - PowerState */
#define SII_POWER_D0            (0)
#define SII_POWER_D2            (1)
#define SII_POWER_D3_HOT        (2)
#define SII_POWER_D3_COLD       (3)

/* 0x26 - Audio */
#define SII_AUDIO_SPDIF         (1 << 6)
#define SII_AUDIO_I2S           (2 << 6)
#define SII_AUDIO_MUTE          (1 << 4)

/* 0x60 - SyncCtrl */
#define SII_SYNC_EXTERNAL       (0)

/* 0x63 - DECtrl */
#define SII_DE_GEN_EN           (1 << 6)
#define SII_DE_VS_NEG           (1 << 5)
#define SII_DE_HS_NEG           (1 << 4)

/* 0x3D - IntStatus */
#define SII_INT_HPD_STATE       (1 << 2)
#define SII_INT_HPD_EVENT       (1 << 0)

/* 内部寄存器 */
#define SII_IDX_SRC_TERM        (0x82)
#define SII_IDX_PAGE0           (0x01)

/* =================== 视频模式 =================== */
typedef struct {
    uint32_t pixel_clock;       /* 像素时钟 Hz */
    uint16_t htotal;            /* 每行总像素 */
    uint16_t vtotal;            /* 总行数 */
    uint16_t vfreq;             /* 刷新率 Hz */
} SiiVideoMode_t;

/* =================== 驱动上下文 =================== */
typedef struct {
    volatile I2cRegs_t *i2c;    /* I2C 外设基址 */
    uint16_t            addr;   /* TPI 从机地址 (7-bit << 1) */
} Sii9022a_t;

/* =================== API =================== */

/* I2C 读写 */
void    SiiRegWrite(Sii9022a_t *dev, uint8_t reg, uint8_t val);
int     SiiRegRead(Sii9022a_t *dev, uint8_t reg);
void    SiiRegWriteBurst(Sii9022a_t *dev, uint8_t reg, const uint8_t *data, int len);
void    SiiIdxWrite(Sii9022a_t *dev, uint8_t page, uint8_t offset, uint8_t val);
int     SiiIdxRead(Sii9022a_t *dev, uint8_t page, uint8_t offset);

/* 初始化与控制 */
int     SiiInit(Sii9022a_t *dev);
void    SiiSetPowerState(Sii9022a_t *dev, uint8_t state);
void    SiiTmdsOutput(Sii9022a_t *dev, int enable);
int     SiiDetect(Sii9022a_t *dev);

/* 视频 */
void    SiiSetVideoMode(Sii9022a_t *dev, const SiiVideoMode_t *vm);
void    SiiSetColorFormat(Sii9022a_t *dev, uint8_t input_cs, uint8_t output_cs);
void    SiiSetAVIInfoFrame(Sii9022a_t *dev, const uint8_t *data, int len);
void    SiiSetSyncPolarity(Sii9022a_t *dev, int hsync_neg, int vsync_neg);
int     SiiSetDeGenerator(Sii9022a_t *dev, uint16_t de_dly, uint16_t de_top,
                          uint16_t de_cnt, uint16_t de_lin);

/* 热插拔 & EDID */
int     SiiIsConnected(Sii9022a_t *dev);
void    SiiClearInterrupt(Sii9022a_t *dev, uint8_t mask);
int     SiiReadEDID(Sii9022a_t *dev, uint8_t *edid, int len);

/* 音频 */
void    SiiAudioSpdifInit(Sii9022a_t *dev);
void    SiiAudioSpdifMute(Sii9022a_t *dev, int mute);

#endif /* __SII9022A_H_ */
