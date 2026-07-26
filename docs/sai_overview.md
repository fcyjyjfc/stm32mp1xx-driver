# SAI (Serial Audio Interface)

RM0436 Chapter 55, pages 2745-2746

## 55.1 Introduction

SAI 提供灵活的串行音频协议支持。通过"自由协议模式"，用户可以自定义音频帧形状、slot 数量、slot 大小、数据大小等。支持的协议包括 I2S、LSB/MSB-justified、PCM/DSP、TDM、AC'97，发送模式下还支持 SPDIF 输出。

SAI 包含两个独立的音频子块（A 和 B），每个子块有自己的时钟发生器和 I/O 线控制器。SAI 可工作在主或从模式，子块之间可同步或异步，多个 SAI 之间也可互相同步。

## 55.2 SAI Main Features

- 两个独立音频子块，均可做发送或接收，各自有独立 FIFO
- 每子块 8-word 集成 FIFO
- 子块间支持同步或异步模式
- 多 SAI 之间可同步
- 各子块独立的主/从模式配置
- 两子块均为主模式时，各自独立的时钟发生器可生成不同采样频率
- 数据位宽: 8 / 10 / 16 / 20 / 24 / 32-bit
- 音频协议: I2S、LSB-justified、MSB-justified、PCM/DSP、TDM、AC'97
- PDM 接口，支持最多 4 对麦克风
- SPDIF 输出
- 最多 16 个 slot，slot 大小可配置
- 每帧位数可配置
- 帧同步极性可配 (偏移/位长/电平)
- 每个 slot 内首 bit 位置可配
- LSB/MSB 优先可配
- Mute 模式
- 立体声/单声道帧能力
- SCK 时钟边沿可配
- 错误标志 + 对应中断
  - 溢出 (overrun) / 欠载 (underrun) 检测
  - 从模式下帧同步信号提前/滞后检测
  - AC'97 接收模式下 Codec 未就绪检测
- 中断源: 错误中断 + FIFO 请求中断
- 2 通道 DMA 接口

## 55.3 SAI Implementation

STM32MP157 有 4 路 SAI 实例，功能差异如下:

| 特性 | SAI1 | SAI2 | SAI3 | SAI4 |
|------|------|------|------|------|
| I2S, LSB/MSB-justified, PCM/DSP, TDM, AC'97 | X | X | X | X |
| FIFO 大小 | 8 words | 8 words | 8 words | 8 words |
| SPDIF | X | X | X | X |
| PDM | X | - | - | X |

- `X` = 支持, `-` = 不支持
- SAI1 和 SAI4 的 PDM 仅信号 D[4:1] 和 CK[2:1] 可用

## 55.4 SAI Functional Description

### 55.4.1 SAI Block Diagram

每个 SAI 由两个音频子块组成，每个子块包含：

- 时钟发生器（独立）
- 32-bit 移位寄存器 + 功能状态机
- 8-word FIFO + FIFO 控制器
- I/O 线管理器（4 个专用 pin: SD, SCK, FS, MCLK）

FIFO 可通过 CPU 或 DMA 访问。两个子块相互独立，也可互相同步。如果两子块声明为同步，部分 pin 可以共享并释放为 GPIO（MCLK 也可在不需要时省掉）。多个 SAI 之间同步时，除 SD_x 外所有 pin 均可释放。

### 55.4.2 SAI Pins and Internal Signals

**内部信号:**

| 信号 | 方向 | 说明 |
|------|------|------|
| sai_a/b_gbl_it | 输出 | 子块 A/B 全局中断 |
| sai_a/b_dma | 输入/输出 | DMA 应答和请求 |
| sai_sync_out_sck/fs | 输出 | 向其他 SAI 提供同步时钟/帧 |
| sai_sync_in_sck/fs | 输入 | 从其他 SAI 接收同步时钟/帧 |
| sai_a/b_ker_ck | 输入 | 子块内核时钟 |
| sai_pclk | 输入 | APB 时钟 |

**外部引脚:**

| 引脚 | 方向 | 说明 |
|------|------|------|
| SAI_SCK_A/B | 输入/输出 | 位时钟 |
| SAI_MCLK_A/B | 输出 | 主时钟 |
| SAI_SD_A/B | 输入/输出 | 串行数据 |
| SAI_FS_A/B | 输入/输出 | 帧同步 |
| SAI_CK[4:1] | 输出 | PDM 位流时钟 |
| SAI_D[4:1] | 输入 | PDM 位流数据 |

### 55.4.3 Main SAI Modes

每个子块由 SAI_xCR1 的 MODE 位设为 Master 或 Slave。

**Master 模式:**
- SAI 向外部器件提供 SCK_x、FS_x、MCLK_x（均为输出）
- 使能后立即生成 SCK，即使 FIFO 为空
- FS 要等 FIFO 收到首笔数据才输出
- FIFO 无数据时：帧内发 0 值 + underrun 标志

**Slave 模式:**
- SCK_x 和 FS_x 由外部提供（输入）
- MCLK_x 不使用，释放为 GPIO
- 若子块与其他 SAI 或另一子块同步，SCK/FS pin 释放为 GPIO
- 必须先写 SAI_xDR → 等 FIFO 不空（FLH ≠ 000）→ 再使能子块
- 首发帧不会 underrun（使能前已预装数据）
- 建议先使能从设备、再使能主设备

**自由协议模式（PRTCFG[1:0] = 0）:**
- 每个子块可独立模拟 I2S / LSB-justified / MSB-justified / PCM / DSP / TDM / AC'97
- 通过 SAI_xFRCR 和 SAI_xSLOTR 自定义帧形状和 slot 格式

**AC'97 特例:**
- FS 始终为输出，即使 SAI 处于 Slave 模式

### 55.4.4 SAI Synchronization Mode

两级同步：子块级（同一 SAI 内）或 SAI 级（多 SAI 之间）。

**内部同步:**

同一 SAI 的两个子块共享 SCK/FS/MCLK，减少外部引脚使用。异步块（SYNCEN=00）占用实际 pin，同步块（SYNCEN=01）的 SCK/FS/MCLK pin 释放为 GPIO。

典型用法：全双工——Block A 做 Master + 另一 Block B 做同步 Slave，或者两者都是 Slave（一个异步块 SYNCEN=00 + 一个同步块 SYNCEN=01）。

约束: PCLK (APB) 频率必须 > 2 倍 SCK 频率（内部重同步要求）。

**外部同步:**

多个 SAI 之间同步时:
1. 源 SAI 通过 SYNCOUT[1:0]（在 SAI_GCR）指定哪个子块输出 FS/SCK
2. 目标 SAI 通过 SYNCIN[1:0]（在 SAI_GCR）选择同步源
3. 每个子块通过 SYNCEN（在 SAI_xCR1）声明是否与该外部 SAI 同步

**Table 399 — 外部同步选择:**

| 当前 SAI | SYNCIN=0 | SYNCIN=1 | SYNCIN=2 | SYNCIN=3 |
|----------|----------|----------|----------|----------|
| SAI1 | Reserved | SAI2 | SAI3 | SAI4 |
| SAI2 | SAI1 | Reserved | SAI3 | SAI4 |
| SAI3 | SAI1 | SAI2 | Reserved | SAI4 |
| SAI4 | SAI1 | SAI2 | SAI3 | Reserved |

例: SAI2 设 SYNCIN=0 即从 SAI1 获取同步时钟。

### 55.4.5 Audio Data Size

SAI_xCR1 的 DS[2:0] 配置数据位宽: 8 / 10 / 16 / 20 / 24 / 32-bit。LSBFIRST 位决定先发 MSB 还是 LSB。

### 55.4.6 Frame Synchronization

FS 信号作为帧起始标识，形状通过 SAI_xFRCR 完全可配。AC'97 和 SPDIF 模式下 SAI_xFRCR 被忽略。

**Frame Length (FRL[7:0])**

帧内 SCK 周期数 = FRL + 1。

- Master 模式：最大 256。NODIV=0 时 (FRL+1) 必须是 2 的幂（8~256），保证 MCLK 每 SCK 周期有整数个脉冲。NODIV=1 时 FRL 无限制
- Slave 模式：FRL 用于告诉 SAI 外部 master 的帧长，检测 FS 提前/滞后错误。无取值约束
- 最小 8 个 SCK

**Frame Synchronization Polarity (FSPOL)**

FS 有效边沿——上升沿或下降沿启动一帧。边沿触发，非电平触发。

**Frame Synchronization Active Level Length (FSALL[6:0])**

FS 有效电平持续多少个 SCK：1~128 个 SCK。

- I2S / LSB-justified / MSB-justified：有效长度 = 半帧（FRL/2）
- PCM / DSP / TDM：有效长度 = 1（单个脉冲）

**Frame Synchronization Offset (FSOFF)**

FS 相对于帧内首 bit 的位置：
- FSOFF=0：FS 与首 bit 对齐（MSB-justified）
- FSOFF=1：FS 在首 bit 之前（I2S 标准——FS 在 slot 最后一 bit 时翻转）

**FS Signal Role (FSDEF)**

- **FSDEF=0**：FS 仅标记帧起始（PCM/DSP/TDM/AC'97 模式）。分左右声道靠 slot 序号：Slot 0=L, Slot 1=R，纯约定
- **FSDEF=1**：FS 同时标记帧起始 + 左右声道标识（I2S/LSB/MSB-justified）。FS 电平区分半边帧。帧长必须为偶数，左声道 slot 和右声道 slot 各占半边
  - 空位处理：如果半帧 slot 没填满，TRIS=0 补 0，TRIS=1 释放为高阻。接收模式空位丢弃

**FSDEF=0 时的剩余 bit：**

如果 slot 总数 × slot 大小 < 帧长：
- 发送：TRIS=0 补 0，TRIS=1 高阻
- 接收：丢弃剩余 bit

**SPDIF 模式特例：**

SPDIF 输出模式下 FS 脚不使用，释放为 GPIO。

### 55.4.7 Slot Configuration

Slot 是音频帧内的基本数据承载单元。通过 SAI_xSLOTR 配置。

**Slot 数量 (NBSLOT[3:0])**

每帧 slot 数 = NBSLOT + 1。最多 16 个。
AC'97 和 SPDIF 模式下自动设置，NBSLOT 被忽略。

**Slot 使能 (SLOTEN[15:0])**

每个 slot 可单独开关。无效 slot 不产生 FIFO 访问。

**Slot 大小 (SLOTSZ[1:0])**

所有 slot 统一大小，不区分有效/无效。

当 SLOTSZ > 数据大小 (DS) 时：发送模式下剩余位补 0，接收模式下忽略。

**首 Bit 偏移 (FBOFF[4:0])**

数据在 slot 内的起始位置。发送时偏移量之前补 0，接收时偏移部分忽略。用于 LSB-justified 协议（FBOFF = SLOTSZ - DS）。

**约束:**
- FBOFF ≤ SLOTSZ − DS
- DS ≤ SLOTSZ
- NBSLOT × SLOTSZ ≤ FRL (帧长)
- FSDEF=1 时 NBSLOT 必须为偶数
- 违反以上条件会导致 SAI 行为异常
- AC'97 和 SPDIF 模式下（PRTCFG=10 或 01），slot 大小自动按协议设置，无须手动配

### 55.4.8 SAI Clock Generator

每个音频子块有独立时钟发生器，从 `sai_x_ker_ck`（RCC 提供）生成 MCLK 和 SCK。

**MCLK (Master Clock) 生成**

SAI 可向外部 Codec 输出 MCLK，提供高精度系统时钟。MCLK 与 SAIEN 无关——MCKEN=1 即输出，可先给外部 Codec 时钟、再启动音频流：

1. 确保 SAIEN = 0
2. 设 MCKDIV[5:0] 分频
3. 设 MCKEN = 1，MCLK 输出稳定
4. 之后再设 SAIEN = 1 开始传输音频数据

约束：
- MCKEN=1 时禁止改 MCKDIV（可能产生毛刺）
- SAIEN=1 时禁止清 MCKEN
- MCKEN 在 SAIEN=0 时开关 MCLK 保证无毛刺

**Table 400: MCLK 激活条件**
- MCKEN=0 → MCLK 关闭
- MCKEN=1 + SAIEN=0 → MCLK 开启（预供时钟）
- MCKEN=1 + SAIEN=1 + NODIV=0/1 → MCLK 开启

**SCK (Bit Clock) 生成**

仅在 Master 模式下 SAI 输出 SCK。Slave 模式下 SCK 来自外部，NODIV 和 OSR 被忽略。

SCK 边沿极性通过 CKSTR 配，主/从模式均有效。

时钟发生器框图（Figure 651）的信号路径：
```
sai_x_ker_ck → MCKDIV 分频 → MCLK_x
                             → OSR 倍频选择 → ÷256 或 ÷(FRL+1) → SCK_x
                                                              → ÷(FRL+1) → FS_x
```

**NODIV 模式：**

NODIV 决定 MCLK 与 FS 的比值关系：

- **NODIV=0**：MCLK/FS 固定为 256 (OSR=0) 或 512 (OSR=1)。帧长 (FRL+1) 必须是 2 的幂
- **NODIV=1**：MCLK = SCK（同频），不分频出 MCLK。帧长无 2 的幂限制。FS 由 SCK 除以 (FRL+1) 得到

**NODIV=0 公式：**
- MCLK = ker_ck / MCKDIV
- FS = ker_ck / (MCKDIV × (OSR+1) × 256)
- SCK = ker_ck × (FRL+1) / (MCKDIV × (OSR+1) × 256)

注：MCKDIV 推荐偶数值或 >10，奇数会导致占空比偏离 50%。MCKDIV=0 等效于 1。

**NODIV=1 公式：**
- SCK = MCLK = ker_ck / MCKDIV
- FS = ker_ck / (MCKDIV × (FRL+1))

注：(FRL+1) 允许 8~256 任意值。

**Table 401: 时钟发生器配置实例 (ker_ck = 98.304 MHz)**

| FS | NODIV | MCKDIV | OSR | FRL+1 | MCLK | MCKEN |
|-----|-------|--------|-----|-------|------|-------|
| 192 kHz | 0 | 2 | 1 (512×) | 64 (2^N-1) | 输出 | 1 |
| 96 kHz | 0 | 4 | 1 | 64 | 输出 | 1 |
| 48 kHz | 0 | 8 | 1 | 64 | 输出 | 1 |
| 192 kHz | 0 | 2 | 0 (256×) | 64 | 输出 | 1 |
| 96 kHz | 0 | 4 | 0 | 64 | 输出 | 1 |
| 48 kHz | 0 | 8 | 0 | 64 | 输出 | 1 |
| 192 kHz | 1 | 2 | - | 8 | - | - |
| 96 kHz | 1 | 2 | - | 16 | - | - |
| 48 kHz | 1 | 2 | - | 32 | - | - |

### 55.4.9 Internal FIFOs

每个音频子块有独立的 8-word FIFO。FIFO 由 FREQ 位（SAI_xSR）和 FTH[2:0]（SAI_xCR2）配合触发中断或 DMA。

**发送模式（TX）：**
FIFO 数据量低于阈值时触发中断（提醒喂数据）：

| FTH | 阈值 | 触发条件 |
|------|--------|----------|
| 000 | 空 | FLVL < 001（FIFO 为空） |
| 001 | 1/4 | FLVL < 010（不足 1/4） |
| 010 | 1/2 | FLVL < 011（不足 1/2） |
| 011 | 3/4 | FLVL < 100（不足 3/4） |
| 100 | 满 | FLVL < 101（不满） |

**接收模式（RX）：**
FIFO 数据量高于阈值时触发中断（提醒取数据）：

| FTH | 阈值 | 触发条件 |
|------|--------|----------|
| 000 | 空 | FLVL ≥ 001（非空即可取） |
| 001 | 1/4 | FLVL ≥ 010（≥ 1/4） |
| 010 | 1/2 | FLVL ≥ 011（≥ 1/2） |
| 011 | 3/4 | FLVL ≥ 100（≥ 3/4） |
| 100 | 满 | FLVL = 101（满） |

**通用规则：**
- FREQ 被硬件自动清除（TX 喂入数据后 / RX 取走数据后）
- DMAEN=1 时 DMA 请求也通过 FREQ 触发
- 每次读写固定 1 word（32-bit），与数据位宽无关
- 数据写入 DR 必须靠右对齐，读出数据也是靠右对齐
- FFLUSH（SAI_xCR2）：SAI 禁止时可复位 FIFO 指针

### 55.4.10 PDM Interface

PDM 脉冲密度调制接口，用于数字麦克风（最多 4 对）。与 CS42L51 等模拟 Codec 无关。

**特性：**
- 只能与 SAI_A 配合，SAI_A 必须配置为 TDM 主模式
- 不适用于 SAI_B
- 一对麦克风共享数据线，通过 LR 脚区别左右（上升沿/下降沿）
- PDM 接口 **不内置抽取滤波**，需软件将 PDM 位流转换为 PCM 采样值

**使能顺序：**
1. 配置 SAI_A 为 TDM 主模式
2. 设 MICNBR 选麦克风数（2/4/6/8）
3. 设 CKEN 使能位时钟
4. PDMEN=1
5. 最后使能 SAI_A
6. 前两帧 SAI_A 收到的是无效数据，需丢弃

*(以下与 CS42L51 模拟 I2S 无关，跳过：*
*- 页码 2762-2768: PDM 配置细节*
*- 页码 2769-2770: AC'97 link controller

### 55.4.12 SPDIF Output

SAI 可输出 IEC60958 数字音频（光纤/同轴标准），仅发送模式。

**特性：**
- 仅用 SD 脚输出，FS/SCK/MCLK 释放
- 强制 Master 模式，数据强制 24-bit（DS 忽略）
- FS 和 Slot 寄存器忽略，硬件自动编帧
- Manchester 编码，bit clock = symbol-rate × 2
- 仅 underrun 错误，（无可接收标志）

**帧结构：**
- 一个 block = 192 帧，每帧 = 2 × 32-bit subframe（左/右声道）
- 每个 subframe: 4-bit preamble + 24-bit data + 4 status bits
- Preamble 标识声道和 block 起止（B/M/W 三种编码）
- SAI_xDR[23:0] = 数据，SAI_xDR[26:24] = status bits
- LSB 优先发送，硬件自动附加奇偶校验位

**Underrun 恢复：**
关 DMA → 关 SAI → 清 COVRUNDR → 刷新 FIFO → 重设 DMA 源地址（对齐新 block）→ 开 DMA → 开 SAI

**时钟：**
NODIV=1，SCK = ker_ck / MCKDIV，FS = SCK / 128

### 55.4.13 Specific Features

SAI_xCR2 中的一些特定位实现的功能，与所选音频协议无关。

#### Mute Mode

**发送方向：**
- MUTE 位随时可置，但只在**当前帧结束时**被硬件锁存，从下一帧起生效
- 每帧结束时硬件重新采样 MUTE 位，决定下一帧是否继续静音
- 静音时 FIFO 指针**仍递增**，FIFO 中的数据被丢弃不进移位寄存器
- NBSLOT ≤ 2（≤ 2 个 slot）：MUTEVAL 选择静音期间发 0 还是发**上一 slot 的最后有效值**
- NBSLOT > 2：MUTEVAL 无效，固定发 0

**接收方向（Mute 检测）：**
- 设置 MUTECNT[5:0] 指定连续检测帧数
- 所有有效 slot 连续收到 0 达到 MUTECNT 帧 → MUTEDET 标志置位 → 可选中断
- 任一帧收到非零数据则计数器清零
- 中断只触发一次，计数器清零后重新使能
- SPDIF 模式不支持静音

#### Mono/Stereo Mode

当 NBSLOT = 2 时有效（SAI_xCR1 的 MONO 位）：

**发送：** slot 0 数据硬件自动复制到 slot 1，FIFO 写入减半。配置：MONO=1, NBSLOT=1, SLOTEN=3。

**接收：** 只存 slot 0，slot 1 丢弃。真实立体声（左右不同）时 MONO 无意义，需软件转换。

#### Companding Mode (G.711 μ-Law / A-Law)

仅 Free Protocol 模式有效，数据路径插入硬件编解码：

```
发送: FIFO [线性 PCM] → 压缩 → [8-bit μ/A-Law] → SD 移位寄存器
接收: SD 移位寄存器 [8-bit μ/A-Law] → 扩展 → [线性 PCM] → FIFO
```

软件完全透明，只看到线性 PCM。

| COMP[1:0] | 模式 |
|-----------|------|
| 00 | 关闭（直通） |
| 10 | μ-Law（14-bit 动态范围） |
| 11 | A-Law（13-bit 动态范围） |

CPL 位选择 1's/2's complement。使能后 DS 硬件锁定为 8-bit。TX 自动压缩，RX 自动扩展。

CS42L51 不需要此功能。

#### Output Data Line Management on an Inactive Slot (TRIS)

发送模式下无效 slot（SLOTEN=0）的 SD 线行为：

- **TRIS = 0**：强制输出 0
- **TRIS = 1**：释放为高阻（Hi-Z），用于 TDM 多设备共享总线

Hi-Z 释放时机：无效 slot 结束时 / padding 期间 / 帧剩佘 bit 段。

FSDEF=1（I2S 模式）时，每个半帧独立管理 Hi-Z。

CS42L51 点对点 I2S，无总线共享，TRIS=0 即可。

### 55.4.14 Error Flags

SAI_xSR 中的错误标志，通过 SAI_xCLRFR 对应位写 1 清除。

**FIFO Overrun（接收溢出）：**
- RX 时 FIFO 满、新数据到达 → 数据丢失，OVRUDR 置位
- 硬件记录发生时的 slot 号，FIFO 有空位后**从该 slot 恢复存储**
- 保证内存与 slot 对齐不偏移

**FIFO Underrun（发送欠载）：**
- TX 时 FIFO 空、需要发送却无数据 → 发 MUTE(00)，OVRUDR 置位
- 同样记录 slot 号，FIFO 有数据后从该 slot 恢复
- Master/Slave 模式都可能发生

**Anticipated FS（帧同步提前）— Slave 专用：**
- 外部 Master 在预期时间前发了 FS
- **不影响当前帧**（仅标记，不干扰正在进行的传输）
- 重同步：关 SAI → 刷新 FIFO → 开 SAI → 等待下一次 FS

**Late FS（帧同步滞后）— Slave 专用：**
- 外部 Master 未在预期时间内发 FS
- 环境噪声导致 SCK 毛刺也可能被检测为此错误
- 重同步步骤同上

**Codec Not Ready — AC'97 专用，无关**

**Wrong Clock Config（WCKCFG）— Master + NODIV=0 专用：**
- (FRL+1) 不是 2 的幂或不在 8~256 范围
- 硬件**自动清除 SAIEN** 关闭音频块

*(页码 2778-2780)*

### 55.4.15 Disabling the SAI

清 SAIEN 后硬件等**当前帧完整发完**才真正停止。读回 SAIEN 确认完全关闭。同步模式下先关 Master。

### 55.4.16 DMA Interface

每个子块有独立 DMA 接口，设 DMAEN 即可：

- **TX：** FIFO 低于阈值 → DMA 请求 → DMA 写 SAI_xDR
- **RX：** FIFO 高于阈值 → DMA 请求 → DMA 读 SAI_xDR

配置顺序：配 SAI + FIFO 阈值 → 配 DMA 通道 → 使能 DMA → 使能 SAI

*(页码 2781)*

### 55.5 SAI Interrupts

7 个中断源，分 FIFO 请求和错误两组：

| 中断 | 组 | 适用模式 | 使能位(SAI_xIM) | 清除方式 |
|------|----|----------|----------------|----------|
| FREQ | FIFO 请求 | 主/从, TX/RX | FREQIE | FIFO 读写自动清 |
| OVRUDR | 错误 | 主/从, TX/RX | OVRUDRIE | SAI_xCLRFR.COVRUDR=1 |
| AFSDET | 错误 | Slave 专用 | AFSDETIE | SAI_xCLRFR.CAFSDET=1 |
| LFSDET | 错误 | Slave 专用 | LFSDETIE | SAI_xCLRFR.CLFSDET=1 |
| CNRDY | 错误 | AC'97 专用 | CNRDYIE | SAI_xCLRFR.CCNRDY=1 |
| WCKCFG | 错误 | Master+NODIV=0 | WCKCFGIE | SAI_xCLRFR.CWCKCFG=1 |

