# GMAC 以太网控制器

RM0436 Rev 7, Chapter 63, Pages 3289 起（约 300+ 页）

注：子节编号以原文页边编号为准，无编号子标题按原文显示。

## 63.1 以太网简介

- 版权归 Synopsys，经许可使用
- 以太网外设支持符合 IEEE 802.3-2008 标准的数据收发
- 可配置，满足广泛的消费和工业应用需求，包括 AV（音视频）节点

## 63.2 以太网主要特性

### 63.2.1 外设构成

以太网外设内嵌专用 DMA（直接内存访问）、媒体访问控制器（MAC）和 PHY 接口块，支持多种格式。

### 63.2.2 标准符合性

- IEEE 802.3-2008：以太网 MAC、GMII、MII
- IEEE 1588-2008：PTP 精密时钟同步
- IEEE 802.1AS-2011 和 802.1Qav-2009：音视频（AV）流量
- IEEE 802.3az-2010：EEE 节能以太网
- AMBA 2.0（AHB 从端口）、AMBA4（AXI 主端口）
- RGMII 规范 2.6 版（HP/Marvell）、RMII 规范 1.2 版（RMII 联盟）

#### MAC 特性

##### MAC Tx/Rx 公共特性

- 独立的发送、接收和控制接口
- 10/100/1000 Mbps 速率，PHY 接口可选：
  - **GMII/MII（默认）**：接外部千兆或快速以太网 PHY
  - **RGMII**：接外部千兆 PHY
  - **RMII**：接外部快速以太网 PHY
- 半双工：CSMA/CD 协议、背压流控、千兆半双工下 packet bursting / packet extension
- IEEE 802.3az EEE 节能（(G)MII 和 RGMII PHY）
- 应用侧 64 位数据传输接口
- 全双工流控：IEEE 802.3x Pause 帧 + 优先级流控（PFC）
- 网络统计：RMON/MIB 计数器（部分支持 RFC2819/RFC2665）
- IEEE 1588-2002/2008 包时间戳（收发状态含 64 位时间戳；发送支持 one-step 和 two-step）
- PPS 输出信号控制（eth_ptp_pps_out、ETH_PPS_OUT）
- **MDIO（Clause 22 和 Clause 45）主机接口**，用于 PHY 配置和管理

##### MAC Tx 特性

- 前导码和帧起始定界符（SFD）插入
- 每个发送包独立的 32 位状态
- 自动 CRC 和填充（pad）生成，可按帧控制
- 可编程包长：标准帧和巨型帧（最大 16 KB）
- 可编程帧间隙（IPG）：40~96 位时间，步进 8
- IEEE 802.3x 流控：全双工下自动发送零量子 Pause 帧
- 源地址插入/替换；VLAN 标签插入/替换/删除（按包或全局静态控制）
- 最多两个 VLAN 标签的插入/替换/删除
- 全双工下可发送缩短前导码的包
- 基于队列/通道的 VLAN 标签插入/替换/删除

##### MAC Rx 特性

- 自动剥离 pad 和 CRC（可选）
- 可关闭自动 CRC 校验
- 前导码和 SFD 删除
- 独立的 112 位或 128 位状态
- 可编程看门狗超时限制
- 灵活地址过滤：
  - 4 个 48 位完美匹配（DA）地址过滤器，每字节可带掩码
  - 4 个 48 位源地址（SA）比较，每字节可带掩码
  - 64 位哈希过滤（组播和单播 DA）
- 可透传所有组播包
- 混杂模式（不过滤，用于网络监控）
- 按过滤结果透传所有包并附状态报告

### 63.2.3 附加包过滤

- VLAN 标签过滤：完美匹配和哈希（基于外层或内层 VLAN 标签）
- L3/L4 过滤：TCP/UDP over IPv4/IPv6
- IEEE 802.1Q VLAN 标签检测和接收时删除选项
- 远程唤醒包和 AMD magic packet 检测
- 可选把收到的 Pause 帧转发给应用（全双工）
- 接收包 L3/L4 校验和卸载
- 剥离最多两个 VLAN 标签并在状态中提供

### 事务层 (MTL) 特性

#### MTL Tx/Rx 公共

- 64 位事务层块（桥接应用和 MAC）
- 面向包传输优化，带包定界符
- 可编程突发长度（最大为 MTL Rx/Tx 队列大小的一半）
- 每队列可编程阈值（默认 64 字节）

#### MTL Tx

- 4096 字节发送 FIFO，可编程阈值
- 发送路径 2 个 Tx 队列，共享一个存储器
- store-and-forward 或阈值（cut-through）模式
- 多队列时可编程队列大小（以 256 字节为单位）
- 半双工冲突包自动重传
- 丢弃以下包并带状态：冲突、过度冲突、过度延迟、下溢
- store-and-forward 模式下计算并插入 IPv4 头校验和、TCP/UDP/ICMP 校验和
- Tx FIFO 下溢丢包时产生统计脉冲
- 每队列带宽统计（125 µs 周期内最多 16 块）
- 包级控制：VLAN 标签插入/替换、源地址插入、L3/L4 校验和插入、one-step 时间戳、时间戳控制、CRC 和填充控制
- 多队列调度算法：WRR（加权轮询）、SP（严格优先）、CBS（信用整形器）

#### MTL Rx

- 4096 字节接收 FIFO，可配置阈值
- 2 个 Rx 队列，共享存储器
- 阈值模式下可编程 Rx 队列阈值（默认固定 64 字节）
- store-and-forward 模式下可选过滤所有错误包（不转发给应用）
- 可选转发过小（undersized）的好包
- 溢出丢包时产生统计脉冲
- 根据 Rx 队列填充水平自动产生 Pause 帧或背压信号给 MAC
- 多队列仲裁：WRR、加权严格优先（WSP）、SP

### DMA 块特性

- DMA 通过软件描述符结构在系统内存和外设之间交换数据
- 64 位数据传输
- 发送路径每队列一个独立 DMA 通道；接收路径两个队列共用一个 DMA 通道
- 面向包 DMA 传输优化（带包定界符）
- 数据缓冲字节对齐寻址
- **双缓冲（环形）描述符支持**
- 描述符架构：最少 CPU 干预下传输大块数据（每描述符最大 32 KB）
- 完整的正常操作和传输错误状态报告
- 发送和接收 DMA 引擎独立可编程突发长度
- 不同操作条件下可编程中断选项
- **每包发送/接收完成中断控制**
- 收发引擎间轮询或固定优先级仲裁
- 启动和停止模式
- 主机控制访问（AHB）和主机数据接口（AXI）独立端口
- Tx DMA 通道支持 TCP 分段卸载（TSO）
- 时间敏感条件包发送：比较描述符提供的时间槽信息（用于 AV 应用）
- 可编程发送描述符 posted write 控制（提高吞吐量）

### 总线接口特性

#### AXI 主接口

- AXI4 兼容，32 位地址、64 位数据
- OKAY/SLVERR/DECERR 响应
- 软件选择 AXI 突发类型：固定/可变长度；扩展长度固定突发 32/64/128/256
- AXI 读写数据通道握手

#### AHB 从接口

- 32 位 CSR 访问
- 支持所有 AHB 突发类型
- 不产生 Split、Retry、Error 响应

### 音视频特性

- AV 模式，100M/1000M 下 AV 数据走独立通道/队列
- 收发路径各支持多个队列
- IEEE 802.1Qav 指定的 CBS 算法（发送通道）
- 所有队列共用一个 Tx FIFO 和一个 Rx FIFO
- 可编程时间槽间隔：1 µs~4096 µs，粒度 1 µs

### 63.2.7 通用队列特性

- 可编程控制：将目的地址为组播/广播的接收包路由到可编程接收队列
- 支持将未打标签（untagged）的接收包路由到可编程接收队列
- 可编程控制：将带 VLAN 标签和未打标签的 IEEE 1588 PTP over Ethernet 接收包路由到相同或不同的可编程接收队列
- 可编程控制：将未通过目的地址过滤的单播/组播接收包路由到单独的可编程接收队列

## 63.3 以太网引脚和内部信号

注：实际有效的引脚取决于所选 PHY 类型（MII/GMII/RMII/RGMII）和器件配置。

### Table 490 外部引脚

| 端口名 | 类型 | 描述 |
|--------|------|------|
| ETH_COL | Input | 冲突检测信号，仅 GMII/MII |
| ETH_CRS | Input | 载波侦听信号，仅 GMII/MII |
| ETH_REF_CLK | Input | RMII 参考时钟 |
| ETH_RX_CLK | Input | MII/GMII 接收数据时序参考 |
| ETH_RXD[7:0] | Input | 接收数据（GMII 8 根，MII/RGMII 4 根，RMII 2 根） |
| ETH_RX_DV | Input | 接收数据有效 |
| ETH_CRS_DV | Input | CRS 与 RX_DV 在交替时钟周期上复用（10M 模式每 10 个时钟周期交替）；解复用后：RX_CLK 上升沿给出 RX_DV，RX_ER = 上升沿与下降沿 CRS_DV 值的异或 |
| ETH_RX_ER | Input | 接收错误 |
| ETH_TX_CLK | Input | MII 发送时序参考 |
| ETH_GTX_CLK | Output | GMII/RGMII 发送时序参考 |
| ETH_TXD[7:0] | Output | 发送数据（GMII 8 根，MII/RGMII 4 根，RMII 2 根） |
| ETH_TX_EN | Output | 发送数据使能 |
| ETH_TX_CTL | Output | TX_CLK 上升沿复用 TX_EN；下降沿为 TX_EN 与 TX_ER 的异或 |
| ETH_TX_ER | Output | 发送错误 |
| ETH_MDC | Output | 管理数据时钟 |
| ETH_MDIO | Input/Output | 管理数据 |
| ETH_PHY_INTN | Input | PHY 中断 |
| ETH_PPS_OUT | Output | PTP 秒脉冲输出 |
| ETH_PTP_AUX_TS | Input | PTP 系统时间辅助快照触发输入（非所有器件都有） |

### Table 491 内部信号

| 信号名 | 类型 | 描述 |
|--------|------|------|
| eth_hclk | Input | AHB 时钟（寄存器访问） |
| eth_aclk | Input | AXI 时钟（DMA 数据） |
| eth_sbd_intr_it | Output | 主以太网中断 |
| lpi_intr_o | Output | 收发进入/退出 LPI 状态的边带信号（EEE） |
| pmt_intr_o | Output | 收到有效远程唤醒包的边带信号 |
| eth_tx_clk | Input | Tx 内核时钟 |
| eth_rx_clk | Input | Rx 内核时钟 |
| eth_rmii_ref_clk | Input | RMII 参考内核时钟 |
| eth_ptp_pps_out | Output | PTP 秒脉冲信号 |
| mac_speed_o[1:0] | Output | MAC 速率信息，供 RCC 使用 |
| clk_ptp_ref_i | Input | PTP 参考时钟输入 |
| eth_ptp_trig[3:0] | Input | PTP 系统时间辅助快照触发输入 |

## 63.4 以太网架构

以太网外设由 4 个主要功能模块组成：
- **CSR**（控制状态寄存器模块）：通过 AHB 32 位从接口控制寄存器访问
- **DMA**：逻辑 DMA 模块，1 个接收物理通道 + 2 个发送物理通道，通过 AMBA AXI4 64 位主接口控制 MAC 与系统内存间的数据传输
- **MAC**：实现以太网协议
- **MTL**：控制应用与 MAC 之间的数据流

另有协议适配模块，用于支持 RMII 或 RGMII PHY 媒体独立接口。

Figure 771 框图（简化）：64 位 AXI 主接口 + 32 位 AHB 从接口 → DMA 仲裁器 → DMA Ch.0（Tx Ch.0 + Rx Ch.0）、DMA Ch.1（Tx Ch.1）→ MTL（4KB Tx FIFO + 4KB Rx FIFO + 控制）→ MAC Tx/MAC Rx → Select（GMII/MII 或 RGMII/RMII 选择）→ PHY 接口。

注：
1. 内部信号定义见 Table 491
2. 以太网时钟架构详见 RCC 章节 "Clock distribution for Ethernet"
3. ETH_PTP_AUX_TS 引脚非所有器件可用

### 63.4.1 DMA 控制器

DMA 有独立的发送（Tx）和接收（Rx）引擎：Tx 引擎把数据从系统内存搬到 MTL，Rx 引擎把数据从 PHY 搬到系统内存。使用描述符以最少 CPU 干预高效搬数据，可编程产生收发完成中断。

#### DMA 数据结构

DMA 与应用之间通过两种数据结构通信：
- 控制状态寄存器（CSR）
- 描述符列表和数据缓冲（描述符位于系统内存，包含指向缓冲的指针）

相关寄存器：
- `ETH_DMACxTXDLAR`：通道 x Tx 描述符列表基地址
- `ETH_DMAC0RXDLAR`：通道 0 Rx 描述符列表基地址
- `ETH_DMACxTXRLR` / `ETH_DMAC0RXRLR`：Tx/Rx 描述符环长度

描述符环规则：
- 描述符列表前向链接，下一描述符在固定偏移处
- DMA 处理完最后一个描述符后自动跳回基地址，形成**描述符环**
- 每个描述符最多指向 **2 个缓冲**（允许非连续内存）
- 缓冲 = 整包或部分包，不能超过一个包；一个包可跨多个缓冲/描述符（数据链）；一个描述符不能跨多个包

#### DMA 发送（默认模式）

1. 应用设置 TDES0~TDES3 和数据缓冲，置 Own 位（TDES3[31]=1）
2. 应用更新通道 Tx 描述符尾指针（`ETH_DMACxTXDTPR`）
3. Run 状态下 DMA 仲裁选择通道，从内存取描述符
4. 悬停条件（置 SR 位 2/16，进入 Suspend）：Own=0（描述符归应用所有）、尾指针=当前指针（环形模式）、错误
5. Own=1 时 DMA 解码缓冲地址，把数据搬到 MTL 直到 EOP；跨多描述符的包关闭中间描述符
6. 发送完成后：状态写入 TDES3、Own 清 0（描述符交还应用）；使能时间戳则时间戳写入 TDES0/TDES1
7. 最后一个描述符置 IOC（TDES2[31]）则发完成中断（SR 位 0）
8. 悬停恢复：往 `ETH_DMACxTXDTPR` 写任意值触发 Transmit Poll demand

#### DMA 发送（OSP 模式）

`ETH_DMACxTXCR` 位 4 使能。Run 状态下可同时获取两个包：第一个包状态未写回之前就获取第二个包并传输，再写第一个包的状态——流水线优化，提高吞吐。

#### DMA 接收

1. 应用设置 RDES0~RDES3 并置 Own 位，更新通道 Rx 尾指针指示最后一个有效描述符（尾指针指向 N，则 DMA 最后一个有效描述符是 N-1）
2. 置 `ETH_DMAC0RXCR` 位 0，DMA 进入 Run
3. DMA 取描述符，解码接收缓冲地址；当前指针到尾指针之间的描述符都可用
4. 包未收完则关闭中间描述符继续；包收完后：状态写入 RDES3、Own 清 0、Last 位置位；帧长写 RDES3，VLAN 标签写 RDES0
5. 无空闲描述符或到环尾则 Suspend；恢复 = Receive Poll demand + 应用更新尾指针

#### Tx/Rx 优先级

`ETH_DMAMR` 的 TXPR 字段配置同一通道 Tx/Rx DMA 的总线优先级。

注：描述符详细格式见 63.10 Descriptors。

### 63.4.2 MTL

- MTL 提供 FIFO 内存接口，缓冲并调节系统内存与 MAC 之间的包
- 负责应用时钟域与 MAC 时钟域之间的数据传递（跨时钟域）
- 两条 64 位数据通路：
  - **Tx 路径**：DMA 把包推入 Tx FIFO 队列；达到队列阈值（threshold 模式）或整包入队（store-and-forward 模式）时弹出传给 MAC；EOP 传出后从 MAC 取发送状态传回 DMA。Tx 队列 4096 字节
  - **Rx 路径**：MAC 把包推入 Rx 队列；队列填充水平超过配置的接收阈值（RTC bits[1:0]，各队列操作模式寄存器）或整包收到时通知 DMA；MTL 指示队列填充水平供 DMA 发起预配置的突发传输。Rx 队列 4096 字节

### 63.4.3 MAC

MAC 负责以太网协议处理：发送 = 从 MTL 取数据传给 PHY 接口；接收 = 从 PHY 接口取数据传给 MTL Rx FIFO。

#### MAC 发送

1. 发送由 MTL 以 SOP（Start of Packet）信号开始推入数据触发
2. 检测到 SOP，MAC 接受数据并向 GMII/MII 传输
3. EOP 传入后：
   - 正常完成传输，把发送状态提供给 MTL
   - 半双工冲突：状态带 Retry 位，重试直至成功或超过最大重试（Excessive Collision 中止传输）
   - 中止条件（半双工）：no carrier、carrier loss、excessive deferral、late collision、jabber——中止后丢弃后续数据直到下一个 SOP
4. 下溢（underflow）：MTL 无法连续提供数据时 MAC 发出下溢状态，丢弃后续数据直到下一个 SOP
5. 前一个包未收到 EOP 时收到 SOP → 视为前一包的延续，忽略新 SOP

#### MAC 接收

1. RxDV 有效 → 接收状态机（RSM）寻找 SFD 字段（手册写 0x5D，标准 SFD 为 0xD5，应为手册笔误）
2. 检测到 SFD → 从目的地址（DA）字段开始把包发给 RPC 模块（前导码和 SFD 被剥离）
3. 使能 IEEE 1588 时间戳时在 SFD 检测处快照系统时间；包未被过滤丢弃则时间戳传给应用
4. Length/Type 字段解释：
   - **< 1536（Length 解释）**且 MACCR 位 20（Auto CRC/Pad Stripping）使能：只发送 Length 字段指定字节数，丢弃填充和 FCS 字段
   - **≥ 1536（Type 解释）**：默认发送全部数据；若 MACCR 位 21（Type 包 CRC 剥离）使能且未使能接收校验和卸载，剥离所有 ether type 包的最后 4 字节
5. 看门狗：默认使能，>2048 字节（Jumbo 使能时 10240）的包在 RPE 模块截断；可用 ETH_MACWTR 位 16 覆盖固定超时；MACCR 位 19 可关闭看门狗；即使关闭，>32 Kbytes 的包仍被截断并给出看门狗超时状态

### 63.4.4 多通道和队列支持

- 结构：**2 个 Tx 队列 + 2 个 Rx 队列**；Tx DMA 每个队列一个通道（共 2 个，与队列 1:1 映射），Rx 只有一个 DMA 通道（两个队列共用）
- 多队列用于支持音视频桥接（AVB）协议
- Tx 通道间仲裁为**固定优先级**：通道 0 > 通道 1；因 AXI 协议支持流水线（读请求先发、数据后到），低优先级通道不会被饿死
- 未分类的 Rx 包默认路由到 **Rx 队列 0**（起步只用队列 0，多队列无需配置）
- MAC→Rx 队列路由按包类型：单播/组播过滤失败、组播/广播过滤通过、VLAN 优先级、AV 控制包、PTP 包（带/不带 VLAN）、普通 VLAN 包、untagged 包
- 相关寄存器：`ETH_MACRXQC0R`（队列使能 RXQxEN）、`ETH_MACRXQC1R`（PTPQ/MCBCQ/UPQ 字段）、`ETH_MACRXQC2R`（VLAN 优先级路由 PSRQ）、`ETH_MACRXQCR`（过滤失败包路由 UFFQ/MFFQ）、`ETH_MACPFR`（包过滤控制）、`ETH_MACVTR`（VLAN 标签）、`ETH_MACTSCR`（时间戳控制）、`ETH_MTLOMR`（MTL 操作模式，RAA 仲裁）、`ETH_MTLRXQxCR`（Rx 队列权重）
- Rx 方向 DMA↔MTL 仲裁：以 PBL（可编程突发长度）为粒度，每完成一个 PBL 传输仲裁一次
- AV 特性：IEEE 802.1Qav-2009 音视频实时传输（时间敏感流量），起步跳过
- 多队列编程指南见 63.9.8（第 3407 页）

#### AV 特性细节（TSN 相关，起步跳过）

- 相关标准：IEEE 802.1Qav（信用整形 CBS）、802.1Qat（网络资源预留）、802.1AS（时间同步）
- 队列调度：队列 0 默认严格优先（best-effort 流量）；队列 1 可用 CBS 信用整形（AVB 流量）
- AV 包识别：EtherType = 0x22F0 判为 AV 包；AV 数据包必带 VLAN 标签，AV 控制包可带可不带（默认 untagged 控制包进队列 0）
- CBS 算法：credit 每发送周期累积/消耗（100M 每 40ns，1000M 每 8ns）；idleSlopeCredit/sendSlopeCredit/hiCredit/loCredit 需按 ×1024 缩放编程（hiCredit 为正、loCredit 为补码）；CC 位（ETH_MTLTXQ1ECR）控制无包时是否继续累积 credit
- 带宽状态：ETH_MTLTXQxESR 报告每 125 µs 槽（1/2/4/8/16 槽）实际发送的平均比特数
- Slot 功能：TDES3 含 4 位槽号（仅 AV 通道有效），DMA 按 125 µs 时槽调度取数，描述符槽号需匹配当前或下一个槽；ETH_DMACxSFCSR 使能槽号检查
- 队列模式：通用流量按 WRR/WSP 排队，AVB 流量按 CBS/SP；Rx 队列类型由 RXQ0EN/RXQ1EN（ETH_MACRXQC0R）决定
- 每个 Tx DMA 有独立的描述符链（取数用）
- Table 493：TCW 字段配置 DMA 通道权重（1/2）
- 接收描述符 Extended Status 字段（见 63.10.4）含 AV 包标志及 VLAN 优先级
- AV 数据包按优先级路由：对应队列发送流控寄存器 bits[15:8]；AV 控制包路由 AVCPQ[2:0]、PTP 包路由 PTPQ[2:0]（均 ETH_MACRXQC1R）
- 剩余带宽分配规则：低优先级 CBS 队列不能用高优先级 CBS 队列未用的预留带宽；严格优先队列可以用
- 完美匹配 MAC 地址寄存器（ETH_MACAxHR）的 DCS 字段可将 Rx 包路由到指定 DMA 通道
- AV 编程指南页码：初始化 DMA（3415）、使能槽号检查（3416）、使能每槽比特报告（3416）

## 63.5 以太网功能描述：MAC

### 63.5.1 双 VLAN 处理

支持最多两个 VLAN 标签（内层/外层）处理：
- Tx：插入/替换/删除最多两个 VLAN 标签
- Rx：基于任一个标签过滤和剥离；剥离最多两个标签并随接收状态提供

#### Tx 路径特性 (Table 495)

| 特性 | 描述 |
|------|------|
| C-VLAN 和 S-VLAN 标签类型 | 内/外层标签可为 C-VLAN 或 S-VLAN，类型分别由 ETH_MACVIR.CSVL 和 ETH_MACIVIR 指定；支持任意内/外层序列，但不支持 "外层 C-VLAN + 内层 S-VLAN" 序列；MAC 不检查包中 VLAN 序列合法性，应用必须保证正确序列 |
| VLAN 标签删除 | 通过 ETH_MACVIR/ETH_MACIVIR 的 VLC 字段使能内或外层标签删除；只有单标签时视为外层标签，若仅使能内层删除则单标签包不删除 |
| VLAN 标签插入/替换 | 通过 VLC 字段使能内或外层插入/替换；由前一寄存器的 VLTI 位决定标签取自寄存器还是控制字 |

#### Rx 路径特性 (Table 496)

- 基于外层或内层 VLAN 标签过滤：ETH_MACVTR.ERIVLT
- 基于 C-VLAN 或 S-VLAN 类型过滤：ETH_MACVTR.ERSVLM
- 剥离外层/内层 VLAN 标签：ETH_MACVTR.EVLS / EIVLS
- Rx 状态中提供 16 位外/内层 VLAN 标签与类型：ETH_MACVTR.EVLRXS / EIVLRXS
- 禁用/跳过外层 VLAN 标签类型检查：ETH_MACVTR.DOVLTC

### 63.5.2 源地址和 VLAN 插入、替换或删除

#### 源地址 (SA) 插入或替换

- 插入：将 MAC 地址寄存器内容填入 SA 字段（发送包中不得带 SA 字段）
- 替换：用 MAC 地址寄存器内容替换 SA 字段（发送包中必须带 SA 字段，替换 DA 之后的六个字节）
- 对所有包使能：编程 ETH_MACCR.SARC 字段
- 对选择性包使能：在包的首个发送描述符 TDES3 的 SA 插入控制字段 (bits[25:23]) 中编程；TDES3 位 25=1 用 MAC 地址 1 寄存器，=0 用 MAC 地址 0 寄存器
- MAC 地址 1 寄存器未使能时，无论该位如何，一律使用 MAC 地址 0 寄存器

#### VLAN 插入、替换或删除

- 删除：删去 VLAN Type 和 VLAN Tag 字段
- 插入/替换：按 ETH_MACVIR.VLTI 位决定（Table 497）：

| 条件 | 描述 |
|------|------|
| VLTI=1 | 插入/替换 VLAN Type 字段 (CSVL 指定 C/S-VLAN) + 标签取自包发送上下文描述符的 VT 字段 |
| VLTI=0 | 插入/替换 VLAN Type 字段 + 标签取自 ETH_MACVIR.VLT 字段 |

- 替换或删除时 MAC 会检查发送包 DA/SA 后两字节是否为 VLAN Type (0x8100 或 0x88A8)，未检测到则不执行；插入时不检查，直接插入
- 对所有包使能：编程 ETH_MACVIR 的 VLC/VLP 字段；对选择性包使能：编程 TDES2 的 VTIR 字段，且 ETH_MACVIR/ETH_MACIVIR 的 VLP 位必须复位

### 63.5.3 包过滤

支持三类过滤，可级联：
- MAC 源/目的地址过滤：地址过滤模块 (AFM) 检查每包 SA/DA
- VLAN 过滤：VLAN 标签过滤 + VLAN Hash 过滤
- L3/L4 过滤：IP 源/目的地址过滤、TCP/UDP 源/目的端口过滤

过滤顺序 (Figure 777)：以太网 DA/SA 过滤 → VLAN 过滤 → IP 过滤 → TCP/UDP 过滤 → 通过则交付主机，任一失败则丢弃
- 未使能的过滤器被旁路，后续过滤器继续应用
- 被丢弃的包可按寄存器控制转发给主机：ETH_MACPFR.RA=1 时所有丢弃包均转发给主机（状态字指示具体哪个过滤器失败）；RA=0 时由 VTFE/IPFE 位控制 VLAN 过滤和 L3/L4 过滤失败的包是否丢弃或转发

#### MAC 源或目的地址过滤

单播 DA 过滤：
- 完美过滤 (HUC=0，默认)：支持 4 个 MAC 地址，比较接收单播地址全部 48 位，任一匹配即通过；MacAddr0 始终使能，MacAddr1-3 各自有位使能
- 逐字节掩码：ETH_MACAxHR 的掩码字节控制位可使对应字节不参与比较（用于组地址过滤）
- Hash 过滤 (HUC=1)：64 位 Hash 表非完美过滤，取接收 DA 的 CRC 高 6 位索引 Hash 表寄存器位 0-63，对应位=1 则通过

多播 DA 过滤：
- ETH_MACPFR.PM=1 时放行所有多播包
- PM=0 时由 HMC 位决定：完美模式用 MAC 地址寄存器比较（支持组地址过滤）；Hash 模式用 64 位 Hash 表（CRC 高 6 位索引）

Hash 或完美过滤 (HPF)：同时置 HPF 和 HUC/HMC 位时，DA 匹配任一过滤器即通过；HPF=0 时只应用其中一种

广播地址过滤：默认不过滤广播包；置 ETH_MACPFR.DBF 拒绝所有广播包

单播 SA 过滤：
- 默认将 SA 与寄存器值比较；置 ETH_MACAxHR 位 30 后该寄存器改为与 SA 比较
- 支持按字节掩码的组过滤
- ETH_MACPFR.SAF=1 时丢弃 SA 过滤失败的包（此时 SA/DA 过滤结果相与决定去留）；否则 SA 过滤结果只作为接收状态字中的状态位 (Table 499)

#### 反向过滤

- 置 ETH_MACPFR.DAIF 反转 DA 过滤最终结果（单播/多播均适用）；置 SAIF 反转 SA 过滤结果

#### DA/SA 过滤总结 (Table 498, Table 499)

DA 过滤 (Table 498)，PR=混杂模式 (Promiscuous)，X=任意值：

| 包类型 | PR | HPF | HUC | DAIF | HMC | PM | DBF | DA 过滤操作 |
|--------|----|----|-----|------|-----|----|-----|-------------|
| 广播 | 1 | X | X | X | X | X | X | 通过 |
| 广播 | 0 | X | X | X | X | X | 0 | 通过 |
| 广播 | 0 | X | X | X | X | X | 1 | 丢弃 |
| 单播 | 1 | X | X | X | X | X | X | 通过全部 |
| 单播 | 0 | X | 0 | 0 | X | X | X | 完美/组过滤匹配则通过 |
| 单播 | 0 | X | 0 | 1 | X | X | X | 完美/组过滤匹配则丢弃 |
| 单播 | 0 | 0 | 1 | 0 | X | X | X | Hash 匹配则通过 |
| 单播 | 0 | 0 | 1 | 1 | X | X | X | Hash 匹配则丢弃 |
| 单播 | 0 | 1 | 1 | 0 | X | X | X | Hash 或完美/组匹配则通过 |
| 单播 | 0 | 1 | 1 | 1 | X | X | X | Hash 或完美/组匹配则丢弃 |
| 多播 | 1 | X | X | X | X | X | X | 通过全部 |
| 多播 | X | X | X | X | X | 1 | X | 通过全部 |
| 多播 | 0 | X | X | 0 | 0 | 0 | X | 完美/组过滤匹配则通过 (PCF=0x 时丢弃 Pause 包) |
| 多播 | 0 | 0 | X | 0 | 1 | 0 | X | Hash 匹配则通过 (同上) |
| 多播 | 0 | 1 | X | 0 | 1 | 0 | X | Hash 或完美/组匹配则通过 (同上) |
| 多播 | 0 | X | X | 1 | 0 | 0 | X | 完美/组过滤匹配则丢弃 (同上) |
| 多播 | 0 | 0 | X | 1 | 1 | 0 | X | Hash 匹配则丢弃 (同上) |
| 多播 | 0 | 1 | X | 1 | 1 | 0 | X | Hash 或完美/组匹配则丢弃 (同上) |

SA 过滤 (Table 499)，PR=1 时全部通过：

| PR | SAIF | SAF | SA 过滤操作 |
|----|------|-----|-------------|
| 1 | X | X | 通过全部 |
| 0 | 0 | 0 | 完美/组过滤匹配则给出通过状态，但不丢弃失败的包 |
| 0 | 1 | 0 | 匹配则给出失败状态，不丢包 |
| 0 | 0 | 1 | 匹配则通过，丢弃失败的包 |
| 0 | 1 | 1 | 匹配则丢弃，失败也丢弃 |

RA=1 时所有包都转发给系统，地址过滤结果正确反映在 Rx 状态中。

#### VLAN 过滤

完美过滤：
- 比较接收包 VLAN 标签（按配置比较低 12 位或全部 16 位）提供 VLAN 匹配状态；不匹配的 VLAN 包被丢弃
- ETH_MACVTR.VTIM 使能反向匹配；ESVL 使能在默认 C-VLAN 外同时匹配 S-VLAN 包
- VLAN 匹配状态由 RDES0 位 10 (VLAN 包状态位) 指示
- 源/目的地址过滤优先于 VLAN 过滤：DA/SA 过滤失败的包直接丢弃，无论 VLAN 过滤结果

Hash 过滤：
- ETH_MACVTR.VTHM=1 使能；取 VLAN 标签 CRC-32 的最高 4 位索引 ETH_MACVHTR (16 位 VLAN Hash 表)，对应位=1 则通过
- CRC 计算使用 16 位还是 12 位标签由 ETH_MACVTR.ETV 决定：ETV=0 时 CRC 高 4 位取反后索引；ETV=1 时直接索引
- 完美与 Hash 同时使能时任一匹配即算匹配；反向匹配模式下两者都失配才转发

特殊规则 (Table 500)：
- ETH_MACVTR.VL 字段编程为 0 时 (VID=0)，所有 VLAN 包视为完美匹配
- RA=1 时所有包都接收，VLAN 匹配状态在 RDES2.VF 位；RA=0 且 ETH_MACPFR.VTFE=1 时最终状态为失配的包被丢弃

#### Layer 3 和 Layer 4 过滤

- L3 过滤 = IPv4/IPv6 包 IP 源/目的地址过滤；L4 过滤 = TCP/UDP 源/目的端口过滤
- 使能 L3/L4 过滤会自动使能 Rx IPC 完整校验和卸载引擎，必须置 ETH_MACCR.IPC
- 匹配包：转发并带状态（须 IPC=1）；多个过滤器同时匹配时只给最低编号过滤器 (Filter 0 最低) 的状态
- 不匹配包：丢弃；支持反向匹配（阻断特定 TCP/UDP over IP 字段、转发其余）；aborted/部分包在 MTL Rx FIFO 丢弃——若 Rx FIFO 为阈值 (cut-through) 模式且阈值很小，应用可能收到带 abort 状态的部分包
- 非 TCP/UDP 的 IP 包默认旁路不过滤，可编程为丢弃
- DA/SA 与 VLAN 过滤优先于 L3/L4 过滤

L3 过滤：
- IP 源/目的地址完美或反向匹配；可比较全部位或掩码低位
- IPv6：寄存器组后 4 个数据寄存器放 128 位源/目的地址（按 IPv6 规范字节序，首字节在寄存器高字节）
- IPv4：寄存器组第 2/3 个数据寄存器放 32 位源/目的地址，其余 2 个保留

L4 过滤：
- TCP 或 UDP 同时只能编程一种；第一个数据寄存器低 16 位为源端口、高 16 位为目的端口（首字节在寄存器高字节）

寄存器组：两组独立过滤寄存器
- 组 0：ETH_MACL3L4C0R + ETH_MACL4A0R + ETH_MACL3A00R/A10R/A20R/A30R
- 组 1：ETH_MACL3L4C1R + ETH_MACL4A1R + ETH_MACL3A01R/A11R/A21R/A31R

### 63.5.4 IEEE 1588 时间戳支持

IEEE 1588 定义精密时间协议 (PTP)，通过局域网（支持多播消息，如以太网）实现测量与控制系统中时钟的精确同步，异构系统同步精度可达亚微秒级。本章节包含：
- IEEE 1588 系统时间源
- IEEE 1588 辅助快照
- 灵活秒脉冲 (PPS) 输出
- PTP 时间戳卸载功能
- One-step 时间戳

#### 特性与版本支持

- 支持 IEEE 1588-2002 (v1，PTP over UDP/IP) 和 IEEE 1588-2008 (v2，PTP over Ethernet)，两者可编程选择
- 可选快照范围：全部包或仅 PTP 包、仅事件消息、按时钟类型 (普通/边界/E2E 透明/P2P 透明)、可选节点作主或从
- 识别直接经以太网发送的 PTP 消息类型、版本和载荷并给出状态
- 可选亚秒时间以数字或二进制格式测量
- 同一时刻只能对 v1 或 v2 之一打快照：ETH_MACTSCR.TSVER2ENA 选择版本 + 快照模式

#### 时钟类型

- 普通时钟 (Ordinary clock)：单端口、单 PTP 状态；可作 grandmaster 或 slave；维护时间戳数据组
  - 快照消息 (Table 501)：Master 对 Delay_Req 打快照；Slave 对 SYNC 打快照
- 边界时钟 (Boundary clock)：多个物理端口；同步/主从层级/信令消息终止在本时钟协议引擎内不转发；数据集和本地时钟为所有端口共享
- 端到端透明时钟 (E2E TC)：像普通桥/路由器一样转发所有消息；SYNC 的驻留时间 (residence time，入口到出口耗时) 写入对应 Follow_Up 包的修正字段，Delay_Req 的驻留时间写入对应 Delay_Resp 包；入口和出口都需打快照 (Table 502: SYNC、Delay_Req)；ETH_MACTSCR.SNAPTYPSEL=10
- 对等透明时钟 (P2P TC)：链路延迟基于与对端的 Pdelay_Req/Pdelay_Resp/Pdelay_Resp_Follow_Up 交换计算；Pdelay_Req 与 Pdelay_Resp 的驻留时间之和写入对应 Pdelay_Resp_Follow_Up 的修正字段 (Table 503: SYNC、Pdelay_Req、Pdelay_Resp)；ETH_MACTSCR.SNAPTYPSEL=11

#### 延迟请求-响应机制 (Figure 778)

主从同步流程：
1. Master 广播 Sync 消息，离开主机系统时刻 t1 被捕获 (在 GMII/MII 接口处)
2. Slave 接收 Sync 并捕获精确到达时刻 t2
3. Master 发 Follow_up 消息携带 t1
4. Slave 发 Delay_Req 消息并记录离开接口时刻 t3
5. Master 接收 Delay_Req 并捕获进入时刻 t4
6. Master 经 Delay_Resp 消息把 t4 发给 Slave
7. Slave 用 t1/t2/t3/t4 四个值同步本地时钟

PTP 大部分实现是 UDP 层之上的软件；硬件的作用仅在包进出 GMII/MII 接口的瞬间捕获精确时间并返回给软件——这是高精度同步的关键。

#### 对等透明时钟 P2P 消息支持 (Figure 779)

P2P TC 传播延迟计算流程：
1. Port 1 发出 Pdelay_Req 并生成时间戳 t1
2. Port 2 接收 Pdelay_Req 生成 t2
3. Port 2 尽快回复 Pdelay_Resp 并生成 t3 (减少两端口频率偏移误差)；t2-t3 差值可放在 Pdelay_Resp 或 Pdelay_Resp_Follow_Up 中，或两个时间戳分开放
4. Port 1 接收 Pdelay_Resp 生成 t4
5. Port 1 用四个时间戳计算平均链路延迟

#### 时间戳修正

- 按 IEEE 1588 规范，时间戳应在消息时间戳点（SFD 八位组后紧随的八位组的第一个比特前沿）越过节点与网络边界时捕获
- MAC 实际在内部某点捕获，偏离真实边界，因此捕获值需修正入/出路径延迟（含 PHY 延迟）及 CDC (时钟域跨越) 采样误差

入向 (Ingress) 修正：捕获时间戳比端口边界晚，需减去入向延迟，软件计算后写入 ETH_MACTSICNR.TSIC，由三个分量组成：
1. PHY 外部延迟（边界点到核输入）：符合 IEEE 802.3 Clause 45 的 PHY 可从 MMD 寄存器读最大/最小延迟取平均；否则查 PHY 数据手册
2. 核内部延迟（输入到内部捕获点）：随 PHY 接口和速率而变 (Table 504)
3. CDC 同步误差：约为 PTP 时钟周期 (clk_ptp_ref_i) 的两倍

写入值必须为负 (二进制补码)：TSCTRLSSR=1 (纳秒十进制格式) 时位 31=1、位 [30:0] 写 "10^9 - 总修正值"，如 -5 ns 写 0xBB9A C9FB；TSCTRLSSR=0 (二进制格式, 粒度 ~0.466 ns) 时写 "2^31 - 总修正值"

出向 (Egress) 修正：捕获时间戳比端口边界早，需补偿出向延迟，写入 ETH_MACTSECNR，三个分量：
1. PHY 外部延迟（核输出到端口与网络边界）
2. 核内部延迟（捕获点到核输出）(Table 504)
3. CDC 同步补偿：EGRESS_SYNC_CORR = (1 - PTP_CLK_PER + 4 x TX_CLK_PER)

Table 504 各 PHY 接口延迟 (ns)：

| PHY 接口 | Egress 延迟 | Ingress 延迟 |
|----------|-------------|--------------|
| RGMII 1 Gbps | 12 | 12 |
| RGMII 100 Mbps | 40 | 40 |
| RGMII 10 Mbps | 400 | 400 |
| RMII 100 Mbps | 40 | 120 |
| RMII 10 Mbps | 400 | 800 |

#### 参考时间时钟 (clk_ptp_ref_i) 频率范围

- 时间戳跨异步时钟域传输，两次连续捕获间至少间隔 4 个 GMII/MII 时钟周期 + 3 个 PTP 时钟周期；间隔不足时第二个包不打快照
- 最大频率：受参考时间分辨率限制 (100 MHz 时 10 ns)，频率越高同步精度越好
- 最小频率：由连续两个 SFD 间的最小间隔决定：3 x PTP 周期 + 4 x GMII/MII 周期 <= 最小 SFD 间隔 (Table 505)，1000 Mbps 全双工下最小 PTP 频率 5 MHz，半双工 (碰撞 JAM 场景) 18.75 MHz

#### PTP 消息识别 (Table 506/507/508)

MAC 通过匹配以太网载荷特定字段来识别 PTP 包类型并控制打快照，三种封装方式：

IPv4 over UDP (Table 507)：
- MAC 包类型 (12,13 字节)：0x0800 IPv4
- IP 版本/头部长度 (14 字节)：0x45；L4 协议 (23 字节)：0x11 UDP
- IP 多播地址 (30-33 字节)：v1 用 224.0.1.129/130/131/132 (0xE0,0x00,0x01,0x81-0x84)，v2 主多播 224.0.1.129、Pdelay 多播 224.0.0.107 (0xE0,0x00,0x00,0x6B)
- UDP 目的端口 (36,37 字节)：0x013F 事件消息、0x0140 普通消息
- v1 控制字段 (74 字节)：0x00 SYNC / 0x01 Delay_Req / 0x02 Follow_Up / 0x03 Delay_Resp / 0x04 Management
- v2 消息类型字段 (42 字节低半字节)：0x0 SYNC / 0x1 Delay_Req / 0x2 Pdelay_Req / 0x3 Pdelay_Resp / 0x8 Follow_Up / 0x9 Delay_Resp / 0xA Pdelay_Resp_Follow_Up / 0xB Announce / 0xC Signaling / 0xD Management
- v2 版本字段 (43 字节低半字节)：0x1 或 0x2

IPv6 over UDP (Table 508)：
- MAC 包类型 (12,13 字节)：0x86DD IPv6；IP 版本 (14 字节高半字节)：0x6；L4 协议 (20 字节)：0x11 UDP
- PTP 多播地址 (38-53 字节)：FF0x::181 主多播、FF02::6B Pdelay 多播
- UDP 目的端口 (56,57 字节)：0x013F/0x0140

PTP 消息头格式 (IEEE 1588-2008, Table 506)：transportSpecific + messageType (偏移 0)、versionPTP (1)、messageLength (2)、domainNumber (4)、flagField (5)、correctionField (8)、sourcePortIdentity (10)、sequenceId (20)、controlField (v1 用, 30)、logMessageInterval (33)

IPv6 续 (Table 508)：v1 控制字段 (94 字节)、v2 消息类型 (62 字节低半字节)、v2 版本 (63 字节低半字节)

以太网直接封装 (Table 509，不用 IP/UDP)：
- MAC 目的多播地址 (0-5 字节)：01-1B-19-00-00-00 或 01-80-C2-00-00-0E (IEEE 1588-2008 Annex F；01-80-C2 地址不用于 v1)
- MAC 包类型 (12,13 字节)：0x88F7 PTP
- v1 控制字段 (46 字节)；v2 消息类型 (14/15 字节半字节)、版本 (15 字节)
- 置 ETH_MACTSCR.TSENMACADDR 时也匹配编程在 MAC 地址 0-31 中的单播 DA

#### 收发路径打戳功能

Tx 路径：
- 包 SFD 在 GMII/MII 接口发出的瞬间捕获时间戳
- 逐包控制：发送描述符 (63.10.3) 中的控制位标记该包是否要打戳——MAC 不分析发送包识别 PTP，由软件指定
- 64 位时间戳写回对应发送描述符的 TDES0 (低 32 位) 和 TDES1

Rx 路径：
- 可编程为对所有接收包打戳，或识别有效 PTP 消息后打戳，选项 (ETH_MACTSCR)：全部包 / v1 或 v2 / 直接以太网或 UDP-IP 封装 / IPv4 或 IPv6 / 仅事件消息 (SYNC、Delay_Req、Pdelay_Req、Pdelay_Resp) / 节点作 master 或 slave 并选快照类型
- 支持带 VLAN 标签的 PTP 消息
- 打戳消息组合由 SNAPTYPSEL/TSMSTRENA/TSEVNTENA 决定 (Table 510)：

| SNAPTYPSEL | TSMSTRENA | TSEVNTENA | 打快照的 PTP 消息 |
|------------|-----------|-----------|-------------------|
| 00 | X | 0 | SYNC、Follow_Up、Delay_Req、Delay_Resp |
| 00 | 0 | 1 | SYNC |
| 00 | 1 | 1 | Delay_Req |
| 01 | X | 0 | SYNC、Follow_Up、Delay_Req、Delay_Resp、Pdelay_Req、Pdelay_Resp、Pdelay_Resp_Follow_Up |
| 01 | 0 | 1 | SYNC、Pdelay_Req、Pdelay_Resp |
| 01 | 1 | 1 | Delay_Req、Pdelay_Req、Pdelay_Resp |
| 10 | X | X | SYNC、Delay_Req |
| 11 | X | X | Pdelay_Req、Pdelay_Resp |

- 时间戳经对应接收描述符返回：消息状态与 IPC 状态在 RDES1 扩展状态中，快照写入上下文描述符的 RDES0/RDES1 字段 (RDES0 为低 32 位)

### 63.5.5 IEEE 1588 系统时间源

#### 时间戳格式

64 位时间戳（IEEE 1588-2002 格式，2008 为 80 位）：
- 秒字段：UInteger48，48 位整数秒，如 2.000000001 s 的秒字段 = 0x0000 0000 0002
- 纳秒字段：UInteger32，两种模式由 ETH_MACTSCR.TSCTRLSSR 选择：
  - 数字翻转模式：最大值 0x3B9A C9FF (10^9-1)，到 10^9 进位给秒
  - 二进制翻转模式：到 0x7FFF FFFF 进位，每比特精度约 0.466 ns

#### 系统时间寄存器模块

- 64 位 PTP 时间由参考输入时钟 clk_ptp_ref_i 驱动递增，用作收发帧打快照的时间源
- 初始化或修正有两种方法：
  - 粗调 (coarse)：把初始值或偏移值写入时间戳更新寄存器——初始化时直接用该值装载计数器；修正时该值被加到/减到系统时间上，单个时钟周期内完成，时间会跳变
  - 细调 (fine)：校正从钟相对主钟的频率漂移，在一段时间内逐渐完成，不引入剧烈跳变（避免 Sync 消息间隔内时间抖动）；32 位累加器反复累加 Addend 寄存器内容 (Figure 780)，产生的算术进位脉冲用于递增系统时间计数器——累加器与加数构成高精度频率倍频/分频器

