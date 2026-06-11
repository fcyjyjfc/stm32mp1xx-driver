# DMA 控制器笔记

> RM0436 Chapter 18

## 架构概览 (pp.1186-1189)

- **DMA1、DMA2** 各 8 个 Stream
- 每个 Stream 由 **DMAMUX 输出通道**驱动
- DMAMUX 从 **116 个请求输入信号**中选择 DMA 请求源（Section 19.3）
- 两个 AHB 主端口：**Memory port** + **Peripheral port**（M→M 时两个端口都可访问存储器）
- 一个 AHB 从端口：编程接口（仅 32bit 访问）

## Stream FIFO (pp.1186-1187)

- 每个 Stream **4×32bit FIFO**

| 模式 | 行为 | 适用场景 |
|------|------|----------|
| 直接模式 (FIFO disabled) | 每次 DMA 请求立即发起单次传输；M→P 时预加载 1 个 PSIZE 大小的数据 | 单数据外设 (DAC/UART)，低延迟 |
| FIFO 模式 | 数据攒够阈值 (1/4, 1/2, 3/4) 再发起 burst 传输 | 高速外设，需要打包/拆包 |

## 传输方向与地址 (p.1190, Table 102)

| DIR[1:0] | 方向 | 源地址 | 目标地址 |
|-----------|------|--------|----------|
| 00 | P→M | PAR | M0AR |
| 01 | M→P | M0AR | PAR |
| 10 | M→M | PAR | M0AR |
| 11 | 保留 | - | - |

## 三种传输模式 (pp.1190-1193)

### P→M
EN=1 后，外设请求 → 读源 → 填 FIFO → 达阈值 → 排空到内存

### M→P
- FIFO 模式：EN=1 后预填满 FIFO → 外设请求 → 出 1 个 → FIFO 低于阈值补满
- **直接模式**：每次只预加载 **1 个** PSIZE 数据 → 外设请求一来就发 → 再补下一个

### M→M
无外设触发，EN=1 后自动进行。**不允许循环模式 + 直接模式**。

## 指针递增 (p.1193)

| 控制位 | 作用 |
|--------|------|
| MINC | 内存地址自动递增 |
| PINC | 外设地址自动递增，单寄存器访问固定地址时 = 0 |
| PINCOS | 外设地址强制 +4（与 PSIZE 无关），仅 Peripheral Port 生效 |

## 循环模式 (p.1194)

- CIRC=1，NDTR 自动重载，连续数据流
- Burst 模式下 NDTR 必须对齐 MBURST × (MSIZE/PSIZE)

## 双缓冲模式 (p.1194, Table 103)

- DBM=1，M0AR 和 M1AR 自动交替
- 自动开循环，不允许 M→M
- 运行时更新规则：CT=0 写 M1AR，CT=1 写 M0AR

## 数据位宽与打包/拆包 (pp.1195-1196)

- **PSIZE**：Peripheral port 数据宽度（AHB 外设端口每次传输的数据大小，8/16/32-bit）
- **MSIZE**：Memory port 数据宽度（AHB 存储器端口每次传输的数据大小，8/16/32-bit）
- **仅 FIFO 模式支持** PSIZE ≠ MSIZE 自动打包/拆包
- **直接模式 (DMDIS=0)**：不允许源和目标宽度不同，**两者都等于 PSIZE，MSIZE 位无意义**
- NDT 以 PSIZE 为单位，总字节数 = NDT × PSIZE
- 仅支持**小端**

### 打包字节通道映射 (Table 104)

- PSIZE=MSIZE 时 1:1 直通
- PSIZE < MSIZE 时打包（如 8bit × 4 → 1 × 32bit）
- PSIZE > MSIZE 时拆包（如 32bit → 4 × 8bit）
- PINCOS=1 时 P 端口地址始终 +4；PINCOS=0 时按 PSIZE 步进

### NDT 对齐约束 (Table 105)

| PSIZE | MSIZE | NDT 要求 |
|-------|-------|---------|
| 8-bit | 16-bit | 2 的倍数 |
| 8-bit | 32-bit | 4 的倍数 |
| 16-bit | 32-bit | 2 的倍数 |

## 单次与 Burst 传输 (p.1196)

- MBURST 和 PBURST 独立配置，Burst 大小指**拍数**（不是字节数）
- Burst 传输**不可分割**：AHB 锁住，仲裁器中途不释放总线
- **直接模式下强制单次传输**，MBURST/PBURST 被硬件忽略
- Burst 不能跨越 **1KB 地址边界**，否则 AHB 错误且 DMA 寄存器不报告
- 每次 DMA 请求发起的传输量：单次 = 1 拍，Burst = 4/8/16 拍
- 末尾数据不足一个 Burst 时自动降级为单次传输

## FIFO 结构与阈值 (pp.1197-1200)

- 每 Stream 独立 4-word FIFO，阈值 1/4、1/2、3/4、full
- DMDIS=1 开启 FIFO 阈值，DMDIS=0 即直接模式
- FIFO 内部按 byte lane 组织，位宽不同时排布不同（图 133）

### FIFO 阈值 vs Burst 约束 (Table 106)

**FIFO 阈值必须容纳整数个 Memory Burst**，否则使能时 FEIF 错误且自动停流。

禁止组合举例：
- MSIZE=Byte, FTH=1/4, MBURST=INCR8
- MSIZE=Half-word, FTH=1/4, 任何 Burst
- MSIZE=Word, 仅 FTH=Full 允许 Burst

### FIFO 冲刷 (Flush)

- EN=0 关流时，FIFO 剩余数据继续传到目标，完后置 TCIF
- 坑：剩余数据 < MSIZE 宽度时，以 MSIZE 宽度强写 → **脏数据写入内存**
- 软件需用 NDTR 判断有效数据范围

### 直接模式约束总结

1. 源和目标宽度必须相等（都 = PSIZE，**MSIZE 无意义**）
2. **Burst 不可能**（PBURST/MBURST 被忽略）
3. **禁止 M→M**

## 传输完成 (pp.1200-1201, 18.3.14)

**DMA 流控模式：**
- NDTR=0 时 TCIF 置位
- P→M 方向：FIFO 剩余数据全部刷入内存后才算完成
- M→P 方向：无 FIFO 排空等待

**外设流控模式：**
- 外设发最后请求 + FIFO 排空（P→M 方向）
- 软件关 EN + FIFO 排空

**非循环模式**：传输完成后硬件自动清 EN，软件需重配才能再启。

## 传输暂停 (p.1201, 18.3.15)

**永久停流**：清 EN=0 → 当前传输完成后停 → TCIF 置位 → NDTR 保留剩余数

**暂停→恢复**：
1. 清 EN=0，确认 EN=0
2. 读 NDTR 得剩余数
3. 按已传数量更新 M0AR/PAR 地址
4. 写 NDTR 为剩余数
5. 重设 EN=1 续传

## 流控制器 (pp.1201-1202, 18.3.16)

| 流控方 | NDTR | 结束条件 |
|--------|------|---------|
| **DMA** (PFCTRL=0) | 软件写 1~65535 | NDTR 减到 0 |
| **外设** (PFCTRL=1) | 硬件强制 **0xFFFF** | 外设发最后一笔硬件信号 |

**外设流控要点：**
- NDTR 写无效，使能后强制 0xFFFF
- 已传数量 = **0xFFFF - NDTR**
- NDTR=0 也会强制停流（最大 65535 笔）
- **循环模式 + 外设流控 = 禁止**
- **M→M 始终是 DMA 流控**（PFCTRL 被硬件强制 0）

## 可能配置总结 (p.1203, Table 107)

| 方向 | 流控 | 循环 | Burst | 直接模式 | 双缓冲 |
|------|------|------|-------|---------|--------|
| P→M | DMA | √ | √ (禁止直接) | √ (禁止 Burst) | √ (禁止 Burst) |
| P→M | 外设 | 禁止 | - | - | - |
| M→P | DMA | √ | √ (禁止直接) | √ (禁止 Burst) | √ (禁止 Burst) |
| M→P | 外设 | 禁止 | - | - | - |
| M→M | DMA only | 禁止 | √ | 禁止 | 禁止 |

## Stream 配置流程 (p.1203, 18.3.18)

1. 确认 **EN=0**（如已使能则先清 EN，**等 EN 读到 0**，清之前所有状态标志）
2. 设 **PAR** — 外设端口地址
3. 设 **M0AR** — 内存地址（双缓冲还要设 M1AR）
4. 设 **NDTR** — 传输数量
5. 配置 **DMAMUX** — 路由请求线到 DMA 通道
6. 如需外设流控，设 **PFCTRL=1**
7. 设 **PL[1:0]** — 优先级
8. 配置 **FIFO**（DMDIS、FTH）
9. 配置 **CR** — 方向、递增、Burst、位宽、循环、双缓冲、中断
10. 设 **EN=1** 启动

> ⚠ **关外设前必须先关 DMA Stream（等 EN=0），否则出问题。**

## 错误管理 (pp.1204-1205, 18.3.19)

| 错误 | 标志 | 触发条件 | 停流 |
|------|------|---------|------|
| 传输错误 | TEIF | AHB 总线错误 / 双缓冲写错 MxAR | **自动停** |
| FIFO 错误 | FEIF | underrun/overrun / FIFO 阈值与 Burst 不兼容 | 仅 Burst 不兼容时**自动停** |
| 直接模式错误 | DMEIF | P→M + 直接模式 + MINC=0，新请求来时上次还没写完 | **不停** |

- TEIF / FEIF(Burst 不兼容) → 硬件自动清 EN，停流
- FEIF(overrun/underrun) / DMEIF → 不停，软件决定。DMA 不确认外设请求直到条件清除，不丢数据
- 但外设方可能因等待 DMA Ack 太久自己丢数据

## DMA 中断 (p.1205, 18.4, Table 108)

| 事件 | 标志 | 使能位 |
|------|------|--------|
| 半传输 | HTIF | HTIE |
| 传输完成 | TCIF | TCIE |
| 传输错误 | TEIF | TEIE |
| FIFO 错误 | FEIF | FEIE |
| 直接模式错误 | DMEIF | DMEIE |

> **设 EN=1 前必须先清对应事件标志**，否则立即触发中断。

## 寄存器 (pp.1206-1218, 18.5)

> DMA 寄存器必须按 **32 位字**访问。

### 全局寄存器

| 偏移 | 寄存器 | 说明 |
|------|--------|------|
| 0x000 | DMA_LISR | 低中断状态 (Stream 0~3) |
| 0x004 | DMA_HISR | 高中断状态 (Stream 4~7) |
| 0x008 | DMA_LIFCR | 低中断清标志 (写 1 清) |
| 0x00C | DMA_HIFCR | 高中断清标志 (写 1 清) |
| 0x3EC | DMA_HWCFGR2 | FIFO_SIZE=4-word, CHSEL_WIDTH=0 |
| 0x3F0 | DMA_HWCFGR1 | 各 Stream 类型 (Regular/DBM) |
| 0x3F4 | DMA_VERR | 版本 1.4 |
| 0x3F8 | DMA_IPIDR | 外设 ID 0x00100002 |
| 0x3FC | DMA_SIDR | 大小 ID 0xA3C5DD01 (1KB) |

### Stream 寄存器块 (每 Stream 间距 0x18)

对 Stream x (x=0~7)，偏移 = 0x010 + 0x18 * x：

| Sx 偏移 | 寄存器 | 说明 |
|----------|--------|------|
| +0x000 | DMA_SxCR | Stream 配置（见下） |
| +0x004 | DMA_SxNDTR | 传输数量 0~65535 |
| +0x008 | DMA_SxPAR | 外设地址 |
| +0x00C | DMA_SxM0AR | 内存 0 地址 |
| +0x010 | DMA_SxM1AR | 内存 1 地址（双缓冲） |
| +0x014 | DMA_SxFCR | FIFO 控制 |

例：S2 的 CR 在 0x010 + 0x18×2 = 0x040，NDTR 在 0x044，依此类推。

### DMA_SxCR 位定义

| 位 | 名称 | 说明 | 写保护 |
|----|------|------|--------|
| 0 | EN | 使能。读 0 才可配其余位。硬件清 0：传输完成 / AHB 错误 / FIFO 阈值与 Burst 不兼容 | - |
| 1 | DMEIE | 直接模式错误中断使能 | 随时 |
| 2 | TEIE | 传输错误中断使能 | 随时 |
| 3 | HTIE | 半传输中断使能 | 随时 |
| 4 | TCIE | 传输完成中断使能 | 随时 |
| 5 | PFCTRL | 外设流控，M→M 时硬件强制 0 | **仅 EN=0** |
| 7:6 | DIR[1:0] | 00=P→M 01=M→P 10=M→M | **仅 EN=0** |
| 8 | CIRC | 循环模式。DBM=1 时强制 1；PFCTRL=1 时强制 0 | 随时（硬件可清） |
| 9 | PINC | 外设地址递增 | **仅 EN=0** |
| 10 | MINC | 内存地址递增 | **仅 EN=0** |
| 12:11 | PSIZE[1:0] | 外设数据宽度 00=8bit 01=16bit 10=32bit | **仅 EN=0** |
| 14:13 | MSIZE[1:0] | 内存数据宽度。**直接模式下 EN=1 时硬件强制=PSIZE** | **仅 EN=0** |
| 15 | PINCOS | 外设递增偏差固定+4（PINC=0 时无意义）。**直接模式或 PBURST≠00 时 EN=1 后硬件强制 0** | **仅 EN=0** |
| 17:16 | PL[1:0] | 优先级 00=Low 01=Med 10=High 11=VeryH | **仅 EN=0** |
| 18 | DBM | 双缓冲模式 | **仅 EN=0** |
| 19 | CT | 当前目标（双缓冲，硬件自动翻转）。EN=0 时软件写首次目标 | **仅 EN=0** |
| 20 | TRBUFF | 缓冲传输使能（UART 必须=1） | 随时 |
| 22:21 | PBURST[1:0] | 外设 Burst 00=单次 01=INCR4 10=INCR8 11=INCR16。**直接模式下 EN=1 后硬件强制 00** | **仅 EN=0** |
| 24:23 | MBURST[1:0] | 内存 Burst。**直接模式下 EN=1 后硬件强制 00** | **仅 EN=0** |
| 31:25 | - | 保留 | - |

> **注意：DMA_SxCR 无 CHSEL 字段。通道选择完全在 DMAMUX 中完成。**

NDTR: 使能后只读，传输后递减。循环模式自动重载。NDTR=0 时即使 EN=1 也不服务请求。

### DMA_SxFCR 位定义

| 位 | 名称 | 说明 | 写保护 |
|----|------|------|--------|
| 1:0 | FTH[1:0] | FIFO 阈值 00=1/4 01=1/2 10=3/4 11=Full。直接模式 (DMDIS=0) 时不用 | **仅 EN=0** |
| 2 | DMDIS | 0=直接模式 1=禁用直接(即 FIFO 模式)。M→M 时硬件强制 1 | **仅 EN=0** |
| 5:3 | FS[2:0] | FIFO 状态 **只读** 000~101 | 只读 |
| 7 | FEIE | FIFO 错误中断使能 | 随时 |

**FS[2:0] 状态编码：** 000=0~1/4 001=1/4~1/2 010=1/2~3/4 011=3/4~满 100=空 101=满

### PAR / M0AR / M1AR 写保护

| 寄存器 | 写入条件 | 双缓冲例外 |
|--------|---------|-----------|
| PAR | **仅 EN=0** | - |
| M0AR | EN=0 | EN=1 且 CT=1 时也可写 |
| M1AR | EN=0 | EN=1 且 CT=0 时也可写 |
| NDTR | **仅 EN=0** | - |

### DMA_HWCFGR2 (0x3EC)

- **FIFO_SIZE[1:0]=01** → 4-word FIFO（所有 Stream 共用）
- **CHSEL_WIDTH[2:0]=0** → DMA_SxCR 中无可编程通道选择字段（CHSEL 全在 DMAMUX）

## DMAMUX (Chapter 19, pp.1223-1232)

### 作用

外设的 DMA 请求先到 DMAMUX，DMAMUX 决定发给哪个 DMA Stream。相当于**路由矩阵**：

```
外设 (108 条 DMA 请求线) → DMAMUX (16 通道) → DMA1/DMA2 的 16 个 Stream
```

### 主要特性

| 特性 | 数量 |
|------|------|
| DMAMUX 输出请求通道数 | 16（通道 0~7→DMA1，通道 8~15→DMA2） |
| 请求发生器通道数 | 8 |
| 触发输入数 | 8 |
| 同步输入数 | 8 |
| 外设请求输入数 | 108 |

### 三种工作模式

1. **直通路由**：DMAMUX 通道选择某个外设请求 ID，DMA 请求直接转发给对应的 DMA Stream
2. **同步路由**：DMA 请求要等某个同步事件（如 extit0）来了才转发，可配极性、请求计数
3. **请求发生器**：用触发输入（如定时器）自动产生 DMA 请求，配计数器可控制每次产生多少笔

### 表 111 — 请求输入映射（共 127 个）

| 输入 | 外设 | 输入 | 外设 | 输入 | 外设 |
|------|------|------|------|------|------|
| 1~8 | dmamux_req_gen0~7 | 44~46 | USART2_TX / USART3 | 87~90 | SAI1/2 / DFSDM / SPDIFRX |
| 9~10 | ADC1 / ADC2 | 47~53 | TIM8_CH1~4/UP/TRIG/COM | 94~96 | SAI4 / DFSDM_FLT0~3 |
| 11~17 | TIM1_CH1~4/UP/TRIG/COM | 55~60 | TIM5_CH1~4/UP/TRIG | 97~98 | TIM15_CH1/UP/TRIG/COM |
| 18~22 | TIM2_CH1~4/UP | 61~62 | SPI3_RX/TX | 99~100 | TIM16_CH1/UP / TIM17_CH1/UP |
| 23~28 | TIM3_CH1~4/UP/TRIG | 63~66 | UART4_RX/TX / UART5 | 101~102 | SAI3 / I2C5 |
| 29~32 | TIM4_CH1~3/UP | **67~68** | **DAC1 / DAC2** | 103~105 | 保留 |
| 33~36 | I2C1/2 | 69~70 | TIM6_UP / TIM7_UP | 106~127 | - |
| 37~42 | SPI1/2 | 71~78 | USART6 / I2C3 / DCMI / CRYP / HASH | - | - |
| 43 | USART2_RX | 79~86 | UART7/8 / SPI4/5 | - | - |

### 表 112/113 — 触发输入与同步输入映射

| 输入 | 来源 |
|------|------|
| 0~2 | dmamux_evt0~2（内部事件） |
| 3 | LPTIMER1_OUT |
| 5 | LPTIMER2_OUT |
| 6 | LPTIMER3_OUT |
| 7 | extit0 |

### 对驱动的影响

- **DMA_SxCR 无 CHSEL 字段**，通道选择完全通过 DMAMUX 完成
- DmaMuxCfg_t 的 `dma_mux_req` 填表 111 中对应外设的输入编号
- **禁止**不同通道配相同非零 DMAREQ_ID（除非保证不同时活跃），否则一个外设请求被两个 Stream 同时服务

### 框图与信号 (pp.1227-1228, 19.4.1-19.4.2)

两个子模块：

```
外设请求 (dmamux_req_inx) ──┐
                             ├──→ Request Multiplexer (通道0~m) ──→ dmamux_req_outx (到DMA)
请求发生器 (dmamux_req_genx) ┘                                  ──→ dmamux_evtx (事件)

触发输入 (dmamux_trgx) ──→ Request Generator (通道0~n)
同步输入 (dmamux_syncx) ──→ 同步控制（在 Multiplexer 内部）
中断输出: dmamux_ovr_it (溢出中断)
```

| 信号 | 说明 |
|------|------|
| dmamux_hclk | AHB 时钟 |
| dmamux_req_inx | 外设 DMA 请求输入 |
| dmamux_trgx | 触发输入（给请求发生器） |
| dmamux_req_genx | 请求发生器输出 |
| dmamux_reqx | 多路复用器输入（外设请求 + 发生器输出） |
| dmamux_syncx | 同步输入（给多路复用器通道） |
| dmamux_req_outx | 输出到 DMA 控制器 |
| dmamux_evtx | 事件输出 |
| dmamux_ovr_it | 溢出中断 |

### 通道配置流程 (p.1228, 19.4.3)

1. **先配 DMA**：完整配置 DMA 通道 y，**但不使能（EN=0）**
2. **再配 DMAMUX**：配置 DMAMUX 通道 y（DMAREQ_ID 等）
3. **最后使能 DMA**：置 EN=1 启动

### 请求线多路复用器 (pp.1228-1229, 19.4.4)

- 每条 DMA 请求线并联到所有 DMAMUX 通道
- **DMAMUX_CxCR.DMAREQ_ID** 选择请求线编号（Table 111），0 = 不选
- 不论请求来自外设还是请求发生器，对多路复用器而言都是统一的输入

### 同步模式 (pp.1229-1230)

同步器相当于一个开关：同步沿到来→闭合→放行 NBREQ+1 个请求→计数减到 0→断开。

| 字段 | 作用 |
|------|------|
| SE | 同步使能 |
| SYNC_ID | 选择同步输入源 |
| SPOL[1:0] | 极性（上升/下降/双边沿） |
| NBREQ | 每次同步后放行 **NBREQ+1** 个请求 |
| EGE | 事件发生使能，计数减到 0 时产生 1 个 AHB 周期的事件脉冲 |

- SE=0 + EGE=1：无门控，请求直接通过，但仍计数并产生事件
- EGE=1 + NBREQ=0：每个请求都产生事件
- 同步来了但没有挂起的请求 → **丢弃**（不是溢出）
- **NBREQ 只能在 SE=0 且 EGE=0 时写入**
- 同步/触发边沿检测需稳定 >2 个 AHB 周期
- 写入 DMAMUX_CxCR 后同步事件被屏蔽 3 个 AHB 周期

### 同步溢出与中断 (p.1231, 19.4.5)

| 条件 | 结果 |
|------|------|
| 上一轮 NBREQ+1 还没服务完，新同步沿来了 | **SOFx** 置位（DMAMUX_CSR） |
| DMA 用完后忘关 SE | 下次同步来→无 ACK→溢出 |

- 清除：写 **CSOFx**（DMAMUX_CFR）
- 中断：**SOIE=1** 时触发 dmamux_ovr_it

### 请求发生器 (pp.1231-1232)

用触发输入**凭空产生** DMA 请求，无需外设参与。

| 字段 | 作用 |
|------|------|
| GE | 使能发生器通道 x |
| SIG_ID | 选择触发输入源（Table 112/113） |
| GPOL | 触发极性（上升/下降/双边沿） |
| GNBREQ | 每次触发后产生 **GNBREQ+1** 个请求 |

- 触发沿到来 → 产生请求 → 每服务一个减 1 → 减到 0 停止，等下次触发重载
- **GNBREQ 只能在 GE=0 时写入**（无硬件写保护）
- 写入 DMAMUX_RGxCR 后触发事件屏蔽 3 个 AHB 周期

### 触发溢出

| 条件 | 结果 |
|------|------|
| 上一轮 GNBREQ+1 还没服务完，新触发来了 | **OFx** 置位（DMAMUX_RGSR） |
| DMA 用完后忘关 GE | 下次触发来→无 ACK→溢出 |

- 清除：写 **COFx**（DMAMUX_RGCFR）
- 中断：**OIE=1** 时触发 dmamux_ovr_it

### DMAMUX 中断 (p.1232, 19.5, Table 115)

两种溢出共用一根中断线 **dmamux_ovr_it**：

| 中断源 | 标志 | 清除 | 使能 |
|--------|------|------|------|
| 多路复用器通道 x 同步溢出 | SOFx | CSOFx | SOIE |
| 请求发生器通道 x 触发溢出 | OFx | COFx | OIE |
