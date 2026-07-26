#ifndef __CS42L51_H_
#define __CS42L51_H_

#include <stdint.h>
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_i2c.h"


/* I2C 从机地址宏: 7-bit 地址左移 1 位 (适配 I2cMstWrite CR2 写格式) */
#define CS42L51_ADDR(ad0)      ((uint16_t)((0x4A + ((ad0) ? 1 : 0)) << 1))

/* ================================================================== */
/*  寄存器地址                                                          */
/* ================================================================== */

#define CS42L51_REG_POWER_CTL1     0x02
#define CS42L51_REG_SPEED_CTL      0x03
#define CS42L51_REG_IFACE_CTL      0x04
#define CS42L51_REG_DAC_OUT_CTL    0x08
#define CS42L51_REG_DAC_CTL        0x09
#define CS42L51_REG_PCMMIXA_VOL    0x10
#define CS42L51_REG_PCMMIXB_VOL    0x11
#define CS42L51_REG_AOUTA_VOL      0x16
#define CS42L51_REG_AOUTB_VOL      0x17
#define CS42L51_REG_CHARGE_PUMP    0x21

/* ================================================================== */
/*  位域定义                                                           */
/* ================================================================== */

/* 02h Power Control 1 */
#define CS42L51_PDN_DACB   (1 << 7)
#define CS42L51_PDN_DACA   (1 << 6)
#define CS42L51_PDN_PGAB   (1 << 5)
#define CS42L51_PDN_PGAA   (1 << 4)
#define CS42L51_PDN_ADCB   (1 << 3)
#define CS42L51_PDN_ADCA   (1 << 2)
#define CS42L51_PDN        (1 << 0)

/* 03h Speed Control */
#define CS42L51_AUTO       (1 << 7)
#define CS42L51_SPEED_QSM  (0x03 << 5)  /* 4-12.5 kHz */
#define CS42L51_SPEED_HSM  (0x02 << 5)  /* 12.5-25 kHz */
#define CS42L51_SPEED_SSM  (0x01 << 5)  /* 4-50 kHz */
#define CS42L51_SPEED_DSM  (0x00 << 5)  /* 50-100 kHz */
#define CS42L51_MCLKDIV2   (1 << 0)

/* 04h Interface Control */
#define CS42L51_MS_SLAVE   (0 << 6)
#define CS42L51_MS_MASTER  (1 << 6)
#define CS42L51_DIF_I2S    (0x00 << 3)
#define CS42L51_DIF_LJ     (0x01 << 3)
#define CS42L51_DIF_RJ24   (0x02 << 3)
#define CS42L51_DIF_RJ20   (0x03 << 3)
#define CS42L51_DIF_RJ18   (0x04 << 3)
#define CS42L51_DIF_RJ16   (0x05 << 3)

/* 08h DAC Output Control */
#define CS42L51_HP_GAIN_Msk  (7 << 5)
#define CS42L51_HP_GAIN_0p6  (0x03 << 5)  /* 0.6047×, 复位值 */
#define CS42L51_HP_GAIN_1p0  (0x06 << 5)  /* 1.000×, 0dB */
#define CS42L51_HP_GAIN_1p14 (0x07 << 5)  /* 1.143×, +1.1dB */
#define CS42L51_DACA_MUTE    (1 << 0)
#define CS42L51_DACB_MUTE    (1 << 1)

/* 09h DAC Control */
#define CS42L51_DATA_SEL(s)  (((s) & 3) << 6)
#define CS42L51_DATA_SEL_PCM   0   /* PCM → DAC */
#define CS42L51_DATA_SEL_DSP   1   /* DSP → DAC */
#define CS42L51_DATA_SEL_ADC   2   /* ADC → DAC */
#define CS42L51_DEEMPH     (1 << 3)
#define CS42L51_AMUTE      (1 << 2)

/* 10h/11h PCM Mixer Volume */
#define CS42L51_PCMMIX_MUTE  (1 << 7)

/* 16h/17h AOUT Volume */
#define CS42L51_AOUT_MUTE    (1 << 7)

/* ================================================================== */
/*  驱动上下文结构体                                                    */
/* ================================================================== */

typedef struct {
    volatile I2cRegs_t  *i2c;        /* I2C 外设基址 */
    uint16_t             slave_addr;  /* 从机地址 (7-bit << 1) */
    volatile GpioRegs_t *rst_port;   /* RESET 引脚 GPIO 端口 (NULL=不控制) */
    uint8_t              rst_pin;    /* RESET 引脚号 */
    uint8_t              vol;        /* 当前音量缓存 (供 Mute 恢复) */
} Cs42l51_t;

/* ================================================================== */
/*  API                                                                */
/* ================================================================== */

void Cs42l51RegWrite(Cs42l51_t *codec, uint8_t reg, uint8_t val);
uint8_t Cs42l51RegRead(Cs42l51_t *codec, uint8_t reg);
void Cs42l51Init(Cs42l51_t *codec);
void Cs42l51SetVolume(Cs42l51_t *codec, uint8_t vol);
void Cs42l51Mute(Cs42l51_t *codec, uint8_t mute);
void Cs42l51Deinit(Cs42l51_t *codec);

/* ---- 音量换算 (输入: half_db = 0.5dB/单位, 如 2=1dB, 24=+12dB) ---- */
uint8_t Cs42l51AoutHalfDbToReg(int half_db);
uint8_t Cs42l51PcmmixHalfDbToReg(int half_db);


#endif /* __CS42L51_H_ */
