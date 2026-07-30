# SiI9022A HDMI Transmitter 笔记

## 芯片基本信息

- 24-bit RGB 并行输入 + HS/VS/DE + 像素时钟，直接输出 HDMI
- 支持 HDCP（SiI9024A），**SiI9022A 无 HDCP**
- I2C 地址：TPI=0x72（CI2CA=LOW）或 0x76（CI2CA=HIGH）
- 完整 TPI 手册：SiI-PR-1064-A

## 引脚连接 (LTDC → SiI9022A)

| SiI9022A | LTDC | 说明 |
|----------|------|------|
| D[7:0] | LCD_B[7:0] | 蓝色 |
| D[15:8] | LCD_G[7:0] | 绿色 |
| D[23:16] | LCD_R[7:0] | 红色 |
| IDCK | LCD_CLK | 像素时钟 |
| HSYNC | LCD_HSYNC | 水平同步 |
| VSYNC | LCD_VSYNC | 垂直同步 |
| DE | LCD_DE | 数据使能 |

所有数据线建议串联 33Ω 电阻。

## TPI 寄存器表

| 地址 | 名称 | 说明 |
|:---|:---|:---|
| 0x00-0x01 | PixelClock | 像素时钟 / 10000，16bit |
| 0x02-0x03 | VFreq | 垂直刷新率 Hz，16bit |
| 0x04-0x05 | Pixels | 每行总像素数，16bit |
| 0x06-0x07 | Lines | 总行数，16bit |
| 0x08 | InputBus | 输入总线配置 |
| 0x09 | InputFormat | 输入色彩格式 |
| 0x0A | OutputFormat | 输出色彩格式 |
| 0x0B | YCInputMode | YC 输入模式 |
| 0x0C-0x19 | AVI InfoFrame | AVI 信息帧 |
| 0x1A | SysCtrl | 系统控制 |
| 0x1B-0x1D | DeviceID | 设备 ID (0xB0) + 版本 |
| 0x1E | PowerState | 电源状态 |
| 0x1F-0x28 | Audio | 音频配置 |
| 0x29-0x3A | HDCP | HDCP（SiI9022A 无用） |
| 0x3C | IntEnable | 中断使能 |
| 0x3D | IntStatus | 中断状态 |
| 0x60 | SyncCtrl | 同步控制/DE 生成器选择 |
| 0x61 | SyncDetect | 同步极性检测 (RO) |
| 0x62-0x6D | DE Generator | DE 生成器参数（0x60[7]=0） |
| 0xBF-0xDE | Misc InfoFrame | 其他信息帧 |
| 0xC7 | TPIMode | TPI 模式使能 |
| 0xBC | PageSel | 内部寄存器页选择 |
| 0xBD | IdxAddr | 内部寄存器偏移地址 |
| 0xBE | IdxData | 内部寄存器读写数据 |

### 0x08 - InputBus

| 位 | 功能 | 值 |
|:---|:---|:---|
| [7:6] | TClkSel | 00=x0.5, 01=x1(默认), 10=x2, 11=x4 |
| 5 | InputBusFmt | 0=半像素, 1=全像素(默认) |
| 4 | EdgeSel | 0=下降沿, 1=上升沿(默认) |
| [3:0] | PR | 像素重复因子: 0000=无重复 |

**LTDC 配置：** `0x60`（全像素24bit + 上升沿 + 无像素重复 + TClkSel x1）

### 0x09 / 0x0A - InputFormat / OutputFormat

| 位 | 功能 |
|:---|:---|
| [7:6] | InputColorDepth: 00=8bit, 01=10bit, 10=12bit, 11=12bit |
| [5:4] | InputColorSpace: 00=RGB, 01=YCbCr422, 10=YCbCr444 |
| [3:2] | OutputColorSpace: 00=RGB, 01=YCbCr422, 10=YCbCr444 |
| [1:0] | Range: 00=auto, 01=force, 10=block |

**LTDC 配置：** InputFormat=`0x00`（RGB 8bit），OutputFormat=`0x00`（RGB 输出）

### 0x0B - YCInputMode

| 位 | 功能 |
|:---|:---|
| 7 | Swap MSB/LSB in each channel |
| 6 | DDR 输入引脚选择（0=低12位, 1=高12位）|
| 3 | Non Gap Mode |
| [2:0] | YC Input Mode: 000=normal, 100=24bit 连续 |

**LTDC 配置：** `0x00`

### 0x1A - SysCtrl (系统控制)

| 位 | 功能 |
|:---|:---|
| 7 | Link Integrity Mode (HDCP) |
| 6 | RSVD |
| 5 | RSVD |
| **4** | **TMDS 输出控制：0=使能, 1=关闭(默认)** |
| 3 | AV Mute：1=消隐画面 |
| 2 | DDC 总线请求（读 EDID 前置1）|
| 1 | DDC 授权状态（读=状态，写=强制总线所有权） |
| 0 | 输出模式：0=DVI, 1=HDMI |

DDC 读 EDID 流程：
1. 写 `0x1A[2]=1` 请求 DDC 总线
2. 轮询 `0x1A[1]` 直到返回1（授权）
3. 写 `0x1A=0x06` 关闭开关，后续 I2C 访问走 DDC
4. 读取 EDID
5. 写 `0x1A=0x00` 释放请求
6. 回读 `0x1A[2:1]` 确认已清零

**LTDC 配置：** 初始 `0x10`（关闭TMDS），配好后 `0x00`（使能TMDS）。HDMI 模式需设 `0x1A[0]=1`。

### 0x1E - PowerState

| 位 | 功能 |
|:---|:---|
| 2 | Wakeup: 0=Hot (RSEN+HPD), 1=Cold (仅HPD) |
| [1:0] | 00=D0(全速), 01=D2(待机), 10=D3 Hot, 11=D3 Cold |

D3 Cold 最省电但 I2C 不可访问，需硬件复位恢复。
D2 可读寄存器，HPD 变化可唤醒。
D0 全速运行。

**LTDC 配置：** `0x00` → D0

### 0x60 - SyncCtrl

| 位 | 功能 |
|:---|:---|
| **7** | **0=显式同步(HS/VS/DE), 1=内嵌同步** |
| 5 | YC Mux 模式使能 |
| 3 | DE_ADJ#：0=使能VSYNC调整(推荐), 1=禁用 |
| [2:0] | 帧/极性检测 (RO) |

注意：写 0x19（AVI InfoFrame 末字节）后，0x60[5] 会被复位为默认值。

**LTDC 配置：** `0x00` → 显式同步 + DE_ADJ# 禁用（保持默认）

### 0x62-0x6D - DE 生成器（0x60[7]=0 时可用）

| 地址 | 位域 | 说明 |
|:---|:---|:---|
| 0x62 | DE_DLY[7:0] | HSYNC 宽度+HBP+左边框（像素） |
| 0x63[6] | DE Generator | 1=使能 DE 生成 |
| 0x63[5] | VSYNC Pol | 0=正极性, 1=负极性 |
| 0x63[4] | HSYNC Pol | 0=正极性, 1=负极性 |
| 0x63[1:0] | DE_DLY[9:8] | DE_DLY 高2位 |
| 0x64 | DE_TOP[6:0] | VSYNC 宽度+VBP+上边框（行） |
| 0x66 | DE_CNT[7:0] | 有效宽度（像素） |
| 0x67[3:0] | DE_CNT[11:8] | 有效宽度高4位 |
| 0x68 | DE_LIN[7:0] | 有效高度（行） |
| 0x69[2:0] | DE_LIN[10:8] | 有效高度高3位 |
| 0x6A-0x6B | H_RES (RO) | 实测每行像素数 |
| 0x6C-0x6D | V_RES (RO) | 实测总行数 |

**LTDC 场景：** LTDC 已提供 DE 信号，不需要 DE 生成器。`0x63[6]=0` 禁用即可。
但 **HSYNC/VSYNC 极性**必须按 LTDC 实际输出设置。

### 0x3C / 0x3D - 中断

**0x3C (IntEnable)：**
| 位 | 中断源 |
|:---|:---|
| 7 | HDCP 认证状态变化 |
| 4 | 音频错误 |
| 2 | RxSense 事件 |
| 1 | 热插拔事件 |
| 0 | 连接事件 |

**0x3D (IntStatus)：**
| 位 | 状态 |
|:---|:---|
| 3 | RxSense 当前状态 (RO) |
| 2 | HPD 当前状态 (RO) |
| 1 | RxSense/CPI 事件待处理 |
| 0 | HPD 连接事件待处理 |

写 1 清除中断待处理位。

### 音频配置 (0x1F-0x28)

| 地址 | 说明 |
|:---|:---|
| 0x1F | I2S 通道映射 |
| 0x20 | I2S 时钟/极性配置 |
| 0x21-0x24 | I2S 通道状态字节 |
| 0x25 | 音频处理模式 + 字长 |
| 0x26[7:6] | 音频接口: 00=禁用, 01=S/PDIF, 10=I2S |
| 0x26[5] | Layout: 0=2ch, 1=多通道 |
| 0x26[4] | Mute: 0=正常, 1=静音 |
| 0x26[3:0] | 音频编码类型 |
| 0x27[2:0] | 采样率: 000=自动, 011=48kHz |
| 0x27[5:3] | 采样大小: 01=16bit, 11=24bit |

S/PDIF 基本全自动，写入 `0x26=0x50`(S/PDIF+Mute) → `0x27` → `0x26=0x40`(Unmute) 即可。

I2S 需额外配 0x1F(引脚映射) 和 0x20(时钟格式)。

## 初始化序列

LTDC 已输出 RGB 信号时，SiI9022A 配置步骤：

```
1. 硬件复位 (RESET# 拉低 >= 50us)
2. 写 0xC7 = 0x00                // 进入 TPI 模式 (必须第一步)
3. 读 0x1B-0x1D                  // 确认 DeviceID=0xB0, TPI Rev=0x03
4. 写 0x1E = 0x00                // D0 全速运行

5. // 使能源端终止 (内部索引寄存器)
   写 0xBC=0x01                  // Internal page 0
   写 0xBD=0x82                  // Indexed reg 0x82
   读 0xBE                       // 读当前值
   写 0xBE bit[0]=1              // 使能源端终止

6. 写 0x08 = 0x60                // 全像素+上升沿+x1+无重复
7. 写 0x09 = 0x00                // InputFormat: RGB 8bit
8. 写 0x0A = 0x00                // OutputFormat: RGB
9. 写 0x0B = 0x00                // YC Input Mode: normal

10. 写 0x60 = 0x00               // 显式同步模式
11. // 配置视频模式参数 (以 720p60 为例)
    写 0x00 = 0x01               // PixelClock LSB = 7425 & 0xFF
    写 0x01 = 0x1D               // PixelClock MSB = 7425 >> 8  (74.25MHz)
    写 0x02 = 0x3C               // VFreq LSB (60Hz)
    写 0x03 = 0x00               // VFreq MSB
    写 0x04 = 0x50               // Pixels/L = 0x660 = 1632 (每行总像素)
    写 0x05 = 0x06               // Pixels MSB
    写 0x06 = 0x1E               // Lines L = 0x3F6 = 1014 (总行数)
    写 0x07 = 0x03               // Lines MSB

12. // AVI InfoFrame (HDMI 必需)
    写 0x0C = checksum           // 需计算
    写 0x0D = 0x00               // Y=RGB, A0=active
    写 0x0E = 0x00               // C=No data, M=No data
    写 0x0F = 0x00               // ITC=0
    写 0x10 = VIC                // 视频识别码
    写 0x11 = 0x00               // 量化/内容类型
    写 0x12-0x19 = 0x00          // bar 信息+像素重复+MSB

13. 写 0x1A = 0x01               // TMDS 使能 + HDMI 模式
```

## 热插拔事件处理

```
1. 轮询或中断检测 0x3D[0]=1
2. 写 0x3D = 0x03               // 清除事件
3. 读 0x3D[3:2]                 // 确认 HPD 状态
4. 请求 DDC 总线 (0x1A 流程)
5. 读 EDID
6. 释放 DDC 总线
7. 根据 EDID 配视频/音频参数
8. 写 0x1E=0x00 → D0
9. 写 0x1A=0x01 → TMDS 使能+HDMI
```

## 色彩空间转换 (CSC)

- RGB ↔ YCbCr，支持 BT.601（标清）和 BT.709（高清）
- 转换公式见数据手册 Table 6.1 / 6.2

## 3D 视频 (HDMI 1.4a)

- Frame Packing、L+Depth、Side-by-Side、Top-and-Bottom
- VRES 计数器仅 11bit（max 2047），1080p24 会溢出但不影响输出

## 音频接口

| 接口 | 通道 | 采样率 | 特点 |
|------|------|--------|------|
| S/PDIF | 2ch PCM 或压缩多声道 | 32-192kHz | 无需外部时钟，全自动 |
| I2S (SD0-SD3) | 2-8ch | 最高 192kHz | 需同源 MCLK/内部 PLL |

- 49-ball 封装只有 SD0（2ch）

## 内部索引寄存器访问

通过 0xBC/0xBD/0xBE 访问隐藏的内部寄存器：

```
写 0xBC = page       // 选择页 (通常 0x01 = page 0)
写 0xBD = offset     // 选择寄存器偏移
读/写 0xBE           // 读写该寄存器
```

**常用索引寄存器：**

| 页 | 偏移 | 位 | 功能 |
|:---|:---|:---|:---|
| 0x01 | 0x82 | [0] | 源端终止使能（初始化必须） |
| 0x01 | 0x72 | [1] | TCLK STABLE 中断（时钟稳定检测） |
| 0x01 | 0x7A | [7:0] | 热插拔去抖延迟（0x13 ≈ 64ms） |
| 0x01 | 0x79 | [2] | INT 输出模式：0=push-pull, 1=open-drain |
| 0x01 | 0x79 | [1] | INT 极性：0=active high, 1=active low |
| 0x01 | 0x0A | [3] | HDCP DEVICE COUNT=0 检查使能 |
| 0x01 | 0x0A | [0] | Ri 检查使能（内嵌同步模式需要） |
