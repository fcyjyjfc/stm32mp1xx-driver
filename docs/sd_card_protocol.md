# SD 卡协议笔记

来源: SD Physical Layer Simplified Specification Version 9.10

---

## 4.2 卡识别模式 (Card Identification Mode)

上电后所有通信在 CMD 线上进行（不涉及数据线），时钟必须 ≤ 400kHz。

### 4.2.1 Card Reset — CMD0

- CMD0 是软复位命令，把卡从任何状态打回 Idle State
- 上电后卡自动在 Idle State，RCA 默认 = 0x0000

### 4.2.2 Operating Condition Validation — CMD8 + ACMD41

**CMD8 (SEND_IF_COND):**
- 告诉卡"我的供电电压是 X"，附带 check pattern
- 卡支持该电压 → 原样回显电压和 pattern（R7 响应）
- 卡不支持 → 不回复（超时）
- CMD8 是强制性的——不发 CMD8 就无法初始化 SDHC/SDXC 卡

**ACMD41 (SD_SEND_OP_COND):**
- 是 ACMD，实际发送时先发 CMD55（RCA=0x0000），再发 CMD41
- 告诉卡我的电压范围 + 是否支持高容量（HCS 位）
- 响应（R3）中 Busy 位：
  - Busy=0：卡还在初始化，继续循环发 ACMD41
  - Busy=1：初始化完成
- 必须在 1 秒内完成，超时则放弃

### 4.2.3 Card Initialization Flow — 完整序列

```
CMD0                    → 复位卡到 Idle
CMD8 (arg=0x1AA)        → 验证电压+版本检测
                           有响应 → SD v2.0+ 卡
                           无响应 → SD v1.x 或非 SD 卡
循环 {
  CMD55 (arg=0x0000)    → 告知下一条是 ACMD
  ACMD41 (arg=HCS|VDD)  → 发送操作条件
} 直到 响应的 Busy=1

从 ACMD41 响应读取 CCS 位:
  CCS=0 → SDSC (≤2GB，用字节地址)
  CCS=1 → SDHC/SDXC (>2GB，用块地址)

CMD2                    → 读 CID（128-bit 卡唯一标识）
CMD3                    → 卡发布 RCA（16-bit 相对地址）
                           卡进入 Stand-by State
```

### ACMD41 参数详解

**Argument (发送):**

| Bit | 字段 | 含义 |
|-----|------|------|
| 30 | HCS | =1 表示主机支持 SDHC/SDXC |
| 28 | XPC | =1 表示最大性能模式（功耗更高） |
| 24 | S18R | =1 请求切换 1.8V 信号（UHS-I） |
| 23:0 | OCR | 支持的电压范围窗口 |

**Response R3 (接收):**

| Bit | 字段 | 含义 |
|-----|------|------|
| 31 | Busy | =1 初始化完成，以下字段才有效 |
| 30 | CCS | =0 SDSC，=1 SDHC/SDXC |
| 29 | UHS-II | =1 卡支持 UHS-II |
| 24 | S18A | =1 卡同意切换 1.8V |
| 23:0 | OCR | 卡支持的电压范围 |

### 状态图 (Figure 4-1)

```
Power-on → Idle ──CMD8──→ (验证电压)
                    │
                    ├─ 无响应 → SD v1.x 或非SD卡
                    ├─ 有响应 → SD v2.0+
                    ▼
              ACMD41 循环
                    │
                    ├─ Ready (busy=1) ──CMD2──→ Identification ──CMD3──→ Stand-by
                    └─ 不兼容电压 → Inactive State
```

---

## 4.3 数据传输模式 (Data Transfer Mode)

卡识别完成（有了 RCA）后进入数据传输模式。

### 进入流程

```
CMD9 (arg=RCA<<16)      → 读 CSD 寄存器（容量、最大时钟等信息）
提频: 25MHz (Default Speed) 或 50MHz (High Speed)
CMD7 (arg=RCA<<16)      → 选中卡（Stand-by → Transfer State）
CMD55+ACMD6 (arg=2)     → 切 4-bit 总线
```

### 状态转换图 (Figure 4-13)

```
Stand-by ──CMD7──→ Transfer ──CMD17/18──→ Sending-data ──完成/CMD12──→ Transfer
                      │
                      ├──CMD24/25──→ Receive-data ──完成──→ Programming ──完成──→ Transfer
                      │                                         (DAT0=LOW, busy)
                      └──CMD16/23/32/33── (参数设置，不换状态)
```

### 4.3.1 宽总线切换

- ACMD6 (arg=0x2): 切到 4-bit 模式
- 前提: 卡在 Transfer State（已 CMD7 选中），且未锁定
- 上电默认 1-bit

### 4.3.3 数据读

- **CMD17 (READ_SINGLE_BLOCK)**: 读单块(512B)，完成后自动回 Transfer State
- **CMD18 (READ_MULTIPLE_BLOCK)**: 读多块，连续传输直到 CMD12 停止
- 块大小固定 512 字节

读操作时序:
```
Host→Card: CMD17/18 (arg=地址) [CMD线]
Card→Host: R1 响应             [CMD线]
Card→Host: 数据块+CRC16        [DAT线, IDMA自动搬运]
```

### 4.3.4 数据写

- **CMD24 (WRITE_BLOCK)**: 写单块
- **CMD25 (WRITE_MULTIPLE_BLOCK)**: 写多块，CMD12 停止
- 写完后卡编程 Flash → DAT0 拉低表示 Busy → 释放表示完成
- 卡有写缓冲: 前一块编程中可以接收下一块，缓冲满了才 Busy

写操作时序:
```
Host→Card: CMD24/25 (arg=地址) [CMD线]
Card→Host: R1 响应             [CMD线]
Host→Card: 数据块+CRC16        [DAT线]
Card→Host: CRC Status (3bit)   [DAT0]
Card:       DAT0=LOW (Busy)    — 正在编程
Card:       DAT0=HIGH          — 完成
```

**ACMD23 预擦除加速:**
- 多块写之前用 ACMD23 告诉卡 "我要写 N 块"
- 卡可以批量预擦除，比逐块擦写快得多
- 不用 ACMD23 也能写，只是慢一些

### 4.3.5 擦除

```
CMD32 (arg=起始块地址)  → 设置擦除起始
CMD33 (arg=结束块地址)  → 设置擦除结束
CMD38                   → 执行擦除（卡 Busy）
```

### 块地址 vs 字节地址

- **SDSC（≤2GB）**: CMD17/24 的 arg 是字节地址。读第2块: arg = 2 × 512 = 1024
- **SDHC/SDXC（>2GB）**: CMD17/24 的 arg 是块地址。读第2块: arg = 2

初始化时从 ACMD41 响应的 CCS 位判断卡类型。

---

## 完整流程总结

```
=== 识别阶段 (≤400kHz, 1-bit) ===
CMD0  → 复位
CMD8  → 电压验证
CMD55+ACMD41 (循环) → 等 ready, 读 CCS
CMD2  → 读 CID
CMD3  → 获取 RCA

=== 配置阶段 ===
CMD9  → 读 CSD（容量/速度信息）
CMD7  → 选中卡（进入 Transfer State）
CMD55+ACMD6 → 切 4-bit
可选: CMD6 (Switch Function) → 切 High Speed (50MHz)

=== 数据阶段 ===
CMD17/18 → 读块
CMD24/25 → 写块
CMD23    → 预设块数（多块写加速）
CMD12    → 停止多块传输
CMD32+CMD33+CMD38 → 擦除
```

---

## 4.7 命令定义 (Commands)

### 命令格式 (48-bit)

```
| Start(0) | Trans(1=host) | CmdIndex(6) | Argument(32) | CRC7(7) | End(1) |
```

### 命令类型

- **bc**: 广播，无响应 (CMD0)
- **bcr**: 广播，有响应 (CMD2/CMD3/CMD8/ACMD41)
- **ac**: 寻址命令，无数据 (CMD7/CMD13/CMD55)
- **adtc**: 寻址命令，有数据传输 (CMD17/CMD18/CMD24/CMD25)

### 我们需要的命令表

| CMD | 类型 | Argument | 响应 | 名称 | 作用 |
|-----|------|----------|------|------|------|
| 0 | bc | 0 | 无 | GO_IDLE_STATE | 复位卡 |
| 2 | bcr | 0 | R2 | ALL_SEND_CID | 读 CID |
| 3 | bcr | 0 | R6 | SEND_RELATIVE_ADDR | 获取 RCA |
| 7 | ac | RCA<<16 | R1b | SELECT_CARD | 选中卡 |
| 8 | bcr | VHS+pattern | R7 | SEND_IF_COND | 电压验证 |
| 9 | ac | RCA<<16 | R2 | SEND_CSD | 读 CSD |
| 12 | ac | 0 | R1b | STOP_TRANSMISSION | 停止多块传输 |
| 13 | ac | RCA<<16 | R1 | SEND_STATUS | 查询卡状态 |
| 17 | adtc | 地址 | R1 | READ_SINGLE_BLOCK | 读单块 |
| 18 | adtc | 地址 | R1 | READ_MULTIPLE_BLOCK | 读多块 |
| 23 | ac | 块数 | R1 | SET_BLOCK_COUNT | 预设块数 |
| 24 | adtc | 地址 | R1 | WRITE_BLOCK | 写单块 |
| 25 | adtc | 地址 | R1 | WRITE_MULTIPLE_BLOCK | 写多块 |
| 32 | ac | 地址 | R1 | ERASE_WR_BLK_START | 擦除起始 |
| 33 | ac | 地址 | R1 | ERASE_WR_BLK_END | 擦除结束 |
| 38 | ac | 0 | R1b | ERASE | 执行擦除 |
| 55 | ac | RCA<<16 | R1 | APP_CMD | "下一条是ACMD" |
| ACMD6 | ac | bus_width | R1 | SET_BUS_WIDTH | 切4-bit(arg=2) |
| ACMD23 | ac | 块数 | R1 | SET_WR_BLK_ERASE_COUNT | 预擦除 |
| ACMD41 | bcr | HCS\|VDD | R3 | SD_SEND_OP_COND | 操作条件 |
| ACMD51 | adtc | 0 | R1 | SEND_SCR | 读 SCR |

注意:
- 地址字段: SDSC 用字节地址，SDHC/SDXC 用块地址（512B单位）
- adtc 类型 = 命令带数据传输（驱动里 cmdtrans=1）
- ac 类型 = 纯命令无数据（cmdtrans=0）
- CMD6 (Switch Function) 用来切高速模式，Group 1 Access Mode

### CMD8 参数格式

```
Argument[11:8] = VHS (Voltage supplied): 0001b = 2.7~3.6V
Argument[7:0]  = Check pattern: 建议用 0xAA
→ 完整 arg = 0x000001AA
```

### ACMD41 参数格式

```
Argument:
  [30] HCS = 1 (支持SDHC/SDXC)
  [28] XPC = 1 (最大性能) 或 0
  [24] S18R = 0 (不切1.8V，我们不用UHS-I)
  [23:0] OCR 电压窗口 = 0xFF8000 (3.2~3.4V 全覆盖)
→ 常用 arg = 0x40FF8000 (HCS=1, 电压全范围)
```

---

## 4.9 响应格式 (Responses)

### R1 — 通用短响应 (48-bit)

```
| Start(0) | Trans(0) | CmdIndex(6) | Card Status(32) | CRC7(7) | End(1) |
```
- Card Status: 32-bit 状态/错误标志集合
- 驱动里 resp[0] = Card Status
- SDMMC 映射: SDMMC_RESP_SHORT

### R1b — 同 R1 + Busy

- 格式完全相同
- 区别: 响应后卡可能拉低 DAT0（编程/擦除进行中）
- CMD7、CMD12、CMD38 返回 R1b
- SDMMC 映射: 同样用 SDMMC_RESP_SHORT，驱动额外等 BUSYD0END

### R2 — 长响应 (136-bit)

```
| Start(0) | Trans(0) | Reserved(111111) | CID/CSD[127:1](127bit) | End(1) |
```
- 没有外部 CRC7 字段（CID/CSD 内部自带 CRC）
- CmdIndex 固定 111111，不校验
- resp[0]~resp[3] 拼出 CID/CSD 内容
- SDMMC 映射: SDMMC_RESP_LONG

### R3 — OCR 响应 (48-bit)

```
| Start(0) | Trans(0) | Reserved(111111) | OCR(32) | Reserved(1111111) | End(1) |
```
- **没有 CRC！** 硬件会报 CCRCFAIL，但不是真正的错误
- CmdIndex 固定 111111，不校验
- resp[0] = OCR（Busy=bit31, CCS=bit30）
- SDMMC 映射: SDMMC_RESP_SHORT_NOCRC

### R6 — RCA 响应 (48-bit)

```
| Start(0) | Trans(0) | CmdIndex(000011=CMD3) | RCA(16)+Status(16) | CRC7(7) | End(1) |
```
- CMD3 专用
- resp[0] 高 16 位 = RCA，低 16 位 = 部分 Card Status
- SDMMC 映射: SDMMC_RESP_SHORT

### R7 — 接口条件响应 (48-bit)

```
| Start(0) | Trans(0) | CmdIndex(001000=CMD8) | Reserved(18) | VoltAccepted(4) | CheckPattern(8) | CRC7(7) | End(1) |
```
- CMD8 专用
- resp[0] bit[11:8] = Voltage Accepted (应为 0x1 = 2.7~3.6V)
- resp[0] bit[7:0] = Check Pattern 回显 (应为 0xAA)
- 验证: (resp[0] & 0xFFF) == 0x1AA 则卡支持
- SDMMC 映射: SDMMC_RESP_SHORT

### 响应类型与 SDMMC 寄存器映射

| 响应类型 | WAITRESP 值 | 枚举 | 使用场景 |
|---------|------------|------|---------|
| 无响应 | 00 | SDMMC_RESP_NONE | CMD0 |
| R1/R1b/R6/R7 | 01 | SDMMC_RESP_SHORT | 大部分命令 |
| R3 | 10 | SDMMC_RESP_SHORT_NOCRC | ACMD41 |
| R2 | 11 | SDMMC_RESP_LONG | CMD2/CMD9 |

---

## 5.1 OCR 寄存器 (32-bit)

通过 ACMD41 响应（R3）获取，resp[0] = 完整 OCR。

| Bit | 字段 | 含义 |
|-----|------|------|
| 31 | Busy | =1 上电完成（初始化循环出口条件） |
| 30 | CCS | =0 SDSC，=1 SDHC/SDXC（决定地址模式） |
| 29 | UHS-II | =1 支持 UHS-II |
| 24 | S18A | =1 接受切 1.8V |
| 23:15 | 电压窗口 | 每位对应一个电压范围（2.7~3.6V） |
| 7 | Low Voltage | Dual Voltage 卡标志（收到CMD8后置1） |

关键用法: 轮询到 bit31=1 退出循环，然后读 bit30(CCS) 判断卡类型。

---

## 5.2 CID 寄存器 (128-bit)

通过 CMD2 获取（R2 长响应），resp[0]~resp[3] 拼出。

| 字段 | 位宽 | CID[bit] | 含义 |
|------|------|----------|------|
| MID | 8 | [127:120] | 厂商 ID |
| OID | 16 | [119:104] | OEM ID（2字符 ASCII） |
| PNM | 40 | [103:64] | 产品名（5字符 ASCII） |
| PRV | 8 | [63:56] | 产品版本（BCD: n.m） |
| PSN | 32 | [55:24] | 序列号 |
| MDT | 12 | [19:8] | 生产日期（[19:12]=年+2000, [11:8]=月） |
| CRC | 7 | [7:1] | CRC7 |

主要用于打印卡信息调试。

---

## 5.3 CSD 寄存器 (128-bit)

通过 CMD9 获取（R2 长响应）。**最关键：容量计算。**

### 区分版本

CSD_STRUCTURE 字段 [127:126]:
- 00 = CSD V1.0 → SDSC (≤2GB)
- 01 = CSD V2.0 → SDHC/SDXC (2GB~2TB)
- 10 = CSD V3.0 → SDUC (>2TB)

### CSD V2.0 (SDHC/SDXC，最常用)

关键字段:

| 字段 | 位宽 | CSD[bit] | 含义 |
|------|------|----------|------|
| CSD_STRUCTURE | 2 | [127:126] | =01 (V2.0) |
| TRAN_SPEED | 8 | [103:96] | 0x32=25MHz, 0x5A=50MHz |
| CCC | 12 | [95:84] | 命令类支持位图 |
| READ_BL_LEN | 4 | [83:80] | =9 (固定512B) |
| C_SIZE | 22 | [69:48] | 容量计算用 |
| WRITE_BL_LEN | 4 | [25:22] | =9 (固定512B) |

容量计算:
```
容量(字节) = (C_SIZE + 1) × 512KB
总块数     = (C_SIZE + 1) × 1024
```

示例: 32GB 卡, C_SIZE ≈ 65535, 容量 = 65536 × 512KB = 32GB

### CSD V1.0 (SDSC，≤2GB 老卡)

关键字段:

| 字段 | 位宽 | CSD[bit] |
|------|------|----------|
| C_SIZE | 12 | [73:62] |
| C_SIZE_MULT | 3 | [49:47] |
| READ_BL_LEN | 4 | [83:80] |

容量计算:
```
块数 = (C_SIZE + 1) × 2^(C_SIZE_MULT + 2)
容量 = 块数 × 2^READ_BL_LEN 字节
```

### resp[] 到 CSD 字段的映射

SDMMC 硬件把 R2 的 127-bit 放入 RESP1~RESP4:
```
resp[0] = CSD[127:96]
resp[1] = CSD[95:64]
resp[2] = CSD[63:32]
resp[3] = CSD[31:0]
```

提取 C_SIZE (V2.0, CSD[69:48]):
```c
uint32_t c_size = ((resp[1] & 0x3F) << 16) | (resp[2] >> 16);
uint64_t capacity_bytes = (uint64_t)(c_size + 1) * 512 * 1024;
uint32_t total_blocks = (c_size + 1) * 1024;
```

提取 TRAN_SPEED (CSD[103:96]):
```c
uint8_t tran_speed = (resp[0] >> 0) & 0xFF;  // resp[0] = CSD[127:96], 所以[103:96]在bit[7:0]
// 0x32 = 25MHz (Default Speed)
// 0x5A = 50MHz (High Speed)
```

---

## 5.4 RCA 寄存器 (16-bit)

CMD3 响应（R6）里卡自动分配的 16-bit 相对地址。之后所有寻址命令用 `RCA<<16` 作为参数高16位。
默认值 0x0000 保留用于取消选中所有卡。

---

## 5.6 SCR 寄存器 (64-bit)

通过 ACMD51 读取（adtc 类型，卡在 DAT 线上发 8 字节数据）。

### 关键字段

| 字段 | 位宽 | SCR[bit] | 含义 |
|------|------|----------|------|
| SD_SPEC | 4 | [59:56] | SD 规范主版本 |
| DATA_STAT_AFTER_ERASE | 1 | [55] | 擦除后数据是0还是1 |
| SD_BUS_WIDTHS | 4 | [51:48] | 支持的总线宽度 |
| SD_SPEC3 | 1 | [47] | =1 则 ≥V3.0 |
| SD_SPEC4 | 1 | [42] | =1 则 ≥V4.0 |
| SD_SPECX | 4 | [41:38] | 更高版本号 |
| CMD_SUPPORT | 5 | [36:32] | 可选命令支持位 |

### SD_BUS_WIDTHS

- bit0 = 支持 1-bit (DAT0)
- bit2 = 支持 4-bit (DAT0-3)

所有 SD 卡都必须支持 1-bit 和 4-bit（值至少为 0101b）。

### CMD_SUPPORT 位

| Bit | 含义 |
|-----|------|
| 32 | CMD20 (Speed Class Control) |
| 33 | CMD23 (Set Block Count, 预设块数) |
| 34 | CMD48/49 (Extension Register Single Block) |
| 35 | CMD58/59 (Extension Register Multi-Block) |
| 36 | ACMD53/54 (Secure Receive/Send) |

### 版本判断

| SD_SPEC | SD_SPEC3 | SD_SPEC4 | SD_SPECX | 版本 |
|---------|----------|----------|----------|------|
| 0 | 0 | 0 | 0 | V1.0/1.01 |
| 1 | 0 | 0 | 0 | V1.10 |
| 2 | 0 | 0 | 0 | V2.00 |
| 2 | 1 | 0 | 0 | V3.0x |
| 2 | 1 | 1 | 0 | V4.xx |
| 2 | 1 | x | 1 | V5.xx |
| 2 | 1 | x | 2 | V6.xx |

### 初始化中的用法

切 4-bit 之前可以先读 SCR 确认支持:
```c
// CMD55 + ACMD51 → 读 8 字节 SCR 数据
// 检查 SCR[51:48] bit2=1 (支持4-bit)
// CMD55 + ACMD6(arg=2) → 切 4-bit
```
实际所有 SD 卡都支持 4-bit，但规范建议先确认。

---

## 4.10 Card Status (32-bit)

嵌在 R1 响应中返回（`resp[0]` = Card Status），报告卡当前状态和上一条命令的错误。

### 状态字段

| Bit | 名称 | 含义 |
|-----|------|------|
| [12:9] | **CURRENT_STATE** | 卡当前状态机位置 |
| [8] | READY_FOR_DATA | =1 卡缓冲区空闲，可接收下一块数据 |
| [5] | APP_CMD | =1 卡已收到 CMD55，下条命令解释为 ACMD |

### CURRENT_STATE 编码 [12:9]

| 值 | 状态名 | 含义 |
|----|--------|------|
| 0 | idle | 上电 / CMD0 后 |
| 1 | ready | ACMD41 初始化完成 |
| 2 | ident | CMD2 已读 CID |
| 3 | stby | CMD3 获得 RCA，待机 |
| 4 | **tran** | CMD7 选中，可读写数据 |
| 5 | data | 正在传输数据 |
| 6 | rcv | 正在接收写数据 |
| 7 | prg | 正在编程 Flash |
| 8 | dis | 断开连接中 |

正常工作状态: 初始化完成后卡在 **tran(4)**，读写时短暂进入 data/rcv/prg，完成后回到 tran。

### 错误位（高位，类型 E，读即清）

| Bit | 名称 | 含义 |
|-----|------|------|
| [31] | OUT_OF_RANGE | 地址超出卡容量范围 |
| [30] | ADDRESS_ERROR | 地址未对齐到块边界（仅 SDSC） |
| [29] | BLOCK_LEN_ERROR | 块长度不合法 |
| [26] | WP_VIOLATION | 写入写保护区域 |
| [23] | COM_CRC_ERROR | 上一条命令 CRC 失败 |
| [22] | ILLEGAL_COMMAND | 当前状态下不支持该命令 |
| [19] | ERROR | 通用内部错误 |

### 驱动中的使用

```c
uint32_t card_status = resp[0];

// 检查错误（bit[31:19] 中任一位为1）
if (card_status & 0xFFF90000)
    // 处理错误...

// 提取当前状态
uint8_t state = (card_status >> 9) & 0xF;
// state == 4 (tran) 才能发读写命令
```
