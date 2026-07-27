#ifndef __CS42L51_H_
#define __CS42L51_H_

#include <stdint.h>
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_i2c.h"


/* I2C 从机地址宏: 7-bit 地址左移 1 位 (适配 I2cMstWrite CR2 写格式) */
#define CS42L51_ADDR(ad0)      ((uint16_t)((0x4A + ((ad0) ? 1 : 0)) << 1))


/* ================================================================== */
/*  寄存器地址枚举                                                      */
/* ================================================================== */

typedef enum {
    CS42L51_REG_POWER_CTL1    = 0x02,   /* 电源控制 1 */
    CS42L51_REG_SPEED_CTL     = 0x03,   /* 速度控制 / 电源控制 2 */
    CS42L51_REG_IFACE_CTL     = 0x04,   /* 数字接口格式 */
    CS42L51_REG_MIC_CTL       = 0x05,   /* MIC 偏压/增益 */
    CS42L51_REG_ADC_CTL       = 0x06,   /* ADC HPF 配置 */
    CS42L51_REG_ADC_INPUT_SEL = 0x07,   /* ADC 输入选择 / 反相 */
    CS42L51_REG_DAC_OUT_CTL   = 0x08,   /* DAC 输出 (+ HP 增益) */
    CS42L51_REG_DAC_CTL       = 0x09,   /* DAC 控制 (数据源/去加重) */
    CS42L51_REG_ALCA_PGAA     = 0x0A,   /* ALC A / PGA A */
    CS42L51_REG_ALCB_PGAB     = 0x0B,   /* ALC B / PGA B */
    CS42L51_REG_ADCA_ATT      = 0x0C,   /* ADC A 衰减 */
    CS42L51_REG_ADCB_ATT      = 0x0D,   /* ADC B 衰减 */
    CS42L51_REG_ADCMIXA_VOL   = 0x0E,   /* ADC 混音 A 音量 */
    CS42L51_REG_ADCMIXB_VOL   = 0x0F,   /* ADC 混音 B 音量 */
    CS42L51_REG_PCMMIXA_VOL   = 0x10,   /* PCM 混音 A 音量 */
    CS42L51_REG_PCMMIXB_VOL   = 0x11,   /* PCM 混音 B 音量 */
    CS42L51_REG_BEEP_FREQ     = 0x12,   /* 蜂鸣频率 / 开启 */
    CS42L51_REG_BEEP_OFF      = 0x13,   /* 蜂鸣关闭 / 音量 */
    CS42L51_REG_BEEP_CTL      = 0x14,   /* 蜂鸣控制 / 音调 */
    CS42L51_REG_TONE_CTL      = 0x15,   /* 高低音 */
    CS42L51_REG_AOUTA_VOL     = 0x16,   /* 模拟输出 A 音量 */
    CS42L51_REG_AOUTB_VOL     = 0x17,   /* 模拟输出 B 音量 */
    CS42L51_REG_PCM_ADC_MIXER = 0x18,   /* PCM/ADC 混音器 */
    CS42L51_REG_LIM_THRESH    = 0x19,   /* 限幅器阈值 */
    CS42L51_REG_LIM_RELEASE   = 0x1A,   /* 限幅器释放 */
    CS42L51_REG_LIM_ATTACK    = 0x1B,   /* 限幅器攻击 */
    CS42L51_REG_ALC_CTL       = 0x1C,   /* ALC 使能 / 攻击 */
    CS42L51_REG_ALC_RELEASE   = 0x1D,   /* ALC 释放 */
    CS42L51_REG_ALC_THRESH    = 0x1E,   /* ALC 阈值 */
    CS42L51_REG_NOISE_GATE    = 0x1F,   /* 噪声门 */
    CS42L51_REG_STATUS        = 0x20,   /* 状态 (只读) */
    CS42L51_REG_CHARGE_PUMP   = 0x21,   /* 电荷泵频率 */
} Cs42l51Reg_t;


/* ================================================================== */
/*  位域定义                                                           */
/* ================================================================== */

/* ---- 02h Power Control 1 ---- */
#define CS42L51_PDN_DACB   (1 << 7)
#define CS42L51_PDN_DACA   (1 << 6)
#define CS42L51_PDN_PGAB   (1 << 5)
#define CS42L51_PDN_PGAA   (1 << 4)
#define CS42L51_PDN_ADCB   (1 << 3)
#define CS42L51_PDN_ADCA   (1 << 2)
#define CS42L51_PDN        (1 << 0)

/* ---- 03h Speed Control ---- */
#define CS42L51_AUTO       (1 << 7)
#define CS42L51_SPEED_QSM  (0x03 << 5)  /* 4-12.5 kHz */
#define CS42L51_SPEED_HSM  (0x02 << 5)  /* 12.5-25 kHz */
#define CS42L51_SPEED_SSM  (0x01 << 5)  /* 4-50 kHz */
#define CS42L51_SPEED_DSM  (0x00 << 5)  /* 50-100 kHz */
#define CS42L51_MCLKDIV2   (1 << 0)

/* ---- 04h Interface Control ---- */
#define CS42L51_MS_SLAVE   (0 << 6)
#define CS42L51_MS_MASTER  (1 << 6)
#define CS42L51_DIF_I2S    (0x00 << 3)
#define CS42L51_DIF_LJ     (0x01 << 3)
#define CS42L51_DIF_RJ24   (0x02 << 3)
#define CS42L51_DIF_RJ20   (0x03 << 3)
#define CS42L51_DIF_RJ18   (0x04 << 3)
#define CS42L51_DIF_RJ16   (0x05 << 3)

/* ---- 05h MIC Control ---- */
#define CS42L51_MIC_GAIN_Msk    (3 << 6)
#define CS42L51_MIC_GAIN_0dB    (0 << 6)
#define CS42L51_MIC_GAIN_15dB   (1 << 6)
#define CS42L51_MIC_GAIN_22p5dB (2 << 6)
#define CS42L51_MIC_GAIN_30dB   (3 << 6)
#define CS42L51_MIC2_SEL        (1 << 5)  /* MIC2 接到两个 ADC 通道 */
#define CS42L51_BIAS_EN         (1 << 4)  /* MIC 偏压使能 */

/* ---- 07h ADC Input Select ---- */
#define CS42L51_AINA_SEL(s) (((s) & 0xF) << 4)
#define CS42L51_AINB_SEL(s) (((s) & 0xF) << 0)

/* ADC 输入选择编码 (用于 AINA_SEL / AINB_SEL) */
#define CS42L51_IN_SEL_AIN1  0   /* LINEIN1 */
#define CS42L51_IN_SEL_AIN2  1   /* LINEIN2 */
#define CS42L51_IN_SEL_AIN3  2   /* LINEIN3 */
#define CS42L51_IN_SEL_MIC2  10  /* MICIN2 */
#define CS42L51_IN_SEL_MIC1  11  /* MICIN1 */

/* ---- 08h DAC Output Control ---- */
#define CS42L51_HP_GAIN_Msk  (7 << 5)
#define CS42L51_HP_GAIN_0p6  (0x03 << 5)  /* 0.6047×, 复位值 */
#define CS42L51_HP_GAIN_1p0  (0x06 << 5)  /* 1.000×, 0dB */
#define CS42L51_HP_GAIN_1p14 (0x07 << 5)  /* 1.143×, +1.1dB */
#define CS42L51_DACA_MUTE    (1 << 0)
#define CS42L51_DACB_MUTE    (1 << 1)

/* ---- 09h DAC Control ---- */
#define CS42L51_DATA_SEL(s)  (((s) & 3) << 6)
#define CS42L51_DATA_SEL_PCM   0   /* PCM → DAC */
#define CS42L51_DATA_SEL_DSP   1   /* DSP → DAC (使能音量控制) */
#define CS42L51_DATA_SEL_ADC   2   /* ADC → DAC (自回环) */
#define CS42L51_DEEMPH     (1 << 3)
#define CS42L51_AMUTE      (1 << 2)

/* ---- 0Eh/0Fh ADCMIX Volume ---- */
#define CS42L51_ADCMIX_MUTE    (1 << 7)
/* ADCMIX 音量值: 0=0dB, 1=+0.5dB ... 24=+12dB */
#define CS42L51_ADCMIX_VOL_12dB  0x18  /* +12dB */

/* ---- 10h/11h PCM Mixer Volume ---- */
#define CS42L51_PCMMIX_MUTE  (1 << 7)

/* ---- 16h/17h AOUT Volume ---- */
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
