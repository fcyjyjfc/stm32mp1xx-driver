# CAN / FDCAN 笔记

## 59.4 FDCAN functional description

### Figure 721. FDCAN block diagram

- **CAN core** — 协议控制器 + RX/TX 移位寄存器，处理 ISO 11898-1:2015 协议，支持 11-bit 和 29-bit ID
- **Sync** — 同步 APB 时钟域和 CAN 内核时钟域
- **Interrupts Interface** — 双中断线 (fdcan_intr0_it / fdcan_intr1_it)，通过 FDCAN_ILE.EINT0/EINT1 分别使能
- **Tx Handler** — 消息 RAM → CAN core 的传输控制
- **Rx Handler** — CAN core → 消息 RAM 的接收控制
- **Message RAM Interface** — 通过 RAM controller/arbiter 连接 10KB 外部消息 RAM

### 59.4.1 Tx handler

- 最多 32 个 Tx buffers，可用作 dedicated Tx buffers / Tx FIFO / Tx queue 或其组合
- Tx event FIFO：存储发送时间戳 + 对应消息 ID
- 支持 Transmit cancellation
- FDCAN1 额外包含 FSE (frame synchronization entity)，控制 TTCAN 的时间触发通信

### Rx handler

- 2 个 Rx FIFO，大小可配置
- 最多 64 个 dedicated Rx buffers（只存特定 ID 的消息）
- 最多 128 个 11-bit ID 过滤器 / 64 个 29-bit ID 过滤器
- 每条消息存储 Rx timestamp

## Operating modes

### Software initialization

1. 硬件复位或 Bus_Off → FDCAN_CCCR.INIT 自动置 1，停止总线通信，FDCAN_TX 输出隐性 (high)
2. INIT=1 且 CCE=1 才能访问配置寄存器
   - CCE 只能在 INIT=1 时修改，清 INIT 时 CCE 自动清零
3. 配置完成 → 清 INIT → BSP 等待 11 个连续隐性位 (Bus_Idle) 后同步总线，开始通信

CCE 置位时以下寄存器自动复位：
- FDCAN_HPMS / RXF0S / RXF1S / TXFQS / TXBRP / TXBTO / TXBCF / TXEFS
- FDCAN_TTOST / TTLGT / TTCTC / TTCSM (TTCAN 相关，仅 FDCAN1)

CCE 置位时 TX handler 和 Rx handler 的状态机保持在 idle。

其他限制：
- FDCAN_TXBAR (Tx buffer add request) 和 FDCAN_TXBCR (Tx buffer cancellation request) 只能在 CCE=0 时写入
- CCCR.TEST / CCCR.MON / CCCR.DAR 只能在 INIT=1 且 CCE=1 时设置

### Normal operation

FDCAN1 默认模式为 event-driven CAN communication without time triggers (FDCAN_TTOCF.OM = 00)。
清 INIT 后，FDCAN 同步到 CAN 总线，进入通信状态。
接收消息通过 acceptance filter 后存入 dedicated Rx buffer / Rx FIFO 0 / Rx FIFO 1。

### CAN FD operation

两种变体：
- **Long Frame Mode (LFM)** — 数据段超过 8 字节
- **Fast Frame Mode (FFM)** — 数据段以更高比特率传输，可与 LFM 组合

FDF 位：
- FDF recessive = CAN FD 帧，FDF dominant = 经典 CAN 帧
- BRS recessive = 数据段切换高速率
- FDF + res recessive → PXE 协议异常事件 (FDCAN_PSR.PXE)

使能：FDCAN_CCCR.FDOE = 1，可同时收发经典 CAN 帧和 CAN FD 帧
- FDOE=0 时收到 CAN FD 帧 → error frame
- FDOE=1 / BRSE=0 仅评估 FDF 位
- FDOE=1 / BRSE=1 使能 CAN FD bitrate switching

位时序：
- 仲裁段：FDCAN_NBTP (Nominal Bit Timing and Prescaler)
- 数据段：FDCAN_DBTP (Data Bit Timing and Prescaler)

DLC 编码 (Table 448. DLC coding in FDCAN)：

| DLC | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|-----|---|---|---|---|---|---|---|
| 字节数 | 12 | 16 | 20 | 24 | 32 | 48 | 64 |

DLC 0~8 与经典 CAN 相同。

ESI (Error Status Indicator)：
- 发送器 error passive → ESI recessive，否则 dominant
- CAN FD remote frame 中 ESI 始终 dominant

Tx buffer 配置 DLC > 8 时：前 8 字节按配置发送，后续填充 0xCC。
接收 DLC > 8 时：只存前 8 字节，剩余丢弃。

### Transceiver delay compensation

CAN FD 数据阶段只有单节点发送，总线长度无影响。
TX→CAN 收发器→RX 环路延迟若超过 TSEG1 → 误报 bit error。
延迟补偿通过 DBTP.TDC 位使能，在 Secondary Sample Point (SSP) 比较发送/接收数据以检测 bit error。
仅在 CAN FD 数据阶段有效，仲裁阶段始终禁用。

SSP 位置 = 测量到的 FDCAN_TX→收发器→FDCAN_RX 延迟 + FDCAN_TDCR.TDCO 偏移。
SSP 位置向下取整到最接近的整数 mtq (minimum time quanta，即 fdcan_tq_ck 一个周期)。

边界条件：
- 测量延迟 + TDCO 必须小于 6 个数据阶段 bit time
- 测量延迟 + TDCO 必须 ≤ 127 mtq，超过则取最大值 127 mtq
- 数据阶段在 CRC delimiter 的采样点结束，停止在 SSP 检查接收位

测量过程：使能 TDC 后，在每个 CAN FD 帧的 FDF→res 下降沿开始测量，在发送端 FDCAN_RX 看到该边沿时停止，分辨率 1 mtq。

延迟补偿滤波窗口 (FDCAN_TDCR.TDCF)：防止 FDF 位内的 dominant glitch 提前结束测量导致 SSP 过早。
- FDCAN_RX 上的 dominant 边沿若导致 SSP 更早则被忽略
- 仅当 SSP 位置 ≥ TDCF 且 FDCAN_RX 为低时测量才停止

### Restricted operation mode

- 节点可接收数据/remote 帧，可应答有效帧
- **不发送**：数据帧、remote 帧、active error 帧、overload 帧
- 错误/过载时不发送 dominant 位，等待 bus idle 重新同步
- 错误计数器 FDCAN_ECR.REC/TEC 冻结，FDCAN_ECR.CEL 仍活动
- 设置：FDCAN_CCCR.ASM = 1 (需 CCE=1 且 INIT=1)
- 清除：软件可随时清零 ASM
- 自动进入：Tx handler 未能及时从消息 RAM 读取数据时
- 可用于 CAN 比特率检测：测试不同比特率，收到有效帧后退出
- 不可与 loop back mode (internal/external) 组合使用

### Bus monitoring mode

- 设置：FDCAN_CCCR.MON = 1，或进入 error level S3 (FDCAN_TTOST.EL = 11)
- 可接收有效数据帧和 remote 帧，**不能发起传输**
- 仅发送隐性位，若需发送 dominant 位 (ACK、overload flag、active error flag) 则内部回环，不影响总线
- FDCAN_TXBRP 保持在复位状态
- 用于分析 CAN 总线流量而不影响通信 (Figure 723)

### Disabled automatic retransmission (DAR) mode

- 默认自动重传使能 (CAN 规范，仲裁丢失或错误干扰自动重传)
- FDCAN_CCCR.DAR = 1 禁用自动重传，用于时间触发通信 (ISO 11898-1:2015, chapter 9.2)
- DAR 模式下所有传输在开始后自动取消：
  - **成功传输**：TXBTO.TOx=1，TXBCF.CFx=0
  - **成功传输但被取消**：TXBTO.TOx=1，TXBCF.CFx=1
  - **仲裁丢失或错误**：TXBTO.TOx=0，TXBCF.CFx=1
  - 成功传输且 Tx event 使能时写入 ET=10 (transmission in spite of cancellation)

### Power down (Sleep mode)

- 控制：FDCAN_CCCR.CSR (clock stop request)，读回 1 表示请求激活
- 流程：
  1. 所有待发传输完成后等待 bus idle
  2. 置 INIT=1 阻止后续传输
  3. 置 CSA=1 确认就绪，此时可关闭时钟
  4. 写 INIT 无效
- 唤醒：开启时钟 → 清 CSR → FDCAN 清 CSA → 清 INIT 恢复通信

### Test modes

- 使能：FDCAN_CCCR.TEST = 1 才可写 FDCAN_TEST 寄存器
- FDCAN_TEST.TX 选择 FDCAN_TX 输出功能：串行数据输出 / CAN Sample Point 信号 / 恒定 dominant / 恒定 recessive
- FDCAN_TEST.RX 读取 FDCAN_RX 引脚实际电平
- 可用于检查 CAN 总线物理层
- 注意：CAN 内核与 APB 时钟域同步会导致写入 TEST.TX 到引脚可见有若干 APB 时钟周期延迟
- 仅用于生产测试或自检，不推荐应用中使用

**External loop back mode** (FDCAN_TEST.LBCK=1)：
- 发送的消息视为接收消息，通过验收过滤后存入 Rx FIFO
- 忽略 acknowledge error (ACK 槽采样到隐性位)
- 内部从 TX 输出回环到 RX 输入，无视 FDCAN_RX 引脚实际值
- 发送消息可在 FDCAN_TX 引脚上监测 (Figure 724 左)

**Internal loop back mode** (FDCAN_TEST.LBCK=1 + FDCAN_CCCR.MON=1)：
- 用于 "Hot Selftest"，不影响运行中的 CAN 系统
- FDCAN_RX 引脚断开，FDCAN_TX 引脚保持隐性 (Figure 724 右)

### Application watchdog (FDCAN1 only)

- 通过读取 FDCAN_TTOST 喂狗
- 超时 → FDCAN_TTOST.AWE=1，停止所有 TTCAN 通信，进入 bus monitoring 模式
- 禁用：FDCAN_TTOCF.AWL=0x00 (TTCAN 应用中不应禁用)

### Timestamp generation

- 16-bit 回环计数器
- 预分频 FDCAN_TSCC.TCP：1~16 CAN bit time
- 计数器值读取：FDCAN_TSCV.TCV
- 写 FDCAN_TSCV 复位计数器为 0
- 回环时置中断标志 FDCAN_IR.TSW
- 帧接收/发送开始时捕获计数值存入 Rx buffer/Rx FIFO (RXTS[15:0]) 或 Tx event FIFO (TXTS[15:0])
- FDCAN_TSCC.TSS 选择 16-bit 时间戳

### Timeout counter

- 16-bit 递减计数器
- 使用与 timestamp 相同的预分频 (FDCAN_TSCC.TCP)
- 配置：FDCAN_TOCC
- 计数值读取：FDCAN_TOCV.TOC
- 仅 INIT=0 时运行，INIT=1 时停止 (如 Bus_Off)
- 操作模式由 FDCAN_TOCC.TOS 选择：
  - **Continuous 模式**：清 INIT 时启动，写 FDCAN_TOCV 预设为 FDCAN_TOCC.TOP 后开始递减
  - **FIFO 控制模式**：空 FIFO 时预设为 TOP，存入第一个 FIFO 元素时开始递减，写 FDCAN_TOCV 无效
- 计数到 0 置中断标志 FDCAN_IR.TOO，Continuous 模式下立即从 TOP 重新开始
- 时钟源来自 CAN core sample point 信号，因此递减时刻会受 CAN 同步/再同步机制影响
- 使用 CAN FD 比特率切换时，仲裁段和数据段的超时计数速率不同

### Error management (Figure 725)

错误处理完全由硬件管理，使用两个计数器：
- **TEC** (Transmit Error Counter) — 发送错误 +8，发送成功 -1
- **REC** (Receive Error Counter) — 接收错误 +8，接收成功 -1

节点状态分三级 (Figure 725)：

```
ERROR ACTIVE ──── TEC 或 REC > 127 ────→ ERROR PASSIVE
    ↑                                            │
    └──── TEC 和 REC 都 < 128 ───────────────────┘
                                                 │
                                            TEC > 255
                                                 ↓
                                            BUS OFF
```

- **Error Active** — 正常，检测到错误发 active error flag (6 个 dominant 位)
- **Error Passive** — 只发 passive error flag (6 个 recessive 位)，不干扰总线
- **Bus Off** — TEC > 255，完全断开不收不发

Bus-Off 恢复：
- 需要检测到 **128 次 × 11 个连续隐性位** (128 occurrences of 11 consecutive recessive bits on CAN RX)
- PSR.BO 指示 Bus-Off 状态
- 初始化模式下 CAN 不监视 RX 信号，**无法完成恢复序列**，必须回到 normal mode

### 59.4.3 Message RAM (Figure 726)

FDCAN 通过 RAM controller/arbiter 连接外部消息 RAM，按 32-bit word 寻址，最多配置 2560 words。

| 区域 | 元素数 | 最大占用 |
|---|---|---|
| **11-bit filter** (SIDFC.FLSSA) | 0~128 个 | 0~128 words |
| **29-bit filter** (XIDFC.FLESA) | 0~64 个 | 0~128 words |
| **Rx FIFO 0** (RXF0C.F0SA) | 0~64 个 | 0~1152 words |
| **Rx FIFO 1** (RXF1C.F1SA) | 0~64 个 | 0~1152 words |
| **Rx buffer** (RXBC.RBSA) | 0~64 个 | 0~1152 words |
| **Tx event FIFO** (TXEFC.EFSA) | 0~32 个 | 0~64 words |
| **Tx buffers** (TXBC.TBSA) | 0~32 个 | 0~576 words |
| **Trigger memory** (TMC.TMSA) | 0~64 个 | 0~128 words |

- 各区域可不全部配置，也无顺序要求
- 起始地址是 32-bit word 地址，只取 bit[15:2]，低 2 位忽略
- **FDCAN 不检查配错**，区域重叠会导致数据错乱或丢数据

### Acceptance filter

Rx handler 控制接收过滤，将接收到的消息存入 Rx buffer / Rx FIFO 0 / Rx FIFO 1。

配置寄存器：
- FDCAN_GFC (Global Filter Configuration)
- FDCAN_SIDFC (Standard ID Filter Configuration)
- FDCAN_XIDFC (Extended ID Filter Configuration)
- FDCAN_XIDAM (Extended ID AND Mask)

过滤流程：
1. 从 filter 表第 0 个元素开始**顺序匹配**
2. 匹配到第一个即停止，后续元素不再检查
3. 过滤在完整 ID 接收后开始
4. 若匹配到 Rx buffer/FIFO，以 32-bit 为单位写入数据
5. 若之后检测到错误 (如 CRC 错误)，该消息丢弃：
   - Rx buffer：新数据标志不置位，但 buffer 已被部分覆盖
   - Rx FIFO：Put index 不更新，但 FIFO 已被部分覆盖
   - 错误类型查 FDCAN_PSR.LEC / DLEC

过滤匹配后动作 (由 SFEC/EFEC 配置)：
- 存入 FIFO 0 或 FIFO 1
- 存入 Rx buffer
- 存入 Rx buffer 并在 filter event pin 产生脉冲
- **拒绝接收** (丢弃)
- 置高优先级消息中断 FDCAN_IR.HPM
- 置 HPM 并存入 FIFO 0 或 FIFO 1

#### 三种过滤方式

**① Range filter (范围过滤)**
- 消息 ID 落在 [SF1ID, SF2ID] 或 [EF1ID, EF2ID] 范围内即匹配
- 扩展帧有两种模式：
  - EFT=00：接收 ID 先与 FDCAN_XIDAM 做 AND，再应用 range filter
  - EFT=11：不经过 XIDAM，直接范围比较

**② Dedicated ID filter (专用 ID 过滤)**
- 匹配一个或两个特定 ID
- SF1ID == SF2ID 即为匹配单个 ID

**③ Classic bit mask filter (位掩码过滤, Figure 中为经典掩码)**
- SF1ID/EF1ID = 目标 ID，SF2ID/EF2ID = 掩码
- 掩码位=1 → 这一位必须匹配，掩码位=0 → 这一位不关心
- 掩码全 1 → 精确匹配一个 ID
- 掩码全 0 → 所有 ID 都匹配

#### 标准 ID 过滤流程 (Figure 727)

收到 11-bit ID 帧后的处理路径：

```
收到有效帧 → 判断 remote frame?
             是 → GFC.RRFS=0 → 拒绝
             否 → GFC.RRFS=1 → 继续
             
             SIDFC.LSS=0 (无过滤器) → GFC.ANFS 决定接受或拒绝
             SIDFC.LSS>0 → filter #0 开始顺序匹配
                            → 匹配 → 目标 FIFO 满?
                                      是 → 丢弃，置 IR.RFnL
                                      否 → 存入 FIFO
                            → 不匹配 → 继续匹配下一个
                                       → 全不匹配 → GFC.ANFS 决定
```

GFC.ANFS[1]=1 → 接受非匹配帧，GFC.ANFS[1]=0 → 拒绝非匹配帧。

#### 扩展 ID 过滤流程 (Figure 728)

29-bit 帧流程类似，区别：
- 接收 ID 先与 **FDCAN_XIDAM** 做 AND，再送过滤
- GFC.RRFE 控制是否拒绝 remote frame
- XIDFC.LSS 控制过滤器数量

### Rx FIFOs

Rx FIFO 0/1 各可配最多 64 个元素，配置寄存器 FDCAN_RXF0C / FDCAN_RXF1C。

- 过滤匹配的消息按 filter 元素配置存入对应 FIFO
- FIFO 满时 (FDCAN_IR.RFnF 指示)，不再写入新消息，直到至少读出一条
- 满时收到新消息 → 丢弃，置中断标志 **FDCAN_IR.RFnL** (message lost)
- **Watermark 水位线** (RXFnC.FnWM)：FIFO 填充到该值时置 IR.RFnW 中断，预防溢出
- 读 FIFO 时：get index (RXFnS.FnGI) + 元素大小 + FIFO 起始地址 (RXFnC.FnSA)

**Blocking mode** (默认，FnOM=0)：
- FIFO 满时新消息直接丢弃

**Overwrite mode** (FnOM=1)：
- FIFO 满时新消息**覆盖最旧的消息**
- Put index 和 Get index 都 +1
- 注意事项：CPU 读 FIFO 的同时硬件可能正在写，可能读到不一致数据
  - 建议从 **get index + offset** 开始读 (跳过可能正在写的位置)
  - 读完将最后读取的元素号写入 **RXFnA.FnA** (acknowledge index) → 硬件更新 get index
  - 若 put index 尚未到该元素，FnF 满标志清零

### Dedicated Rx buffers

最多 64 个，起始地址由 FDCAN_RXBC.RBSA 配置。

使用方式：
- filter 元素配置 SFEC/EFEC=111 且 SFID2/EFID2[10:9]=00，SFID2/EFID2[5:0] 指向 Rx buffer 编号
- 消息写入后置对应 **NDAT1/2** (New data flag)，IR.DRX 中断置位
- **NDAT=1 期间该 Rx buffer 锁定**，过滤不会匹配到它，继续查下一个 filter 元素
- 软件读完数据后写 1 清除 NDAT 位 (写 1 清零)

| Filter 元素 | SFID1 (目标 ID) | SFID2[10:9] | SFID2[5:0] (Rx buffer#) |
|---|---|---|---|
| 0 | ID message 1 | 00 | 00 0000 |
| 1 | ID message 2 | 00 | 00 0001 |
| 2 | ID message 3 | 00 | 00 0010 |

### Filtering for Debug messages

SFEC/EFEC=111 且 SFID2/EFID2[10:9]≠00 时用于 debug 消息：
- 不设 NDAT，不置 IR.DRX
- 通过 FDCAN_RXF1S.DMS 监控
- SFID2/EFID2[10:9] 控制 debug 状态机，[5:0] 控制存储位置

### Tx handling

Tx handler 扫描 FDCAN_TXBRP 找最高优先级待发送消息 (ID 最小)，触发条件：
- TXBRP 更新时
- 一次传输启动时

帧发送配置 (Table 451)：

| FDOE | BRSE | Tx buffer FDF | Tx buffer BRS | 实际发送 |
|---|---|---|---|---|
| 0 | X | X | X | 经典 CAN |
| 1 | 0 | X | X | 经典 CAN |
| 1 | 1 | 0 | X | 经典 CAN |
| 1 | 1 | 1 | 0 | CAN FD 不切换速率 |
| 1 | 1 | 1 | 1 | CAN FD 数据段高速 |

### Transmit pause

防止单个节点连续发送霸占总线：
- 发送成功一次后，必须等待 **2 bit time 的 bus idle** 才能发下一条
- 这期间其他节点的待发消息可利用空闲启动，避免与高优先级 ID 仲裁
- 适用场景：CAN 消息 ID 固定不能改，但某些消息不应频繁占用总线

### Tx buffer 操作模式

32 个 Tx buffer 可以用三种方式组织：**Dedicated、FIFO、Queue**，也可以混合。

FDCAN_TXBC.TFQM 选择 FIFO 还是 Queue，FDCAN_TXBC.NDTB 和 TFQS 分配数量。

元素大小由 FDCAN_TXESC.TBDS 配置：

| TBDS | 数据段 (bytes) | RAM (32-bit words) |
|---|---|---|
| 000 | 8 | 4 |
| 001 | 12 | 5 |
| 010 | 16 | 6 |
| 011 | 20 | 7 |
| 100 | 24 | 8 |
| 101 | 32 | 10 |
| 110 | 48 | 14 |
| 111 | 64 | 18 |

#### Dedicated Tx buffers — 固定 ID，CPU 完全控制

每个 buffer 对应一个固定 ID，CPU 决定什么时候发哪个。

- 写入数据后写 **FDCAN_TXBAR.ARn** 请求发送
- 多个 buffer 配相同 ID 时，**编号小的先发**
- 地址 = TBSA + TBDS × buffer 编号

#### Tx FIFO — 先入先出 (TFQM=0)

按写入顺序发，不按 ID 优先级。

- **Put index** (TFQPI) — 新消息写到这个位置，请求发送后自动 +1
- **Get index** (TFGI) — 当前发到这个位置，发完后自动 +1
- **Free level** (TFFL) = get - put，表示还剩几个空位
- 满标志 **TFQF=1** 时不能再加，等发出一条腾出位置
- 支持批量：n 条消息写到连续 n 个 buffer，put index 一次 +n
- 地址 = TBSA + TBDS × TFQPI

#### Tx Queue — 按优先级发 (TFQM=1)

不管写入顺序，**ID 最小的先发**。

- 同样用 put/get index 管理空位，满标志 TFQF=1 时不能加
- ID 相同时编号小的先发
- 也可以用 TXBRP 直接找空闲 buffer 放消息（不通过 put index）

#### 混合模式

32 个 buffer 分成两段：
- **NDTB** (Number of Dedicated Tx Buffers) — 前半段作为 dedicated
- **TFQS** (Tx FIFO/Queue Size) — 后半段作为 FIFO 或 Queue
- TFQS=0 时全部是 dedicated

**Dedicated + FIFO** (Figure 729, TFQM=0)：

仲裁规则：**所有 dedicated** + **FIFO 中 get index 指向的那一条**（即最早待发的），一起比 ID 大小，最小的先发。

FIFO 内的消息必须按顺序发，后面的消息即使 ID 更小也要等前面的先发完。

**Dedicated + Queue** (Figure 730, TFQM=1)：

仲裁规则：**所有 buffer（包括 Queue 里的全部）** 一起比 ID，最小的先发。Queue 里的消息不按顺序，按 ID 优先级。

#### 三种方式对比

| 模式 | 谁决定发送顺序 | 适用场景 |
|---|---|---|
| Dedicated | CPU 逐个请求 | 固定周期或事件触发的特定消息 |
| FIFO | 写入顺序 | 只需要按顺序发送，不关心优先级 |
| Queue | ID 优先级 | 需要高优先级消息插队 |
| Dedicated + FIFO | Dedicated 随意 + FIFO 按序 | 部分固定消息 + 批量发送 |
| Dedicated + Queue | ID 优先级 | 部分固定消息 + 优先级队列 |

### Transmit cancellation

取消一个待发送的请求：写 **FDCAN_TXBCR** 对应位 → 成功后 **TXBCF** 对应位置 1。

注意：
- 不适用于 Tx FIFO（FIFO 不能跳着取消）
- 如果已经在发了，等发完再取消
  - 发成功 → TXBTO 和 TXBCF 都置位
  - 发失败 → 不重发，只置 TXBCF
- 取消时有一个短窗口不发送，可能让其他节点的低优先级消息趁机发出

### Tx event handling

发送完成后，硬件自动把**消息 ID + 时间戳 + message marker** 存入 Tx event FIFO。

这样做的好处是和 Tx buffer 解耦：发完就能重用 buffer 发下一条，不用先读状态再覆盖。

- 最大 32 个元素
- 地址 = EFSA + 2 × EFGI (每个元素固定 2 words)
- **Watermark** (EFWM) — 填充到该值时置 IR.TEFW 中断
- 满时置 **IR.TEFF**，丢事件置 **IR.TEFL**
- 读完数据后写 acknowledge index 更新 get index

### FIFO acknowledge

Rx FIFO 0/1 和 Tx event FIFO 的 get index 由软件控制——读完数据后写对应的 acknowledge index 寄存器，告诉硬件这个位置可以覆盖了。

写 acknowledge index 时，硬件将 get index 设为 acknowledge index + 1，更新 FIFO fill level。

两种用法：
- 读了一条 → 把 get index 写回 acknowledge index
- 连续读多条 → 只写最后一条的 index，硬件一次更新

**注意**：如果不按 get index 顺序读 FIFO（比如从中间挑了一条高优先级消息），不要写 acknowledge index，否则 get index 跳到错的位置，会丢失旧消息。FDCAN 不检查写入值是否合法，软件必须确保正确。

### Bit timing

CAN 总线上**每个 bit** 都由三段组成 (Figure 731)：

```
| SYNC_SEG |   BS1   |   BS2   |
|<-- 1tq -->|<-- N -->|<-- M -->|
             ↑
          采样点 (这里读总线电平)
```

- **SYNC_SEG** — 固定 1 tq，信号边沿应在此段发生
- **BS1** — 决定采样点位置，可自动延长补偿正相位偏移
- **BS2** — 决定发送点位置，可自动缩短补偿负相位偏移

**波特率计算**：

```
bit time = tSyncSeg + tBS1 + tBS2
波特率 = 1 / bit time
```

**仲裁段 (FDCAN_NBTP)**：
- tq = (NBRP + 1) × tfdcan_tq_ck     ← 对总线时钟分频
- tSyncSeg = 1 tq
- tBS1 = (NTSEG1 + 1) 个 tq
- tBS2 = (NTSEG2 + 1) 个 tq

**数据段 (FDCAN_DBTP)**：
- tq = (DBRP + 1) × tfdcan_tq_ck
- tBS1 = (DTSEG1 + 1) 个 tq
- tBS2 = (DTSEG2 + 1) 个 tq

**SJW (同步跳变宽度)**：1~4 tq，BS1 最多延长多少 / BS2 最多缩短多少，补偿各节点时钟差异。

寄存器只能在 INIT=1 且 CCE=1 时配置。

### 59.4.7 Application

当 FDCAN 没有精确时钟源（如内部 RC 振荡器）时，通过总线上其他石英晶振节点的信号校准时钟。

#### Bypass 模式 (CCFG.BCC=1)

跳过校准，直接用内部时钟。

- **CDIV=0**：时钟直通不分频，fdcan_ker_ck ≥ 80MHz，**支持 CAN FD**
- **CDIV≠0**：通过 CDIV 分频，要求 fdcan_tq_ck < fdcan_pclk

#### 软件校准 (CCFG.BCC=1 + 手动调整)

通过读 CCU 状态寄存器手动微调片内振荡器 (trimming)：

```
启动 → 置 ASM=1, 清 INIT → 调整时钟直到 Precision_Calibrated → 清 ASM 恢复正常
```

- fdcan_ker_ck ≥ 80MHz
- 运行中定期检查校准状态，漂移了重复上述流程
- **不支持 CAN FD**

#### 硬件自动校准 (BCC=0)

CCU 全自动控制时钟，fdcan_ker_ck ≥ 80MHz。**不支持 CAN FD**。

- 如果 FDCAN1 进入 Bus_Off 或 INIT 被置位，CCU 回到 Not_Calibrated，FDCAN1/2 都进入 restricted mode
- 复位后默认模式（若 BCC 复位值为 0）

#### 正常校准 (BCC=0)

校准状态机 (Figure 733)：

```
Not_Calibrated → Basic_Calibrated → Precision_Calibrated
                      ↑                   │
                      └──── 超时或配置变化 ──┘
```

**① Basic calibration**：
- 测量 FDCAN_RX 上两个连续下降沿的最小距离（假设 = 2 bit time），算出时钟分频值
- 收到一个有效 CAN 帧即完成，进入 Basic_Calibrated

**② Precision calibration**：
- 测量帧内一段 32 或 64 位的精确长度 (CCFG.CFL 配置)
- 需要来自石英晶振节点的帧
- 必须在固定最大间隔内重复校准（校准看门狗）

**状态说明**：
- 未达到 Precision_Calibrated 之前，FDCAN 处于 restricted mode（不能发帧，不能发 error/overload flag，不计错误）
- 状态变化置中断标志 CCU_IR.CSC
- 从 Precision 退回 Basic 时，FDCAN 完成当前传输后进入 restricted mode
- 如果总线上只有一个石英晶振节点，启动后第一条消息的数据段或 ID 中必须包含至少一个 1010 序列，否则非石英节点无法完成 Basic calibration

**配置寄存器**：FDCAN_CCFG（仅 FDCAN1 的 CCE+INIT 置位时可写）

### 59.4.8 TTCAN operations (FDCAN1 only)

TTCAN (Time-Triggered CAN) 是 ISO 11898-4 标准，硬件按时间表自动发送，不用仲裁。

**Reference message** — time master 周期发送的参考消息，所有节点用来同步时间：
- ID 低 3 bit 定义 master 优先级（000 最高，111 最低）
- 发参考消息的节点是 current time master，挂了则由 potential time master 中优先级最高的接替
- 被干扰时硬件立即重传，Master_Ref_Mark 更新
- 数据长度：Level 1 ≥ 1 字节，Level 0/2 ≥ 4 字节
- 通过 FDCAN_TTRMC 配置

三种级别：

**Level 1** (OM=01)：
- NTU = 1 CAN bit time
- 参考消息第一字节：Next_is_Gap + Cycle_Count
- 不支持外部时钟同步

**Level 2** (OM=10)：
- NTU 是物理秒的一小部分，可配置
- 参考消息前四字节：Next_is_Gap、Cycle_Count、NTU_Res、Master_Ref_Mark
- 支持外部时钟同步和漂移补偿

**Level 0** (OM=11)：
- 与 Level 2 类似但不支持外部事件同步
- Next_is_Gap 始终为 0

同一网络 potential time master 必须同一 level，time slave 可在 Level 2 网络运行 Level 1。

### 59.4.9 TTCAN configuration

**TTCAN timing**：
- **NTU** (Network Time Unit) — 整个网络的统一时基
  - Level 1：NTU = 1 CAN bit time
  - Level 0/2：NTU = CAN clock period × TUR
- **TUR** = FDCAN_TURNA.NAV / FDCAN_TURCF.DC（非整数比率）
- 复位默认：DC=0x1000，NC=0x10000 → NTU = 16 CAN clock periods
- 写 FDCAN_TURCF.ELT 锁定配置并启动本地时间

**漂移补偿** (Level 0/2)：
- 硬件自动调整 NAV 来同步时钟差异
- 偏差超过 SDL (Synchronization Deviation Limit) 时挂起补偿
- FDCAN_TTOCF.EECS = 1 使能外部时钟同步
- 写 FDCAN_TTOCN.ECS 触发 NAV 在下一个参考消息时更新

**Application watchdog**：
- 需定期读 FDCAN_TTOST 喂狗，否则停止所有 CAN 总线活动
- FDCAN_TTOCF.AWL 配超时 NTU 数（最大 256）
- AWL=0x00 禁用（开发阶段）

**Timing of interface signals**：
- 定时事件产生脉冲输出 (fdcan_tmp / fdcan_rtp / cycle_fdcan_soc)
- 可用于同步两个 TTCAN 网络

### 59.4.10 Message scheduling

- FDCAN_TTOCF.TM 选择 potential time master 或 time slave
- **IRTO** (Initial Reference Trigger Offset)：backup time master 等待多久后发参考消息，推荐 = master priority × 时钟漂移系数
- **OM=00** 回到普通 event-driven CAN（非 TTCAN）
- 配 OM 是 TTCAN 配置的最后一步

#### Trigger memory

TTCAN 的触发列表存在外部消息 RAM，最多 64 个元素。每个 basic cycle 开始时硬件从头扫描列表，找到 cycle code 匹配的 trigger，等 cycle time 到达 time mark 就执行。

每个 trigger 元素包含：

| 字段 | 说明 |
|---|---|
| TM (Time mark) | 触发时刻（cycle time 到达此值执行） |
| CC (Cycle code) | 在哪些 basic cycle 中生效 |
| TYPE | 触发类型 |
| MNR | 要发送/接收的消息编号 |
| MSC | 调度错误计数 |
| TMIN/TMEX | 内部/外部时间标记事件 |

**Trigger 类型**：

| 类型 | 作用 |
|---|---|
| Tx_Ref_Trigger (0000) | time master 发参考消息 |
| Tx_Ref_Trigger_Gap (0001) | Gap 模式下的参考消息 |
| Tx_Trigger_Single (0010) | 独占窗口发一次 |
| Tx_Trigger_Continuous (0011) | 独占窗口重复发，每个匹配 cycle 都发 |
| Tx_Trigger_Arbitration (0100) | 开始仲裁窗口 |
| Tx_Trigger_Merged (0101) | 合并仲裁窗口（连续多个仲裁窗口合并） |
| Watch_Trigger (0110) | 监控参考消息是否超时 |
| Watch_Trigger_Gap (0111) | Gap 模式下监控 |
| Rx_Trigger (1000) | 检查周期接收的消息是否到达 |
| Time_Base_Trigger (1001) | 产生内部/外部事件脉冲 |
| End_of_List (1010~1111) | 非法类型，触发配置错误 |

**时间窗口**：
- **Exclusive window** — 独占时间槽，只有指定节点能发（Tx_Trigger_Single/Continuous）
- **Arbitrating window** — 仲裁窗口，多节点可抢（Tx_Trigger_Arbitration）
- **Merged arbitrating window** — 连续仲裁窗口合并，最后一个必须为 Tx_Trigger_Arbitration

**限制**：
- 不能有两个 trigger 在相同 time mark + 相同 cycle count
- Rx_Trigger 和 Time_Base_Trigger 不能放在 Tx_Trigger 的 Tx enable window 内
- Watch_Trigger 之后的 trigger 永远不会激活
- 所有未使用的 trigger memory 必须设为 End_of_List

**典型 trigger list 顺序**：
```
Tx_Triggers → Rx_Triggers → Tx_Ref_Trigger → Watch_Trigger → End_of_List
```
Time master 有完整列表，time slave 没有 Tx_Ref_Trigger。

**配置错误** (FDCAN_TTOST.EL=11, severity 3)：
- trigger 的 time mark 小于当前 cycle time
- Tx_Trigger_Merged 后接了非 Merged/Arbitration 的 trigger
- 遇到 End_of_List（配置不完整）
- time slave 遇到 Tx_Ref_Trigger
- time mark 位于 Tx_Trigger 的 Tx enable window 内
- time mark 太靠近 Tx_Ref_Trigger 导致 IRTO 偏移后顺序错乱

#### TTCAN 调度初始化

清 INIT 后 TTCAN 开始同步，所有节点 cycle time = 0，SYS=00 (Out of Sync)，只有参考消息能发。

同步流程：

```
清 INIT → SYS=00 → 收到第1个参考消息 → SYS=01 (Synchronizing)
         → 收到第2个参考消息 → SYS=10 (In_Gap) 或 SYS=11 (In_Schedule)
                               → 所有 Tx/Rx_Trigger 使能
```

**Time slave**：
- 到 Watch_Trigger 没收到任何消息 → Init_Watch_Trigger（内部计数器，最大值 0xFFFF），FSE 冻结，cycle time 无效。收到第一个参考消息后重启
- 到 Watch_Trigger 前收到消息但不是参考消息 → **致命错误** (EL=11, severity 3)，关 CAN 输出进 bus monitoring mode。必须 INIT=1 再清 INIT 才能恢复

**Potential time master**：
- 到 Tx_Ref_Trigger 发参考消息，收不到应答就到 Init_Watch_Trigger 取消重试
- 收到消息但不是参考消息 → 同 time slave，进 fatal error
- 发成功 → 成为 current time master (FDCAN_TTOST.MS=11)，否则是 backup (MS=10)
- 全部配好后，优先级最高的成为 current time master

### 59.4.11 TTCAN gap control

Gap = 两个 basic cycle 之间的空闲间隔，总线 idle。只在外部事件同步模式 (GEN=1) 下可用。

**触发方式**：
- **软件**：写 FDCAN_TTOCN.NIG=1，下一个参考消息置 Next_is_Gap=1
- **硬件**：写 FDCAN_TTOCN.GCS=1，外部引脚 fdcan_evt 为高时自动置 Next_is_Gap=1

**流程**：
1. 参考消息带 Next_is_Gap=1
2. 所有节点收到 WFE 标志
3. 当前 basic cycle 跑完最后一个时间窗口
4. 进入 gap time，总线 idle
5. 下一个参考消息到来时开始新 basic cycle

**Gap 中**：
- time master 置 GSI=1，time slave GSI=0
- potential time master 在 In_Gap 状态下有 4 种方式结束 gap

结束 gap 的 4 种方式：
1. **软件**：写 FDCAN_TTOCN.FGP=1，立即发参考消息
2. **硬件**：fdcan_evt 引脚下降沿 → 自动置 FGP，重新开始调度
3. **时间触发**：FDCAN_TTOCN.TMG=1 → 下一个寄存器时间标记中断时自动置 FGP
4. **超时**：到 Tx_Ref_Trigger_Gap 还没同步事件，自动结束 gap

在 gap 开始前置 FGP 则跳过 gap time，直接发参考消息。

严格时间触发模式 (GEN=0) 下 Next_is_Gap、fdcan_evt、NIG/FGP/TMG 全部忽略。

### 59.4.12 Stop watch

外部引脚 fdcan_swt 的边沿触发捕获当前 local time / cycle time / global time 到 FDCAN_TTCPT.SWV，置中断 SWE。软件读完后清 SWE 使能下一次捕获。

### 59.4.13 Local time, cycle time, global time, and external clock synchronization

TTCAN 的三层时间：

**Local time**：
- 16-bit 循环计数器，每 NTU +1
- 内部带 3-bit 小数部分（NTU 不足 8 CAN 时钟周期时调整）
- 每个节点独立运行

**Cycle time** (当前 basic cycle 内的偏移)：
- 每帧 SoF 采样点捕获 local time → **Sync_Mark**
- 收到/发送参考消息 → **Ref_Mark** = Sync_Mark
- Cycle time = local time - Ref_Mark
- 通过 FDCAN_TTCTC.CT 读取

**Global time** (Level 0/2 才有)：
- Time master 把 Ref_Mark 作为 Master_Ref_Mark 放入参考消息发出
- 接收节点计算 **Local_Offset** = Master_Ref_Mark - 本地 Ref_Mark
- Global time = local time + Local_Offset
- 有低通滤波保证连续单调递增（Disc_Bit=1 时除外）
- 通过 FDCAN_TTLGT.GT 读取
- Potential time master 没收到过其他参考消息时 Local_Offset=0
- 成为 current time master 后 Local_Offset 冻结

**漂移补偿** (Figure 735, Level 0/2)：
- 每个 basic cycle 比较本地长度和 time master 长度
- 有差异且 Disc_Bit=0 → 重新计算 NAV 调整
- 偏差超过 SDL → 挂起补偿，QCS=0
- Level 1 无漂移补偿，QCS 始终为 1
- Time master 可通过 EECS 使能外部时钟同步

外部时钟同步：
- **调速度**：写 FDCAN_TURCF.NCL → 置 TTOCN.ECS 生效（DC 运行中不可改）
- **调相位**：写 FDCAN_TTGTP → 置 TTOCN.SGT 生效，下次参考消息 Disc_Bit=1
- FDCAN_TTOST.QGTP 指示全局时间是否与 time master 同步

### 59.4.14 TTCAN error level

ISO 11898-4 定义 4 级错误：

| 级别 | 名称 | 行为 |
|---|---|---|
| S0 | No error | 正常 |
| S1 | Warning | 通知应用，应用自行处理 |
| S2 | Error | 关闭所有独占/仲裁窗口传输，potential time master 仍可发参考消息 |
| S3 | Severe error | 停止所有 CAN 操作，进 bus monitoring，需重新配置 |

错误类型：
- **S3**：Config_error、Watch_Trigger_Reached、Application_Watchdog、CAN_Bus_Off
- **S2**：Scheduling_Error_2（MSC 达 7）、Tx_Overflow
- **S1**：Scheduling_Error_1（MSC 差值 > 2）、Tx_Underflow

### 59.4.15 TTCAN message handling

**Reference message**：
- ID 由 FDCAN_TTRMC.RID 配置，不需要专用 Tx buffer，FSE 直接提供
- 带额外数据：RMPS=1 + TXBRP.TRP0=1 → 从 Tx buffer 0 取附加载荷
- Level 1 至少 1 字节，Level 0/2 至少 4 字节

**Message reception** — 与 event-driven CAN 相同，用 Rx FIFO：
- Rx_Trigger 检查周期消息是否按时到达
- **MSC** (Message Status Count)：匹配的过滤元素编号一致则 -1，不一致则 +1
- 建议把 Rx_Trigger 用的过滤器放在 filter list 开头

**Message transmission**：
- TTCAN 模式只能用 **32 个 dedicated Tx buffers**，不能用 Tx FIFO/Queue
- 每个 Tx_Trigger 指向一个 Tx buffer，多个 trigger 可指向同一 buffer
- 应用必须按 cycle time 同步更新数据

更新 Tx buffer 步骤：
- **Single/Merged/Arbitration**：读 TXBTO 确认发完 → 更新 buffer → 写 TXBAR 请求
- **Continuous**：先写 TXBCR 取消 → 等 TXBCF → 更新 → 重新请求

**Exclusive window** — 独占，无仲裁。Single 发完复位请求，Continuous 保持请求。

**Arbitrating window** — 多节点同时发，CAN 仲裁。MSC 不更新，失败下次重试。

**Merged arbitrating window** — 连续仲裁窗口合并，按 trigger list 顺序发，最多连续处理 3 条。不是时间触发的，消息可能提前或延迟发。

### 59.4.16 TTCAN interrupt and error handling

FDCAN_TTIR 分四段，每段可单独使能 (FDCAN_TTIE)：

- **第一段** (CER/AW/WT/IWT) — 致命错误，CAN 停止。除 IWT 外都需重新配置
- **第二段** (ELC/SE1/SE2/TXO/TXU/GTE) — 通信受影响但可恢复，硬件自动处理
- **第三段** (GTD/GTW/SWE/TTMI/RTMI) — 全局时间事件和 stop watch
- **第四段** (SOG/CSM/SMC/SBC) — 应用同步到通信调度

### 59.4.17 Level 0

Level 0 **不是 ISO 11898-4 标准**，借用 Level 2 的校准全局时间功能跑 event-driven CAN (OM=11)。

特点：
- 没有固定调度，GEN 忽略，不支持外部事件同步
- **trigger memory 不工作**，无需配置
- 参考消息同步维持时间基

时间标记替代：
- **TTMI**：cycle time ≥ FDCAN_TTOCF.IRTO × 0x200 时触发，提醒给 message buffer 0 设发送请求
- **WT**：cycle time ≥ 0xFF00 时触发（Watch_Trigger）

参考消息配置同 Level 2，但只能用 **message buffer 0** 发送，没有 reference trigger offset。

同步状态机 (Figure 736)：
```
Sync_Off → Synchronizing → In_Schedule
   ↑                           |
   └──── S3 错误 ──────────────┘
```

错误处理：
- 只有 S0 和 S3，**没有 S1/S2**
- S3 **不会进 bus monitoring**（与 Level 1/2 不同）
- S3 通过收到/发送下一个参考消息**自动恢复**
- S3 时复位 SYS 和 MS

主从关系 (Figure 737)：Current_Master / Backup_Master / Slave，S3 回到 Master_Off。

### 59.4.18 Synchronization to external time schedule

将一个 TTCAN 网络调度同步到另一个外部网络（如第二个 TTCAN 或 FlexRay），仅限 current time master。

原理：
- fdcan_evt 引脚上升沿触发
- 比较当前 cycle time 与 FDCAN_TTGTP.CTP（目标相位值）
- 差值 > 9 NTU → SPL=0，置 CSM 中断

步骤：
1. 用 gap control 调整两个网络相位
2. 置 TTOCN.ESCN → SPL 置位
3. fdcan_evt 上升沿自动完成同步

相位锁定：差值 ≤ 9 NTU → SPL 保持，差值作为 reference trigger offset 调整下一个参考消息。每个 basic cycle 开始时使能一次边沿检测。

### 59.4.19 FDCAN Rx buffer and FIFO element

Rx buffer/FIFO 元素格式见 Table 460 (Rx buffer and FIFO element) 和 Table 461 (Rx timestamp).

### 59.4.20 FDCAN Tx buffer element

Tx buffer 元素格式见 Table 462 (Tx buffer element) 和 Table 463 (Tx event FIFO element).

### 59.4.21 FDCAN Tx event FIFO element

存储已发送消息的状态信息，按发送顺序排列。状态通过 FDCAN_TXEFS 查询。格式见 Table 464 (Tx Event FIFO element) 和 Table 465 (Tx Event FIFO element description)。

关键字段：
- **ET[1:0]** — event type：01=正常发送, 10=取消后仍发送（DAR 模式下总是此值）
- **MM[7:0]** — message marker，从 Tx buffer 复制过来，用于回查
- **TXTS[15:0]** — 帧起始时捕获的时间戳

### 59.4.22 FDCAN standard message ID filter element

最多 128 个过滤元素，用于 11-bit 标准 ID。地址 = FDCAN_SIDFC.FLSSA + 索引 (0~127)。格式见 Table 466/467。

**SFT[1:0]** — 过滤类型：
- 00: Range — SFID1~SFID2 范围匹配
- 01: Dual — SFID1 或 SFID2 任一匹配
- 10: Classic — SFID1 = filter, SFID2 = mask
- 11: 禁用

**SFEC[2:0]** — 匹配时动作：
- 000: 禁用
- 001: 存 Rx FIFO 0
- 010: 存 Rx FIFO 1
- 011: Reject
- 100: 设优先级
- 101: 设优先级 + 存 FIFO 0
- 110: 设优先级 + 存 FIFO 1
- 111: 存 Rx buffer 或 debug 消息（此时 SFT 忽略）

SFEC=111 时，SFID2 进一步控制：
- SFID2[10:9]：00=Rx buffer, 01/10/11=Debug message A/B/C
- SFID2[8:6]：filter event pin 使能（匹配时产生一个 fdcan_pclk 脉冲）
- SFID2[5:0]：Rx buffer 偏移（相对 FDCAN_RXBC.RBSA）

### 59.4.23 FDCAN extended message ID filter element

最多 64 个过滤元素，用于 29-bit 扩展 ID。地址 = FDCAN_XIDFC.FLESA + 2 × 索引 (0~63)。格式见 Table 468/469。

**EFEC[2:0]** — 匹配动作（同 SFEC）：disable / FIFO0 / FIFO1 / reject / priority / priority+FIFO / Rx buffer

**EFT[1:0]** — 过滤类型：
- 00: Range — EFID1~EFID2 范围匹配
- 01: Dual — EFID1 或 EFID2
- 10: Classic — EFID1 = filter, EFID2 = mask
- 11: Range without XIDAM mask — 范围匹配，但忽略 FDCAN_XIDAM 掩码

EFEC=111 时 EFID2[10:9] 控制 Rx buffer/debug 选择，[8:6] 控制 filter event pin，[5:0] 控制 buffer 偏移（同标准 ID）。

### 59.4.24 FDCAN trigger memory element

TTCAN 触发存储器，最多 64 个元素。地址 = FDCAN_TTTMC.TMSA + 索引 (0~63)。格式见 Table 470/471。

**TM[15:0]** — time mark，触发激活的 cycle time

**CC[6:0]** — cycle code，周期有效性：
- 0b000000x — 所有周期有效
- 0b000001c — 每 2 周期有效 (mod2 = c)
- 0b00001cc — 每 4 周期有效 (mod4 = cc)
- 依此类推至 0b1cccccc — 每 64 周期有效 (mod64 = cccccc)

**TYPE[3:0]** — 触发类型（10 种）：
- 0000: Tx_Ref_Trigger — 非 gap 时发送参考消息
- 0001: Tx_Ref_Trigger_Gap — gap 时发送参考消息
- 0010: Tx_Trigger_Single — 排他窗口单次发送
- 0011: Tx_Trigger_Continuous — 排他窗口连续发送
- 0100: Tx_Trigger_Arbitration — 仲裁窗口发送
- 0101: Tx_Trigger_Merged — 合并仲裁窗口
- 0110: Watch_Trigger — 非 gap 时监视
- 0111: Watch_Trigger_Gap — gap 时监视
- 1000: Rx_Trigger — 检查接收
- 1001: Time_Base_Trigger — 仅控制 TMIN/TMEX
- 1010~1111: End_of_List — 非法，触发配置错误

**MNR[6:0]** — 消息号：发送时指向 Tx buffer (0~31)，接收时指向过滤元素号

**MSC[2:0]** — 消息状态计数，统计排他窗口中周期性消息的调度错误次数
