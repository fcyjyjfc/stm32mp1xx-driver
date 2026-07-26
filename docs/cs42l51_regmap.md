# CS42L51 寄存器参考

基于 DS679F1 (JUL '06).

## 寄存器速查

| 地址 | 名称            | 复位值     | 用途               |
|------|-----------------|-----------|-------------------|
| 01h  | Chip ID         | 0xB1 (r)  | 芯片版本           |
| 02h  | Power Ctl 1     | 0x00      | DAC/ADC/PGA 电源   |
| 03h  | Speed Ctl & Pwr 2 | 0x51    | 采样率/时钟分频     |
| 04h  | Interface Ctl   | 0x00      | I2S 格式/主从       |
| 05h  | MIC Control     | 0x00      | MIC 偏置/增益       |
| 06h  | ADC Control     | 0xA0      | ADC HPF 配置        |
| 07h  | ADC Input Sel   | 0x00      | 输入选择/反相/静音   |
| 08h  | DAC Output Ctl  | 0x60      | 耳机增益/自动静音    |
| 09h  | DAC Control     | 0x00      | 去加重/数据选择      |
| 0Ah  | ALCA & PGAA     | 0x00      | ALC/PGA 左声道      |
| 0Bh  | ALCB & PGAB     | 0x00      | ALC/PGA 右声道      |
| 0Ch  | ADCA Att        | 0x00      | ADC 左衰减          |
| 0Dh  | ADCB Att        | 0x00      | ADC 右衰减          |
| 0Eh  | ADCMIXA Vol     | 0x00      | ADC混音左音量      |
| 0Fh  | ADCMIXB Vol     | 0x00      | ADC混音右音量      |
| 10h  | PCMMIXA Vol     | 0x00      | PCM混音左音量      |
| 11h  | PCMMIXB Vol     | 0x00      | PCM混音右音量      |
| 12h  | Beep Freq & On  | 0x00      | 蜂鸣频率/开启时间   |
| 13h  | Beep Off & Vol  | 0x00      | 蜂鸣关闭时间/音量   |
| 14h  | Beep Ctl & Tone | 0x00      | 蜂鸣配置            |
| 15h  | Tone Control    | 0x88      | 高低音               |
| 16h  | AOUTA Vol       | 0x00      | 左声道主音量(- mute)|
| 17h  | AOUTB Vol       | 0x00      | 右声道主音量(- mute)|
| 18h  | PCM/ADC Mixer   | 0x00      | 声道混音             |
| 19h  | Limiter Thresh  | 0x00      | 限幅器阈值           |
| 1Ah  | Limiter Release | 0x7F      | 限幅器释放速率       |
| 1Bh  | Limiter Attack  | 0x00      | 限幅器攻击速率       |
| 1Ch  | ALC Enable/Attk | 0x00      | ALC 使能/攻击速率    |
| 1Dh  | ALC Release     | 0x7F      | ALC 释放速率         |
| 1Eh  | ALC Threshold   | 0x00      | ALC 阈值             |
| 1Fh  | Noise Gate      | 0x00      | 噪声门               |
| 20h  | Status          | 0x00 (r)  | 状态 / 溢出标志      |
| 21h  | Charge Pump Freq| 0x05      | 电荷泵频率           |

## 关键寄存器说明 (播放场景)

### 02h — Power Control 1

| 位 | 名称      | 说明                                |
|----|-----------|-------------------------------------|
| 7  | PDN_DACB  | 0=右DAC正常运行                      |
| 6  | PDN_DACA  | 0=左DAC正常运行                      |
| 5  | PDN_PGAB  | 0=右PGA正常运行                      |
| 4  | PDN_PGAA  | 0=左PGA正常运行                      |
| 3  | PDN_ADCB  | 0=右ADC正常运行                      |
| 2  | PDN_ADCA  | 0=左ADC正常运行                      |
| 1  | Reserved  |                                     |
| 0  | PDN       | 0=全局正常运行, 1=全局断电 (复位默认) |

播放时: 清 `PDN` 和 `PDN_DACA/B`. ADC/PGA 可保持断电 (PDN=1) 省电.

> 注意: 单通道断电必须先 PDN=1, 再设单通道 PDN, 再清 PDN. 否则仅静音不省电.

### 03h — Speed Control & Power Control 2

| 位 | 名称         | 说明                                       |
|----|--------------|--------------------------------------------|
| 7  | AUTO         | 1=自动检测 MCLK/LRCK 比例 (复位默认)         |
| 6  | SPEED1       | 采样率模式: 00=单速(4-50k), 01=倍速(50-100k) |
| 5  | SPEED0       |                                            |
| 4  | 3-ST_SP      | 0=正常SDIN输出, 1=高阻 (用于多CODEC复用)   |
| 3  | PDN_MICB     | 1=右MIC断电                                |
| 2  | PDN_MICA     | 1=左MIC断电                                |
| 1  | PDN_MICBIAS  | 1=MIC偏置断电                              |
| 0  | MCLKDIV2     | 0=MCLK不分频, 1=MCLK÷2                    |

播放时: 保留 `AUTO=1`(或手动配 `SPEED`), MIC 相关可断电.

### 04h — Interface Control

| 位 | 名称       | 说明                                    |
|----|------------|----------------------------------------|
| 7  | SDOUT->SDIN| 0=正常, 1=SDIN引脚复用为SDOUT (用于I2S环回) |
| 6  | M/S        | 0=从机(SCLK/LRCK由外部提供), 1=主机       |
| 5  | DAC_DIF2   | DAC数字接口格式:                          |
| 4  | DAC_DIF1   | 000=I2S (复位默认), 001=LJ, 010=RJ-16    |
| 3  | DAC_DIF0   | 011=RJ-24, 100=RJ-32, 101=Do'          |
| 2  | ADC_DIF    | ADC格式: 0=I2S/LJ (由DAC_DIF决定)       |
| 1  | DIGMIX     | 数字混音: 0=正常, 1=ADC+PCM混音到DAC    |
| 0  | MICMIX     | MIC混音: 0=正常, 1=MIC混音到ADC         |

播放时: `M/S=0`(从机), `DAC_DIF=000`(I2S).

### 08h — DAC Output Control

| 位 | 名称        | 说明                                   |
|----|-------------|----------------------------------------|
| 7  | HP_GAIN2    | 耳机增益 (电压倍率):                     |
| 6  | HP_GAIN1    | 011=0.6047× (~ -20dB, 复位)            |
| 5  | HP_GAIN0    | 000=0.3959×, 001=0.4571×, 010=0.5111× |
| 4  | DAC_SNG     | 100=0.7099×, 101=0.8399×, 110=1.000×  |
|    |             | 111=1.143× (~ +4dB)                    |
| 4  | DAC_SNGVOL  | 0=立体声, 1=单声道 (左复制到右)          |
| 3  | INV_PCMB    | PCM B 反相                              |
| 2  | INV_PCMA    | PCM A 反相                              |
| 1  | DACB_MUTE   | 右 DAC 静音 (注意: 与 AMUTE 不同!)      |
| 0  | DACA_MUTE   | 左 DAC 静音                             |

播放时: 设 `HP_GAIN` 按负载选增益, 注意复位 `DACA/B_MUTE=0` 正常.

> 注意: 寄存器描述中位 1/0 实际为 DACA/B_MUTE, 自动静音 AMUTE 在 09h bit2.

### 09h — DAC Control

| 位 | 名称         | 说明                                   |
|----|--------------|----------------------------------------|
| 7  | DATA_SEL1    | DAC 数据源选择:                         |
| 6  | DATA_SEL0    | 00=PCM→DAC (默认), 01=DSP→DAC          |
|    |              | 10=ADC→DAC, 11=Reserved               |
| 5  | FREEZE       | 1=冻结寄存器更新 (批量修改时用)          |
| 4  | Reserved     |                                        |
| 3  | DEEMPH       | 1=使能去加重 (44.1kHz, 需 DATA_SEL=01) |
| 2  | AMUTE        | 1=自动静音 (检测到 8192 个零样本时), 0=关 |
| 1  | DAC_SZC1     | 音量过渡模式:                           |
| 0  | DAC_SZC0     | 00=立即, 01=过零, 10=软斜坡            |
|    |              | 11=过零+软斜坡 (复位默认)              |

> DATA_SEL=01 是 DSP 引擎路径, 使能音调/去加重/蜂鸣/限幅等功能.
> AOUT/PCMMIX 音量控制需 DATA_SEL=01 才生效, 否则需通过 PCMMIX 调节.

播放时: `DATA_SEL=01` (使能音量控制), `AMUTE=0` (关自动静音).

### 10h — PCMMIXA Vol / 11h — PCMMIXB Vol

| 位 | 名称           | 说明                              |
|----|----------------|-----------------------------------|
| 7  | MUTE_PCM_MIXx  | 1=静音 (复位默认), 0=取消静音       |
| 6:0| PCMMIXx_VOL[6:0] | 音量 -102dB ~ +12dB (步进0.5dB) |

播放时: **必须清 MUTE** 且音量 ≥ 某个值才有输出.

### 16h — AOUTA Vol / 17h — AOUTB Vol

| 位 | 名称         | 说明                                |
|----|--------------|-------------------------------------|
| 7  | MUTE         | 1=静音 (复位默认), 0=取消静音        |
| 6:0| AOUTx_VOL[6:0]| 主音量 -102dB ~ +12dB (步进0.5dB) |

复位值 0x00 = MUTE=1, VOL=0 (-102dB). 播放时清 MUTE 并设合适音量.

> 这些寄存器需 `DATA_SEL=01` 才生效.

### 18h — PCM Channel Mixer

| 位 | 名称     | 说明                     |
|----|----------|--------------------------|
| 7  | PCMA1    | PCM A 声道混音:           |
| 6  | PCMA0    | 00=左→左, 右→右 (默认)   |
| 5  | PCMB1    | 01=左→左右, 右→右        |
| 4  | PCMB0    | 10=左→左, 右→左右 (交换) |
| 3  | ADCA1    | 11=左→左右, 右→左右 (单声)|
| 2  | ADCA0    | ADC 混音同理             |
| 1  | ADCB1    |                          |
| 0  | ADCB0    |                          |

播放时保持默认 0x00.

### 20h — Status (只读)

| 位 | 名称        | 说明                          |
|----|-------------|-------------------------------|
| 6  | SP_CLKERR   | MCLK/LRCK 比例无效            |
| 5  | SPEA_OVFL   | DSP 左声道溢出                |
| 4  | SPEB_OVFL   | DSP 右声道溢出                |
| 3  | PCMA_OVFL   | PCM A 溢出                    |
| 2  | PCMB_OVFL   | PCM B 溢出                    |
| 1  | ADCA_OVFL   | ADC A 溢出                    |
| 0  | ADCB_OVFL   | ADC B 溢出                    |

读后自动清零. 上电初始化后 `SP_CLKERR` 会短暂置位, 属于正常.

### 21h — Charge Pump Frequency

| 位 | 名称         | 说明                                         |
|----|--------------|----------------------------------------------|
| 7:4| CHRG_FREQ[3:0] | 电荷泵开关频率: 复位 0101 (见下表)           |
| 3:0| Reserved     |                                              |

CHRG_FREQ 选择:

| CHRG_FREQ | 频率     | MCLK@单速 | MCLK@倍速 |
|-----------|----------|-----------|-----------|
| 0000      | 60 kHz   | —         | —         |
| 0001      | 120 kHz  | —         | —         |
| 0010      | 240 kHz  | 12.288M   | —         |
| 0011      | 480 kHz  | —         | —         |
| **0101**  | **960 kHz** | **≈49M** | **≈24.576M** |
| 0110      | 1.92 MHz | —         | —         |
| 0111      | 3.84 MHz | —         | —         |

复位值 0101 通常 OK.

## 时钟模式

### 速度模式

| 模式 | 简称 | 采样率范围 | 说明 |
|------|------|-----------|------|
| Quarter-Speed | QSM | 4 ~ 12.5 kHz | 针对 8kHz 优化，噪声最低 |
| Half-Speed | HSM | 12.5 ~ 25 kHz | 针对 16kHz 优化 |
| Single-Speed | SSM | 4 ~ 50 kHz | 通用单速模式 |
| Double-Speed | DSM | 50 ~ 100 kHz | 倍速模式 |

### MCLK/LRCK 比例 (Slave, AUTO=1)

| 速度 | 支持的 MCLK/LRCK 比例 |
|------|----------------------|
| QSM | 1024, 1536, 2048\*, 3072\* |
| HSM | 512, 768, 1024\*, 1536\* |
| SSM | 256, 384, 512\*, 768\* |
| DSM | 128, 192, 256\*, 384\* |

> \* 需 MCLKDIV2=1.

以 8kHz 播放为例:
- MCLK = 256 × 8k = **2.048MHz** → SSM (自动检测)
- MCLK = 512 × 8k = **4.096MHz** → HSM (自动检测)

> SSM 在 8kHz 时 DAC 带外噪声可能折进音频带，QSM 可改善但需更高 MCLK.

## 播放初始化流程

1. **配 SAI**: I2S, 从机, 24bit, 对应采样率
2. **写 02h** = 0x00: 开全局电源, 开 DAC (ADC/PGA 也上电)
3. **写 03h** = 0x51: AUTO=1, MIC 断电
4. **写 04h** = 0x00: I2S 从机 (复位值即可)
5. **写 08h** = 0x60: HP_GAIN=0.6x (复位), DACA/B_MUTE=0
6. **写 09h** = 0x40: DATA_SEL=01 (DSP→DAC), AMUTE=0
7. **写 10h** = 0x00: PCMMIXA 清静音, 0dB
8. **写 11h** = 0x00: PCMMIXB 清静音, 0dB
9. **写 16h** = ~0x60: AOUTA 清静音, 约 0dB
10. **写 17h** = ~0x60: AOUTB 清静音, 约 0dB
11. **21h** = 0x05: 电荷泵频率 (复位值即可)

> 仅播放不需要配 ADC/ALC/Limiter/Beep/Noise Gate/Tone 相关寄存器.

## 上电顺序 (Software Mode)

1. **RESET=0** → 等待电源稳定
2. **RESET=1** → 芯片复位释放
3. **10ms 内** I2C 写 `02h = 0x01`（PDN=1），进入 Standby 模式
   - 若不写则芯片自动切到 Hardware Mode，无法 I2C 控制
4. 在 Standby 中配置所有寄存器（PDN=1 状态）
5. **启动 MCLK**（SAI 输出）
6. I2C 写 `02h = 0x00`（PDN=0），正式上电
7. 等待 ~**50ms**（DAC 初始化 + 电荷泵充电 + VQ 建立）
8. **启动 LRCK/SCLK**，开始送音频数据
9. 正常播放

### 下电顺序

1. 静音 DAC/ADC
2. 写 `02h = 0x01`（PDN=1）→ 等待完全静音
3. RESET=0
