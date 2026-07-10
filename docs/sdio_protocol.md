# SDIO 协议笔记 (SDIO Simplified Specification V3.00)

参考: SD Association 《Part E1: SDIO Simplified Specification Version 3.00》

## 概述

SDIO (Secure Digital Input/Output) 在 SD 物理总线上扩展了 I/O 功能。卡类型:

| 类型 | 说明 |
|------|------|
| **IO-only** | 仅有 I/O 功能, 无存储 (如 AP6212 WiFi) |
| **Combo (存储+I/O)** | 同时支持 SD 存储和 I/O 功能 |

SDIO 与 SD 存储共享物理层 (电压、时钟、总线宽度), 但命令集和寄存器不同。

---

## 第 3 章: 卡识别 (初始化)

### 3.1 主机类型

两种主机初始化方式:

| 主机类型 | 特点 |
|---------|------|
| **Non-I/O aware host** | 仅支持 SD 存储卡, 将 SDIO 卡当 SD 存储卡初始化, 会卡在 CMD8/ACMD41, 卡在此时进入非初始化模式保持兼容 |
| **I/O aware host** | 使用 CMD5 (IO_SEND_OP_COND) 检测和初始化 SDIO 卡 |

### 3.2 卡状态

SDIO 卡的状态与 SD 存储类似, 但以 CMD5 替代 CMD1/ACMD41:

```
Idle → CMD5 → Ready → CMD3 → Stand-by → CMD7 → Transfer
```

IO-only 卡跳过 CMD2 (ALL_SEND_CID), 无 CID/CSD 寄存器。

### 3.3 CMD5 — IO_SEND_OP_COND

48-bit 命令, 用于检测 SDIO 卡及其操作条件。

#### 命令格式

| 位 | 47 | 46 | [45:40] | [39:8] | [7:1] | 0 |
|----|----|----|---------|--------|-------|---|
| 内容 | Start=0 | Direction=1 | CMD5=000101 | Argument (32-bit) | CRC7 | End=1 |

#### 参数 (Argument)

| 位 | 字段 | 说明 |
|----|------|------|
| [31:28] | S18R | 1.8V 切换请求 (支持 1.8V 时设 1) |
| [27] | Reserved | 0 |
| [26:24] | Number of I/O Functions (写 0) | 初始化时写 0 |
| [23:0] | I/O OCR | I/O 电压范围 |

#### 响应 R4 (48-bit)

CMD5 的响应为 **R4**, 格式:

| 位 | 47 | 46 | [45:40] | [39:8] | [7:1] | 0 |
|----|----|----|---------|--------|-------|---|
| 内容 | Start=0 | Direction=0 | CMD5=000101 | R4 Data (32-bit) + CRC7 | CRC7 | End=1 |

#### R4 Data 字段 (Argument Field)

| 位 | 字段 | 说明 |
|----|------|------|
| [31] | C (Card Ready) | 0=卡忙, 1=卡就绪 |
| [30:28] | Number of I/O Functions (NF) | 支持的 I/O 功能数 (0~7) |
| [27] | Reserved | 0 |
| [26] | MP (Memory Present) | 0=IO-only, 1=Combo (有存储空间) |
| [25] | S18A (1.8V Switch Acknowledge) | 1=支持 1.8V 切换 |
| [24:0] | I/O OCR | 卡支持的 I/O 电压范围 |

#### 电压窗口

标准 SDIO 卡使用与 SD 卡相同的电压窗口。OCR 编码:

| Bit | 电压 | 说明 |
|-----|------|------|
| 23 | 2.7-2.8V | — |
| 22 | 2.8-2.9V | — |
| 21 | 2.9-3.0V | — |
| 20 | 3.0-3.1V | — |
| 19 | 3.1-3.2V | — |
| 18 | 3.2-3.3V | — |
| 17 | 3.3-3.4V | — |
| 16 | 3.4-3.5V | — |
| 15 | 3.5-3.6V | — |

### 3.4 初始化流程

#### IO-only 卡

```
CMD0          → 复位到 Idle State
CMD5(arg=0)   → 查询卡是否 SDIO, R4 返回 NF + MP + OCR
CMD5(arg=OCR) → 设工作电压, 轮询直到 C=1
CMD3          → 分配 RCA (RCA 由主机分配, 卡返回新 RCA)
CMD7(RCA)     → 选中卡, 进入 Transfer State
CMD52         → 读 CCCR 确认功能, 配置总线宽度等
```

#### Combo 卡 (存储 + I/O)

Combo 卡有独立的存储和 I/O 子系统。初始化需要对两个子系统分别进行:

```
CMD0           → 复位

— I/O 初始化 —
CMD5(arg=0)    → 发现 SDIO 功能, R4 中 MP=1
CMD5(arg=OCR)  → 初始化 I/O 部分, 等 C=1

— 存储初始化 —
CMD8           → 检查 SDHC/SDXC 支持
ACMD41         → 初始化存储部分 (需先 CMD55)

— 公共识别 —
CMD2           → 读 CID (存储部分的 CID)
CMD3           → 分配 RCA (I/O 和存储共用同一 RCA)
CMD7(RCA)      → 选中卡
```

| 注意 | 说明 |
|------|------|
| Combo 卡有**两个** OCR | I/O OCR (CMD5) 和 Memory OCR (ACMD41) |
| 存储和 I/O 的电压范围可能不同 | 主机需分别协商 |
| 共用同一 RCA | CMD3 只分配一次, 后续 CMD7 选中后同时访问 I/O 和存储 |
| 不支持组合的 CMD52/53 | CMD52/53 仅用于 I/O 功能, 存储访问用 SD 标准命令 |

#### 流程图

```
                    ┌───────────────┐
                    │   Idle State  │
                    └───────┬───────┘
                            │ CMD0
                            ▼
                    ┌───────────────┐
                    │  Card Reset   │
                    └───────┬───────┘
                            │ CMD5(arg=0)
                            ▼
                    ┌───────────────┐
                    │  CMD5 响应 ?  │─── No ──→ SD 存储初始化流程
                    └───────┬───────┘
                            │ Yes
                            ▼
                    ┌─────────────────┐
                    │ CMD5(arg=OCR)   │─── 轮询直到 C=1
                    │ 等 Card Ready   │
                    └───────┬─────────┘
                            │
              ┌─────────────┼─────────────┐
              │ IO-only     │ Combo       │
              ▼             ▼             ▼
       ┌──────────┐  ┌──────────┐  ┌──────────┐
       │ Skip CMD2│  │  CMD8    │  │  CMD2    │
       │ No CID   │  │ ACMD41   │  │ (CID)    │
       └────┬─────┘  └────┬─────┘  └────┬─────┘
            │             │             │
            └─────────────┼─────────────┘
                          │ CMD3 (RCA)
                          ▼
                    ┌───────────────┐
                    │  Stand-by     │
                    └───────┬───────┘
                            │ CMD7(RCA)
                            ▼
                    ┌───────────────┐
                    │  Transfer     │
                    │  (I/O Ready)  │
                    └───────┬───────┘
                            │ CMD52 读 CCCR
                            ▼
                    ┌───────────────┐
                    │  Configure    │
                    │  Bus Width    │
                    │  Speed Mode   │
                    └───────────────┘
```

### 3.5 初始化时序参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 识别时钟 f_OD | ≤400kHz | 初始化期间时钟频率 |
| CMD5 超时 | 5ms (典型) | 充满电的卡, 从第 2 个 CMD5 到 C=1 |
| CMD5 超时 (新卡) | 1s (最坏) | 未充电的新卡可能需要更长时间 |
| N_CR | 0~64 时钟 | CMD5 到 R4 响应延迟 |

### 3.6 IO-only 卡与 Combo 卡的关键区别

| 特性 | IO-only | Combo |
|------|---------|-------|
| CMD5 MP bit | 0 | 1 |
| CMD2 (ALL_SEND_CID) | 不支持 | 支持 (读存储部分 CID) |
| CID/CSD 寄存器 | 无 | 有 (存储部分) |
| 电压协商 | 仅 CMD5 | CMD5 + ACMD41 分别协商 |
| 功能 | 纯 I/O 外设 | 存储 + I/O 外设 |

### 3.7 CCCR 寄存器 (初始化后配置用)

初始化完成后, 通过 CMD52 访问 CCCR (Card Common Control Register, Function 0) 确认卡能力和配置总线。

| CCCR 地址 | 寄存器 | 说明 |
|-----------|--------|------|
| 0x00 | CCCR_SDIO_REV | SDIO 规范版本 |
| 0x01 | SD_REV | SD 物理层版本 |
| 0x02 | IO_ENABLE | I/O 功能使能 (写 1 启用对应功能) |
| 0x03 | IO_READY | I/O 功能就绪状态 |
| 0x04 | INT_ENABLE | 中断使能 |
| 0x05~0x06 | IO_BUS_CTL | 总线宽度控制: [1:0]=00(1-bit)/01(reserved)/10(4-bit) |
| 0x07~0x08 | CARD_CAPS | 卡能力 (1.8V, 4-bit, HS 支持等) |
| 0x09~0x0A | CIS_PTR | CIS 指针 (指向卡信息结构) |
| 0x0F | BUS_SUSPEND | 总线挂起/恢复 |

---

## 第 4 章: 与 SD 存储规范的差异

### 4.1 命令列表概览

SDIO 卡接受的命令集与 SD 存储卡不同。完整列表见规范附录 Table C-1 (SD 模式) 和 Table C-2 (SPI 模式)。

### 4.2 支持/不支持的 SD 存储命令

#### 4.2.1 支持的 SD 存储命令

| 命令 | SDIO | 说明 |
|------|------|------|
| CMD0 | CMD0 | 首次 CMD0 用于选择 SD 或 SPI 模式。未收到 CMD0 时 SDIO 卡默认 SD 模式 |
| CMD11 | CMD11 | 电压切换序列, 进入 UHS-I 模式 |
| CMD19 | CMD19 | Tuning, 仅在 1.8V 信号电平有效, 否则视为非法命令 |

CMD11 和 CMD19 功能参考 Physical Layer Specification V3.0x。

#### 4.2.2 不支持的 SD 存储命令

| SD 存储命令 | SDIO 替代 | 说明 |
|------------|----------|------|
| CMD0 (复位) | CMD52 写 CCCR RES bit | CMD0 仅复位存储部分, I/O 复位需写 CCCR 寄存器 6 bit3=1 |
| CMD2 | 无 | IO-only 卡无 CID 寄存器 |
| CMD4 | 无 | IO-only 卡无 DSR 寄存器 |
| CMD9 | 无 | IO-only 卡无 CSD 寄存器 |
| CMD10 | 无 | IO-only 卡无 CID 寄存器 |
| CMD12 (中止) | CMD52 写 CCCR abort | 中止 I/O 传输通过写 CCCR 寄存器 6 的 ASx bit |
| CMD13 | 无 | IO-only 不支持 SEND_STATUS 协议, 状态通过响应返回 |
| CMD16 (设块长) | CMD52 写 FBR | 每个 I/O function 的块长通过 CMD52 写 FBR 设置 |
| ACMD6 (总线宽度) | CMD52 写 CCCR | 总线宽度通过写 CCCR 设置 |
| ACMD13 | 无 | IO-only 无 SD Status 寄存器 |
| ACMD41 | CMD5 | SDIO 使用 CMD5 替代 ACMD41 |
| ACMD42 (DAT3 检测) | CMD52 写 CCCR | 卡检测电阻控制通过 CCCR |
| ACMD51 | 无 | IO-only 无 SCR 寄存器 |
| CMD17/18/24/25 | CMD53 | I/O 数据传输使用 CMD53 替代块读写 |

### 4.3 修改的 R6 响应 (CMD3)

IO-only 卡对 CMD3 的 R6 响应中, 状态位 (bits[23:8]) 定义与 SD 存储不同:

| 位 | 标识符 | 类型 | 值 | 说明 |
|----|--------|------|-----|------|
| 15 | COM_CRC_ERROR | E R | 0=无错, 1=错误 | 前一命令 CRC 失败 |
| 14 | ILLEGAL_COMMAND | E R | 0=无错, 1=错误 | 前一命令非法 |
| 13 | ERROR | E R X | 0=无错, 1=错误 | 操作期间发生未知错误 |
| 12:0 | Undefined | — | 0 | IO-only 读为 0, 主机忽略 |

### 4.4 SDIO 复位

| 方式 | 作用范围 | 方法 |
|------|---------|------|
| CMD0 | 仅复位存储部分 (或 Combo 卡的存储部分) | IO-only 卡的 I/O 部分不受 CMD0 影响 |
| I/O Reset | 复位 I/O 所有功能 | CMD52 写 CCCR 地址 6 bit3 (RES)=1 |
| Card Reset | 复位整卡 | 硬件复位或断电上电 |

**约束:**
- IO-only 卡或 Combo 卡的 I/O 部分不被 CMD0 复位
- I/O Reset 在 CMD52 响应结束后生效
- SD 模式下 CMD0 仅用于选择 SPI 模式, SDIO 卡必须支持

### 4.5 总线宽度

| 卡类型 | 设置方法 |
|--------|---------|
| SD 存储 | ACMD6 |
| SDIO (IO-only) | CMD52 写 CCCR |
| Combo (仅 I/O 已初始化) | 仅 CCCR |
| Combo (仅存储已初始化) | 仅 ACMD6 |
| Combo (I/O 和存储都已初始化) | CCCR + ACMD6 都要设, 且必须一致 |

**约束:**
- Low-Speed SDIO 卡可选支持 4-bit, 主机需先确认卡能力再切
- Combo 卡 I/O 和存储的总线宽度必须一致
- UHS-I 仅支持 4-bit 模式 (CMD42 除外, 解锁需在 1-bit 下发数据块)
- 支持 lock/unlock 的 Combo 卡在锁定状态下不能切总线宽度, 需先用 CMD42 解锁

### 4.6 卡检测电阻 (DAT3 上拉)

| 卡类型 | 控制方法 |
|--------|---------|
| SD 存储 | ACMD42 |
| SDIO | CMD52 写 CCCR |
| Combo | 两者都要管理 |

Combo 卡电阻状态 = I/O 使能 AND 存储使能:

| I/O 电阻 | 存储电阻 | 卡电阻 |
|----------|---------|--------|
| Enabled | Enabled | 连接 |
| Enabled | Disabled | 断开 |
| Disabled | Enabled | 断开 |
| Disabled | Disabled | 断开 |

**约束:**
- 上电后默认两者均为 Enabled (电阻连接)
- I/O Reset 不影响 I/O 电阻使能状态
- 旧版 (SDIO 1.0) Combo 卡使用 OR 逻辑, 建议主机对任何 Combo 卡都禁用两者以避免兼容问题

### 4.7 时序

本章节不在简化规范中。

### 4.8 数据传输块大小

| 特性 | SD 存储 | SDIO |
|------|---------|------|
| 传输模式 | 固定块传输 | 多字节 (1~512B) 或可选块模式 |
| 块大小 | 2 的幂 (512/1024/2048) | 1~2048 字节任意值 |
| 限制来源 | Physical Layer Spec | CIS 中每个 function 可定义最大块大小 |

### 4.9 数据传输中止

#### 4.9.1 读中止

- 任何时候发 CMD52 写 CCCR abort 位即可中止 I/O 读
- 数据在 abort 命令结束位后 2 个时钟停止
- 即使卡已经开始传输不需要的数据块, 中止仍然有效

#### 4.9.2 写中止

- 在数据块**之间**发 CMD52 写 CCCR abort
- 当前块传输 (含 CRC 响应) 必须完成
- Abort 命令结束位需在当前块 CRC 响应结束位前最多 2 个时钟出现
- 不可在写块中间发送 abort
- Abort 后卡拉低 DAT0 进入 Busy, 直到完成最后一个数据块处理
- Busy 期间主机可通过写 CCCR BR bit 释放总线

#### Abort 操作总结

| 操作 | 方法 | 说明 |
|------|------|------|
| I/O 中止 | CMD52 写 CCCR 寄存器 6 bit[2:0] | ASx (Abort Select x) |
| 总线释放 | CMD52 写 CCCR BR bit | Bus Release, Busy 期间可用 |

### 4.10 寄存器变化

SD 存储规范的 7 个固定寄存器在 SDIO 中的变化:

| 寄存器 | IO-only | Combo | 访问命令 |
|--------|---------|-------|---------|
| OCR | 有 (24-bit, 通过 CMD5) | 可能有两个 (I/O + 存储) | CMD5 / ACMD41 |
| CID | 无 | 有 (来自存储部分) | CMD10 |
| CSD | 无 | 有 (来自存储部分) | CMD9 |
| RCA | 有 (唯一) | 有 (唯一, 共用) | CMD3 |
| DSR | 无 | 可选 | — |
| SCR | 无 | 有 (来自存储部分) | ACMD51 |
| SD Status | 无 | 有 (来自存储部分) | ACMD13 |

#### 4.10.1 OCR 寄存器

- IO-only 卡有一个 OCR (24-bit, 通过 CMD5 读取)
- Combo 卡可能有两个 OCR: I/O OCR (CMD5) 和 Memory OCR (ACMD41/CMD58)
- 两个 OCR 的电压范围可能不同
- I/O OCR 是所有 I/O function 电压范围的逻辑与
- 每个 I/O function 的电压可在 CIS 中查询

#### 4.10.2 CID 寄存器

- 每张卡最多一个 CID
- Combo 卡: CID 来自存储部分
- IO-only: **无 CID**, CMD10 不响应 (SD 模式无响应, SPI 模式返回非法命令错误)

#### 4.10.3 CSD 寄存器

- 同 CID, IO-only 卡无 CSD, CMD9 不响应

#### 4.10.4 RCA 寄存器

- 每张卡唯一一个 RCA
- 所有 I/O function 和存储共用同一地址

#### 4.10.5 DSR 寄存器

- IO-only 卡不支持
- Combo 卡可选支持 (由 Physical Layer Spec 定义)

#### 4.10.6 SCR 寄存器

- 同 CID, IO-only 卡无 SCR, ACMD51 不响应

#### 4.10.7 SD Status

- 同 CID, IO-only 卡无 SD Status, ACMD13 不响应

#### 4.10.8 卡状态寄存器 (R1 响应)

IO-only 卡的 R1 状态位与 SD 存储不同。不适用位固定返回 0:

| Bit | 标识符 | 类型 | 值 | 说明 |
|-----|--------|------|-----|------|
| 31 | OUT_OF_RANGE | E R X | 0/1 | 命令参数超出范围 |
| 30 | ADDRESS_ERROR | N/A | 0 | SDIO 不使用 |
| 29 | BLOCK_LEN_ERROR | N/A | 0 | SDIO 不使用 |
| 28 | ERASE_SEQ_ERROR | N/A | 0 | SDIO 不使用 |
| 27 | ERASE_PARAM | N/A | 0 | SDIO 不使用 |
| 26 | WP_VIOLATION | N/A | 0 | SDIO 不使用 |
| 25 | CARD_IS_LOCKED | N/A | 0 | SDIO 不使用 |
| 24 | LOCK_UNLOCK_FAILED | N/A | 0 | SDIO 不使用 |
| 23 | COM_CRC_ERROR | E R | 0/1 | 前一命令 CRC 失败 (注 1) |
| 22 | ILLEGAL_COMMAND | E R | 0/1 | 前一命令非法 (注 2) |
| 21 | CARD_ECC_FAILED | N/A | 0 | SDIO 不使用 |
| 20 | CC_ERROR | N/A | 0 | SDIO 不使用 |
| 19 | ERROR | E R X | 0/1 | 操作期间发生未知错误 |
| 18 | UNDERRUN | N/A | 0 | SDIO 不使用 |
| 17 | OVERRUN | N/A | 0 | SDIO 不使用 |
| 16 | CID/CSD_OVERWRITE | N/A | 0 | SDIO 不使用 |
| 15 | WP_ERASE_SKIP | N/A | 0 | SDIO 不使用 |
| 14 | CARD_ECC_DISABLED | N/A | 0 | SDIO 不使用 |
| 13 | ERASE_RESET | N/A | 0 | SDIO 不使用 |
| **12:9** | **CURRENT_STATE** | S X | **0xF** | **IO-only 固定返回 0xF (15), 表示非存储状态** |
| 8 | READY_FOR_DATA | N/A | 0 | SDIO 不使用 |
| 7:6 | Reserved | — | 0 | — |
| 5 | APP_CMD | N/A | 0 | CMD55 不用于 SDIO |
| 4 | Reserved | — | 0 | — |
| 3 | AKE_SEQ_ERROR | N/A | 0 | SDIO 不使用 |
| 2:0 | Reserved | — | 0 | — |

**重要约束:**
- Combo 卡的 CURRENT_STATE (bits 12:9) 反映存储控制器状态, 而非 I/O
- IO-only 卡 CURRENT_STATE 固定 0xF (15), 用于识别为 IO-only 卡
- CRC 错误和非法命令不响应该命令本身, 而是在**下一条命令的响应**中报告 (SD 模式)
- 这一规则同样适用于 Combo 卡, 即使两个命令不指向同一模块 (存储或 I/O)

---

## 第 5 章: I/O 读写命令

### 5.1 CMD52 — IO_RW_DIRECT (单字节读写)

最简单的单字节访问方式, 一条命令 + 一条响应完成读写, 用于初始化寄存器或监控状态。

#### 命令格式 (48-bit)

| 位 | 47 | 46 | [45:40] | [39:28] | [27] | [26] | [25:9] | [8] | [7:0] |
|----|----|----|---------|---------|------|------|--------|-----|-------|
| 内容 | S=0 | D=1 | 110100b | R/W flag | Func# | RAW | Register Address | Stuff | Write Data |

#### 字段说明

| 字段 | 位 | 说明 |
|------|-----|------|
| R/W Flag | 39:28 (1) | 0=读, 1=写 |
| Function Number | 27 (3) | 0=CIA (CCCR), 1~7=I/O 功能 |
| RAW | 26 (1) | 读后写标志: 0=写时返回写入值; 1=写后读回寄存器实际值 |
| Register Address | 25:9 (17) | 寄存器地址, 范围 0~0x1FFFF (128KB/Function) |
| Write Data | 7:0 (8) | 写操作时写入的数据; 读操作设 0 |

**RAW 标志行为:**
- R/W=0 (读): 忽略 RAW, 返回实际读回的值
- R/W=1 (写) + RAW=0: R5 返回的 data = 写入的值 (不回读)
- R/W=1 (写) + RAW=1: 写完成后自动回读该寄存器, R5 返回读回的实际值

### 5.2 R5 响应 (CMD52/CMD53)

#### SD 模式 (48-bit)

| 位 | 47 | 46 | [45:40] | [39:24] | [23:16] | [15:8] | [7:0] |
|----|----|----|---------|---------|---------|--------|-------|
| 内容 | S=0 | D=0 | 110100b | Stuff=0 | Response Flags (8-bit) | Read/Write Data (8-bit) | CRC7+E |

**R5 Flag 位 (Table 5-1):**

| 位 | 标识符 | 类型 | 值 | 说明 |
|----|--------|------|-----|------|
| 7 | COM_CRC_ERROR | E R | 0/1 | 前一命令 CRC 失败 |
| 6 | ILLEGAL_COMMAND | E R | 0/1 | 非法命令 |
| **5:4** | **IO_CURRENT_STATE** | **S** | **00=DIS, 01=CMD, 10=TRN** | **I/O 控制器状态** |
| 3 | ERROR | E R X | 0/1 | 操作期间发生未知错误 |
| 2 | RFU | — | 0 | 保留 |
| 1 | FUNCTION_NUMBER | E R | 0/1 | 请求的 Function 号无效 |
| 0 | OUT_OF_RANGE | E R X | 0/1 | 命令参数超出范围 |

**IO_CURRENT_STATE 编码:**

| 值 | 状态 | 说明 |
|-----|------|------|
| 00 | DIS (Disabled) | 初始化中、Standby、Inactive (卡未选中) |
| 01 | CMD (Command) | DAT 线空闲: 等待命令 / CMD53 挂起 / 执行 CMD52 |
| 10 | TRN (Transfer) | 正在 DAT 线上传输数据 (CMD53) |
| 11 | RFU | 保留 |

#### SPI 模式 (16-bit)

SPI 模式下 R5 为 16-bit, 低 8 位 = 读写数据, 高 8 位为 SPI R1 状态 (包含功能号错误、参数错误等)。

### 5.3 CMD53 — IO_RW_EXTENDED (多字节/块传输)

批量数据传输, 支持 byte 模式和 block 模式, 提供最高传输速率。

#### 命令格式 (48-bit)

| 位 | 47 | 46 | [45:40] | [39:28] | [27] | [26] | [25] | [24:9] | [8:0] |
|----|----|----|---------|---------|------|------|------|--------|-------|
| 内容 | S=0 | D=1 | 110101b | R/W flag | Func# | BlockMode | OP Code | Register Address | Count |

#### 字段说明

| 字段 | 位 | 说明 |
|------|-----|------|
| R/W Flag | 1 | 0=读 (卡→主机), 1=写 (主机→卡) |
| Function Number | 3 | 0=CIA, 1~7=I/O 功能 |
| Block Mode | 1 | 0=Byte mode, 1=Block mode |
| OP Code | 1 | 0=固定地址, 1=递增地址 |
| Register Address | 17 | 起始地址, 范围 0~0x1FFFF |
| Byte/Block Count | 9 | 见下方计数表 |

**OP Code:**

| OP Code | 操作 | 用途 |
|---------|------|------|
| 0 | 固定地址多字节读写 | FIFO 寄存器访问, 所有数据在同一地址传输 |
| 1 | 递增地址多字节读写 | RAM 式数据缓冲区, 地址自动递增 |

**Byte/Block Count 编码:**

| Count 值 | Byte 模式 (传输字节数) | Block 模式 (传输块数) |
|----------|----------------------|----------------------|
| 0x000 | 512 字节 | 无限块 (需 abort 停止) |
| 0x001 | 1 字节 | 1 块 |
| 0x002 | 2 字节 | 2 块 |
| ... | ... | ... |
| 0x1FF | 511 字节 | 511 块 |

#### 5.3.1 数据传输格式

| 模式 | 类似 SD 命令 | 说明 |
|------|-------------|------|
| Byte 读 | CMD17 (READ_SINGLE_BLOCK) | 传输 1~512 字节 |
| Byte 写 | CMD24 (WRITE_BLOCK) | 同上 |
| Block 读 (定长) | CMD18 (READ_MULTIPLE_BLOCK) | 传完指定块数自动停止 |
| Block 读 (无限) | CMD18 + CMD12 | Block Count=0, 需 abort 停止 |
| Block 写 (定长) | CMD25 (WRITE_MULTIPLE_BLOCK) | 传完指定块数自动停止 |
| Block 写 (无限) | CMD25 + CMD12 | Block Count=0, 需 abort 停止 |

**约束:**
- CMD52/CMD53 的响应同为 R5, 数据字段 (bytes[7:0]): CMD53 时为 stuffing bits, 读为 0x00
- Block mode 需要卡支持 (读 CCCR SMB bit 确认)
- 每 Function 的块大小通过 FBR 设置 (Function 0 的块大小在 CCCR 中), 最大块大小可在 CIS tuple TPLFE_MAX_BLK_SIZE 中查到
- Byte 模式下最大传输 512 字节; Block 模式下 Count=0 表示无限传输, 需 CMD52 写 CCCR abort 停止

---

## 第 6 章: SDIO 卡内部操作

### 6.1 概述

- 每张 SDIO 卡可有 1~7 个 I/O Function + 可选存储 (Combo)
- 每个 Function 最多 128KB (2^17) 寄存器空间, 按字节寻址
- 支持三种访问方式: 单字节、固定地址多字节 (FIFO)、递增地址多字节 (RAM)
- 每个 Function 是独立的 I/O 设备, 可相同或完全不同

### 6.2 寄存器访问时间

- 所有寄存器读写必须在 **1 秒内** 完成
- 卡通过拉低 DAT0 (写忙) 或延迟数据起始位 (读) 通知主机等待
- 超过 1 秒不响应, 主机视为超时

### 6.3 中断

- 每个 Function 有独立的中断使能位 (IENx) 和中断挂起位 (INTx)
- 卡级总中断使能 (IENM), 两者都设才能向主机发中断
- 电平触发: 中断源不清除, 中断信号不释放
- Asynchronous Interrupt (SDIO 3.00): SD 4-bit 模式下无需时钟即可发中断

### 6.4 挂起/恢复 (Suspend/Resume)

- 多 Function 或 Combo 卡上, 主机可暂停一个传输以让高优先级传输执行
- 可选功能, Function 0 不支持挂起
- 支持挂起/恢复的卡必须同时支持 Read Wait 和 Direct Commands (CMD52 在传输中执行)

### 6.5 Read Wait

- 多块读期间, 主机可通过 DAT[2] 信号让卡暂停发数据
- 暂停期间主机可发 CMD52 (否则停下来时钟就不能发命令)
- 仅 SD 1-bit/4-bit 模式有效

### 6.6 数据传输期间的 CMD52

- 若卡支持 Direct Commands (CCCR 08h SDC bit), 可在 CMD53 传输中间发 CMD52
- 无论 SDC bit 为何值, 传输出错时卡必须接受 CMD52 (用于 abort 和 reset)

### 6.7 SDIO 内部地址映射

```
0x000000-0x0000FF  CIA (Function 0) — CCCR
0x000100-0x0001FF  CIA — FBR (Function 1)
0x000200-0x0002FF  CIA — FBR (Function 2)
        ...
0x000700-0x0007FF  CIA — FBR (Function 7)
0x000800-0x000FFF  CIS Area
0x001000-0x017FFF  RFU
0x018000-0x01FFFF  CIS Pointers
0x020000-0x1FFFFF  Function 1~7 独立 128KB 寄存器空间
```

### 6.8 Common I/O Area (CIA)

所有 SDIO 卡必须实现 CIA, 通过 Function 0 (CMD52/53 中 Func#=0) 访问。包含三类寄存器:

1. **CCCR** — 卡通用控制寄存器
2. **FBR** — 各 Function 基本信息寄存器
3. **CIS** — 卡信息结构

### 6.9 CCCR 寄存器 (Card Common Control Registers)

#### 寄存器布局 (Table 6-1)

| 地址 | 名称 | 位 7 | 位 6 | 位 5 | 位 4 | 位 3 | 位 2 | 位 1 | 位 0 |
|------|------|------|------|------|------|------|------|------|------|
| 00h | CCCR/SDIO Rev | SDIO ver[3:0] | CCCR/FBR ver[3:0] |
| 01h | SD Spec Rev | RFU | SD 物理层版本[3:0] |
| 02h | I/O Enable | IOE7 | IOE6 | IOE5 | IOE4 | IOE3 | IOE2 | IOE1 | RFU |
| 03h | I/O Ready | IOR7 | IOR6 | IOR5 | IOR4 | IOR3 | IOR2 | IOR1 | RFU |
| 04h | Int Enable | IEN7 | IEN6 | IEN5 | IEN4 | IEN3 | IEN2 | IEN1 | IENM |
| 05h | Int Pending | INT7 | INT6 | INT5 | INT4 | INT3 | INT2 | INT1 | RFU |
| 06h | I/O Abort | RFU | RFU | RFU | RFU | **RES** | **AS2** | **AS1** | **AS0** |
| 07h | Bus Interface Ctrl | CD Disable | SCSI | ECSI | RFU | RFU | S8B | **Bus Width1** | **Bus Width0** |
| 08h | Card Capability | 4BLS | LSC | E4MI | S4MI | SBS | SRW | SMB | SDC |
| 09h-0Bh | Common CIS Pointer | CIS 地址指针 (24-bit, little-endian) |
| 0Ch | Bus Suspend | RFU | RFU | RFU | RFU | RFU | RFU | **BR** | **BS** |
| 0Dh | Function Select | **DF** | RFU | RFU | RFU | **FS3** | **FS2** | **FS1** | **FS0** |
| 0Eh | Exec Flags | EX7 | EX6 | EX5 | EX4 | EX3 | EX2 | EX1 | EX0 |
| 0Fh | Ready Flags | RF7 | RF6 | RF5 | RF4 | RF3 | RF2 | RF1 | RF0 |
| 10h-11h | FN0 Block Size | Function 0 块大小 (16-bit, little-endian) |
| 12h | Power Control | RFU | RFU | RFU | RFU | RFU | RFU | EMPC | SMPC |
| 13h | Bus Speed Select | RFU | RFU | RFU | RFU | **BSS2** | **BSS1** | **BSS0/SHS** | **EHS** |
| 14h | UHS-I Support | RFU | RFU | RFU | RFU | RFU | SDDR50 | SSDR104 | SSDR50 |
| 15h | Driver Strength | RFU | RFU | DTS1 | DTS0 | RFU | SDTD | SDTC | SDTA |
| 16h | Interrupt Ext | RFU | RFU | RFU | RFU | RFU | RFU | EAI | SAI |
| 17h-EFh | RFU | 保留 |
| F0h-FFh | Vendor | 厂商自定义 |

#### 关键寄存器详细说明 (Table 6-2)

**00h — CCCR/SDIO Revision (R/O)**

| 值 | CCCR/FBR 版本 | SDIO 版本 |
|-----|---------------|-----------|
| 00h | SDIO 1.00 | — |
| 01h | SDIO 1.10 | SDIO 1.10 |
| 02h | SDIO 2.00 | SDIO 1.20 (未发布) |
| 03h | SDIO 3.00 | SDIO 2.00 |
| 04h | — | SDIO 3.00 |

**01h — SD Spec Revision (R/O)** — SD 物理层规范版本。

**02h — I/O Enable (R/W)**
- IOEx: 置 1 使能 Function x, 清 0 禁用。上电复位后全 0
- 可用作逐 Function 复位: 清 IOEx → 等 IORx=0 → 再设 IOEx=1

**03h — I/O Ready (R/O)**
- IORx=1 表示 Function x 已就绪
- 不存在的 Function 该位恒 0

**04h — Interrupt Enable (R/W)**
- IENx: Function x 中断使能
- IENM (bit 0): 主中断使能, 清 0 时卡不发任何中断

**06h — I/O Abort (W/O)**
- AS[2:0]: 选择要中止的 Function 号 (如 3=中止 Function 3 的传输)
- RES (bit 3): I/O 卡软复位, 写 1 复位所有 I/O Function
  - 自动清除, 不需回写 0
  - 不影响 SD/SPI 模式选择和 CD Disable
  - 不影响 Combo 卡的存储部分
  - 只写位, 读返回不确定值

**07h — Bus Interface Control**
- Bus Width[1:0] (R/W): 00=1-bit, 01=保留, 10=4-bit, 11=8-bit(仅 embedded)
  - 上电默认 1-bit, 所有 Full-Speed 卡必须支持 1-bit 和 4-bit
- CD Disable (R/W): DAT[3] 上拉检测电阻, 0=连接, 1=断开
  - 上电默认 0 (连接), reset 不影响此位
  - **发 CMD53 前必须先设 1**
- SCSI (R/O): 支持 SPI 连续中断
- ECSI (R/W): 使能 SPI 连续中断
- S8B (R/O): 支持 8-bit 模式 (仅 embedded)

**08h — Card Capability**

| 位 | 名称 | 类型 | 说明 |
|----|------|------|------|
| 0 | SDC | R/O | 支持 CMD52 在数据传输期间执行 |
| 1 | SMB | R/O | 支持 CMD53 Block 模式 |
| 2 | SRW | R/O | 支持 Read Wait |
| 3 | SBS | R/O | 支持 Suspend/Resume |
| 4 | S4MI | R/O | 支持 4-bit 块间隙中断 |
| 5 | E4MI | R/W | 使能 4-bit 块间隙中断, reset 后清 0 |
| 6 | LSC | R/O | =1 表示 Low-Speed 卡 |
| 7 | 4BLS | R/O | Low-Speed 卡支持 4-bit |

**约束:**
- 支持 Suspend/Resume 的卡必须同时支持 SDC 和 SRW
- 传输错误时无论 SDC=0, 卡也必须接受 CMD52 (用于 abort/reset)
- 速率 >25MB/s (SDR50/SDR104/DDR50) 时 block gap interrupt 不能用
- **发 CMD53 前必须将 CD Disable 设 1**

**0Ch — Bus Suspend**
- BS (R/O): Bus Status, 1=当前 Function 正在 DAT 上传输
- BR (R/W): Bus Release Request, 请求暂停当前传输

**0Dh — Function Select**
- FS[3:0] (R/W): 选择 Function (0~7) 或 Combo 存储 (8) 用于挂起/恢复
- DF (R/O): Data Flag, 恢复后是否还有数据要传

**0Eh — Exec Flags (R/O)**: EXx=1 表示 Function x 正在执行命令

**0Fh — Ready Flags (R/O)**: RFx=1 表示可接收写数据或读数据已就绪

**10h-11h — FN0 Block Size (R/W)**: Function 0 块大小, 范围 1~2048, 默认 0

**13h — Bus Speed Select**

3.3V 信号时 BSS[0] 有效:

| BSS[0] | 模式 | 最大频率 |
|--------|------|---------|
| 0 | Default Speed | 25MHz |
| 1 | High Speed | 50MHz |

1.8V 信号 (UHS-I) 时 BSS[2:0] 有效:

| BSS[2:0] | 模式 | 最大频率 |
|----------|------|---------|
| 000 | SDR12 | 25MHz |
| 001 | SDR25 | 50MHz |
| 010 | SDR50 | 100MHz |
| 011 | SDR104 | 208MHz |
| 100 | DDR50 | 50MHz |

**14h — UHS-I Support (R/O)**: SSDR50 / SSDR104 / SDDR50

**15h — Driver Strength**: SDTA/SDTC/SDTD (R/O) 支持驱动类型; DTS[1:0] (R/W) 选择

### 6.10 FBR (Function Basic Registers)

每个 Function 有一个 256 字节的 FBR 区域, 地址 `00n00h~00nFFh` (n=Function 号 1~7)。

#### FBR 布局 (Table 6-3)

| 地址 | 位 7 | 位 6 | 位 5:4 | 位 3:0 |
|------|------|------|--------|--------|
| n00h | CSA Enable | CSA Support | RFU | **Standard Function Interface Code** |
| n01h | **Extended Standard Function Interface Code** |
| n02h | RFU | RFU | PS3 | PS2 | PS1 | PS0 | EPS | SPS |
| n03h-08h | RFU |
| n09h-0Bh | **CIS 指针** (24-bit, little-endian) |
| n0Ch-0Eh | **CSA 指针** (24-bit, little-endian) |
| n0Fh | **CSA 数据窗口** (8-bit) |
| n10h-11h | **I/O Block Size** (16-bit, little-endian) |
| n12h-1FFh | RFU |

#### FBR 字段说明 (Table 6-4)

| 字段 | 类型 | 说明 |
|------|------|------|
| Standard Function Interface Code (n00h[3:0]) | R/O | 标识 Function 的标准接口类型 |
| | | 0h=无标准接口 |
| | | 1h=UART, 2h=Bluetooth A, 3h=Bluetooth B |
| | | 4h=GPS, 5h=Camera, 6h=PHS |
| | | **7h=WLAN (WiFi, AP6212 应为此值)** |
| | | 8h=ATA, 9h=Bluetooth AMP |
| | | Fh=编码超过 Eh, 查 n01h |
| CSA Support (n00h[6]) | R/O | =1 支持 CSA (Code Storage Area) |
| CSA Enable (n00h[7]) | R/W | =1 使能 CSA 访问, reset 后清 0 |
| SPS (n02h[0]) | R/O | 支持 Power Selection |
| EPS (n02h[1]) | R/W | 电源模式: 0=高电流, 1=低电流 (EMPC=1 时有效) |
| PS[3:0] (n02h[5:2]) | R/W | 电源状态控制 (SDIO 3.00) |
| CIS 指针 (n09h-0Bh) | R/O | 指向该 Function 的 CIS (24-bit, 低 17 位有效) |
| CSA 指针 (n0Ch-0Eh) | R/W | CSA 地址指针, 读写后自动递增 |
| CSA 数据窗口 (n0Fh) | R/W | CSA 数据读写窗口 |
| I/O Block Size (n10h-11h) | R/W | 块大小 1~2048, 默认 0。设 0 时 CMD53 Block 模式返回 OUT_OF_RANGE |

### 6.11 CIS (Card Information Structure)

- 所有 SDIO 卡必须有一个通用 CIS + 每个 Function 的 CIS
- CIS 位于 CIA 地址 `001000h~017FFFh`
- 基于 PCMCIA Tuple 结构, 具体定义见第 16 章
- 通过 CCCR 的 Common CIS Pointer (09h-0Bh) 和 FBR 的各 Function CIS 指针访问

### 6.12 多 Function 卡

- Function 从 1 开始编号, CMD5 返回的 NF 包含所有 Function (含 "dummy")
- R5 ERROR 状态位只在下一个发给**同一个 Function** 的命令响应中返回

### 6.13 CMD53 块大小设置

- 块大小写入 FBR 的 `n10h-11h` (I/O Block Size), 用 CMD52 写
- **不允许用 CMD53 Block 模式写块大小寄存器**
- 非法块大小 → CMD53 Block 模式响应 OUT_OF_RANGE, 不传输数据

### 6.14 总线状态图 (Figure 6-2)

```
上电 → [INI] Initialization State
        CMD5 / CMD3 / CMD15 有效
        │ CMD3 (RCA 分配)
        ▼
   [STB] Standby State
        CMD3 / CMD7 / CMD15 有效
        │ CMD7 (选中, RCA 匹配)
        ▼
   [TRN] Transfer State
        CMD52 有效
        │ CMD53 (有效 Function)
        ▼
   [CMD] Command State (DAT Bus Active, BS=1)
        CMD7 / CMD52 / CMD53 / CMD15 有效
        │ 执行完成 / Function Suspend / CMD52 Abort
        ▼
   [TRN] Transfer State (DAT Bus Free, BS=0)
        │ CMD53 (无效 Function → 残留 CMD)
        │ Function Resume → 回 CMD
        └── CMD7 (取消选择) → STB
            CMD15 / OCR 不匹配 → Inactive
```

**关键状态说明:**

| 状态 | BS bit | 说明 |
|------|--------|------|
| INI | — | 初始化中, 只接受 CMD5/CMD3/CMD15 |
| STB | — | Standby, 等待 CMD7 选中 |
| TRN | 0 | Transfer, DAT 空闲, 可发命令 |
| CMD | 1 | 正在 DAT 上传输数据 |
| Inactive | — | 进入 Inactive 后只能断电恢复 |

**约束:**
- CMD53 发给无效 Function (未使能/未就绪/已挂起) → 卡留在 CMD State 不传输, R5 返回 FUNCTION_NUMBER 错误
- CMD7 取消选择 → 回 Standby
- CMD15 → 进 Inactive

---

## 第 7 章: CSA (Code Storage Area)

可选功能, 每个 Function 可有独立的 16MB 代码存储区, 用于存放驱动/固件。

### 访问流程

1. 读 FBR `n00h bit6` (CSA Support), =1 表示支持
2. 写 FBR `n00h bit7` (CSA Enable) =1 使能
3. 写 FBR `n0Ch~n0Eh` 设 24-bit CSA 地址指针
4. 读写 FBR `n0Fh` (CSA 数据窗口) — 每读写一次指针自动递增

### 数据格式

CSA 内部使用 FAT12/FAT16 文件系统 (ISO/IEC9293:1994)。

**约束:**
- AP6212 这类 WiFi 芯片一般不用 CSA, 固件直接通过 CMD53 写到 Function 的 RAM
- 仅 embedded SDIO 设备可能使用

---

## 第 8 章: 中断

SDIO 中断复用 DAT[1] 线 (4-bit 模式) 或独占 (1-bit/SPI)。电平触发, 低电平有效。

### 中断模式

| 模式 | 中断信号 | 说明 |
|------|---------|------|
| SD 1-bit | DAT[1] 专用于中断 | 随时可发, 无时序约束 |
| SPI | pin 8 | CS=1 时可禁止中断 (除非 SCSI+ECSI 使能) |
| SD 4-bit | DAT[1]/IRQ 复用 | 只在 **Interrupt Period** 内采样中断 |

### 中断控制

| CCCR 寄存器 | 作用 |
|------------|------|
| IENx (04h bits 7:1) | Function x 中断使能 |
| IENM (04h bit 0) | 主中断使能, 清 0 时所有中断屏蔽 |
| INTx (05h bits 7:1) | Function x 中断挂起状态 (R/O) |

### 中断清除

- 电平触发 → 中断源不消除, 信号不释放
- 主机通过 I/O 读写 Function 特定寄存器清除中断源
- 清除后卡必须在规定时间内释放 DAT[1]

**约束:**
- 中断时序细节 (Interrupt Period、块间隙中断、异步中断等) 在简化规范中已省略
- UHS-I 模式 (>25MB/s) 中断作为异步信号处理

---

## 第 9 章: Suspend/Resume (挂起/恢复)

多 Function 卡上暂停低优先级传输, 执行高优先级, 再恢复。

### 流程

1. 主机查 `0Eh EXx` 确定哪个 Function 在用 DAT 线
2. 主机写 `0Ch BR=1` 请求暂停
3. 等 `0Ch BS=0` (DAT 空闲)
4. 执行高优先级传输
5. 写 `0Dh FSx` 选择要恢复的 Function
6. 恢复传输

**约束:**
- Function 0 不支持挂起
- 需要卡支持 (CCCR 08h SBS=1)
- 简化规范仅保留流程概述, 时序细节省略

---

## 第 10 章: Read Wait (读暂停)

多块读期间暂停数据流, 以便发 CMD52 做其他操作。

### 两种方法

| 方法 | 做法 | 缺点 |
|------|------|------|
| 停 SDCLK | 停止 SDIO 时钟 | 时钟停后不能发任何命令 |
| Read Wait | 拉高 DAT[2] 信号 (4-bit) | 可在暂停期间发 CMD52 |

**约束:**
- Read Wait 仅在 SD 1-bit/4-bit 模式有效
- 需要卡支持 (CCCR 08h SRW=1)
- 不支持 Read Wait 的卡只能用停时钟方式
- 支持 Suspend/Resume 的卡必须同时支持 Read Wait

---

## 第 11 章: 电源控制

### 11.1 功耗等级

| 功耗等级 | 最大电流 | 适用场景 |
|---------|---------|---------|
| 标准 (720mW 以下) | ≤200mA @ 3.6V | SDIO 1.00 卡 |
| 高功率 (EMPC=1) | 超过 200mA | 需主机支持, 通过 CCCR SMPC/EMPC 控制 |

### 11.2 电源选择

**Power Selection** (EPS, FBR n02h):
- EPS=0: 高电流模式 (全性能)
- EPS=1: 低电流模式 (降性能)
- 仅 2.7V-3.6V 范围有效

**Power State Control** (PS[3:0], FBR n02h):
- 最多 15 个电源状态, 通过 CIS 的 Power State Tuple 描述
- 各状态定义功耗值 (mW), 主机选可承受的状态
- 支持多电压设备

### 11.3 主电源控制

| CCCR 位 | 作用 |
|---------|------|
| SMPC (12h bit 0) | =1 表示卡支持主电源控制 |
| EMPC (12h bit 1) | =1 使能高功率模式 (超过 720mW) |

EMPC=0: 卡总功耗 ≤720mW (3.6V × 200mA)
EMPC=1: 可超过 720mW, 需配合 EPS/PS 管理各 Function 功耗

### 11.4 温度限制 (Tc)

- 高功耗卡 (>720mW) 的 Thermal Spec 通过 Tc (外壳温度) 管理
- 主机需根据自身散热能力选择可承受的功耗状态
- Embedded SDIO 更易散热, 电流限制也更灵活

### 与 AP6212 的关系

AP6212 作为 embedded SDIO 设备, 上电直接用 GPIO 控制 VDD/VDDIO 即可, 不需要协议级的电源管理。SMPC=0 (标准功耗) 默认工作。

---

## 第 12 章: High-Speed 模式

### 12.1 切换方法

SDIO High-Speed 模式下时钟 50MHz, 吞吐量 25MB/s (4-bit)。

通过 CCCR 13h 控制:

| 位 | 名称 | 说明 |
|----|------|------|
| SHS (bit 0, R/O) | Support High-Speed | =1 表示卡支持 HS 模式 |
| EHS (bit 1, R/W) | Enable High-Speed | =1 使能 HS, =0 切回 Default |

### 切换流程

主机可用 **RAW 模式** 一次完成检查和设置:

1. CMD52 RAW=1 写 EHS=1
2. 读 R5 回查 SHS 和 EHS
3. 如果 SHS=1 且 EHS=1 → 切到 High-Speed 模式
4. 如果 SHS=0 或 EHS=0 → 忽略, 卡仍保持 Default 模式

**约束:**
- 设/清 EHS 后, 卡必须在 **8 个时钟内** 切到目标模式
- 切换期间主机应屏蔽中断, 发完 CMD52 后 8 个时钟内不发其他命令
- Combo 卡: 存储和 I/O 共享速度模式, 任一复位 (CMD52 RES 或 CMD0) 都回到 Default
- 锁定状态的卡不能切速度, 需先 CMD42 解锁

---

## 第 13 章: 物理特性

SDIO 支持三种外形:

| 类型 | 兼容卡槽 |
|------|---------|
| Full-Size SDIO | 标准 SD 卡槽, 允许外接天线/连接器 |
| miniSDIO | miniSD 卡槽, 可通过适配器转 SD |
| microSDIO | microSD 卡槽, 不支持机械扩展 |

AP6212 是 embedded SDIO, 不涉及外形因素。

---

## 第 14 章: 电源要求

- SDIO 2.00+ 工作电压范围: **2.7V ~ 3.6V**
- 2.0V~2.7V 范围在 SDIO 2.00 后废弃
- AP6212 典型供电: VDD=3.3V, VDDIO=1.8V 或 3.3V

---

## 第 15 章: 浪涌电流限制

简化规范中已省略此章。

---

## 第 16 章: CIS 格式

CIS 基于 PCMCIA 的 Tuple (元组) 链结构, 用于描述卡和 Function 的能力。

### 基本 Tuple 格式

| 字节 | 字段 | 说明 |
|------|------|------|
| 00h | TPL_CODE | Tuple 代码 (CISTPL_xxx) |
| 01h | TPL_LINK | 到下一个 Tuple 的偏移 (body 字节数) |
| 02h+ | TPL_BODY | Tuple 数据 |

- TPL_CODE = FFh → 链结束
- TPL_LINK = 0 → body 为空
- TPL_LINK = FFh → 本 tuple 是链中最后一个 (推荐用 FFh code 代替)
- 最大 257 字节 (1+1+255)
- 多字节数值用 little-endian

### 支持的 Tuple 类型

| Code | 名称 | Common | Function | 说明 |
|------|------|--------|----------|------|
| 00h | CISTPL_NULL | O | O | 空 |
| 15h | CISTPL_VERS_1 | R | R | 版本/产品信息 |
| 20h | CISTPL_MANFID | **M** | O | 厂商 ID |
| 21h | CISTPL_FUNCID | n/a | **M** | 功能标识 (SDIO=0Ch) |
| 22h | CISTPL_FUNCE | n/a | **M** | 功能扩展信息 |
| 80h-8Fh | Vendor Unique | O | O | 厂商自定义 |
| 91h | CISTPL_SDIO_STD | n/a | M(标准)/O(非标) | 标准 SDIO Function 信息 |
| FFh | CISTPL_END | M | M | 链结束 |

### 主要 Tuple 说明

**CISTPL_MANFID (20h):** 厂商 ID, 含 TPLMID_MANF (厂商代码) + TPLMID_CARD (产品号/版本)。

**CISTPL_FUNCID (21h):** Function 标识, TPLFID_FUNCTION=0Ch (SDIO), 所有 CIS 链必须包含。

**CISTPL_FUNCE (22h) Function 0:** 最大块大小、最大传输速率 (Full-Speed 卡=25Mb/s=32h)、Tc/功耗对。

**CISTPL_FUNCE (22h) Function 1-7 (01h):** 含功能信息、CSA 大小、最大块大小、OCR、各模式下功耗、使能超时。

**CISTPL_FUNCE (22h) Function 1-7 (02h):** Power State Tuple, 最多 15 个电源状态。

**约束:**
- 对 AP6212 (embedded), CIS 可选, 不需要读
- Function 1 的 Standard Function Interface Code=7h (WLAN) 已在 FBR 中

---

## 第 17 章: Embedded SDIO

### 17.1 与可插拔 SDIO 的差异

| 特性 | SDIO 卡 | Embedded SDIO |
|------|---------|---------------|
| 外形 | 必须符合 SD 卡规格 | 无要求, 直接焊板 |
| CIS | 必须有 | **可选** — 主机通过其他方式识别 |
| 8-bit 模式 | 不支持 | 支持 (时钟 ≤50MHz, UHS-I tuning 不适用) |
| 功耗 | 受卡槽电流限制 | 可放宽 (靠 PCB 散热) |
| 中断 | DAT[1] 复用 | 可提供独立 INT# 引脚 (开漏低电平) |

### 17.2 接口信号

Embedded SDIO 推荐接口包含:
- 标准 SD 总线: CLK, CMD, DAT[3:0]
- 可选 DAT[7:4] (8-bit 模式)
- 可选 **INT#** 独立中断引脚 (开漏, 适用于 Shared Bus)
- 后端功能电源与接口电源可分离 (例如 VDDIO=1.8V, VDD=3.3V)

### 17.3 Shared Bus 配置

多个 embedded SDIO 设备可共享同一条 SD 总线 (除 CLK 外):

```
Host ──CLK─── Device1 (独立 CLK)
      │       Device2 (独立 CLK)
      ├─CMD─── 共享
      ├─DAT─── 共享
      └─INT#── 共享 (线或)
```

- 每个设备有独立的时钟引脚, 未选中的设备停时钟省电
- INT# 开漏线或, 任意设备可唤醒主机
- 需上拉电阻

### 与 AP6212 的关系

AP6212 是 embedded SDIO 设备:
- **CIS 可跳过**, 硬件配置已知
- 接口电源 VDDIO=1.8V, 核心电源 VDD=3.3V (分离供电)
- 支持独立中断引脚 (WL_HOST_WAKE), 不一定用 DAT[1]
- 上电用 GPIO 控制, 不需热插拔检测
- Shared Bus 时钟切换: 从切换时钟引脚选择开始, **8 个时钟内**必须输出新时钟, 各设备时钟不重叠
