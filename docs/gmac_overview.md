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
