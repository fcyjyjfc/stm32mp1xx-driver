/*
 * cs42l51.c — CS42L51 CODEC 驱动
 *
 *   Cs42l51RegWrite  — I2C 单字节寄存器写
 *   Cs42l51Init      — 完全初始化 (standby + 配置 + PDN=0)
 *   Cs42l51SetVolume — 模拟输出音量 (AOUTA/B)
 *   Cs42l51Mute      — 静音 / 取消静音
 *   Cs42l51Deinit    — 下电
 */

#include "cs42l51.h"


/* ---- 粗略忙等待 (Cortex-A7 @ ~650MHz, 仅供参考) ------------------- */
static void cs42l51_delay_ms(uint32_t ms)
{
    volatile uint32_t loop = ms * 325000U;
    while (loop--)
        __asm__("nop");
}


/* ---- I2C 写辅助: 先发 MAP 再发数据 -------------------------------- */
static void cs42l51_i2c_write(Cs42l51_t *codec, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = { reg, val };
    I2cMstWrite(codec->i2c, codec->slave_addr, data, 2,
                I2C_BUS_START, I2C_BUS_STOP);
    /* CS42L51 连续写入需间隔, 否则后续写静默丢失 */
    cs42l51_delay_ms(1);
}

/* ---- I2C 读辅助: 发 MAP 地址 + Restart + 读 1 字节 ----------------- */
static uint8_t cs42l51_i2c_read(Cs42l51_t *codec, uint8_t reg)
{
    volatile I2cRegs_t *i2c = codec->i2c;
    uint8_t val = 0;

    /* Step 1: 写寄存器地址 (无 STOP, 后续 Restart) */
    I2cMstWrite(i2c, codec->slave_addr, &reg, 1,
                I2C_BUS_START, I2C_BUS_NO_STOP);

    /* 等待传输完成 (TC) 再发起 Restart */
    while (!(i2c->ISR & (1U << 6)))
        ;

    /* Step 2: Restart + 读 1 字节 + STOP */
    I2cMstRead(i2c, codec->slave_addr, &val, 1,
               I2C_BUS_RESTART, I2C_BUS_STOP);

    return val;
}

/* ================================================================== */
/*  Cs42l51RegWrite                                                     */
/* ================================================================== */

/**
 * @brief  写 CS42L51 单字节寄存器
 * @param  codec  驱动上下文 (I2C + 地址)
 * @param  reg    寄存器地址 (01h~21h)
 * @param  val    写入值
 */
void Cs42l51RegWrite(Cs42l51_t *codec, uint8_t reg, uint8_t val)
{
    cs42l51_i2c_write(codec, reg, val);
}


/* ================================================================== */
/*  Cs42l51RegRead                                                      */
/* ================================================================== */

/**
 * @brief  读 CS42L51 单字节寄存器
 * @param  codec  驱动上下文
 * @param  reg    寄存器地址 (01h~21h)
 * @return        寄存器值, 0xFF 表示通信失败
 */
uint8_t Cs42l51RegRead(Cs42l51_t *codec, uint8_t reg)
{
    return cs42l51_i2c_read(codec, reg);
}


/* ================================================================== */
/*  Cs42l51Init — 完全初始化                                            */
/* ================================================================== */

/**
 * @brief  初始化 CS42L51 (I2S 从机播放)
 *
 *   如果 rst_port != NULL, 会先释放 RESET 引脚.
 *   调用前 SAI MCLK 必须已稳定输出.
 *   函数内部: RESET↑ → PDN=1 → 配置寄存器 → PDN=0 → 等待 ~50ms.
 *
 *   初始化完成后 AOUTA/B 仍处于静音状态 (MUTE=1, VOL=0),
 *   需调用 Cs42l51SetVolume 设音量.
 *
 * @param  codec  驱动上下文
 */
void Cs42l51Init(Cs42l51_t *codec)
{
    /* ---- 0. 释放 RESET (若指定引脚) ---- */
    if (codec->rst_port)
    {
        GpioOutHi(codec->rst_port, codec->rst_pin);

        /* 等待复位后稳定 */
        volatile uint32_t t = 10000;
        while (t--);
    }

    /* ---- 1. PDN=1, 进入 Standby (10ms 内必须写入) ---- */
    cs42l51_i2c_write(codec, CS42L51_REG_POWER_CTL1, CS42L51_PDN);

    /* ---- 2. 配置寄存器 ---- */
    /* 03h: AUTO=1 (自动检测 MCLK/LRCK), MIC 断电 */
    cs42l51_i2c_write(codec, CS42L51_REG_SPEED_CTL,
                      CS42L51_AUTO);

    /* 04h: I2S 从机 (复位值即可) */
    cs42l51_i2c_write(codec, CS42L51_REG_IFACE_CTL,
                      CS42L51_MS_SLAVE | CS42L51_DIF_I2S);

    /* 08h: HP_GAIN=0.6× (复位), DACA/B_MUTE=0 */
    cs42l51_i2c_write(codec, CS42L51_REG_DAC_OUT_CTL,
                      CS42L51_HP_GAIN_0p6);

    /* 09h: DATA_SEL=DSP (使能 AOUT/PCMMIX 音量控制), AMUTE=0 */
    cs42l51_i2c_write(codec, CS42L51_REG_DAC_CTL,
                      CS42L51_DATA_SEL(CS42L51_DATA_SEL_DSP));

    /* 10h/11h: PCMMIX +8dB (MUTE=0, VOL=16=+8dB) */
    cs42l51_i2c_write(codec, CS42L51_REG_PCMMIXA_VOL, 0x10);
    cs42l51_i2c_write(codec, CS42L51_REG_PCMMIXB_VOL, 0x10);

    /* 16h/17h: AOUT 静音, 音量最小 (稍后由 SetVolume 打开) */
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTA_VOL, CS42L51_AOUT_MUTE);
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTB_VOL, CS42L51_AOUT_MUTE);

    /* 21h: 电荷泵频率 = 960kHz (复位值 0x05, 8kHz/256Fs 适用) */
    cs42l51_i2c_write(codec, CS42L51_REG_CHARGE_PUMP, 0x05);

    /* 缓存音量 = 0 (MUTE) */
    codec->vol = 0;

    /* ---- 3. PDN=0, 上电 (此时 MCLK 必须已稳定) ---- */
    cs42l51_i2c_write(codec, CS42L51_REG_POWER_CTL1, 0x00);

    /* ---- 4. 等待 DAC + 电荷泵初始化 ---- */
    cs42l51_delay_ms(50);

    /* ---- 5. 上电后再写一次关键寄存器 (有些寄存器需 PDN=0 后才生效) ---- */
    cs42l51_i2c_write(codec, CS42L51_REG_POWER_CTL1, 0x00);     /* PDN=0 再确认 */
    cs42l51_i2c_write(codec, CS42L51_REG_DAC_CTL,
                      CS42L51_DATA_SEL(CS42L51_DATA_SEL_DSP));  /* DATA_SEL=DSP */
    cs42l51_i2c_write(codec, CS42L51_REG_PCMMIXA_VOL, 0x10);    /* PCMMIX +8dB */
    cs42l51_i2c_write(codec, CS42L51_REG_PCMMIXB_VOL, 0x10);
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTA_VOL, CS42L51_AOUT_MUTE);  /* 保持静音 */
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTB_VOL, CS42L51_AOUT_MUTE);
}


/* ================================================================== */
/*  Cs42l51SetVolume                                                    */
/* ================================================================== */

/**
 * @brief  设模拟输出音量 (AOUTA + AOUTB)
 * @param  codec  驱动上下文
 * @param  vol    音量值
 *              bit7: 0=正常, 1=静音
 *              bit6:0 编码 (0=-102dB, 0x60=0dB, 0x18=+12dB)
 *
 *   内部缓存当前音量, 供 Cs42l51Mute 恢复用.
 */
void Cs42l51SetVolume(Cs42l51_t *codec, uint8_t vol)
{
    codec->vol = vol;
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTA_VOL, vol);
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTB_VOL, vol);
}


/* ================================================================== */
/*  Cs42l51Mute                                                         */
/* ================================================================== */

/**
 * @brief  静音 / 取消静音
 * @param  codec  驱动上下文
 * @param  mute   1=静音, 0=恢复静音前音量
 *
 *   静音时写 AOUT=0x80 (MUTE=1, VOL=0).
 *   取消静音时恢复缓存音量.
 */
void Cs42l51Mute(Cs42l51_t *codec, uint8_t mute)
{
    if (mute) {
        cs42l51_i2c_write(codec, CS42L51_REG_AOUTA_VOL, CS42L51_AOUT_MUTE);
        cs42l51_i2c_write(codec, CS42L51_REG_AOUTB_VOL, CS42L51_AOUT_MUTE);
    } else {
        uint8_t v = codec->vol;
        cs42l51_i2c_write(codec, CS42L51_REG_AOUTA_VOL, v);
        cs42l51_i2c_write(codec, CS42L51_REG_AOUTB_VOL, v);
    }
}


/* ================================================================== */
/*  Cs42l51Deinit                                                        */
/* ================================================================== */

/**
 * @brief  下电 (PDN=1, Standby)
 * @param  codec  驱动上下文
 */
void Cs42l51Deinit(Cs42l51_t *codec)
{
    /* 先静音 */
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTA_VOL, CS42L51_AOUT_MUTE);
    cs42l51_i2c_write(codec, CS42L51_REG_AOUTB_VOL, CS42L51_AOUT_MUTE);

    /* PDN=1 → Standby */
    cs42l51_i2c_write(codec, CS42L51_REG_POWER_CTL1, CS42L51_PDN);

    /* RESET 拉低 (若指定引脚) */
    if (codec->rst_port)
        GpioOutLow(codec->rst_port, codec->rst_pin);
}


/* ================================================================== */
/*  音量换算                                                          */
/* ================================================================== */

/**
 * @brief  half-dB → AOUT 寄存器值
 * @param  half_db  0.5dB 单位 (2=1dB, 24=+12dB), 仅增益区
 * @return 0x00~0x18, >0x18 截断
 */
uint8_t Cs42l51AoutHalfDbToReg(int half_db)
{
    if (half_db <= 0) return 0;
    return (half_db > 24) ? 24 : (uint8_t)half_db;
}


/**
 * @brief  half-dB → PCMMIX 寄存器值
 * @param  half_db  0.5dB 单位 (+24=+12dB, -102=-51dB)
 * @return 0x00~0x18 (增益) 或 0x19~0x7F (衰减)
 */
uint8_t Cs42l51PcmmixHalfDbToReg(int half_db)
{
    if (half_db > 24)
        half_db = 24;
    if (half_db >= 0)
        return (uint8_t)half_db;            /* 0x00~0x18 增益 */
    /* 衰减: half_db + 128 = 0x19~0x7F */
    return (uint8_t)((half_db + 128) & 0x7F);
}
