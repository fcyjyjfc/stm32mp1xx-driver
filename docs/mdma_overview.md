# MDMA Controller (RM0436 Chapter 17, p1161-1184)

## 17.1 MDMA Introduction

Master DMA (MDMA) 用于提供内存到内存或外设到内存的高速数据传输，无需 CPU 参与。

MDMA 通过 AXI 主接口访问主存储器和外设寄存器（system access port）。

MDMA 与标准 DMA 控制器（DMA1/DMA2）协同工作，提供多达 32 个通道，每个通道可管理来自 DMA stream memory buffer 或其他外设的内存访问请求（带集成 FIFO）。

## 17.2 MDMA Main Features

- AXI 主总线架构
- 32 个通道
- 多达 40 个硬件触发源，每个通道可软件配置选择任意请求源，block 传输结束时可自动切换触发源
- 每个通道也支持软件触发
- 256 级缓冲区，分为两个 128 级 FIFO（双缓冲）：
  - 一个 FIFO 存储当前 block 传输数据（burst 或 single 模式）
  - 另一个 FIFO 可预取下一个 buffer（同通道或下一通道）
- 4 级软件优先级（Very High / High / Medium / Low），相同优先级时硬件仲裁（通道号小的优先）
- 独立的源/目的数据宽度（byte / half-word / word / double-word），宽度不同时自动 pack/unpack
- 源和目的的地址增量大小可独立设置
- 支持地址递增、递减、固定三种模式
- Pack/unpack 遵循 little-endian 约定
- 支持增量突发传输，burst 最大 128 字节（如 16x64bit 或 32x32bit）
- TCM 访问时 burst 仅允许增量=数据宽度且 <=32bit
- 5 个事件标志（均可触发中断）：
  - Channel Transfer Complete
  - Block Transfer Complete
  - Block Repeat Transfer Complete
  - Buffer Transfer Complete
  - Transfer Error

## 17.3 MDMA Functional Description (p1163-1164)

### 17.3.1 Block Diagram

内部信号：

| 信号 | 类型 | 说明 |
|------|------|------|
| mdma_hclk | 输入 | MDMA AHB 时钟 |
| mdma_it | 输出 | MDMA 中断 |
| mdma_sec_it | 输出 | MDMA 安全中断 |
| mdma_str[0:39] | 输入 | 40 路硬件触发请求 |

结构：32 通道仲裁器 → 读写请求 → 双 FIFO → 64-bit AXI master

### 17.3.3 MDMA Overview

MDMA 作为 AXI master 发起总线事务，支持三种传输类型：
- **memory-to-memory**（软件触发）
- **peripheral-to-memory**
- **memory-to-peripheral**

后两种中 memory 也可以是 memory-mapped 外设（无 flow control）。当请求来自 DMA1/DMA2 时，外设寄存器访问被替换为对该 DMA 使用的 memory buffer 的访问。

Non-incrementing/decrementing 模式不用于 memory 访问。

AHB slave port 用于编程 MDMA（支持 8/16/32-bit 访问）。

### 触发模式 TRGM[1:0]

单次触发传输的数据量由 TRGM[1:0] 选择，四个选项为：
- Buffer transfer size
- Block size
- Repeated block
- Complete channel data（直到链表指针 NULL）

用户应根据可用数据量（通常在 DMA1/2 memory buffer 中）和其他 MDMA 通道的实时性需求来选择（buffer transfer 是不经过通道重新仲裁的最小数据聚合单位）。

具体编码见寄存器描述章节。

### 三层数据粒度（Three key data array sizes）

| 层级 | 手册定义 |
|------|---------|
| Burst size | 可在 burst 模式下传输的数据长度。在总线仲裁级别不可中断，会阻塞其他 master 访问总线 |
| Buffer transfer size | 一个通道在检查其他通道请求之前传输的数据长度。在 MDMA 级别不可中断（其他通道请求不能打断） |
| Block size | 含义一：链表中一个 block 结构描述的数据长度（对应链表一个条目）；含义二：当 TRGM=01 时，单次请求激活传输的数据长度 |

## 17.3.4 MDMA Channel (p1165)

每个通道提供源到目的的单向传输链路，支持三种传输类型：

| 类型 | 说明 |
|------|------|
| Single block transfer | 传完一个 block 后通道禁用，产生 end-of-channel 中断 |
| Repeated block transfer | 传完若干 block 后通道禁用 |
| Linked-list transfer | 当前 block（或 repeated block 的最后一个 block）完成后，从内存加载新的 block 控制结构，开始新 block |

- 每次请求传输的最小数据量（buffer size）可编程，最大 128 字节
- Block 总数据量可编程，最大 64 KB，每次传输后递减，到零时根据 repeat counter 和/或链表结构决定下一步动作
- 若 block 长度不是 buffer 长度的整数倍，最后一个 buffer transfer 自动缩短覆盖剩余字节
- 链表地址指向有效内存 → 从该地址重新加载整个通道描述符寄存器内容，下次请求时开始新 block
- 链表地址为 0x0 → 当前/repeated block 传输结束后通道禁用，产生 end-of-channel 中断

## 17.3.5 Source, destination and transfer modes (p1165)

源和目的地址可寻址整个 4 GB 空间（0x00000000 ~ 0xFFFFFFFF）。

地址可固定（用于 FIFO/单数据寄存器外设）或递增/递减。传输可配为 single 或 burst 模式。

## 17.3.6 Pointer update (p1165)

地址更新由 SINC[1:0] / DINC[1:0]（MDMA_CxCR）控制：
- 禁止增量：地址固定（适用于外设单寄存器/FIFO 访问）
- 使能增量/递减：每次传输后地址 +/- 1, 2, 4 或 8，由 SINCOS[1:0] / DINCOS[1:0] 设置

增量步长与数据宽度独立可编程，以优化 packing 操作。

## 17.3.7 MDMA buffer transfer (p1166)

Buffer transfer 是单次 MDMA 请求事件在一个通道上传输的最小逻辑数据量（最大 128 字节）。

由若干次 single 或 burst 数据传输组成。数据项数量、宽度（8/16/32/64-bit）、burst 长度均独立可编程。

DMA/外设发送请求信号给 MDMA，MDMA 根据通道优先级服务请求。

请求确认机制：
- 设置了 mask address 寄存器：向 mask address 写入 mask data value 来确认
- mask address = 0x00：通过读写外设数据来确认（若请求来自目的外设，写入须设为 non-bufferable，避免误触发新请求）

**TRGM[1:0] = 00（单 buffer 模式）**：
- 传完一个 buffer 后等待同通道的下一次请求
- 当前通道写阶段结束前，不再响应同通道的硬件请求
- 即使当前通道仍在写阶段，其他通道（包括低优先级）可以开始读阶段——通道间可交错执行

**TRGM[1:0] ≠ 00（多 buffer 模式）**：
- 当前通道的请求在内部保持有效（internally memorized），直到 TRGM 定义的整个传输（block / repeated block / 整个链表）完成
- 每个 buffer 传输后重新仲裁（外部新请求 vs 内部保持的请求），若无更高优先级请求则继续当前通道
- 因此高优先级请求不会被低优先级通道阻塞超过一个 buffer transfer 的时间（每个 buffer 后都有仲裁机会）

## 17.3.8 Request arbitration (p1167)

MDMA 空闲时及每次 buffer transfer 结束后，检查所有使能通道的请求（硬件或软件）。

两级优先级：
1. 软件优先级（MDMA_CxCR）：Very High / High / Medium / Low
2. 硬件优先级：相同软件优先级时，通道号小的优先（如 ch2 > ch4）

## 17.3.9 FIFO (p1167)

中央 FIFO 结构，所有通道共享，用于暂存源数据再写入目的。

并行机制：
- Buffer 传输期间，FIFO 数据够一次目的 burst 就开始写
- 当前 buffer 的读阶段全部完成后，立即启动仲裁；仲裁后下一个 buffer 数据可以开始读入 FIFO

通道因错误被禁用时，FIFO 中剩余数据被丢弃。

## 17.3.10 Block transfer (p1167)

Block = 连续数据数组，最大 64 KB，由多次 buffer transfer 完成。每个 block 由起始地址和长度定义。

Block 完成后三种动作：
1. 属于 repeated block → 重载 block 长度，根据 MDMA_CxBRUR 计算新起始地址
2. 单 block 或 repeated block 的最后一个 → 从链表地址（MDMA_CxLAR）加载下一个 block 信息
3. 最后一个 block 且 MDMA_CxLAR = 0 → 通道禁用，不再接受请求

## 17.3.11 Block repeat mode (p1168)

Repeat counter ≠ 0 时，每次 block 完成后：
- 重载 BNDT（block 长度）
- 根据 BRSUM/BRDUM 更新 SAR/DAR（源/目的起始地址）
- Repeat counter 减 1

Repeat counter = 0 时，当前 block 按 single block 处理。

## 17.3.12 Linked-list mode (p1168)

从 MDMA_CxLAR 指向的地址加载新配置（CxTCR, CxBNDTR, CxSAR, CxDAR, CxBRUR, CxLAR, CxTBR, CxMAR, CxMDR），地址必须在 AXI 系统总线可访问的 memory 上。

加载后通道按新配置的 block/repeated block 接受请求，或当 TRGM=11 时继续传输。

触发源可通过加载 MDMA_CxTBR 自动切换。TRGM=11 时不得修改 TRGM 和 SWRM 值。

## 17.3.13 MDMA transfer completion (p1168)

设置 CTCIF 的条件：
- BNDT 计数器到零 + Block Repeat Counter = 0 + 链表指针 = 0
- 或通道被禁用（清 EN）且 FIFO 剩余数据全部写入目的

## 17.3.14 MDMA transfer suspension (p1168-1169)

两种情况：

1. **禁用（不再重启）**：清 EN 位，等待当前 buffer 完成后通道停止。CTCIF 置位确认结束。CxNDTR 保存剩余数据量。
2. **挂起（稍后重启）**：挂起后 CTCIF 置位。若不修改 CxBNDTR/CxSAR/CxDAR，重新使能通道即可继续传输。重启前须先清 CTCIF。若挂起点恰好是 block 末尾，配置寄存器会自动更新以备重启。

重新编程通道前必须等待 CTCIF 置位，确保任何进行中的操作已完成。

## 17.3.15 Error management (p1169)

检测以下错误（置 TEIF）：
- 读/写访问时总线错误
- 地址对齐与数据宽度不匹配
- Block size 不是数据宽度的整数倍（最后一次传输时报错，错误地址指向该次传输）

## 17.4 MDMA interrupts (p1169)

| 中断事件 | 标志位 | 使能位 |
|---------|--------|--------|
| Channel transfer completed | CTCIF | CTCIE |
| Block-transfer repeat completed | BTRIF | BTRIE |
| Block-transfer completed | BTIF | BTIE |
| Buffer transfer completed | TCIF | TCIE |
| Transfer error | TEIF | TEIE |

设置使能位前须先清对应标志位，否则可能立即触发中断。

当标志位和对应使能位同时为 1 时，通道中断位在 MDMA_SGISR 中置位，输出中断信号（需 NVIC 中对应通道使能）。

## 17.5 MDMA Registers (p1170-1184)

寄存器支持 word / half-word / byte 访问。每通道寄存器组间距 0x40。

### 寄存器总览

| 寄存器 | Offset | 说明 |
|--------|--------|------|
| MDMA_GISR0 | 0x00 | 全局中断状态，bit[31:0] 对应 ch31~ch0，各 bit 为该通道所有中断标志的 OR |
| MDMA_SGISR0 | 0x08 | 安全全局中断状态（同上） |
| MDMA_CxISR | 0x40+0x40*x | 通道中断状态：CRQA(16), TCIF(4), BTIF(3), BRTIF(2), CTCIF(1), TEIF(0) |
| MDMA_CxIFCR | 0x44+0x40*x | 中断标志清除：写 1 清对应 ISR 标志 |
| MDMA_CxESR | 0x48+0x40*x | 错误状态：BSE(11), ASE(10), TEMD(9), TELD(8), TED(7), TEA[6:0] |
| MDMA_CxCR | 0x4C+0x40*x | 通道控制 |
| MDMA_CxTCR | 0x50+0x40*x | 传输配置 |
| MDMA_CxBNDTR | 0x54+0x40*x | Block 数据量 + 重复计数 |
| MDMA_CxSAR | 0x58+0x40*x | 源地址 |
| MDMA_CxDAR | 0x5C+0x40*x | 目的地址 |
| MDMA_CxBRUR | 0x60+0x40*x | Block repeat 地址更新值 |
| MDMA_CxLAR | 0x64+0x40*x | 链表地址（须双字对齐） |
| MDMA_CxTBR | 0x68+0x40*x | 触发源选择 |
| MDMA_CxMAR | 0x70+0x40*x | Mask 地址（ACK 时写入目标） |
| MDMA_CxMDR | 0x74+0x40*x | Mask 数据（ACK 时写入的值） |

### MDMA_CxISR — 通道中断状态

| Bit | 名称 | 说明 |
|-----|------|------|
| 16 | CRQA | 通道请求激活标志。软件写 SWRQ 或硬件请求到来时置位，请求完成（最后一个 buffer 写阶段结束）时硬件清除 |
| 4 | TCIF | Buffer transfer complete。每完成一个 buffer 置位 |
| 3 | BTIF | Block transfer complete |
| 2 | BRTIF | Block repeat transfer complete |
| 1 | CTCIF | Channel transfer complete。最后一个 block 完成且通道自动禁用时置位；或通道被挂起（EN 清零）时置位 |
| 0 | TEIF | Transfer error |

### MDMA_CxESR — 错误状态

| Bit | 名称 | 说明 |
|-----|------|------|
| 11 | BSE | Block size 不是数据宽度的整数倍 |
| 10 | ASE | 地址对齐与数据宽度不匹配 |
| 9 | TEMD | 写 mask data 时发生传输错误 |
| 8 | TELD | 读链表数据结构时发生传输错误 |
| 7 | TED | 错误方向：0=读访问错误，1=写访问错误 |
| 6:0 | TEA | 产生错误的地址低 7 位（加到当前 SAR/DAR 可还原故障地址） |

### MDMA_CxCR — 通道控制

| Bit | 名称 | 说明 |
|-----|------|------|
| 16 | SWRQ | 软件请求，写 1 触发传输 |
| 14 | WEX | Word 字节序交换（双字内两个 word 互换） |
| 13 | HEX | Half-word 字节序交换（word 内两个 half-word 互换） |
| 12 | BEX | Byte 字节序交换（half-word 内两个 byte 互换） |
| 8 | SM | 安全模式使能 |
| 7:6 | PL | 优先级：00=Low, 01=Medium, 10=High, 11=Very High |
| 5 | TCIE | Buffer transfer complete 中断使能 |
| 4 | BTIE | Block transfer complete 中断使能 |
| 3 | BRTIE | Block repeat transfer complete 中断使能 |
| 2 | CTCIE | Channel transfer complete 中断使能 |
| 1 | TEIE | Transfer error 中断使能 |
| 0 | EN | 通道使能。硬件可自动清除（传输完成/错误）。EN=0 时等待当前 buffer 完成后停止，CTCIF 置位确认 |

### MDMA_CxTCR — 传输配置

| Bit | 名称 | 说明 |
|-----|------|------|
| 31 | BWM | 目的写操作是否 bufferable：0=non-bufferable, 1=bufferable。所有 MDMA 目的访问均为 non-cacheable |
| 30 | SWRM | 软件请求模式：0=响应硬件请求并 ACK, 1=忽略硬件请求，仅软件 SWRQ 触发 |
| 29:28 | TRGM | 触发模式：00=buffer, 01=block, 10=repeated block, 11=whole channel |
| 27:26 | PAM | Padding/alignment：00=右对齐补零, 01=右对齐符号扩展, 10=左对齐补零 |
| 25 | PKE | Pack 使能：1=源数据按目的宽度 pack/unpack，右对齐小端 |
| 24:18 | TLEN | Buffer 传输长度 = TLEN+1 字节。须为源/目的数据宽度的整数倍 |
| 17:15 | DBURST | 目的 burst：000=single, N=2^N beats。burst 须小于 TLEN+1 |
| 14:12 | SBURST | 源 burst：同上 |
| 11:10 | DINCOS | 目的增量步长：00=1B, 01=2B, 10=4B, 11=8B |
| 9:8 | SINCOS | 源增量步长：同上 |
| 7:6 | DSIZE | 目的数据宽度：00=8bit, 01=16bit, 10=32bit, 11=64bit |
| 5:4 | SSIZE | 源数据宽度：同上 |
| 3:2 | DINC | 目的增量模式：00=固定, 10=递增, 11=递减 |
| 1:0 | SINC | 源增量模式：同上 |

### MDMA_CxBNDTR — Block 数据量

| Bit | 名称 | 说明 |
|-----|------|------|
| 31:20 | BRC | Block repeat count（0~4095）。通道使能后只读，每完成一个 block 减 1 |
| 19 | BRDUM | Block repeat 目的地址更新方向：0=加 DUV, 1=减 DUV |
| 18 | BRSUM | Block repeat 源地址更新方向：0=加 SUV, 1=减 SUV |
| 16:0 | BNDT | Block 字节数（0~65536）。通道使能后只读递减。须为源/目的数据宽度的整数倍 |

### MDMA_CxBRUR — Block repeat 地址更新

| Bit | 名称 | 说明 |
|-----|------|------|
| 31:16 | DUV | 目的地址更新值。每次 block repeat 后加/减到 DAR。须为 DSIZE 的整数倍。BRC=0 时忽略 |
| 15:0 | SUV | 源地址更新值。同上，对应 SAR 和 SSIZE |

### MDMA_CxLAR — 链表地址

LAR[31:0]：当前 block/repeated block 完成后，从此地址加载新的通道描述符（CxTCR, CxBNDTR, CxSAR, CxDAR, CxBRUR, CxLAR, CxTBR, CxMAR, CxMDR）。

- LAR = 0：不加载，通道禁用，CTCIF 置位
- LAR ≠ 0：须在 AXI 地址空间，双字对齐（LAR[2:0] = 000）

### MDMA_CxTBR — 触发源选择

| Bit | 名称 | 说明 |
|-----|------|------|
| 5:0 | TSEL | 选择硬件触发输入（RQ）。ACK 输出使用相同索引值。SWRM=1 时忽略 |

若多个通道选择相同 TSEL 值，所有通道并行触发，但只有最低索引通道发送 ACK。

**注：手册 MDMA 章节未提供 TSEL 值到具体触发源的映射表。各外设章节分别说明其 MDMA 触发连接（如 QUADSPI 提到 "FIFO threshold trigger for MDMA" 和 "transfer complete trigger for MDMA"）。**

### MDMA_CxMAR / MDMA_CxMDR — Mask 地址和数据

当 ACK 信号产生时，MDMA 将 MDR 值写入 MAR 地址。用途：清除 DMA2 的中断标志寄存器以释放请求信号。

- MAR = 0：此功能禁用
- MAR ≠ 0：ACK 时自动写 MDR 到 MAR 地址
