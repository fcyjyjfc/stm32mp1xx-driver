# SDMMC 手册笔记 (RM0436 Chapter 58)

## 58.1 主要特性

- 接口：AHB 总线 ↔ SD/SDIO/eMMC 卡
- eMMC 5.1：1-bit/4-bit/8-bit 数据线，HS200 支持（HS400 不支持）
- SD 卡 v6.0：SDR104 支持（SPI 模式和 UHS-II 不支持）
- SDIO v4.0：1-bit/4-bit
- 最大数据速率 208 MB/s（8-bit 模式，取决于 I/O 最大允许速度）
- 支持 IDMA 链表
- 同时只支持一张卡（或 eMMC stack）

## 58.2 实例差异

| 特性 | SDMMC1 | SDMMC2 | SDMMC3 |
|------|--------|--------|--------|
| Variable delay | ✓ | ✓ | ✓ |
| SDMMC_CKIN | ✓ | ✓ | ✓ |
| SDMMC_CDIR, SDMMC_D0DIR | ✓ | ✗ | ✗ |
| SDMMC_D123DIR | ✓ | ✗ | ✗ |

## 58.3 总线拓扑

通信基于 命令/响应 和 数据传输。

### 数据传输方式

| 模式 | 块大小 | 说明 |
|------|--------|------|
| Block 模式 | 2^N 字节 (N=0~14) | SD/eMMC 通用 |
| SDIO 多字节模式 | 1~512 字节 | SDIO 专用 |
| eMMC Stream 模式 | 连续流 | 仅 1-bit 总线，DS/HS/SDR 模式 |

### 操作时序 (Figure 686~690)

**无响应操作 (Figure 686)：**
- CMD →（无响应，无数据）

**有响应无数据：**
- CMD → Response

**单 block 读 (Figure 687)：**
- CMD → Response → Data block + CRC

**多 block 读 (Figure 687)：**
- CMD → Response → [Data block + CRC] × N → Stop CMD + Response
- 预定义 block 数量时（eMMC）不需要 Stop CMD

**单 block 写 (Figure 688)：**
- CMD → Response → Data block + CRC → CRC status → Busy

**多 block 写 (Figure 688)：**
- CMD → Response → [Data block + CRC → CRC status → Busy] × N → Stop CMD + Response
- 预定义 block 数量时不需要 Stop CMD
- Busy 期间 D0 被卡拉低，主机不能发送新数据

**Stream 读 (Figure 689，eMMC)：**
- CMD → Response → 连续数据流 → Stop CMD + Response

**Stream 写 (Figure 690，eMMC)：**
- CMD → Response → 连续数据流 → Stop CMD + Response → Busy

## 58.4 操作模式

### SD/SDIO 速度模式

| 模式 | 最大速率 | 最大时钟 | 信号电压 | 说明 |
|------|---------|---------|---------|------|
| DS (Default Speed) | 12.5 MB/s | 25 MHz | 3.3V | |
| HS (High Speed) | 25 MB/s | 50 MHz | 3.3V | |
| SDR12 | 12.5 MB/s | 25 MHz | 1.8V | |
| SDR25 | 25 MB/s | 50 MHz | 1.8V | |
| DDR50 | 50 MB/s | 50 MHz | 1.8V | 双沿采样 |
| SDR50 | 50 MB/s | 100 MHz | 1.8V | |
| SDR104 | 104 MB/s | 208 MHz | 1.8V | 需要 variable delay 调谐 |

- 以上速率基于 4-bit 总线宽度
- SDR = 单沿采样，DDR = 双沿采样（上升+下降沿都采数据）
- SDR104 必须使用 sampling point tuning（variable delay）
- SDR50 可选使用 variable delay

### eMMC 速度模式

| 模式 | 最大速率 | 最大时钟 | 信号电压 |
|------|---------|---------|---------|
| Legacy | 26 MB/s | 26 MHz | 3/1.8/1.2V |
| HS SDR | 52 MB/s | 52 MHz | 3/1.8/1.2V |
| HS DDR | 104 MB/s | 52 MHz | 3/1.8/1.2V |
| HS200 | 200 MB/s | 200 MHz | 1.8/1.2V |

- 以上速率基于 8-bit 总线宽度
- HS200 必须使用 sampling point tuning

## 58.5 功能描述

### 58.5.1 内部结构 (Figure 691)

1. **AHB Slave 接口** — 访问 SDMMC 寄存器，产生中断和 IDMA 控制信号
2. **SDMMC Adapter** — 核心功能块：时钟生成、命令路径、数据收发路径
3. **IDMA** — 内部 DMA，带 AHB Master 接口，支持链表
4. **DLYB（Delay Block）** — 接收数据采样时钟对齐，不属于 SDMMC 本体；SDR104/HS200 必须使用

### 时钟信号

| 信号 | 方向 | 说明 |
|------|------|------|
| sdmmc_ker_ck | 输入 | SDMMC 内核时钟（产生卡时钟） |
| sdmmc_hclk | 输入 | AHB 总线时钟 |
| sdmmc_io_in_ck | 输入 | 卡反馈时钟，DS/HS 模式下内部连接到 SDMMC_CK |
| sdmmc_fb_ck | 输入 | 经过 DLYB 延迟后的反馈时钟（SDR50/DDR50/SDR104/HS200） |

### 58.5.2 引脚

| 引脚 | 类型 | 说明 |
|------|------|------|
| SDMMC_CK | 输出 | 时钟输出到卡 |
| SDMMC_CKIN | 输入 | 外部驱动器的时钟反馈（SDR12/25/50/DDR50） |
| SDMMC_CMD | 双向 | 命令/响应线 |
| SDMMC_D[7:0] | 双向 | 数据线 |
| SDMMC_CDIR | 输出 | CMD 线方向指示（用于外部电平转换器） |
| SDMMC_D0DIR | 输出 | D0 方向指示 |
| SDMMC_D123DIR | 输出 | D[3:1] 方向指示 |

方向指示引脚仅 SDMMC1 支持，用于连接外部电压转换收发器。

### 58.5.3 数据总线宽度

- 默认：1-bit（仅 SDMMC_D0）
- 初始化后主机可切换总线宽度：
  - SD/SDIO：1-bit 或 4-bit（D[3:0]）
  - eMMC：1-bit、4-bit 或 8-bit（D[7:0]）
- 所有数据线工作在推挽模式

### 时钟生成与相位

SDMMC_CK 由 sdmmc_ker_ck 分频产生：
- CLKDIV=0：旁路模式，CK = ker_ck（要求 ker_ck 占空比 50%）
- CLKDIV>0：分频模式，CK = ker_ck / (2 × CLKDIV)

**CMD/Data 输出相位 (Figure 692, Table 423)：**

| CLKDIV | DDR | NEGEDGE | CMD/Data 输出时机 |
|--------|-----|---------|------------------|
| 0 | x | x | ker_ck 下降沿 |
| >0 | 0 | 0 | CK 上升沿之后的 ker_ck 下降沿 |
| >0 | 0 | 1 | 产生 CK 下降沿的同一个 ker_ck 上升沿 |
| >0 | 1 | 0 | CK 上升沿之后的 ker_ck 下降沿（DDR: Data 在 CK 任一边沿后的 ker_ck 下降沿） |
| >0 | 1 | 1 | 产生 CK 下降沿的同一个 ker_ck 上升沿 |

**接收时钟选择：**
- 默认：sdmmc_io_in_ck（来自 SDMMC_CK 引脚，DS/HS 模式）
- DLYB 延迟后：sdmmc_fb_ck（SDR104/HS200 必须，SDR50/DDR50 可选）
- 外部驱动器：SDMMC_CKIN（SDR12/25/50/DDR50）

### 58.5.4 SDMMC Adapter 子单元

| 子单元 | 时钟域 | 功能 |
|--------|--------|------|
| 寄存器 + FIFO | sdmmc_hclk (AHB) | 配置和数据缓冲 |
| 控制单元 | sdmmc_ker_ck | 电源管理、时钟管理、I/O 方向 |
| 命令路径 (TX) | sdmmc_ker_ck | 发送命令到卡 |
| 数据发送路径 | sdmmc_ker_ck | 发送数据到卡 |
| 响应路径 (RX) | sdmmc_rx_ck | 接收卡响应 |
| 数据接收路径 | sdmmc_rx_ck | 接收卡数据 |
| IDMA | sdmmc_hclk | 内部 DMA，AHB Master |

### 电源阶段 (Figure 693)

三个阶段：power-off → power-up → power-on

时钟输出在以下情况无效：
- 复位后
- power-off / power-up 阶段
- 省电模式（PWRSAV=1）：总线空闲 8 个时钟后停止 CK，CPSM/DPSM 激活时恢复

### 命令/响应路径 (CPSM, Figure 694)

- 写入命令寄存器且 EN=1 后，开始发送命令
- 命令发送完自动附加 CRC
- 发送完成后：
  - 不需要响应：回到 Idle
  - 需要响应：等待接收
- 收到响应后：
  - 有 CRC 的响应：校验 CRC，设置状态标志
  - 无 CRC 的响应：不校验
- CPSMACT 位指示 CPSM 是否处于活动状态

### CPSM 状态机 (Figure 695)

```
Idle ──┬─ WAITPEND=0, BOOTEN=0 ──→ Send ──┬─ WAITRESP=00 ──→ Idle (CMDSENT)
       │                                   └─ WAITRESP≠00 ──→ Wait ──┬─ 检测到起始位 → Receive → Idle
       ├─ WAITPEND=1 ──→ Pending ──→ Send                            └─ 超时 → Idle (CTIMEOUT)
       └─ BOOTEN=1 ──→ Boot
```

**各状态：**
- **Idle**：不活动。写入命令寄存器且 CPSMEN=1 后转出。至少保持 8 个 CK 周期（满足 NCC/NRC）
- **Send**：发送命令 + CRC。CMDTRANS=1 时在命令结束后向 DPSM 发 DataEnable 信号
- **Wait**：等待响应起始位。超时 = 64 个 SDMMC_CK 周期，超时置 CTIMEOUT
- **Receive**：接收响应，校验 CRC。通过→CMDREND，失败→CCRCFAIL。更新 RESPCMDR/RESPxR
- **Pending**：数据对齐命令（如 CMD12 Stop），等 DPSM 就绪后进入 Send
- **Boot**：eMMC 启动模式

### 命令格式 (Table 424)

48 位固定长度：

| 位 | 宽度 | 值 | 描述 |
|----|------|---|------|
| 47 | 1 | 0 | Start bit |
| 46 | 1 | 1 | Transmission bit (host=1) |
| [45:40] | 6 | x | Command index |
| [39:8] | 32 | x | Argument |
| [7:1] | 7 | x | CRC7 |
| 0 | 1 | 1 | End bit |

命令数据来自两个寄存器：32-bit argument + 6-bit command index。

### 响应格式

**短响应 (48 bit, Table 425/426)：**

| 位 | 宽度 | 描述 |
|----|------|------|
| 47 | 1 | Start bit (0) |
| 46 | 1 | Transmission bit (card=0) |
| [45:40] | 6 | Command index 或 111111 |
| [39:8] | 32 | Card status / OCR / Argument |
| [7:1] | 7 | CRC7（无 CRC 时为 1111111） |
| 0 | 1 | End bit (1) |

**长响应 (136 bit, Table 427)：**

| 位 | 宽度 | 描述 |
|----|------|------|
| 135 | 1 | Start bit (0) |
| 134 | 1 | Transmission bit (0) |
| [133:128] | 6 | Reserved (111111) |
| [127:1] | 127 | CID 或 CSD（含内部 CRC7） |
| 0 | 1 | End bit (1) |

响应存储：RESPCMDR（6-bit command index）+ RESP1~4R（4 × 32-bit 数据）

命令/响应路径为半双工：同一时刻只能发送命令或接收响应。

### CRC 校验

CRC7 生成多项式：G(x) = x^7 + x^3 + 1

- 短响应/命令：对前 40 bit（start + transmission + index + argument/status）计算
- 长响应（R2）：仅对 120-bit CID/CSD 内容计算，不含 start/transmission/reserved 位

CRC[6:0] = remainder[(M(x) × x^7) / G(x)]

### CPSM 特殊命令 (Table 428)

通过控制位组合触发不同行为：

| VSWITCH | BOOTEN | BOOTMOD | CMDTRAN | WAITPEND | CMDSTOP | WAITINT | 行为 |
|---------|--------|---------|---------|----------|---------|---------|------|
| 1 | x | x | x | x | x | x | 电压切换序列 |
| 0 | 1 | 0 | x | x | x | x | 正常 Boot |
| 0 | 1 | 1 | x | x | x | x | 交替 Boot |
| 0 | 0 | 1 | x | x | x | x | 停止交替 Boot |
| 0 | 0 | 0 | 1 | x | x | x | 命令 + 数据传输 |
| 0 | 0 | 0 | 0 | 1 | 1 | x | eMMC stream，STOP_TRANSMISSION 挂起直到数据结束 |
| 0 | 0 | 0 | 0 | 1 | 0 | x | eMMC stream，非 STOP 命令挂起直到数据结束 |
| 0 | 0 | 0 | 0 | 0 | 1 | x | 发送 STOP_TRANSMISSION，停止正在进行的数据传输 |
| 0 | 0 | 0 | 0 | 0 | 0 | 1 | 进入 eMMC Wait-IRQ 模式 |
| 0 | 0 | 0 | 0 | 0 | 0 | 0 | 普通命令 |

- CMDTRAN=1：命令发送完后自动触发 DPSM（DataEnable 信号）
- WAITPEND=1：命令挂起，等 DPSM 传输完再发送
- CMDSTOP=1：发送停止命令

### 命令路径状态标志 (Table 429)

| 标志 | 含义 |
|------|------|
| CMDSENT | 无响应命令发送完成（Send→Idle） |
| CMDREND | 响应 CRC 校验通过（Receive→Idle） |
| CCRCFAIL | 响应 CRC 校验失败（Receive→Idle） |
| CTIMEOUT | 等待响应超时，未检测到起始位（Wait→Idle） |
| CKSTOP | 电压切换命令后时钟停止 |
| VSWEND | 电压切换超时（5ms + 1ms） |
| CPSMACT | CPSM 非 Idle 状态（传输进行中） |

### 命令路径错误处理 (Table 430)

| 错误 | CPSM 状态 | 原因 | 主机动作 |
|------|-----------|------|---------|
| Timeout | Wait | 超时内未收到响应起始位 | 复位或重新上电 |
| CRC Fail | Receive | 传输错误导致 CRC 负面状态 | 重发命令 |

注：若 CMDTRANS=1（命令绑定了数据传输），错误恢复时还需发送 STOP_TRANSMISSION 让 DPSM 回到 Idle。

---

## 58.5.5 数据路径 (DPSM, Figure 696)

数据路径子单元通过 SDMMC_D[7:0] 线收发数据：
- **数据发送路径**：时钟源 = SDMMC_CK，向卡发送数据
- **数据接收路径**：时钟源 = sdmmc_rx_ck，从卡接收数据

内部结构（Figure 696）：
- Odd/Even 两套移位寄存器 + CRC 模块（为 DDR 模式设计）
- 接收侧：sdmmc_rx_ck 驱动
- 发送侧：SDMMC_CK 驱动
- 中间通过 FIFO 与 AHB 总线交互

### 数据总线宽度

由 WIDBUS 位配置：
- 1-bit：仅 SDMMC_D0（默认，初始化阶段）
- 4-bit：SDMMC_D[3:0]（SD 卡常用）
- 8-bit：SDMMC_D[7:0]（eMMC 专用）

### SDR / DDR 采样模式

由 DDR 位控制，仅作用于数据线（CMD 线始终 SDR）：
- **SDR**：仅上升沿采样
- **DDR**：上升沿 + 下降沿都采样，带宽翻倍

**DDR 模式规则 (Figure 697/698)：**
- 上升沿采 odd 字节，下降沿采 even 字节
- 数据载荷必须是 2 字节的整数倍
- 每条数据线有两个 CRC16（odd CRC + even CRC）
- Start/End bit、CRC status、Busy 信号为全周期，仅上升沿采样
- DDR 模式要求 CLKDIV ≥ 2（不能旁路）

### DPSM 状态机 (Figure 699)

| 状态 | 说明 |
|------|------|
| Idle | 空闲，等待激活 |
| Wait_S | 等待发送，FIFO 有数据且 DTHOLD=0 后进入 Send |
| Send | 向卡发送数据 |
| Wait_R | 等待接收，检测到起始位后进入 Receive |
| Receive | 从卡接收数据 |
| Busy | 写操作后卡拉低 D0 表示忙，等卡释放 |
| R_W | SDIO Read-Wait 状态 |
| Wait_Ack | eMMC Boot 等待确认 |

**DPSM 激活条件（Idle 退出）：**

三种方式：
1. CMDTRANS=1 的命令完成后，CPSM 发出 DataEnable 信号（最常见）
2. 软件直接写 DTEN=1
3. 检测到 D0 Busy（R1b 类型响应后卡拉低 D0）

激活后根据 DTDIR 方向位：
- DTDIR=0（发送）→ Wait_S → Send
- DTDIR=1（接收）→ Wait_R → Receive
- DTDIR=1 且 BOOTACKEN=1 → Wait_Ack → Wait_R → Receive

**关键转换：**
- Send 完成 → Busy（等卡 CRC status + D0 释放）
- Busy 退出：CRC OK 且 D0 不忙 → Idle；CRC Fail → Idle（报错）
- CPSM Abort：任意状态强制回 Idle（需等 FIFO 空）
- DTHOLD=1：暂停传输，回 Idle

### DPSM 各状态详细行为

**Wait_R（等待接收）：**
- Block 模式：检测到起始位 → Receive，加载 DBLOCKSIZE 作为块计数
- SDIO 多字节：检测到起始位 → Receive，加载 DATALENGTH
- Stream 模式：检测到起始位 → Receive，加载 DATALENGTH
- DATACOUNT=0 → 等 FIFO 空 → Idle，置 DATAEND
- 超时未检测到起始位 → 置 DTIMEOUT，留在 Wait_R
- DTHOLD=1 → 暂停回 Idle，置 DHOLD

**Receive（接收中）：**
- Block 模式：收满 DBLOCKSIZE 字节后等 CRC
  - CRC OK → Wait_R（继续收下一个 block）或 R_W（SDIO Read-Wait）
  - CRC Fail → 置 DCRCFAIL，阻止后续接收
- Stream 模式：DATACOUNT 减到 0 停止
- FIFO 满溢出 → 置 RXOVERR，阻止后续接收
- CPSM Abort → 回 Idle，置 DABORT

**Wait_S（等待发送）：**
- FIFO 非空且 DTHOLD=0 → Send
- DATACOUNT=0 → Idle，置 DATAEND
- 至少停留 2 个时钟周期（满足 NWR 时序：响应到数据之间的最小间隔）
- DTHOLD=1 → 暂停回 Idle，置 DHOLD

**Send（发送中）：**
- Block 模式：发完 DBLOCKSIZE 字节 → 自动追加 CRC + end bit → Busy
- SDIO 多字节：发完 DATALENGTH 字节 → CRC + end → Busy
- Stream 模式：DATACOUNT 减到 0 → Busy，最后字节前触发 CPSM 发挂起命令（CMD12）
- FIFO 空但数据未发完 → 置 TXUNDERR（underrun）
- CPSM Abort → 发完当前位 + end bit → Busy → 等不忙 → Idle + DABORT

**Busy（等待卡释放）：**
- CRC status = OK + D0 释放 → Wait_S（继续发下一个 block）
- CRC status = Fail → 置 DCRCFAIL
- D0 持续拉低超时 → 置 DTIMEOUT
- R1b 响应触发的 Busy → D0 释放后置 BUSYD0END → Idle

**R_W（SDIO Read-Wait）：**
- RWSTOP=1 → Wait_R（恢复接收）
- CPSM Abort → 等 FIFO 空 → Idle + DABORT

**Wait_Ack（eMMC Boot 确认）：**
- 收到正确确认 → Wait_R
- 收到错误模式 → 置 ACKFAIL，留在 Wait_Ack
- 超时 → 置 ACKTIMEOUT，留在 Wait_Ack
- CPSM Abort → Idle + DABORT

### 数据超时定时器 (DATATIME)

DATATIME 寄存器配置超时周期，超时时置 DTIMEOUT：

| 场景 | 超时条件 |
|------|---------|
| 接收 | DATACOUNT>0 但在超时期内未检测到下一个起始位 |
| 发送 | 8 个 CK 周期内未收到 CRC status 起始位，或 Busy 超时 |
| R1b 响应 | Busy 持续时间超过超时期 |

DATATIME=0 时超时窗口最短（2 个时钟周期）。

### 数据帧格式 (Table 431)

| 模式 | 帧格式 | CRC | DTMODE |
|------|--------|-----|--------|
| Block | Start(0) + Data(DBLOCKSIZE) + CRC16 + End(1) | 有 | 00 |
| SDIO 多字节 | Start(0) + Data(DATALENGTH) + CRC16 + End(1) | 有 | 01 |
| eMMC Stream | Start(0) + Data(DATALENGTH) + End(1) | 无 | 10 |

- Block 模式总传输量由 DATALENGTH 决定，每个 block 大小由 DBLOCKSIZE 决定
- Block 和 SDIO 多字节帧格式相同，区别在于 block 可连发多帧、大小必须是 2^N

### 数据路径状态标志 (Table 432)

| 标志 | 场景 | 含义 | DPSM 动作 |
|------|------|------|-----------|
| DATAEND | TX | 所有数据发完 + CRC OK + Busy 结束 + DTHOLD=0 + DATACOUNT=0 | Wait_S → Idle |
| | RX/Boot | 所有数据收完 + CRC OK + FIFO 空 + DATACOUNT=0 | Wait_R → Idle |
| DCRCFAIL | TX | 卡返回 CRC status = Fail + Busy 结束 | DATACOUNT>0: 留 Busy 等 Abort；=0: → Idle |
| | RX/Boot | 接收 CRC 校验失败 + FIFO 空 | DATACOUNT>0: 留 Receive 等 Abort；=0: → Idle |
| DTIMEOUT | TX | 未收到 CRC token 起始位(Ncrc)或 Busy 超时 | 留 Busy 等 Abort |
| | RX/Boot | 数据起始位超时 | 留 Wait_R 等 Abort |
| | CMD R1b | R1b 响应后 Busy 超时 | 留 Busy 等 Abort |
| DBCKEND | TX | DTHOLD=1：一个 block 发完 + CRC OK + Busy 结束，DATACOUNT>0 | Busy → Wait_S |
| | RX/Boot | RWSTART=1：一个 block 收完 + CRC OK，DATACOUNT>0 | Receive → R_W |
| DHOLD | TX | DTHOLD=1：block 传输完成后暂停 | Wait_S → Idle |
| | RX | DTHOLD=1：block 收完 + FIFO 空，DATACOUNT>0 | Wait_R → Idle |
| | CMD R1b | Abort + Busy 结束 | Busy → Idle |
| DABORT | TX/RX/Boot | CPSM Abort 在传输最后 2 bit 之前到达 | 任意状态 → Idle |
| BUSYD0END | CMD R1b | R1b 响应的 Busy 正常结束（D0 释放） | Busy → Idle |
| ACKFAIL | Boot | Boot 确认模式错误 | 留 Wait_Ack 等 Abort |
| ACKTIMEOUT | Boot | 等待 Boot 确认超时 | 留 Wait_Ack 等 Abort |
| RXOVERR | RX | 接收 FIFO 满溢出 | 留 Receive 等 Abort |
| TXUNDERR | TX | 发送 FIFO 空下溢 | 留 Send 等 Abort |
| DPSMACT | — | DPSM 非 Idle 状态（传输进行中） | — |

注：大多数错误标志置位后 DPSM **不会自动回 Idle**，需要软件发 STOP 命令触发 CPSM Abort 才能恢复。

### 数据路径错误处理 (Table 433)

| 错误 | 发生状态 | 原因 | 主机恢复动作 |
|------|---------|------|-------------|
| Timeout | Wait_Ack | Boot 确认超时 | 复位 SDMMC（RCC.SDMMCxRST） |
| Timeout | Wait_R | 数据起始位超时 | 发 STOP 命令 |
| Timeout | Busy（数据传输） | 卡 Busy 太久 | 发 STOP 命令 |
| Timeout | Busy（R1b） | R1b Busy 太久 | 发 reset 命令 |
| CRC Fail | Receive | 接收 CRC 错误 | 发 STOP 命令 |
| CRC Fail | Busy | 卡返回 CRC status = Fail | 发 STOP 命令 |
| Ack Fail | Wait_Ack | Boot 确认格式错误 | 停止 Boot |
| Overrun | Receive | 接收 FIFO 满（软件/DMA 读太慢） | 发 STOP 命令 |
| Underrun | Send | 发送 FIFO 空（软件/DMA 写太慢） | 发 STOP 命令 |

统一恢复模式：出错 → 发 STOP 命令 → CPSM Abort → DPSM 回 Idle → 重试。

### Data FIFO

- 大小：32-bit × 16 words = **64 字节**
- TX/RX 共用同一个 FIFO（由 DTDIR 位选择方向）
- 工作在 AHB 时钟域（sdmmc_hclk），与 SDMMC_CK 域之间有同步逻辑
- 容量很小，高速传输必须依赖 IDMA，否则容易 overrun/underrun

**FIFO 访问方式 (Table 434)：**

| IDMAEN | 访问方式 | 说明 |
|--------|---------|------|
| 0 | 软件通过 AHB slave | CPU 手动读写 FIFO |
| 1 | IDMA 通过 AHB master | 硬件自动搬运，不需要 CPU 参与 |

**发送流程（软件模式，IDMAEN=0）：**

1. 配置 DATALENGTH（总字节数）和 DBLOCKSIZE（块大小）
   - Block 模式要求 DATALENGTH 是 DBLOCKSIZE 的整数倍
2. 设置 DTDIR=0（发送方向）
3. 启动传输：发带 CMDTRANS=1 的命令，或写 DTEN=1
4. 等 DPSMACT=1 后开始写 FIFO
5. 循环：等 TXFIFOHE（FIFO 半空）→ 写数据直到 TXFIFOF（满）或写完
6. 等 DATAEND 标志 → 传输完成

出错时：停止写入 + FIFORST 冲洗 FIFO。

**接收流程（软件模式，IDMAEN=0）：**

1. 配置 DATALENGTH 和 DBLOCKSIZE
2. 设置 DTDIR=1（接收方向）
3. 启动传输：发带 CMDTRANS=1 的命令，或写 DTEN=1
4. 等 DPSMACT=1，FIFO 开始接收卡数据
5. 循环：等 RXFIFOHF（FIFO 半满）→ 读数据直到 RXFIFOE（空）
6. 等 DATAEND → 读完剩余数据

出错时：停止读取 + FIFORST。

**FIFO 状态标志 (Table 435/436)：**

| 标志 | 发送含义 | 接收含义 |
|------|---------|---------|
| TXFIFOF / RXFIFOF | FIFO 满 | FIFO 满 |
| TXFIFOE / RXFIFOE | FIFO 空 | FIFO 空 |
| TXFIFOHE / RXFIFOHF | FIFO 半空（可写） | FIFO 半满（可读） |
| TXUNDERR / RXOVERR | 下溢错误 | 溢出错误 |

### CLKMUX — 接收时钟源选择 (Figure 700)

| sdmmc_rx_ck 来源 | SELCLKRX | 适用模式 |
|------------------|----------|---------|
| sdmmc_io_in_ck | 默认 | DS / HS（无外部驱动器，CK 引脚回环） |
| SDMMC_CKIN | 外部 | SDR12/25/50, DDR50（有外部电平转换器） |
| sdmmc_fb_ck | DLYB | SDR104, HS200（经 DLYB 延迟调谐） |

切换时机：必须在 CPSM 和 DPSM 都处于 Idle 时才能改。

### AHB Slave 接口

- FIFO 仅支持 **32-bit word 访问**，半字/字节访问触发 bus fault
- DATALENGTH 非 4 倍数时：
  - TX：最后一次 word 写入只有部分有效字节
  - RX：最后一次 word 读取，无效字节填 0
- 中断：每个状态标志有对应 mask 位，mask=1 时该标志可触发中断

---

## 58.5.6 SDMMC IDMA

SDMMC 内置 DMA，通过 AHB master 接口直接访问内存，不需要外部 DMA1/DMA2。

### 基本特性

- 单通道，方向由 DTDIR 决定（TX 或 RX）
- Burst 传输：固定 8 beats（32 字节/次）
- DATALENGTH 非 burst 整数倍时，剩余部分用 single 传输
- DATALENGTH 非 4 倍数时，最后用 halfword/byte 传输
- 使能位：IDMAEN

### 两种模式（IDMABMODE）

**Single Buffer（单缓冲）：**
- 配置 IDMABASE = 内存起始地址
- IDMA 从该地址线性读/写，直到 DATALENGTH 字节传完
- 传完后置 DATAEND
- 适用于连续内存块的读写（如读 N 个 sector）

**Linked List（链表）：**
- 多缓冲，适用于内存不连续或 scatter-gather 场景
- 每个链表节点描述一个缓冲区
- IDMA 依次处理各节点，传完一个 buffer 自动加载下一个

### 链表节点结构 (Figure 701)

节点地址 = IDMABA（基地址寄存器）+ IDMALA（偏移值）

IDMALAR 中的控制位：

| 位 | 含义 |
|----|------|
| ULA | 1=传完当前 buffer 后加载下一个节点；0=这是最后一个节点 |
| ULS | 1=从链表加载 IDMABSIZE（3-word 节点）；0=沿用上次 size（2-word 节点） |
| ABR | buffer 就绪确认（动态链表用） |

三种链表结构：

| 类型 | 节点大小 | 说明 |
|------|---------|------|
| Variable size | 3 words | IDMALAR + IDMABASE + IDMABSIZE（每个 buffer 大小不同） |
| Fixed size | 2 words | IDMALAR + IDMABASE（所有 buffer 大小相同，省一个 word） |
| Mixed | 混合 | 需要改 size 时用 3-word，不改时用 2-word |

buffer size（IDMABSIZE）必须是 burst size 的整数倍。

### IDMABTC 中断与 ABR 流水线机制

每传完一个链表 buffer（且 ULA=1），产生 **IDMABTC** 中断。

ABR 是安全阀：IDMA 加载下一个节点时检查 ABR 位，ABR=1 才继续，ABR=0 则报错停止。

**动态链表的流水线工作方式：**

```
启动前准备：buffer 0（当前）+ buffer 1（ABR=1，提前量）

时间线：
IDMA:  [传输 buf 0]     [传输 buf 1]     [传输 buf 2]    ...
软件:   ────────────  ↑IDMABTC:准备buf2  ↑IDMABTC:准备buf3
                      ABR=1已就绪,无缝切入
```

- 提前量只需 1 个 buffer：当前传输的下一个 buffer 必须已就绪（ABR=1）
- IDMABTC 中断通知"当前 buffer 传完了"，软件在下一个 buffer 传输期间准备再下一个
- 只要软件能在一个 buffer 传输时间内完成准备，就永远不会触发 ABR 错误
- 第一个 buffer 的 ABR 不检查（启动前必须由软件直接配置寄存器）

### IDMA 传输错误（IDMATE）

触发条件：
- 访问保留地址空间（非法内存）
- 链表 buffer 用完但数据未传完（ULA=0 提前结束）
- ABR=0（下一个 buffer 未就绪）

错误后果：
- IDMA 停止，置 IDMATE 标志
- 硬件流控关闭，通常伴随 TXUNDERR 或 RXOVERR
- 软件需要重新初始化链表后才能重启传输

---

## 58.5.7 AHB 与 SDMMC_CK 时钟关系 (Table 437)

AHB 时钟带宽必须 ≥ SDMMC 总线带宽的 3 倍（burst 传输开销余量）。

| 模式 | 总线宽度 | 最大 SDMMC_CK | 最低 AHB 时钟 |
|------|---------|--------------|--------------|
| eMMC DS | 8-bit | 26 MHz | 19.5 MHz |
| eMMC HS | 8-bit | 52 MHz | 39 MHz |
| eMMC DDR52 | 8-bit | 52 MHz | 78 MHz |
| eMMC HS200 | 8-bit | 200 MHz | 150 MHz |
| SD DS / SDR12 | 4-bit | 25 MHz | 9.4 MHz |
| SD HS / SDR25 | 4-bit | 50 MHz | 18.8 MHz |
| SD DDR50 | 4-bit | 50 MHz | 37.5 MHz |
| SD SDR50 | 4-bit | 100 MHz | 37.5 MHz |
| SD SDR104 | 4-bit | 208 MHz | 78 MHz |

DDR 模式 AHB 要求更高（同频率下数据量翻倍）。

---

## 58.6 卡功能描述

### 58.6.1 SD I/O 模式（SDIO 设备专用，SD 存储卡不涉及）

SDIO 特有功能：
- **SDIO 中断**：设备通过 D1 引脚（pin 8）向主机发中断，电平触发低有效
  - 1-bit 模式：D1 专用做 IRQ，无时序限制
  - 4-bit 模式：仅在数据块间的"中断窗口期"采样 D1
  - 同步中断（DS/HS/SDR12/SDR25）：数据块间 2 CK 周期窗口
  - 异步中断（SDR50/SDR104/DDR50）：最后数据块后 2~4 CK 窗口
- **Read Wait**：主机暂停读数据（停止时钟或 D2 信号），SDIO 流控用
- **Suspend/Resume**：SDIO v4.00 起不再支持，多功能卡分时共享总线的遗留机制

这些功能仅适用于 SDIO 外设卡（WiFi/BT 模块等），SD/TF 存储卡不使用。

SDIO Suspend/Resume 和 Read Wait 的详细流程见手册 p2903-2906（Figure 707~710），此处不展开。
关键点：硬件不保存中断点，软件必须通过 DATACOUNT 自己算剩余数据量。

### 58.6.2 CMD12 使用规则 (Table 440)

CMD12（STOP_TRANSMISSION）用于终止数据传输，卡在 end bit 后 2 CK 停止。

**何时需要 CMD12：**

| 操作 | 是否需要 CMD12 |
|------|---------------|
| Stream 读/写 | 必须 |
| Open-ended 多 block 读/写（CMD18/CMD25） | 必须 |
| 预定义 block 数量读/写（CMD23 + CMD18/CMD25） | 不需要（传完自动停，发则视为非法） |
| 任何操作出错时 | 必须（abort） |

**数据中止流程（CMD12 中止正在进行的传输）：**

1. 配置 CMD12，置 CMDSTOP=1 → 发送后自动生成 CPSM Abort 信号
2. 清 WAITPEND → 立即发送
3. FIFO 处理：
   - IDMAEN=1：硬件自动（写：停 IDMA + 冲洗；读：IDMA 搬空剩余）
   - IDMAEN=0：软件处理（写：停写 + FIFORST；读：读空 + FIFORST）
4. DABORT 标志确认中止完成

**Block 操作正常结束流程（open-ended 多 block）：**

写：
1. DTMODE = "block data transfer ending with STOP_TRANSMISSION"
2. 等 DATAEND（所有数据发完 + CRC OK + Busy 结束）
3. 发 CMD12 → 卡进入 Idle

读：
1. DTMODE = "block data transfer ending with STOP_TRANSMISSION"
2. 等 DATAEND（所有数据收完 + FIFO 空）
3. 发 CMD12（CMDSTOP=1）→ 卡停止发数据

注：DPSM 不会收/发超过 DATALENGTH 的数据。CMD12 只是通知卡结束，主机侧数据量由 DATALENGTH 精确控制。

### 58.6.3 eMMC Sleep — CMD5

eMMC 通过 CMD5 在 Standby ↔ Sleep 状态切换（Figure 712）：
- CMD5(SLEEP)：Standby → Sleep，卡拉低 D0 表示转换中，等 BUSYD0END 确认进入 Sleep
- CMD5(AWAKE)：Sleep → Standby，先恢复 Vcc 再发命令，等 BUSYD0END 确认

流程：
1. 使能 BUSYD0END 中断
2. 发 CMD5
3. BUSYD0END 中断到来 → 状态切换完成
4. Sleep 状态下可关闭 Vcc；AWAKE 前必须先恢复 Vcc

### 58.6.4 Interrupt Mode — Wait-IRQ（eMMC 专用）

eMMC 的卡中断机制，通过 CMD40（GO_IRQ_STATE）进入：
- 主机和卡同时进入中断等待状态，无数据传输
- 卡有内部事件时在 CMD 线上发中断服务请求响应（open-drain 模式）
- 主机检测到起始位 → 接收响应 → 退出中断模式

**约束：**
- CLKDIV > 1
- SELCLKRX 选 sdmmc_io_in_ck 或 SDMMC_CKIN
- 时钟必须保持活动
- 卡必须在 Standby 状态才能发 CMD40

**流程：**
1. 设置 CK 频率匹配 open-drain 模式数据速率
2. 加载 CMD40
3. 置 WAITINT=1
4. 使能 CPSM → 发 CMD40，CPSM 停在 Wait 状态等待响应
5. 退出方式：
   - 卡发来中断响应起始位 → CPSM 进 Receive → CMDREND/CCRCFAIL
   - 主机主动退出：清 WAITINT → 主机发送自己的中断服务请求响应

冲突处理：主机和卡同时发响应时，逐位仲裁，主机在 transmission bit 后让出。

### 58.6.5 Boot Operation（eMMC 专用，Figure 713）

eMMC 可直接从 Boot 分区读启动数据，无需标准初始化流程。

**两种启动方式：**
- **Normal boot**：保持 CMD 线低电平 ≥ 74 CK → 卡自动发 boot 数据
- **Alternative boot**：发 CMD0 参数 = 0xFFFFFFFA

**Boot 配置（EXT_CSD 寄存器）：**
- Byte[179]：Boot 分区选择 + 是否启用 boot 确认
- Byte[226]：Boot 数据大小
- Byte[177]：Boot 期间总线配置

**Boot 确认：** 卡在 50ms 内发送 "010" 模式到 D0（可选，BOOTACKEN 控制）

**Normal boot 流程：**
1. 复位卡
2. 可选：使能 BOOTACKEN，配置 ACKTIME，使能 ACKFAIL/ACKTIMEOUT 中断
3. 配置 DPSM 接收模式（DTDIR=1）+ DATALENGTH = boot 数据量
4. 使能 DTIMEOUT、DATAEND、CMDSENT 中断
5. 设置 BOOTMODE=normal，BOOTEN=1，使能 CPSM：
   - CMD 拉低（触发卡启动 boot）
   - ACK 超时计时开始
   - DPSM 使能，开始接收数据
6. 检查 boot 确认：
   - ACKFAIL：确认模式错误
   - ACKTIMEOUT：确认超时
7. DATAEND：所有 boot 数据接收完成
8. 读空 FIFO
9. 清 BOOTEN → CMD 拉高，56 CK 后 CMDSENT 确认 boot 结束
10. CMDSENT 表示 boot 流程完成，卡可接受新命令

中止 boot：在数据接收完之前清 BOOTEN → CPSM Abort → DABORT

**eMMC 分区结构：**

| 分区 | 说明 |
|------|------|
| Boot partition 1 | ≥128KB，存 bootloader |
| Boot partition 2 | ≥128KB，备份 |
| RPMB | Replay Protected Memory Block，安全存储 |
| User Data Area | 主数据区（默认访问对象） |
| General Purpose 1~4 | 可选通用分区 |

**应用程序访问 Boot 分区（正常模式下）：**

Boot 模式仅用于 ROM 代码上电启动（卡未初始化时）。应用程序通过分区切换访问：

1. CMD6 (SWITCH) → EXT_CSD[179] PARTITION_ACCESS 字段：
   - 0: User Data Area（默认）
   - 1: Boot partition 1
   - 2: Boot partition 2
   - 3: RPMB
2. CMD17/CMD18 正常读取
3. CMD6 (SWITCH) → 切回 PARTITION_ACCESS = 0

Boot 分区与 User Data 物理隔离，用户数据损坏不影响启动。

**Alternative boot 流程（Figure 714）：**

与 Normal boot 区别：用 CMD0(0xFFFFFFFA) 触发，用 CMD0(Reset) 结束。

1. 关电 → 复位卡 → 开电（保证 74 CK 空闲）
2. 可选 BOOTACKEN + ACKTIME
3. 配置 DPSM 接收 + DATALENGTH
4. BOOTMODE=alternative，加载 CMD0(0xFFFFFFFA)，BOOTEN=1，使能 CPSM
5. CMDSENT → 立即清 BOOTEN
6. 等 boot 确认 / 数据 / 错误
7. 数据收完（DATAEND）→ 发 CMD0(Reset) 终止 boot
8. CMDSENT 确认结束，清 BOOTMODE

---

### 58.6.6 R1b 响应处理 (Figure 715)

R1b = R1 响应 + D0 Busy 信号。卡发完响应后拉低 D0 表示内部操作中。

**常见 R1b 命令：**
- CMD12（STOP_TRANSMISSION）— 多 block 写后处理数据
- CMD6（SWITCH）— eMMC 切换分区/模式
- CMD38（ERASE）— 擦除（可能几百 ms）

**硬件支持：**
- BUSYD0 寄存器位：实时反映 D0 线状态
- BUSYD0END 标志：D0 从忙变空闲时置位
- DATATIME 寄存器：配置 Busy 超时保护

**处理流程：**
1. 配置 DATATIME（预期最大 Busy 时间）
2. 使能 CMDREND 中断
3. 发 R1b 类型命令
4. CMDREND 到来 → 检查 BUSYD0：
   - 不忙 → 直接完成
   - 忙 → 等 BUSYD0END 或 DTIMEOUT
5. BUSYD0END → 操作完成
6. DTIMEOUT → 超时错误

---

## 58.6.7 复位与上电流程 (Figure 716/717)

SDMMC 电源状态由 PWRCTRL 寄存器控制：

| 状态 | PWRCTRL | 信号线状态 | 说明 |
|------|---------|-----------|------|
| Reset | — | HiZ | 复位后默认，SDMMC 禁用 |
| Power-cycle | 10 | 全部驱动低 | 防止信号线给卡供电 |
| Power-off | 00 | 全部驱动高 | 等待上电稳定 |
| Power-on | 11 | CK 运行 | 74 CK 后可发命令 |

**卡上电复位流程（Card cycle power）：**
1. RCC 复位 SDMMC 外设（RCC.SDMMCxRST）
2. 关闭卡 Vcc 电源
3. PWRCTRL = Power-cycle（信号全拉低，≥1ms，防止信号线反向供电）
4. 开启 Vcc，等电源稳定
5. PWRCTRL = Power-off（信号拉高，≥1ms）
6. PWRCTRL = Power-on（CK 开始运行）
7. 74 CK 周期后 → 可发第一条命令（CMD0）

---

## 58.7 硬件流控 (Hardware Flow Control)

防止 FIFO underrun/overrun：在 FIFO 跟不上时**自动停 SDMMC_CK**。

- TX：FIFO 空 → 停 CK → FIFO 半满后恢复
- RX：FIFO 满 → 停 CK → FIFO 半空后恢复
- 停 CK 期间 AHB 接口仍然活着，FIFO 可被 IDMA/软件继续操作

使能：HWFC_EN = 1（默认关闭）

**停时钟不会断开通信**：SD/MMC 是纯同步协议，卡只认时钟边沿。时钟停了卡也冻住，计数器/超时都停，恢复后无缝继续。类似 SPI 中 CS 保持低而 CLK 暂停的状态。

**限制：**
- SDR104/HS200 使用 DLYB 时不能用（延迟 >1 周期导致停/恢复后相位失准）
- IDMA 链表传输错误时硬件流控自动关闭

**建议：** DS/HS 模式下 HWFC_EN=1 开启；SDR104/HS200 不开。

---

## 58.8 UHS-I 电压切换 (Figure 718)

UHS-I 模式（SDR12/25/50/104、DDR50）需要 1.8V 信号电压。卡上电后默认 3.3V，通过 CMD11 触发电压切换。

**完整流程（10 步）：**

1. 设置 CK 频率到 100~400 kHz
2. 置 VSWITCHEN=1，发 CMD11
3. 卡回 R1：
   - CRC OK → 主机释放 CMD/D[3:0]，硬件停 CK，CKSTOP 标志置位
   - CRC Fail / 超时 → 中止
4. 卡把 CMD 和 D[3:0] 拉低
5. 主机在 CKSTOP 后读 BUSYD0：
   - D0=低 → 切换外部电压调节器到 1.8V，置 VSWITCH 位
   - D0=高 → 失败，cycle power 复位
6. 卡检测 CK 停止 → 开始内部切换到 1.8V
7. 硬件保持 CK 低 ≥5ms（2^12 个内部时钟周期，400kHz 下约 10.24ms），然后自动恢复 CK
8. 卡在 CK 恢复后 1ms 内把 CMD/D[3:0] 拉高（证明 1.8V 工作正常）
9. 硬件在 CK 恢复 1ms 后采样 D0，置 VSWEND 标志
10. 主机检查 BUSYD0：
    - D0=高 → 切换成功，进入 UHS-I（默认 SDR12）
    - D0=低 → 切换失败，cycle power

**时序细节：**
- 5ms CK 停止由硬件自动计时（2^12 cycles @ CLKDIV 频率）
- 1ms 卡响应检测由硬件自动计时（2^9 cycles @ CLKDIV 频率）
- 软件只需在合适时机设置 VSWITCH 位，其余由硬件完成

### 外部电压转换器 (Figure 719)

1.8V/3.3V 切换需要外部电平转换芯片（如 ST6G3244ME）。

配套方向控制信号（仅 SDMMC1 支持）：

| 信号 | 功能 |
|------|------|
| SDMMC_CDIR | CMD 线方向指示 |
| SDMMC_D0DIR | D0 方向指示 |
| SDMMC_D123DIR | D1~D3 方向指示 |
| SDMMC_CKIN | 外部时钟反馈输入 |

- 转换器 EN（使能）和 SEL（电压选择）通过 GPIO 控制
- DIRPOL 位配置方向信号极性
- 无电压转换器的板子只能跑 DS/HS（3.3V，最高 25MB/s）

---

## 58.9 SDMMC 中断 (Table 441)

所有中断源命名规律：FLAG（状态标志）/ FLAGIE（中断使能）/ FLAGC（写 1 清除）。

| 类别 | 中断标志 | 含义 | 清除 |
|------|---------|------|------|
| 命令路径 | CCRCFAIL | 响应 CRC 失败 | CCRCFAILC |
| | CTIMEOUT | 响应超时 | CTIMEOUTC |
| | CMDREND | 响应接收完成（CRC OK） | CMDRENDC |
| | CMDSENT | 无响应命令发送完成 | CMDSENTC |
| 数据路径 | DCRCFAIL | 数据 CRC 失败 | DCRCFAILC |
| | DTIMEOUT | 数据超时 | DTIMEOUTC |
| | DATAEND | 数据传输完成 | DATAENDC |
| | DHOLD | 数据传输暂停 | DHOLDC |
| | DBCKEND | 一个 block 传输完成 | DBCKENDC |
| | DABORT | 数据传输被中止 | DABORTC |
| FIFO | TXUNDERR | 发送下溢 | TXUNDERRC |
| | RXOVERR | 接收溢出 | RXOVERRC |
| | TXFIFOHE | TX FIFO 半空 | 自动 |
| | RXFIFOHF | RX FIFO 半满 | 自动 |
| | TXFIFOF | TX FIFO 满 | 自动 |
| | RXFIFOF | RX FIFO 满 | 自动 |
| | TXFIFOE | TX FIFO 空 | 自动 |
| | RXFIFOE | RX FIFO 空 | 自动 |
| Busy | BUSYD0END | D0 Busy 释放 | BUSYD0ENDC |
| SDIO | SDIOIT | SDIO 设备中断 | SDIOITC |
| Boot | ACKFAIL | Boot 确认失败 | ACKFAILC |
| | ACKTIMEOUT | Boot 确认超时 | ACKTIMEOUTC |
| 电压切换 | VSWEND | 电压切换完成 | VSWENDC |
| | CKSTOP | CK 已停止 | CKSTOPC |
| IDMA | IDMATE | IDMA 传输错误 | IDMATEC |
| | IDMABTC | IDMA buffer 传输完成 | IDMABTCC |

FIFO 状态标志（TXFIFOHE/RXFIFOHF 等）实时反映 FIFO 状态，无需手动清除。

所有中断均可将 SDMMC 从 Sleep 模式唤醒。

**基本 block 读写驱动最常用的中断：**
- CMDREND — 命令完成
- DATAEND — 数据传输完成
- CTIMEOUT — 卡不响应
- DCRCFAIL / DTIMEOUT — 数据错误
- BUSYD0END — R1b Busy 结束

---

## 58.10 SDMMC 寄存器

所有寄存器必须 32-bit word 访问，字节/半字访问触发 AHB bus fault。

### 寄存器总表

#### 核心控制

| 偏移 | 名称 | 功能 | 关键字段 |
|------|------|------|---------|
| 0x000 | SDMMC_POWER | 电源控制 | PWRCTRL[1:0], VSWITCH, VSWITCHEN, DIRPOL |
| 0x004 | SDMMC_CLKCR | 时钟控制 | CLKDIV[9:0], WIDBUS[1:0], DDR, HWFC_EN, NEGEDGE, PWRSAV, BUSSPEED, SELCLKRX[1:0] |
| 0x008 | SDMMC_ARGR | 命令参数 | CMDARG[31:0] |
| 0x00C | SDMMC_CMDR | 命令寄存器 | CMDINDEX[5:0], WAITRESP[1:0], CPSMEN, CMDTRANS, CMDSTOP, WAITPEND, WAITINT, DTHOLD, BOOTEN, BOOTMODE, CMDSUSPEND |

#### 响应

| 偏移 | 名称 | 功能 |
|------|------|------|
| 0x010 | SDMMC_RESPCMDR | 响应命令索引 RESPCMD[5:0]（只读） |
| 0x014 | SDMMC_RESP1R | 短响应数据 / 长响应 [127:96] |
| 0x018 | SDMMC_RESP2R | 长响应 [95:64] |
| 0x01C | SDMMC_RESP3R | 长响应 [63:32] |
| 0x020 | SDMMC_RESP4R | 长响应 [31:0] |

#### 数据路径

| 偏移 | 名称 | 功能 | 关键字段 |
|------|------|------|---------|
| 0x024 | SDMMC_DTIMER | 数据/Busy 超时 | DATATIME[31:0]（CK 周期数） |
| 0x028 | SDMMC_DLENR | 数据长度 | DATALENGTH[24:0]（最大 16MB） |
| 0x02C | SDMMC_DCTRL | 数据控制 | DTEN, DTDIR, DTMODE[1:0], DBLOCKSIZE[3:0], FIFORST, BOOTACKEN, SDIOEN, RWMOD/STOP/START |
| 0x030 | SDMMC_DCNTR | 数据计数（只读） | DATACOUNT[24:0]（剩余字节数） |

#### 状态/中断

| 偏移 | 名称 | 功能 |
|------|------|------|
| 0x034 | SDMMC_STAR | 状态寄存器（只读，静态标志 + 动态标志） |
| 0x038 | SDMMC_ICR | 中断清除（写 1 清对应标志） |
| 0x03C | SDMMC_MASKR | 中断使能掩码 |
| 0x040 | SDMMC_ACKTIMER | Boot 确认超时 ACKTIME[24:0] |

#### IDMA

| 偏移 | 名称 | 功能 | 关键字段 |
|------|------|------|---------|
| 0x050 | SDMMC_IDMACTRLR | IDMA 控制 | IDMAEN, IDMABMODE |
| 0x054 | SDMMC_IDMABSIZER | Buffer 大小 | IDMABNDT[11:0]（×32 = 字节数） |
| 0x058 | SDMMC_IDMABASER | Buffer 基地址 | IDMABASE[31:0]（word 对齐） |
| 0x064 | SDMMC_IDMALAR | 链表地址 | ULA, ULS, ABR, IDMALA[13:0] |
| 0x068 | SDMMC_IDMABAR | 链表基地址 | IDMABA[29:0] |

#### FIFO 和标识

| 偏移 | 名称 | 功能 |
|------|------|------|
| 0x080~0x0BC | SDMMC_FIFORx | 数据 FIFO（16 × 32-bit = 64B） |
| 0x3F4 | SDMMC_VERR | 版本号 |
| 0x3F8 | SDMMC_IPIDR | IP 标识 |
| 0x3FC | SDMMC_SIDR | Size 标识 |

### 关键字段编码

**CLKDIV 分频：**
- CLKDIV=0：SDMMC_CK = sdmmc_ker_ck（旁路，不支持 DDR）
- CLKDIV>0：SDMMC_CK = sdmmc_ker_ck / (2 × CLKDIV)
- 初始化阶段 CK ≤ 400kHz

**WAITRESP 响应类型：**

| 值 | 含义 | 等待标志 |
|----|------|---------|
| 00 | 无响应 | CMDSENT |
| 01 | 短响应 + CRC | CMDREND 或 CCRCFAIL |
| 10 | 短响应无 CRC（R3/OCR） | CMDREND |
| 11 | 长响应 + CRC（R2/CID/CSD） | CMDREND 或 CCRCFAIL |

**DTMODE 数据传输模式：**

| 值 | 含义 |
|----|------|
| 00 | Block，靠 block count 结束 |
| 01 | SDIO 多字节 |
| 10 | eMMC Stream |
| 11 | Block，以 STOP_TRANSMISSION 结束 |

**DBLOCKSIZE：** log2 编码，值 N = 2^N 字节。SD 卡固定用 9（= 512B）。

**IDMABNDT buffer 大小：** 寄存器值 × 32 = 字节数（最小 32B，最大 64KB）。

### 写驱动时的典型寄存器配置顺序

```
1. SDMMC_POWER: PWRCTRL=11 → 上电，等 74 CK
2. SDMMC_CLKCR: CLKDIV=大值(≤400kHz), WIDBUS=00(1-bit)
3. 发初始化命令序列 (CMD0/CMD8/ACMD41/CMD2/CMD3/CMD7)
4. SDMMC_CLKCR: CLKDIV=小值(提速), WIDBUS=01(4-bit), HWFC_EN=1
5. 读写 block:
   a. SDMMC_DTIMER = 超时值
   b. SDMMC_DLENR = 512 (或 N×512)
   c. SDMMC_DCTRL: DBLOCKSIZE=9, DTMODE=11, DTDIR=1(读)/0(写)
   d. SDMMC_IDMABASER = buffer 地址
   e. SDMMC_IDMACTRLR: IDMAEN=1
   f. SDMMC_CMDR: CMD17/18/24/25 + CMDTRANS=1 + CPSMEN=1
   g. 等 DATAEND (成功) 或 错误标志
```
