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

1. 包先存入**浅缓冲 (shallow buffer)** 等地址过滤完成；过滤失败直接在 MAC 内丢弃（Figure 776 流程：CRC 错误或需过滤 → 丢弃包只发状态给应用层）
2. RxDV 有效 → 接收状态机（RSM）寻找 SFD 字段（手册写 0x5D，标准 SFD 为 0xD5，应为手册笔误）
3. 检测到 SFD → 从目的地址（DA）字段开始把包发给 RPC 模块（前导码和 SFD 被剥离）
4. 使能 IEEE 1588 时间戳时在 SFD 检测处快照系统时间；包未被过滤丢弃则时间戳传给应用
5. Length/Type 字段解释：
   - **< 1536（Length 解释）**且 MACCR 位 20（Auto CRC/Pad Stripping）使能：只发送 Length 字段指定字节数，丢弃填充和 FCS 字段
   - **≥ 1536（Type 解释）**：默认发送全部数据；若 MACCR 位 21（Type 包 CRC 剥离）使能且未使能接收校验和卸载，剥离所有 ether type 包的最后 4 字节
6. 看门狗：默认使能，>2048 字节（Jumbo 使能时 10240）的包在 RPE 模块截断；可用 ETH_MACWTR 位 16 覆盖固定超时；MACCR 位 19 可关闭看门狗；即使关闭，>32 Kbytes 的包仍被截断并给出看门狗超时状态

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

#### IEEE 1588 系统时间源

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

细调参数计算 (Figure 780)：
- 系统时间更新逻辑要求 50 MHz 时钟达到 20 ns 精度；频率分频比 = 参考时钟频率 / 50 MHz
- 例：clk_ptp_ref_i = 66 MHz，比值 66/50 = 1.32，默认加数值 = 2^32 / 1.32 = 0xC1F0 7C1F
- 时钟漂移更低 (65 MHz) 时加数 = 2^32/1.30 = 0xC4EC 4EC4；更高 (67 MHz) 时 = 0xBF0B 7672
- 亚秒寄存器累加的常数为 43 (20 ns 步进)
- 初始加数 FreqCompensationValue0 = 2^32 / FreqDivisionRatio

频率锁定算法（假设相邻 Sync 的 MasterToSlaveDelay 相同，几个 Sync 周期后锁定）：
1. 第 n 次 Sync：主钟 MasterSyncTime_n 发出，从钟本地 SlaveClockTime_n 收到，算 MasterClockTime_n = MasterSyncTime_n + MasterToSlaveDelay_n
2. 主钟计数 MasterClockCount_n = MasterClockTime_n − MasterClockTime_(n−1)
3. 从钟计数 SlaveClockCount_n = SlaveClockTime_n − SlaveClockTime_(n−1)
4. 计数差 ClockDiffCount_n = MasterClockTime_n − SlaveClockTime_n
5. 频率缩放因子 FreqScaleFactor_n = (MasterClockCount_n + ClockDiffCount_n) / SlaveClockCount_n
6. 新加数 FreqCompensationValue_n = FreqScaleFactor_n × FreqCompensationValue_(n−1)

理论上一个 Sync 周期即可锁定（算法自校正），实际因网络延迟变化需多个周期。

#### IEEE 1588 辅助快照

- 基于外部事件（eth_ptp_trgx 边带信号上升沿，x = 1 到 4）存储系统时间快照
- 最多 4 个辅助快照输入，最多存 4 个快照，经 FIFO 访问：ETH_MACATSSR (秒) 和 ETH_MACATSNR (纳秒) 读出；各输入快照存于同一 FIFO (仅 64 位)
- ETH_MACTSSR 指示 FIFO 顶部是哪个输入的快照；存好快照时产生中断通知应用
- FIFO 满时再触发则不写入，置 ATSSTM (快照触发丢失) 标志；读出后腾出空间
- ETH_MACACR.ATSFC 清空 FIFO；FIFO 中快照数量在 ETH_MACTSSR bits[27:25]

#### 灵活秒脉冲输出 (PPS)

ETH_PPS_OUT / eth_ptp_pps_out 输出支持固定或灵活模式，默认固定模式（一秒一个脉冲）：

固定模式 (ETH_MACPPSCR.PPSEN0=0)：
- 所有 PPS 输出由 PPSCTRL_PPSCMD 字段控制，不支持单独控制每个输出
- 只能通过 PPSCTRL0 字段改变 PPS 输出频率
- ETH_MACPPSTTSR/PPSTTNR 仅用于产生目标时间到达中断，不用于 PPS 输出
- TRGTMODSEL0/1/2/3 必须编程为 0

灵活模式 (PPSEN0=1)：
- 软件可编程脉冲的启动/停止时间、宽度和间隔
- 启动/停止时间：ETH_MACPPSTTSR (秒) + ETH_MACPPSTTNR (纳秒)
- 宽度和间隔：ETH_MACPPSWR / ETH_MACPPSIR，以系统时间亚秒增量值的个数为单位
- 可编程单个脉冲起点、脉冲串的起止点（64 位系统时间）；停止时间可提前编程（早于先前开始时间到达前）
- 宽度/间隔范围 1 到 2^32-1 个亚秒增量单位；例：PTP 参考时钟 50 MHz 时，宽度 40 ns 写 2、间隔 100 ns 写 5（单位 = 20 ns）；参考时钟越快，粒度越细
- 先编宽度和间隔，再发触发脉冲/脉冲串的命令
- 重新编程开始/停止时间仅可在前值同步到 PTP 时钟域后（ETH_MACPPSTTNR 位 31 指示同步完成）
- 已过期的开始/停止时间：MAC 置错误状态位并产生 Target Time Reached 中断；仅未过期时可取消请求（过期后取消无效）
- 建议编程超前的系统时间以保证正确输出

#### PTP 时间戳卸载功能 (PTP offload)

- 使 MAC 作为 PTP 网络中特定节点时自动生成 PTP 包：可周期生成、由主机软件触发，或收到特定 PTP 消息后自动应答——卸载部分 PTP 节点功能，精度更高、响应延迟更低
- 通过 ETH_MACPOCR 选择；80 位 PTP 节点身份由 ETH_MACSPI0R/SPI1R/SPI2R 配置
- 消息生成条件 (Table 511)：

| 编程 (SNAPTYPSEL/TSMSTRENA/TSEVNTENA) | 模式 | 生成条件 | 生成的包 |
|----------------------------------------|------|----------|----------|
| 00/0/1 | 普通或边界从钟 | 收到 SYNC | Delay_Req |
| 00/0/1 | 同上 | 周期或应用触发 | SYNC、Delay_Resp、Pdelay_Req、Pdelay_Resp |
| 00/1/1 | 普通或边界主钟 | 收到 Delay_Req | Delay_Resp |
| 00/1/1 | 同上 | 周期或应用触发 | SYNC、Delay_Req |
| 01/0/1 | 透明从钟 | 收到 Pdelay_Req | Pdelay_Resp |
| 01/0/1 | 同上 | 收到 SYNC；周期或应用触发 | Delay_Req、Pdelay_Req |
| 01/1/1 | 透明主钟 | 收到 Delay_Req | Delay_Resp |
| 01/1/1 | 同上 | 周期或应用触发 | SYNC、Pdelay_Req |
| 11/X/X | 对等透明钟 | 周期或应用触发；收到 Pdelay_Req | Pdelay_Req、Pdelay_Resp |

- 其他编程组合对 PTP offload 无效
- ETH_MACPOCR.DRRDIS：控制是否响应 delay request/response（按 IEEE 1588-2008 对等延迟机制钟不应生成 delay 消息，此位提供灵活性）；如透明从钟模式仅在 DRRDIS=0 时响应收到的 SYNC 生成 Delay_Req
- 多播模型：生成的 PTP over Ethernet 包 DA 用特殊多播地址——普通消息 0x011B19000000、对等延迟机制消息 0x0180C200000E；应答收到的单播 SYNC/Delay_Req/Pdelay_Req 时，取接收包 SA 作应答包 DA
- 收到的 PTP 消息仍全部转发给应用，Rx 状态指示该响应是否由 MAC 生成
- 自动生成的 Pdelay_Req/Delay_Req 出向时间戳在 Tx 时间戳状态 (寄存器 + 中断) 提供

额外匹配字段（除 messageType/versionPTP 外，用于确认收到的 PTP 消息类型）：
1. domainNumber 与 CSR 编程值匹配
2. flagField 的 twoStepFlag 检查 one-step 指示 (0b0)
3. transportSpecific 检查 Default PTP over Ethernet (0b0000) 或 802.1AS 模式 (0b1111)（使能时）

自动生成包格式：公共 PTP 消息头模板见 Table 512（同 Table 506：transportSpecific+messageType、versionPTP、messageLength、domainNumber、flagField、correctionField、sourcePortIdentity、sequenceId、controlField、logMessageInterval）

生成的包头字段规则：
- messageType 编码：SYNC 0000 / Delay_Req 0001 / Pdelay_Req 0010 / Pdelay_Resp 0011 / Delay_Resp 1001；versionPTP 恒为 2；transportSpecific：Default PTP over Ethernet 0000 或 802.1AS 0001
- domainNumber 取自 ETH_MACPOCR；flagField：alternateMasterFlag (SYNC/Delay_Resp 为 0)、twoStepFlag (SYNC/Pdelay_Resp 为 0)、unicastFlag (多播 0 / 单播 1)
- sourcePortIdentity 取自 ETH_MACSPI0R/1R/2R
- sequenceId：Pdelay_Resp/Delay_Resp 沿用收到的 Pdelay_Req/Delay_Req 的 sequenceId；SYNC/Delay_Req、Pdelay_Req 各自独立计数器，每生成发送一次 +1
- logMessageInterval：SYNC 用 ETH_MACLMIR 的 logSyncInterval；Delay_Resp 用 DRSYNCR + logSyncInterval (多播) 或 0x7F (单播)；Delay_Req/Pdelay_Req/Pdelay_Resp 为 0x7F
- logSyncInterval 范围 -15 到 15 (2^-15 = 32.768μs 到 2^15 秒)；Sync 间隔：N 为正时 2^N 秒，N 为负时 2^(30+N) ns (例：1 → 2 秒，-1 → 536 ms，-5 → 33.55 ms)；负值因纳秒字段非二进制性质可能轻微偏离
- messageLength：34 字节公共头 + 消息体——SYNC/Delay_Req 为 44，Delay_Resp/Pdelay_Req/Pdelay_Resp 为 54
- 消息特定字段：originTimestamp = SYNC/Delay_Req/Pdelay_Req 的出向时间戳；receiveTimestamp (Delay_Resp) = 对应收到的 Delay_Req 的入向时间戳；requestingPortIdentity (Delay_Resp/Pdelay_Resp) = 收到的 Delay_Req/Pdelay_Req 的 sourcePortIdentity；requestReceiptTimestamp (Pdelay_Resp) = 0

#### One-step 时间戳

- 识别包内偏移，在该偏移处插入应用提供的时间戳
- Tx 时更新：PTP 头 correctionField，以及 SYNC/Delay_Req/Pdelay_Req 的 originTimestamp 字段
- 使能：TDES3 上下文描述符位 20 (OSTC)；需更新 correctionField 时把入向时间戳放入 TSSL/TSSH 字段
- 仅支持 PTP over Ethernet，不支持 PTP over IPv4/IPv6
- 驻留时间/周转时间 = 出向捕获时间戳 − 入向时间戳
- TTSE (TDES2) 独立于 OST 功能：置 TTSE 时 MAC 捕获时间戳并返回应用
- Table 513 各模式 one-step 操作（SNAPTYPSEL/TSMSTRENA/TSEVNTENA → 每包控制 TTSE/OSTC/TTS → 处理的字段）：
  - 00/X/0 端到端透明：Sync/Delay_Req correctionField (驻留时间 + 入/出向非对称修正)
  - 00/0/1 普通或边界从钟：Delay_Req originTimestamp 或 correctionField (出向非对称修正)
  - 00/1/1 普通或边界主钟：Sync originTimestamp 或 correctionField (亚纳秒修正)
  - 01/X/0 端到端透明支持对等延迟：Sync/Pdelay_Req/Pdelay_Resp correctionField (驻留时间 + 非对称修正)
  - 01/0/1 从钟带对等延迟或 P2P 透明：Sync correctionField、Delay_Req/Pdelay_Req originTimestamp 或 correctionField、Pdelay_Resp correctionField (周转时间)
  - 01/1/1 主钟带对等延迟：Sync/Pdelay_Req originTimestamp 或 correctionField、Pdelay_Resp correctionField
  - 10/X/X 端到端透明：Sync/Delay_Req correctionField (驻留时间)
  - 11/X/X P2P 透明：Sync correctionField、Pdelay_Req originTimestamp 或 correctionField、Pdelay_Resp correctionField

### 63.5.5 校验和卸载引擎 (COE)

- TCP/UDP over IP 校验和：Tx 路径计算并插入校验和（减轻软件负担、提高吞吐），Rx 路径错误检测
- Tx 每包控制：TDES3 bits[17:16] (CIC 字段)

#### Tx 校验和的隐含约束

- TCP/UDP/ICMP 校验和必须对完整包计算后再插入头字段 → Tx FIFO 自动以存储转发 (store-and-forward) 模式工作，即使 MAC 配置为阈值 (cut-through) 模式
- 约束：仅对满足 Packet size < TxQSize − (PBL + 5) × 9 的包使能校验和插入（TxQSize = ETH_MTLTXQxOMR.TQS，PBL = ETH_DMACxTXCR.TxPBL）——FIFO 空间不足时 MTL 提前读包头，校验和插入会失败
- 参考 RFC 791/793/768/792/2460/4443 (IPv4/TCP/UDP/ICMP/IPv6/ICMPv6)

#### IP 头校验和引擎

- 识别条件：以太网 Type = 0x0800 且 IP 版本 = 0x4；计算 16 位头校验和，忽略输入包的校验和字段并替换为计算值
- IPv6 头无校验和字段，不改动
- 结果状态：TDES3 写回格式位 0 (IP Header Error)——以下情况置位：
  - IPv4：Type=0x0800 但版本 ≠ 0x4；IPv4 头长度字段 < 0x5 (20 字节)；总包长小于头长度字段值
  - IPv6：Type=0x86DD 但版本 ≠ 0x6；包在 IPv6 头 (40 字节) 或扩展头完整接收前结束

#### TCP/UDP/ICMP 校验和引擎

- 处理 IPv4/IPv6 头（含扩展头）确定载荷是 TCP/UDP/ICMP，计算校验和插入对应字段，两种模式：
  1. 不包含伪头 (pseudo-header)：假定输入包校验和字段里已有伪头，引擎把校验和字段计入计算后替换
  2. 包含伪头：忽略校验和字段，把伪头数据计入计算后覆写
- ICMP-over-IPv4 两种模式下校验和字段必须为 0x0000（无伪头定义），否则插入错误校验和
- 结果状态：TDES3 写回格式位 12 (Payload Checksum Error)——包在 EOP 写入 FIFO 前就被转发给 MAC 发射机（store-and-forward 下），或包在 IP 头载荷长度字段指示的字节数前结束；包长于载荷长度时多余部分按填充字节忽略不报错；第一类错误不修改 TCP/UDP/ICMP 头，第二类错误仍插入计算值

Table 514 各包类型的校验和插入能力 (摘要)：
- 非 IPv4/IPv6 包：两者都不插
- 普通 IPv4 + TCP/UDP/ICMP：都插；带 IP 选项 (头 > 20 字节)：都插；TCP 头带选项：都插
- IPv4 分片包：只插 IP 头校验和，不插 TCP/UDP
- IPv4 隧道 (IPv4-in-IPv4 / IPv6-in-IPv4)：只对 IPv4 隧道头插校验和，内层不插；IPv6 隧道：都不插
- IPv6 各 next header 组合：仅 TCP/UDP/ICMP 或部分扩展头 (hop-by-hop、destination options、routing segment left 0) 支持载荷校验和；带认证头 (Authentication) 等不支持
- 带 802.3ac VLAN 标签的 IPv4/IPv6：正常支持
- 带安全特性 (封装安全载荷等) 的帧：不插载荷校验和

#### Rx 校验和卸载引擎 (Rx COE)

- 使能：ETH_MACCR.IPC；使能后同时检测处理收到的 IPv4 (Type=0x0800) 和 IPv6 (0x86DD) 包；识别适用于单 VLAN 标签包，双 VLAN 标签包需置 ETH_MACVTR.EDVLP
- IPv4 头校验和：计算并比对接收的校验和字段，结果 (通过/失败) 经 RFC 模块插入接收状态字；IP Header Error 位条件：载荷类型 (以太网 Type) 与 IP 头版本不符、包字节数不足 (少于 IPv4 头 Length 字段指示值或 IPv4/IPv6 头不足 20 字节)
- 有 TCP/IP 错误 (头或载荷) 的包：ETH_MTLRXQxOMR.DIS_TCP_EF=0 且 FEP=1 时在 MTL 丢弃
- TCP/UDP/ICMP 载荷校验和：包含 TCP/UDP/ICMPv6 伪头字节计算，比对接收的校验和字段；结果作为接收状态字 Payload Checksum Error 位；TCP/UDP/ICMP 载荷长度与 IP 头指示的期望长度不符也置该位
- MAC 不向接收包追加任何载荷校验和字节
- Table 515 (Rx 支持表) 与 Table 514 (Tx) 基本对称：普通 IPv4+TCP/UDP/ICMP 头与载荷都检查；IPv4 分片只查 IP 头；隧道/认证头/安全载荷场景限制相同

### 63.5.6 TCP 分段卸载 (TSO)

- DMA 把一个大 TCP 包拆成多个小段 (按 MSS) 交给 MTL (Figure 781)；使能：ETH_DMACxTXCR.TSE；仅全双工模式支持
- TSO/USO 选择：由包首发送描述符 TDES3 的 THL 字段 (TCP Header Length) 决定——THL=2 为 USO (UDP)，THL≥5 为 TSO (TCP)

DMA 操作流程 (Figure 782)：
1. 应用设置发送描述符 (TDES0-3) 和数据缓冲区，置 Own 位 (TDES3[31])
2. 增加 DMA Tx 通道描述符尾指针偏移
3. 运行态 DMA 取下一个描述符：
   - 上下文描述符不在包的首尾描述符之间 → 保存上下文值
   - 上下文描述符位于包的首尾之间 → 关闭该描述符并置 Context Descriptor Error (TDES3[23])
   - 普通描述符 → 检查 TSE 位，未置位则默认模式或 OSF 模式
4. 由 TCP 载荷长度 (TDES3[17:0]) 和 MSS 计算段数
5. 通道仲裁取数据缓冲，头与载荷分别取
6. 首段：从系统内存取头存入 TSO 内存 (存在且头长不超 TSO 内存大小时)；非首段时重新取头缓冲——DMA 不关闭含头缓冲的首描述符，直到最后一段的头被取走
7. 按分段需求修改/更新头字段，写入对应 MTL Tx 队列
8. 取载荷缓冲指针，取 MSS 字节直接推入 MTL Tx 队列；缓冲数据不足 MSS (非最后段) 时关闭该描述符
9. 重复直到最后一段写入 Tx 队列
10. 关闭最后描述符和首描述符，继续下一包；无描述符时进入挂起状态

分段时 DMA 自动更新 TCP/IP 头字段 (Table 516)：
- 首段：TCP 序号不变 (用头中原值)；FIN/PSH 置位则清除；TCP 校验和重算。IPv4：Total Length = MSS + TCP 头长 + IP 头长，Identification 字段不改，IP 头校验和重算；IPv6：Payload Length = MSS + TCP 头长 + IP 扩展头长
- 中间段：序号 = 前一段序号 + MSS；FIN/PSH 清除；校验和重算。IPv4：Total Length 同上，Identification = 前一段 + 1，头校验和重算
- 最后段：序号 = 前一段序号 + MSS；原头中 FIN/PSH 置位则恢复置位；校验和重算。IPv4：Total Length = 剩余载荷 + TCP 头长 + IP 头长，Identification +1，头校验和重算

分段包头部/载荷布局 (Figure 783)：
- 头 (以太网 + L3 + L4 头) 必须放在首普通描述符的 Buffer1，头长填在首描述符 TDES2 (FD=1)；FD=0 的描述符 TDES0/TDES1 指向的地址都按载荷缓冲处理
- 载荷从首描述符 Buffer2 起，可跨多个缓冲、多个描述符；各载荷缓冲大小之和必须等于 TDES3[17:0] 填的 TCP 载荷长度

上下文描述符与 MSS：
- 上下文描述符提供 MSS：必须放在对应 TCP 包的首普通描述符之前；新 MSS 需在新包前再建上下文描述符
- MSS 仅在 TDES3 的 TCMSSV 置位且 OSTC 复位时有效；DMA 内部保存该值，用于后续所有置 TSE 的包；MSS 也可编程在 ETH_DMACxCR——以后者 (上下文) 为准
- 上下文描述符位于包首尾描述符之间：DMA 忽略并关闭该描述符、置 Context Descriptor Error、ETH_DMACxIER.CDEE 使能时产生中断 (状态在 ETH_DMACxSR.CDE)

TSO 描述符与包构造规则：
- 置首普通描述符 TDES3.TSE 使能分段；非 TCP/IP 包置 TSE 行为不可预测
- TDES3[17:0] = TCP 载荷长度 (最大 256 KB)；TDES3[22:19] = TCP 头长 (AV 配置下该字段是槽号——TSO 与 AV 同时使能时不要使能槽号检查 ETH_DMACxSFCSR.ESC)
- MAC 总是自动算 CRC 和填充；置 TSE 时 TDES3 的 CPC 字段保留
- 限制：头长 + MSS ≤ 16383 字节 (否则 MAC 在 16383 处截断导致 CRC 错误)；头长 + MSS + PBL < Tx 队列大小 (ETH_MTLTXQxOMR.TQS)，建议 MSS + 头长 ≤ 队列一半
- VLAN：单标签时所有分段沿用同一标签；双 VLAN 特性选中时两个标签都用于全部分段；未选中双 VLAN 时不得对双标签包置 TSE
- TCP 头长 < 5 (无效头) 时 DMA 不分段整包发送，强制 CPC=00 (MAC 做 CRC 和填充)、校验和插入控制强制 11 (硬件算头+载荷)

### 63.5.7 IPv4 ARP 卸载

- 收路径处理 IPv4 ARP 请求包 (Length/Type = 0x0806)，发路径自动生成对应 ARP 应答
- 流程：
  1. 收到 ARP 请求，其 Target Protocol Address 匹配 MAC L3 寄存器编程的 IPv4 地址
  2. 生成 ARP 应答：请求的 Sender Hardware Address → 应答的 DA 字段 + Target Hardware Address；请求的 Sender Protocol Address → 应答的 Target Protocol Address；自己的 MAC 地址 → SA 字段 + Sender Hardware Address；请求的 Target Protocol Address → 应答的 Sender Protocol Address；opcode = 2 (ARP reply)
  3. MAC 重算 CRC 并填充，发送应答
- 一次只处理一个 ARP 请求，不保存多个请求的字段；处理中收到新 ARP 请求不生成应答——转发给应用并置 ARP Reply Not Generated 状态位 (位 34)；power-down 模式下直接丢弃新请求
- ETH_MACECR.DCRCC 置位时 MAC 不检查 ARP 请求的 CRC (但请求包本身必须有效 CRC)；小于 64 字节的 ARP 请求不做应答，按普通包交给应用过滤

### 63.5.8 回环 (Loopback)

- 使能：ETH_MACCR.LM 位，所有 PHY 接口都可用；数据总是经内部异步 FIFO 回环到内部接收 MII/GMII 接口，同时也会经发射 PHY 接口块输出到以太网线上——即回环时包也会出现在线上 (数据反映在 gmii_txd 信号)

使用准则：
- 仅全双工模式使能回环（半双工下载波侦听/碰撞信号采样可能导致丢包）
- 无 PHY 芯片连接时，需外部生成 Tx 和 Rx 时钟提供给 MAC
- 不要回环大包——可能被回环 FIFO 损坏

机制：
- Tx/Rx 时钟异步，回环路径用异步 FIFO（写时钟 eth_tx_clk、读时钟 eth_rx_clk）；每包读取开始时写/读指针重置为偏移 2 或 4 (10/100 Mbps 模式)，避免包传输中溢出/下溢——溢出/下溢只可能发生在包间 IPG 期间
- FIFO 深度 5 或 9 足以支撑最大 9022 字节包（(G)MII Tx/Rx 时钟差 200 ppm 时）
- 收包结束时 Receive Protocol Engine 生成接收状态，Receive Packet Controller 附加控制/丢失/过滤失败状态；回环的 ARP 或 PMT 包不做处理

### 63.5.9 流控 (Flow control)

- 目的：控制对端发包节奏，防止本地接收缓冲溢出；全双工用 Pause 包，半双工用 backpressure

全双工 (IEEE 802.3x Pause 包)：
- Pause 包字段 (Table 517)：DA = 特殊多播地址、SA = MAC 地址 0、Type = 0x8808、MAC Control opcode = 0x0001、PT = ETH_MACQ0TXFCR 的 PT 字段
- 置 FCB 位 (ETH_MACQ0TXFCR) 发送单个 Pause 包；再次置位可再发（不管前一个 Pause time 是否到期）；要延长/提前终止暂停，先编程 Pause Time 再置 FCB

半双工 (backpressure)：
- 应用请求停止接收时，MAC 侦测到包接收就发 32 字节 JAM 模式制造碰撞，对端退避；应用请求发送的包不受影响照发；backpressure 持续过久（>16 次连续碰撞）对端会因过量碰撞中止发送

Tx 流控 (Table 518，EHFC/TFE/DM 组合)：
- TFE=0：不执行任何流控
- TFE=1 DM=0：FCB 置位时 backpressure (半双工)
- TFE=1 DM=1：FCB 置位时发 Pause 包 (全双工)
- EHFC=1 时增加 Rx 队列阈值触发：Rx 队列填充越过阈值 (ETH_MTLRXQxOMR.RFA) 时触发流控，低于 RFD 阈值解除

触发方式：
- 手动：置 FCB 位；自动：Rx 队列填充越过 RFA 阈值 (EHFC 使能)
- 阈值建议：RFA 必须考虑"对端收到 Pause 前还能继续发的包"——理论需 RFA=4 (4KB RXQ)，实际 RFA=2 (2 KB) 溢出概率极低；4KB RXQ 时 RFA 设为 2 KB

Rx 流控 (仅全双工；Table 519，RFE/DM 组合)：
- RFE=0：不检测 Pause 包
- RFE=1 DM=0：不处理 Pause 包，但识别为控制包
- RFE=1 DM=1：检测并处理 Pause 包，停止 MAC 发射机
- 处理流程：
  1. DA 检查：多播地址 0x0180C2000001 (控制包专用多播)，或单播匹配 MAC 地址 0 寄存器且 ETH_MACRXFCR.UP 置位
  2. 解码 Type=0x8808、Opcode=0x0001 (Pause)、捕获 Pause time
  3. 状态字节计数 64 字节且无 CRC 错误时，发射机暂停传输 Pause Time × slot time (64 字节时间) 的时长
- 前一个 Pause Time 到期前又收到新 Pause 包：Pause 定时器更新为新值
- 使能接收流控：置 ETH_MACRXFCR.RFE

### 63.5.10 MAC 管理计数器 (MMC)

- 统计收发包的寄存器组，作为 CSR 寄存器地址空间的扩展 (见 63.11.4)；包括：控制寄存器、2 个 32 位中断寄存器 (收/发)、2 个中断掩码寄存器
- 计数器自由运行，无需使能，收到/发出对应包即开始计数
- Rx 计数器只统计通过地址过滤 (AFM) 的包——AFM 丢弃的包不计入 (少于 6 字节的 runt 包例外)；要统计全部包置 ETH_MACPFR 位 0 (混杂模式)；也统计封装 IPv4/IPv6/TCP/UDP/ICMP 载荷
- 碰撞/错误/好包计数器：Tx 单次碰撞好包、Tx 多次碰撞好包、Tx 好包总数 (ETH_TX_*_GOOD)、Rx CRC 错误包、Rx 对齐错误包、Rx 单播好包 (ETH_RX_*)
- LPI 模式转换计数器：Tx/Rx LPI 微秒定时器 + LPI 转换计数器 (ETH_TX/RX_LPI_*)
- "好包"定义——Tx 好包 = 发送成功未被中止 (无 jabber 超时、无载波/载波丢失、晚碰撞、下溢、过量延迟、过量碰撞)；Rx 好包 = 无 CRC 错误、非 runt (<64 字节)、无对齐错误 (仅 10/100M)、无长度错误 (非 Type 包)、无超范围 (非 Type 包 >1518)、无 GMII_RXER 输入错误
- 最大帧大小：Untagged 1518 / VLAN 1522 / Jumbo 9018 / JumboVLAN 9022
- 最大接收包大小 (Table 520，JE/S2KP/GPSLCE/EDVLP 组合)：1518~9026 字节范围 (如 JE=1 → 9018+4×VLAN 数；S2KP=1 → 2000；GPSLCE=1 → GPSL+4×VLAN 数；全 0 → 1518+4×VLAN 数)

### 63.5.11 MAC 中断

- MAC 中断与 DMA 中断合并到 eth_sbd_intr_it 信号；MAC 中断为电平型——保持断言直到软件清除
- 事件状态在 ETH_MACISR；中断默认使能，置 ETH_MACIER 掩码位可阻止事件产生中断
- 状态位只指示事件来源块，清除中断需读对应状态寄存器

### 63.5.12 MAC 和 MMC 寄存器描述

见 63.11.4 节

## 63.6 以太网功能描述：PHY 接口

- MII/GMII 是根接口，RMII/RGMII 由它派生 (Figure 784)；本节包含：SMA (站管理代理)、MII、RMII、RGMII 四部分

### 63.6.1 站管理代理 (SMA)

- SMA = MDIO 控制器，两线站管理接口 (MIM，MDC/MDIO)，应用经它访问 PHY 寄存器
- 支持最多 32 个 PHY × 每 PHY 32 个寄存器，一次只能访问一个
- MDC 时钟由 CSR 时钟 (eth_hclk) 分频产生，IEEE 802.3 规定最大 2.5 MHz；分频由 ETH_MACMDIOAR.CR[3:0] 选择 (Table 521)：

| CR[3:0] | MDC 时钟 |
|---------|----------|
| 0000 | eth_hclk/42 |
| 0001 | eth_hclk/62 |
| 0010 | eth_hclk/16 |
| 0011 | eth_hclk/26 |
| 0100 | eth_hclk/102 |
| 0101 | eth_hclk/124 |
| 0110, 0111 | - |

- ETH_MDIO 是三态缓冲线；C45E 位选 Clause 22 或 Clause 45 模式

Clause 45 帧 (Figure 786, Table 522)：PREAMBLE 32 个连续 1 → START 00 → OPCODE (00 地址 / 01 写 / 10 读+读后地址递增 / 11 读) → PHY ADDR 5 位 → DEV TYPE 5 位 → TA (Z0 读、10 写) → DATA 16 位 (地址周期含寄存器地址；写周期 MAC 驱动，读周期 PHY 驱动)

Clause 22 帧 (Figure 787, Table 523)：PREAMBLE 32 个 1 → START 01 → OPCODE (10 读 / 01 写) → PHY ADDR 5 位 → REG ADDR 5 位 → TA (Z0 读、10 写) → DATA 16 位 (写由 MAC 驱动、读由 PHY 驱动)；Clause 45 模式还支持读后地址递增 (post-read increment)

写操作流程 (Figure 788)：
- ETH_MACMDIOAR bits[3:2]=01 且位 0=1 启动写（数据在 ETH_MACMDIODR）；写数据包按 IEEE 802.3-2002 22.2.4.5 格式发到 MDIO，MAC 全程驱动 MDIO 线
- Busy 位 (MDIOAR 位 0) 高电平期间 CSR 忽略对 MDIOAR/MDIODR 的写操作；操作完成后 CSR 复位 Busy
- 写包格式 (Figure 789)：Z + 32 个 1 + 01 + 01 + AAAAA + RRRRR + 10 + DDD..DDD

读操作流程：
- ETH_MACMDIOAR bits[3:2]=11 且位 0=1 启动读；完成后 CSR 复位 Busy 并把读到的数据更新到 ETH_MACMDIODR
- 读包格式 (Figure 790)：Z + 32 个 1 + 01 + 10 + AAAAA + RRRRR + Z0 + DDD..DDD——MAC 全程驱动 MDIO，DATA 段由 PHY 驱动

实用控制：
- 前导码抑制：PSE 位——只发 1 位前导码 (标准要求 32 位)
- 尾时钟：NTC[2:0] 编程 0-7 个尾时钟 (MDIO 帧后 PHY 需要 MDC 继续活跃时用)；空闲期间不驱动 MDC
- 背靠背事务：BTB 位——MDIO 帧完成后立即清 GB (GMII busy) 位，允许在前一帧尾时钟期间启动下一命令；不使能时 GB 在尾时钟完成后才清
- 事务完成中断：MDIO 读写完成可产生中断，无需轮询 GB 位

### 63.6.2 介质独立接口 (MII)

- MII 定义 MAC 子层与 PHY 之间 10/100 Mbit/s 数据传输接口 (Figure 791)

信号 (Figure 791)：
- TX_CLK：连续时钟，Tx 数据传输时序参考；10 Mbit/s = 2.5 MHz、100 Mbit/s = 25 MHz
- TXD[3:0]：4 位发送数据，MAC 驱动，TX_EN 断言时有效；TXD[0] 为最低位；TX_EN 未断言时发送数据对 PHY 无影响
- TX_EN：发送使能，与 TX_CLK 同步；须与前导码第一个半字节一起断言并保持到所有半字节发送完
- TX_ER (可选)：仅 EEE (能效以太网) 需要；通过反转 CRC 指示发送错误，对端靠错误 CRC 检测
- RX_CLK：连续时钟，Rx 数据传输时序参考；10M = 2.5 MHz、100M = 25 MHz
- RXD[3:0]：4 位接收数据，PHY 驱动，RX_DV 断言时有效；RX_DV 未断言而 RX_ER 断言时用特定 RXD 值传 PHY 信息
- RX_DV：接收数据有效，与 RX_CLK 同步；从前导码第一个半字节断言保持到最后；须在最后一拍前解除；必须覆盖整个帧，最迟从 SFD 开始
- RX_ER：接收错误，断言一个或多个 RX_CLK 周期，须与 RX_DV 同时有效
- CRS：载波侦听，发送或接收介质非空闲时断言，两介质都空闲时解除；碰撞期间须保持；不要求与 Tx/Rx 时钟同步；全双工下 MAC 不关心
- COL：碰撞检测，检测到碰撞断言并保持；不要求与时钟同步；全双工下 MAC 不关心

### 63.6.3 千兆介质独立接口 (GMII)

- 10/100/1000 Mbit/s 数据传输 (Figure 792)；是 MII 的千兆扩展：数据 8 位、1000M 时钟 125 MHz

信号 (Figure 792)：
- GTX_CLK：1000 Mbit/s 时由 STM32 内部振荡器输出给 PHY（发送时钟方向与 10/100M 相反）
- TX_CLK：10/100 Mbit/s 的发送时序参考 (2.5/25 MHz)
- TXD[7:0]：8 位发送数据，TX_EN 断言时有效，TXD[0] 最低位
- TX_EN：发送使能（与 MII 同规则，从前导码第一个半字节断言）
- TX_ER：指示发送错误或 TXD[7:0] 上的载波扩展
- RX_CLK：接收时钟，10M = 2.5 MHz、100M = 25 MHz、1000M = 125 MHz
- RXD[7:0]：8 位接收数据，RX_DV 断言时有效
- RX_DV / RX_ER / CRS / COL：与 MII 相同语义（RX_DV 覆盖整个帧最迟 SFD 开始；全双工下 CRS/COL 不关心）

### 63.6.4 精简介质独立接口 (RMII)

- 把 MII 的 16 个数据/控制引脚减到 7 个 (IEEE 802.3u)；仅 10/100 Mbps，不支持 1000 Mbps
- RMII 模块作为以太网外设的一部分实例化在 MAC 输出处，把 MAC 的 MII 翻译成 RMII
- 提供独立的 2 位宽收发路径，两个时钟参考由外部提供

RMII 信号 (Figure 793)：
- RMII_REF_CLK：连续 50 MHz 参考时钟输入（10M 模式也保持 50 MHz，数据只取其中一部分拍）
- TXD[1:0]：发送数据；TX_EN 高时 TXD 有效
- RXD[1:0]：接收数据
- CRS_DV：CRS (载波侦听) 与 RX_DV (接收数据有效) 在交替时钟周期复用；10 Mbit/s 模式每 10 个时钟周期交替一次

位序 (Figure 794/795)：
- 发送：MII 的每个半字节 (nibble) 在 RMII 上拆成两个 2 位组 (di-bit) 依次发送——低两位 (D1/D0) 先发，高两位 (D3/D2) 后发
- 接收：同样先收低两位 (D0/D1)，再收高两位 (D2/D3)

### 63.6.5 精简介质独立接口 (RGMII)

- 精简 GMII/MII 引脚：数据路径和控制信号减少，并在收发时钟的双边沿 (rising+falling) 上多路复用；千兆时钟 125 MHz，10/100 用 2.5/25 MHz
- RGMII 模块实例化在 MAC 的 GMII 与 PHY 之间，翻译 GMII↔RGMII 协议
- 支持 10/100/1000 Mbps
- 不需要额外时钟——收发各用 1 根时钟的双边沿
- 从 PHY 提取带内状态信号（链路速度、链路状态、双工模式）提供给 MAC 做链路检测


## 63.7 以太网低功耗模式

### 63.7.1 低功耗管理

- 三种省电技术：**Magic packet（魔包）**、**Remote wake-up（远程唤醒包）**、**EEE**（Energy Efficient Ethernet，符合 IEEE 802.3az-2010）
- 原理：低功耗模式下主机逻辑大部分时钟和外设可关闭（仅保留 MAC 接收逻辑），收到特定包后 MAC 产生唤醒触发，主机恢复正常。支持的系统工作模式见 PWR 章节
- EEE 与上述两种模式可同时使能
- EEE 目标：线上无流量时省电。主机告知远端"近期无包可发"，将发送端口（MAC 控制器、PCS、PHY）置于低功耗；远端也类似。进出 EEE 模式由 MAC 控制

**Magic packet 模式**：
- 基于 AMD 白皮书；魔包看门狗超时固定 **2048 字节**（与 MACCR.WD、MACWTR.PWE 无关）
- 进入省电：MACPCSR.PWRDWN = 1；退出使能：MACPCSR.MGKPKTEN = 1
- 包格式：DA/SA/Length-Type 之后任意偏移处，6 字节全 1 同步流（FF FF FF FF FF FF，多余 FF 只取最后 6 个）+ 紧接 **16 次重复** 目的地址（完美匹配 MACA0HR/LR 的地址，或组播/广播地址），同步流与重复之间不能断开
- 有效性检查（全部满足才算魔包）：
  - 目的地址完美匹配 MACA0 或为组播/广播
  - 无长度错误、FCS 错误、dribble bit 错误、GMII 错误、碰撞
  - 非 runt（含头与 FCS 至少 64 字节）

**Remote wake-up 模式**：
- 进入省电：PWRDWN = 1；退出使能：RWKPKTEN = 1
- 过滤器查找表在 MACRWKPFR 寄存器中编程（CRC、offset、byte mask、操作命令）
- 模式位于 DA/SA 之后任意偏移处；除 CRC 匹配外，同样需要上述目的地址与无错误/runt 检查
- 收到有效包：MACPCSR.RWKPRCVD 置位，触发 pmt_intr_o；MACISR.PMTIS 置位（若低功耗模式下未关闭电源门控）；MACIER.PMTIE 使能且 CSR 时钟未关时，向 sbd_intr_o 触发中断
- 4 个过滤器可同时选择。Table 524 寄存器结构（ETH_MACRWKPFR）：
  - 字 0-3：Filter 0-3 的 byte mask（按字节掩码）
  - 字 4：Filter 0-3 的命令字段（各 4 位）
  - 字 5：Filter 0-3 的 offset
  - 字 6-7：Filter 0-3 的 CRC-16（每 16 位一个过滤器）
- 各字段含义见 Table 525（下页）

**Table 525 过滤器字段详解**：
- **Byte mask（字 0-3，32 位）**：MSB（bit 31）必须为 0；bit j = 1 表示检查包中 offset+j 字节，= 0 忽略。即"包中哪些字节参与 CRC 计算"
- **Command（字 4，每过滤器 4 位）**：
  - Bit 3（CAST）：地址类型。=1 只匹配组播包，=0 只匹配单播包
  - Bit 2（INV 反相）：反转 CRC16 Hash 匹配结果——匹配反而拒绝。与 bit 1 配合可实现 "Pattern1 AND NOT Pattern2"
  - Bit 1（And_Previous）：与上一个过滤器结果做逻辑 AND。可把超过 32 字节的模式拆到 2-4 个过滤器
  - Bit 0（EN）：过滤器使能。注意：And_Previous 链遇禁用过滤器即断开（如 F2 链 F1 但 F1 未使能，则只看 F2 结果；F0 的 And_Previous 位无效）
  - 陷阱：链上过滤器地址类型互补（一个单播一个组播）时，永远匹配不上
- **Offset（字 5，8 位）**：从包内哪个字节开始检查。最小允许 12（第 13 字节），0 指包首字节
- **CRC-16（字 6-7）**：模式算出的 CRC，多项式 G(x) = x16 + x15 + x2 + 1

**过滤器间接访问机制**：MACRWKPFR 是间接寄存器——4 个过滤器共 8 个内部寄存器 (wkuppktfilter_reg#0-3)，必须**依次顺序写 8 次**才能完整编程；当前指针在 MACPCSR.RWKPTR 中。字节/半字访问时，CPU 访问 Lane 3 才递增内部计数器

**写入时序限制**：写后内容从 CSR 时钟域传到 PHY 接收时钟域，期间不能再写，两次写间隔至少 **4 个 PHY Rx 时钟周期**，否则第二次写丢失

**PMT 中断 (Table 526)**：
- 中断生成组合：EN=1 且 (CAST 匹配包类型) 且 (INV=0 时 CRC MATCH / INV=1 时 CRC MISMATCH)——其余组合都不产生中断
- pmt_intr_o 与 Rx 时钟同步，专为"软件可停掉应用时钟"设计
- **清除延迟**：pmt_intr_o 在 PHY Rx 时钟域生成，读 MACPCSR 后不能立即清除（清信号要跨时钟域），至少延迟 4 个 Rx 时钟周期，10 Mbps 模式下可能很长
- 清 PWRDWN 退出省电不会产生 PMT 中断

**Power-down 序列（软件 6 步）**：
1. 清 DMACxTXCR.ST 禁用发送 DMA
2. 等帧发送完（查 MACDR.TFCSTS/TPESTS、MTLTXQxDR.TXQSTS）
3. 清 MACCR.TE、MACCR.RE 禁用 MAC 收发
4. 等接收 FIFO 排空（MTLRXQxDR.PRXQ = 0）
5. 配置魔包 MGKPKTEN / 远程唤醒 RWKPKTEN
6. 置 MACCR.ARPEN、置 RE、最后置 MACPCSR.PWRDWN
- 进省电时若发送端处于 LPI 模式，GMII/MII 接口钳位为 LPI 模式；否则钳位为全零

**Power-up 序列**：
- MAC 收到魔包/远程唤醒帧 → 断言 pmt_intr_o（若只做了时钟门控，用它恢复被关的时钟）
- 软件：读 MACPCSR 解除 pmt_intr_o → 写回 MACPCSR 和 MACRWKPFR 复位值（同步 always-on 块，否则两域值不一致）→ 重写 MACCR、MACA0HR/LR（否则会出现 RE=0 但接收器仍开着的状态）
- 最后：初始化所有寄存器、使能发送端、编程 DMA 恢复正常运行

### 63.7.2 节能以太网 (EEE)

- 让 MAC + PHY 工作在 **LPI (Low-Power Idle)** 模式;支持 100 Mbps / 1000 Mbps (IEEE 802.3az-2010);**RMII 接口不支持 EEE**
- 链路两侧可协商能力与公共参数;是否进出 LPI 由 MAC 控制并通知 PHY

**发送路径 — 进入 LPI**：
- 软件置 MACLCSR.LPIEN → MAC 完成当前帧、产生发送状态 → 若链路连续 up 时间达到 MACLTCR.LST[9:0] 则开始发 LPI pattern 代替 IDLE pattern(链路状态看 MACLCSR.PLS 或 PHYIF LNKSTS)
- MAC 让 PHY 进 LPI 的 4 步：1) 去断言 TX_EN 2) 断言 TX_ER 3) TXD[3:0]=0x1 (100M) 或 TXD[7:0]=0x01 (1000M) 4) 置 MACLCSR.TLPIEN 并产生中断
- LPI 期间 TX_EN/TX_ER/TXD 保持同一状态

**发送路径 — 退出 LPI**：
- 软件清 LPIEN → MAC 停发 LPI pattern 改发 IDLE → 启动 **LPI TW TIMER**(唤醒时间,自动协商值在 MACLTCR.TWT)——计时未到不能开始发送 → 置 TLPIEX 并产生中断

**Tx 时钟门控**：
- MII 模式按标准 PHY 不能停 TX_CLK;GMII 1000M 下 MAC 可停 GTX_CLK
- MAC 置 sbd_tx_clk_gating_ctrl_o = 1 后应用可停 TX_CLK(该信号在 9 个 Tx 时钟周期 + 1 脉冲同步器延迟 + 1 CSR 时钟后断言,受 MACLCSR.LPITCSE 控制)
- **RGMII 接口必须发 LPI pattern,不能停 Tx 时钟**
- Tx 时钟停止期间不要写同步到 Tx 时钟域的 CSR;LPI 中做软/硬复位会使发送端退出 LPI

**自动进出 LPI (发送路径)**：
- LPITXA=1 且 LPIEN=1：发送路径(DMA/MTL/MAC)空闲即自动进 LPI;任一环节因发包变忙立即退出并清 LPITXEN
- 再加 LPITE=1：空闲持续 MACLETR 定时时间才进 LPI;退出时 LPIEN 不清除,再次空闲可自动重进(无需软件介入)
- LPITE、LPITXA 都清：纯软件通过 LPIEN 手动控制

**接收路径**：
- 远端进 LPI 时 PHY：1) 断言 RX_ER 2) RXD=0x1 (100M) / 0x01 (1000M) 3) 去断言 RX_DV 4) MAC 置 RLPIEN 并立即中断
- 远端退出时：PHY 去断言 RX_ER 恢复正常 → MAC 置 RLPIEX 并中断,同时断言 lpi_intr_o (与 Rx 时钟同步)
- 细节：LPI pattern 短于 2 个 Rx 时钟 → 不进 Rx LPI;两次 LPI 间隔 < 2 个 Rx 时钟 → 不触发进出中断
- PHY 的 RX_CLK_stoppable 位置位时,PHY 可在 LPI 开始 9 个时钟后随时停 RX_CLK
- **EEE 使能时,PHY 发来 LPI pattern MAC 自动进入,软件无法阻止**

**三个 LPI 定时器**：
- **LPI LS TIMER**：毫秒计数,链路 up 后开始,到 LST[9:0] 终值(标准规定 1 秒)才允许断言 LPI pattern;链路 down 清零;10 位最大 1023 ms
- **LPI TW TIMER**：微秒计数,退出 LPI 后计时,到 TWT[15:0] 终值(自动协商的唤醒时间)后恢复发送;16 位最大 65535 µs
- **LPI AUTO ENTRY TIMER**：以 8 µs 步进,发送路径空闲时长达到后才进 LPI;LPITE=1 时生效

- 注意：**GMII/MII 切换前必须把 MACLCSR.PLS 清 0**,复位内部定时器,否则 Tx 时钟频率变化导致计时错误

**LPI 中断**：
- 收发任一侧进出 LPI 都产生中断;读 MACLCSR 清除;sbd_intr_o 与 lpi_intr_o 都会断言
- lpi_intr_o 在 Rx 时钟域生成,清除延迟至少 4 个 Rx 时钟周期(10 Mbps 下明显)
- 完整编程指南见 63.9.13 (3417 页)

## 63.8 以太网中断

### 63.8.1 总览与 DMA 中断

- 整个外设只有**一个中断信号 `eth_sbd_intr_it`**,各种事件通过状态寄存器捕获,每个事件源都有对应使能位
- **层次化组织**,便于软件快速定位中断源：
  1. 第一级：读 `ETH_DMAISR`——指出事件来自哪个大块(DMA 通道 0/1、MTL、MAC)
  2. 第二级：按置位读 `ETH_DMAC0SR/DMAC1SR`(通道状态)、`ETH_MTLISR`、`ETH_MACISR`
- DMA 通道中断分两组,由 DMACxSR bits[15:14] 指示：
  - **Normal 组**:正常传输事件——TI(发送完成)、RI(接收完成)、TBU(发送缓冲不可用)
  - **Abnormal 组**:错误事件
- **中断不排队**:同一事件在驱动处理前再次发生,不产生额外中断;多个事件只产生一次。驱动必须扫描 DMAISR 找原因、清各自状态寄存器;**只有 DMAISR 所有位清完,中断才真正清除**

**周期性调度收发中断(避免每个包都中断)**：
- 方法 1：只在"每 N 个包"的发送描述符里置 IOC(TDES2[31])
- 方法 2：接收描述符只在特定间隔置 IOC(RDES3[30])——包传完且其中某描述符有 IOC 才产生 RI
- 方法 3：**Rx 中断看门狗定时器 `ETH_DMACxRXIWTR`**:置非零值后,Rx 完成传输但因无 IOC 不产生 RI 时定时器启动,超时即置 RI 并断言中断(RIE 使能时);若中途有 IOC 置位产生 RI,定时器停止并清除,下次无 RI 的传输完成后自动重新激活

**Per-channel 传输完成中断**：
- TI:Tx DMA 关闭置了 IOC(TDES2[31]) 的描述符时置位
- RI:Rx DMA 关闭置 LD 且该包任一描述符置 IOC(RDES3[30]) 的描述符时置位
- 是否断言中断信号取决于 DMACxIER 对应使能位

**Table 527 INTM 字段 (ETH_DMAMR bits[17:16]) 三种模式**：
| INTM | TI/RI 行为 |
|---|---|
| 0 / 1 | 检测到传输完成事件即置位,软件写 1 清除;IER 使能时断言中断 |
| 2 | 状态置位,但 **RI/TI 事件不断言中断信号** |
| 3 | 置位规则同 INTM=2,且事件在清除(服务)前又发生时自动再次置位;仍不产生中断信号 |

### 63.8.2 MTL 中断

- MTL 事件与 DMA 事件合并产生中断信号
- `ETH_MTLISR` 报告**队列号**;读 `ETH_MTLQ0ICSR / MTLQ1ICSR` 看具体事件
- MTL 中断**默认使能**;在 MTLISR 置对应 mask 位可屏蔽
- 事件只有两类：**接收队列溢出 (Rx Overflow)**、**发送队列下溢 (Tx Underflow)**

### 63.8.3 MAC 中断

- MAC 事件与 DMA 事件合并;**level 型中断**——软件不清理就保持断言
- `ETH_MACISR` 描述事件;**默认使能**;置 mask 位屏蔽
- 中断位只指出事件来自哪个块,必须读对应状态寄存器并清除才能真正清除中断
- 事件源：Rx 状态、Tx 状态、时间戳、MMC(Rx COE / Tx / Rx)、**LPI**、**PMT**、PHY、RGMII
- 两个侧带信号伴随 LPI/PMT 中断产生：`lpi_intr_o`、`pmt_intr_o`——用于 EXTI 级别的唤醒事件检测

## 63.9 以太网编程模型(初始化序列)

**本节总览**：63.9.1 DMA 初始化、63.9.2 MTL 初始化、63.9.3 MAC 初始化、63.9.4 正常收发、63.9.5 停止/启动发送、63.9.8 多通道多队列、63.9.9 GMII 链路状态转换、63.9.10 IEEE 1588、63.9.12 AV、63.9.13 EEE、63.9.14 PPS、63.9.15 辅助快照、63.9.16 TSO、63.9.17 接收 VLAN 过滤

### 63.9.1 DMA 初始化 (10 步)

1. **软件复位**：置 ETH_DMAMR bit 0(SWR),复位所有 MAC 内部寄存器和逻辑
2. **等待复位完成**：轮询 ETH_DMAMR bit 0,硬件清 0 即完成
3. **编程 ETH_DMASBMR**(系统总线模式)：AAL、ONEKBBE、RD_OSR_LMT、WR_OSR_LMT;固定突发或未定义突发;OSR_LMT 值;若固定长度,选 AXI 最大突发长度(bits[7:1])
4. **创建发送/接收描述符链表**;接收描述符必须置 OWN 位(TDES3/RDES3 bit 31)交给 DMA;**描述符环不能跨越 4GB 边界**
5. 编程 ETH_DMACxTXRLR / ETH_DMAC0RXRLR(环长度,**至少 4**)
6. 编程描述符列表基地址(ETH_DMACxTXDLAR / ETH_DMAC0RXDLAR);再编程**尾指针** ETH_DMACxTXDTPR / ETH_DMAC0RXDTPR 告知 DMA 可用描述符数
7. 编程 ETH_DMACxCR / ETH_DMACxTXCR / ETH_DMAC0RXCR：PBL 最大突发长度、描述符跳过长度、Tx 的 OSP、Rx 的 RBSZ[13:0] 等
8. 编程 ETH_DMACxIER 使能中断
9. **启动**：置 DMAC0RXCR.SR(bit 0)和 DMAC0TXCR.ST(bit 0)
10. 对硬件选用的所有 Tx DMA 通道重复 4-9

### 63.9.2 MTL 初始化 (4 步)

1. 编程 ETH_MTLOMR：多队列时的 Tx 调度算法 SCHALG、接收仲裁算法 RAA
2. 编程 ETH_MTLTXQxOMR：a) TSF(存储转发)或 TTC(阈值模式) b) **TXQEN = 2'b10 使能 Tx 队列 0** c) TQS 队列大小
3. 编程 ETH_MTLRXQxOMR：a) RSF 或 RTC(阈值) b) RFA/RFD 流控阈值 c) FEP/FUP(错误包/欠长好包转发) d) RQS
4. 对所有 MTL 队列重复 2-3

### 63.9.3 MAC 初始化 (6 步)

- MAC 配置寄存器尽量在 DMA 初始化**之前**做;若先配好 MAC,最后一步使能接收器必须等 DMA 已激活,否则收到的帧灌满 Rx FIFO 溢出
1. 提供 MAC 地址寄存器：ETH_MACAxLR、MACA0HR、MACAxHR
2. 编程 ETH_MACPFR 过滤：Receive All、混杂模式、Hash/完美过滤、单播/组播/广播/控制帧过滤
3. 编程 ETH_MACQ0TXFCR 流控：Pause 时间、发送流控位、FCB
4. 按需编程 ETH_MACIER
5. 编程 ETH_MACCR：发送 IPG、jabber disable 等
6. 置 ETH_MACCR bits[0:1] 启动 MAC 收发器

**Jumbo 包支持(>9026 字节)**：MACCR.JE=1、JD=0、WD=0(避免 giant 错误)、GPSLCE=1、MACECR.GPSL > 9026
**16K 包支持**：JD=1、WD=1、GPSLCE=1、MACECR.GPSL = 16383

### 63.9.4 正常收发操作

1. 读中断状态,轮询描述符的 Host 所有权位(收发同理)
2. 把描述符设为合适值,置 OWN 位交回 DMA 恢复传输
3. 描述符不够时 DMA 进入 **Suspend 状态**;释放描述符 + 写尾指针(ETH_DMACxTXDTPR/RXDTPR)即可恢复
4/5. 调试模式读 ETH_DMACxCATXDR/CARXDR 可看当前 Host 描述符地址/缓冲区地址指针

### 63.9.5 停止和启动发送 (6 步)

1. 清 DMACxTXCR.ST 禁用发送 DMA
2. 等上一帧发完(MTLTXQxDR：TRCSTS[1:0] ≠ 01 且 TXQSTS = 0)
3. 清 MACCR.RE、TE 禁用 MAC 收发
4. 确认 Rx FIFO 数据已搬进内存(PRXQ=0 且 RXQSTS=00)后禁用接收 DMA
5. 确认 Tx/Rx 队列都空(TXQSTS=0、RXQSTS=00)
6. 重启：**先启动 DMA,再使能 MAC 收发器**

**重要约束**：
- MAC 收发活跃时**不得更改配置**(双工、速率、端口、环回);DMA 活跃时不得改 DMA 配置——只能停下来改
- 收发停的顺序是 DMA→MAC,启的顺序反过来 MAC→DMA(实际是先 DMA 后 MAC)

### 63.9.6 RxDMA 切换新描述符列表

- RxDMA 在 **Suspend 状态**允许直接切换新描述符列表,无需走 Stop 流程
- 原因:RxDMA 预先准备描述符,若因描述符不足挂起且软件不能立刻释放,会导致 RxFIFO 溢出丢帧——所以允许立即换新列表恢复

### 63.9.7 切换 AXI 时钟频率(动态,无需复位)

1. 停 Tx DMA(若用),等帧发完——Tx FIFO 空、Tx DMA 进 Stop(查 MTLTXQxDR、DMADSR)
2. 清 MACCR 相应位停 MAC 收发
3. 确认 Rx FIFO 数据已搬完(MTLRXQxDR)后停 Rx DMA
4. **确保应用不再做任何寄存器读写**
5. 改变 AXI 时钟频率
6. 重新使能 MAC 收发与收发 DMA

> 目的:切换瞬间 Tx/Rx FIFO 无有效数据,防止数据损坏

### 63.9.8 多通道多队列编程

**Tx 路径**：
1. MTLTXQxOMR.TQS 配队列大小;**通道数 = 队列数,通道↔队列映射固定**
2. 用队列必须置 TXQEN;且 DMACxTXCR.ST 和对应 TXQEN 都要使能
3. MTLOMR.SCHALG 配调度算法
4. AV 队列 1 若用 CBS 算法,还需编程 MTLTXQ1ECR/SSCR/HCR/LCR

**Rx 路径**：
1. MTLRXQxOMR.RQS 配队列大小
2. MACRXQ0CR.RXQ0EN/RXQ1EN 使能接收队列;DMAC0RXCR.SR 也要使能
3. MAC 按包类型把收包路由到队列：
   - AV PTP 包 → MACRXQC1R.PTPQ 指定队列
   - AV 无标签控制包 → MACRXQC1R.AVCPQ
   - 带 VLAN 标签的包 → 按 USP(用户优先级)路由,MACRXQC2R.PSRQ1/PSRQ0 配置;**PSRQ0 和 PSRQ1 的优先级必须唯一**
   - AV 带标签控制/数据包 → MACRXQC2R.PSRQ

### 从 DMA 通道总线错误恢复

**Rx 通道失败**：
1. (可选)置 RPF=1 逐个冲掉包——防止总线错误停 RxDMA 时 Rx 队列 HOL(队头阻塞)
2. 重编程该 DMA 通道寄存器
3. 启动通道

**Tx 通道失败**：
1. 停止该通道(即使处于 active)
2. 冲掉对应 MTL 队列
3. 重编程通道寄存器
4. 启动通道

> **已知限制**:重编程通道寄存器不一定总能从总线错误恢复;若外设仍不正常,用**软复位**兜底

### 禁用接收队列

1. MACCR.RE = 0 禁用接收器
2. 读 MACDR(确认 RPESTS、RFCFCSTS = 0)和所有 MTLRXQxDR
3. 所有 Rx 队列调试寄存器读为 0 后,通过 MACRXQC0R 禁用目标 Rx 队列
4. MACCR.RE = 1 重新使能接收器

### 63.9.9 GMII 链路状态转换编程指南

**场景 A:链路 down 但收发时钟仍在跑**：
1. 清 DMACxCR.ST 停 Tx DMA
2. 清 MACCR.RE 停接收器
3. 等帧发完(MTLTXQxDR.TRCSTS[1:0] ≠ 01)——或直接 **Flush Tx FIFO** 加速排空
4. 清 MACCR.TE 停发送器
5. 确认 Tx/Rx 队列都空(TXQSTS=0、RXQSTS=00)
6. **链路恢复后读 PHY 寄存器确定最新配置,据此重配 MAC**
7. 启动 Tx DMA,再使能 MAC 收发器
   - **Rx DMA 不需要重新使能**:接收器一直关着,Rx FIFO 里没数据

**场景 B:链路 down 且收发时钟也停了**：
1. 清 RE、TE(时钟不在,不立即生效)
2. 等链路恢复、时钟回来
3. 等部分传输的帧收尾(MACDR 全 0);注意 Tx FIFO 可能还残留旧包(发送器停了)
4. 读 PHY 寄存器确定最新工作模式,重配 MAC
5. 置 RE、TE 重启

### 63.9.10 IEEE 1588 时间戳编程指南

**初始化系统时间生成 (9 步)**：
1. 清 MACIER bit 12,屏蔽 Timestamp Trigger 中断
2. 置 MACTSCR bit 0 使能时间戳
3. 按 PTP 时钟频率编程 MACSSIR(亚秒增量)
4. 若用 Fine Correction:编程 MACTSAR(addend),置 MACTSCR bit 5
5. **轮询 MACTSCR 直到 bit 5 被清 0**(硬件装载完成)
6. 需要时置 MACTSCR bit 1 选 Fine Update 方法
7. 用合适的时间值编程 MACSTSUR(秒)、MACSTNUR(纳秒)
8. 置 MACTSCR bit 2
   - **时间戳计数器从写入更新寄存器的值开始走**
   - 若需 One-step 时间戳:a) TDES3 Context 描述符 bit 27 使能 b) 编程 MACTSIACR 更新 PDelay_Req 消息的 correction 字段
9. 使能 MAC 收发器以获得正确时间戳

> 清 MACTSCR bit 0 停用时间戳后,重启需重复全部步骤

**系统时间校正(粗校正 Coarse Correction)**：
1. 在 MACSTSUR/MACSTNUR 写入偏移(正或负)
2. 置 MACTSCR bit 3 (TSUPDT)
   - 该位被清 0 时,更新寄存器中的值被加/减到系统时间

**细调校正 (Fine Correction,减少系统时间抖动) (8 步)**：
1. 按算法算出期望的增减速率
2. 把新值写入 MACTSAR(addend),置 MACTSCR bit 5
3. 等待新 addend 生效所需的时间(可让系统时间到达目标值时触发中断来计时)
4. 把目标时间写入 MACPPSTTSR(秒)、MACPPSTTNR(纳秒)
5. 使能 MACIER bit 12(Timestamp 中断)
6. 置 MACTSCR bit 4(触发使能)
7. 中断产生后读 MACISR
8. 把 MACTSAR 恢复旧值,再置 bit 5——细调完成,时间被平滑拉齐

### 63.9.11 PTP offload 编程指南

**各模式配置位速查**：

| 模式 | SNAPTYPSEL | TSMSTRENA | TSEVNTENA | POCR 使能位 | LMIR 字段 |
|---|---|---|---|---|---|
| Ordinary/Boundary Master(周期 Sync) | 0 | 1 | 1 | ASYNCEN | LSI |
| Transparent Master(周期 Sync) | 1 | 1 | 1 | ASYNCEN | LSI |
| Transparent Slave(周期 Pdelay_Req) | 1 | 0 | 1 | APDREQEN | LMPDRI |
| O/B Master(响应 Delay_Resp) | 0 | 1 | 1 | — | DRSYNCR + LSI |
| O/B Slave(响应 Delay_Req) | 0 | 0 | 1 | — | DRSYNCR |
| Transparent Slave(全功能) | 1 | 0 | 1 | APDREQEN | DRSYNCR + LMPDRI |
| Transparent Master(全功能) | 1 | 1 | 1 | ASYNCEN/APDREQEN | DRSYNCR + LSI + LMPDRI |
| Peer-to-Peer Transparent | 3 | X | X | APDREQEN | LMPDRI(+DRSYNCR) |

**通用步骤(以周期 Sync 为例,其余模式同构)**：
1. 配 MACTSCR 三个字段定节点角色(SNAPTYPSEL: 0=O/B、1=Transparent、3=P2P Transparent;TSMSTRENA=1 表示主)
2. 置 MACPOCR.PTOEN 使能 offload + 配 DN 域号
3. 置 POCR 相应位(ASYNCEN=周期 Sync、APDREQEN=周期 Pdelay_Req)
4. 编程 **80 位 Source Port Identity**(MACSPI0R/1R/2R)——发给对端的身份标识
5. 编程 MACLMIR 间隔字段。**值 = 2^N 秒**:LSI=1 → 每 2 秒;LSI=0xFF(即 -1)→ 每 0.5 秒
6. 使能 MACIER.TSIE 时间戳中断
7. 等 sbd_intr_o 中断(源:MACTSSR.TXTSSIS)——表示 Sync 的发送时间戳已捕获在 MACTXTSSSR/MACTXTSSNR

**响应消息要点**：
- O/B Master:收到 Delay_Req → 自动回 Delay_Resp;DRSYNCR + LSI 之和写入响应的 logMinDelayReqInterval 字段
- O/B Slave:收到 Sync → 自动回 Delay_Req;DRSYNCR 表示"收到几个 Sync 回一个 Delay_Req"
- Transparent Slave:回 Delay_Req + Pdelay_Resp + 周期 Pdelay_Req
- Transparent Master:回 Delay_Resp + Pdelay_Resp + 周期 Sync/Pdelay_Req
- Peer-to-Peer Transparent:回 Pdelay_Resp + 周期 Pdelay_Req(模式配置用 SNAPTYPSEL=3)

**Peer-to-Peer Transparent 模式**：SNAPTYPSEL=3、PTOEN+DN、SPI 身份、LMPDRI(周期)、TSIE、等 TXTSSIS

### 63.9.12 AV 特性编程指南

**DMA 初始化(12 步,基本同 63.9.1,AV 特有部分)**：
- 步骤 4/5 有额外要求:OSF 模式至少 **2 个 Tx 描述符**;软件在**复用描述符前至少要建 3 个以上**收发描述符
- 步骤 8 加配 MTLTXQ0OMR(TSF/TTC/TXQEN=10/TQS)
- 步骤 11 编程 AV 队列的 **CBS 控制寄存器**:idleSlope、sendSlope、hiCredit、loCredit(MTLTXQ1ECR/SSCR/HCR/LCR)
- 步骤 12 启动 DMA

**使能槽号检查(slot number checking)**——让 DMA 按固定时间间隔从内存取帧,保证 AV 流量均匀周期传输;仅时间戳使能且 SSIR 编程后可用。顺序:在步骤 11 之后、12 之前:
1. 按 63.9.10 使能时间戳
2. 确认 TDES3 的 SLOTNUM 字段(位 22-19)是有效槽号(可从 DMACxSFCSR 读当前参考槽号)
3. 置 DMACxSFCSR.ESC 使能槽号检查

**每槽平均比特报告 (ABPS)**：
1. 使能时间戳
2. MTLTXQ1ECR.SLC 编程"在多少个槽上计算平均"
3. 使能 MTLQxICSR.ABPSIE 产生 ABPS 中断——**SLC=0 时每 125 µs 中断一次**,不需要可关掉防中断风暴
4. 每次中断读 MTLTXQxESR.ABS;也可轮询读(ABPSIS=1 表示 ABS 有新值)

**AV 队列禁用流控**：对应 MTLRXQxOMR.EHFC = 0

### 63.9.13 EEE 编程指南

**Tx LPI 进入/退出(初始化期 11 步)**：
1. 通过 MDIO 读 PHY,确认对端 EEE 能力,协商定时器值
2. 通过 MDIO 编程 PHY 寄存器(含 RX_CLK_stoppable——指示 PHY 是否在 LPI 停 Rx 时钟)
3. 编程 MACLTCR 位 25-16(TWT)和位 15-0(LST)
4. 读 PHY 链路状态,更新 MACLCSR 位 17(PLS)——**链路状态一变就要更新**
5. 按 CSR 从口时钟频率编程 MAC1USTCR(1 µs tick 计数器)
6. 编程 MACLETR.LPIET——MAC 空闲多久后自行进 LPI
7. 置 MACLCSR.LPITE、LPITXA(位 20-19)使能自动进出
8. 置 MACLCSR 位 16(LPIEN)让 MAC 发送端进 LPI——**进 LPI 条件:所有排队的帧发完 + 空闲满 LPIET 时间**,进 LPI 后置 TLPIEN
9. 有新包要发(TxDMA 出 IDLE 或 ATI/MTI 接口出现包)时自动退出,等 TWT 时间后置 TLPIEX 再恢复发送
10. 再次空闲满 LPIET 又自动进——进出循环
11. 应用要手动控制时可清 LPIEN 直接退出

**补充**：
- 要确保 "Tx FIFO 所有排队帧发完才进 LPI" 就置 LPITXA
- GMII 发送时钟门控用 sbd_tx_clk_gating_ctrl_o
- 关 CSR 时钟/系统电源:**等 TLPIEN 中断产生后**才能关;恢复时先恢复时钟再执行步骤 6

**LPI 模式下关 CSR 时钟**：
- **Rx LPI**:MAC 收到 LPI pattern → 进 Rx LPI,RLPIEN 置位 → sbd_intr_o 断言(读 MACLCSR 清除)→ **MAC Tx 也在 LPI 时才能关 CSR 时钟**(否则 Tx 侧事件不报告)→ 恢复:等 PHY 的 LPI 退出指示,MAC 在 lpi_intr_o 上断言退出中断,读 MACLCSR 清除
- **Tx LPI**:LPIEN 置位 → TLPIEN 置位 → sbd_intr_o 断言 → **MAC Rx 也在 LPI 时才能关 CSR 时钟** → 恢复:MAC 退出 Tx LPI 后开时钟,再清 LPIEN 退出

### 63.9.14 灵活 PPS 输出编程指南

**生成单个 PPS 脉冲**：
1. 配 MACPPSCR.TRGTMODSEL[1:0] = 11 或 10(中断),指示 MAC 使用 Target Time 寄存器

**生成单个 PPS 脉冲(4 步)**：
1. MACPPSCR.TRGTMODSEL[1:0] = 11 或 10(中断),让 MAC 用 Target Time 寄存器作 PPS 起始时间
2. 在 MACPPSTTSR/MACPPSTTNR 写起始时间值
3. MACPPSWR 写脉冲宽度
4. PPSCMD[3:0] = 0001 → 在 Target Time 时刻输出单个脉冲

**生成下一个脉冲**：
- 上一脉冲开始前可用 PPSCMD=0011 取消
- 下一个脉冲起始时间**必须晚于上一个脉冲的下降沿**;若在上一脉冲拉低前就给新命令,会覆盖旧命令,可能只输出 1 个加长脉冲

**生成脉冲串(7 步)**：
1. TRGTMODSEL = 11/10 → 2. 写起始时间 → 3. MACPPSIR 写脉冲间隔 → 4. MACPPSWR 写宽度
5. PPSCMD = 0010 启动脉冲串(**默认自由运行,需命令才停**)
6. 写停止时间(Target Time 寄存器;写前确认 PPSTTNR.TSTRBUSY 已清 0)
7. PPSCMD = 0100 → 到停止时间后停
- 命令表:0101 = 立即停止、0110 = 取消"到时间停止"、0011 = 起始时间前取消启动

**只产生中断不影响 PPS**：
- TRGTMODSEL = 00 → Target Time 只用于中断;注意改模式会覆盖之前的中断配置
- 目标时间比系统时间小(在过去)→ MACTSSR.TSTRGTERR0 置位(TSIE 使能时产生 sbd_intr_o)
- **推荐写入顺序(避免误中断)**：PPSTTNR → PPSTTSR → PPSIR → PPSWR → PPSCR

### 63.9.15 IEEE 1588 辅助快照编程(4 步)

1. 使能 MACIER.TSIE(快照捕获进 FIFO 时产生中断)
2. 置 MACACR.ATSENx 使能对应触发输入的捕获
3. 不想读快照时置 MACACR.ATSFC 清快照 FIFO
4. TSIS 置位/中断产生时,读 MACTSSR + MACTXTSSSR + MACATSNR 取快照

状态位:
- **AUXTSTRIG**:有快照捕获(读清除;MACCSRSWCR.RCWE=1 时改为写清除)
- **ATSSTN**:触发事件输入源(标识 FIFO 第一个可用条目)
- **ATSNS**:FIFO 中可用快照数
- **TSIS**(DMAISR):1588 功能发生感兴趣事件
- **ATSSTM**:快照被忽略(FIFO 满,触发丢失)

### 63.9.16 TSO 编程指南

- 原理回顾:TSO 引擎把 TCP 分段卸载到硬件;置 TSE 位 + 编程描述符字段使能当前包
- 详细步骤见下页

**TSO 编程步骤 (3 步)**：
1. 置 DMACxTXCR.TSE 使能该 DMA 通道的 TCP 分段
2. 除正常描述符设置外,还需编程：
   - TDES3 bit 18 (TSE)
   - TDES3 bits[17:0]:未分段的 TCP/IP payload 长度
   - TDES3 bits[22:19]:TCP 头长度
   - MSS[13:0] 在 DMACxCR 或 context descriptor 中;**两处都编程时,后写的一处生效**
3. **未分段 TCP/IP 包头必须放在第一个描述符的 Buffer 1,该缓冲不能含任何 payload**;payload 放 Buffer 2 及后续描述符的缓冲

> Caution:对非 TCP/IP 包使能 TDES3.TSE,结果不可预测

### 63.9.17 接收端 VLAN 过滤编程

1. 编程 MACVTR 选择过滤方式:
   - **ETV**:12 位或 16 位 VLAN tag 比较
   - **VTHM**:VLAN tag 哈希表匹配
   - **ERIVLT**:内 VLAN 或外 VLAN 过滤(需 EDVLP 使能双 VLAN 处理)
   - **ERSVLM**:S-VLAN 或 C-VLAN 匹配(需 ESVL 使能)
   - **DOVLTC**:匹配时忽略 VLAN 类型
   - **VTIM**:反相匹配(反向过滤)
2. 编程 MACVTR.VL 字段(12/16 位 VLAN tag 值)
3. 哈希过滤时编程 MACVHTR:
   - ETV=0:对 VLAN tag 计算 CRC-32,**高 4 位取反**后索引哈希表
   - ETV=1:CRC-32 高 4 位**直接**索引
   - 例:hash 值 0b1000 → ETV=1 选 bit 8,ETV=0 选 bit 7

## 63.10 描述符

### 63.10.1 概述

- DMA 基于**描述符链表**传输数据;应用在系统内存 (SRAM) 创建描述符
- 两种类型:
  - **Normal 描述符**:承载包数据 + 提供该包的控制信息
  - **Context 描述符**:只提供该包的控制信息(TSO 的 MSS、时间戳等)
- 每个 normal 描述符含 **2 个缓冲 + 2 个地址指针**(Buffer1/Buffer2)——兼容不同内存管理方案
- 单个包可用的描述符数量**无限制**

### 63.10.2 描述符结构

- 只支持 **ring 结构**(Figure 800):描述符 0-N 环形排列,每个含 Buffer1/Buffer2
- 描述符间距 = DMACxCR.**DSL** 字段(以 64 位字为单位)
- 环总长度(描述符个数)编程在 DMACxTXRLR / DMAC0RXRLR

**尾指针机制(核心!)——"生产者/消费者"模型**：
- 尾指针寄存器 (TXDTPR/RXDTPR) 指向描述符地址 N;**从当前指针到 N-1 的描述符归 DMA 所有**
- DMA 一直处理到 `当前指针 == 尾指针` 时进入 **Suspend 状态**
- 恢复条件:`尾指针 > 当前指针`——应用写尾指针寄存器即唤醒
- **环绕处理**:应用在环尾接着从环头续描述符并更新尾指针时,"尾指针 > 当前指针"条件不满足;DMA 会检测到这种情况,处理到环尾后自动从头继续,直到当前指针 == 尾指针
- **尾指针超环尾**:DMA 理解为"再多给一个完整环"——不想这样,就把尾指针指回环头
- DMA 到达环尾**自动环绕回基地址**(Figure 801)

**所有权 (OWN)**：
- OWN=1 (TDES3/RDES3 bit 31):描述符归 **DMA**
- OWN=0:归 **应用**
- 初始只有一个描述符时:尾指针 = 基地址 + 1,DMA 处理第一个描述符后等应用推进尾指针

**Figure 802/803 尾指针示例**：
- 例 1:当前指针 D4、尾指针 D7 → DMA 拥有 D4-D6 共 3 个
- 例 2:应用释放 D7-D9、D0,尾指针更新为 D1 → DMA 拥有 D4-D9、D0(跨越环尾环绕),最后一个是 D0

### 63.10.3 发送描述符

- 一个发送包至少 1 个描述符;含 2 缓冲、2 字节计数、2 地址指针 + 每包控制字段
- 两种格式:**Read format**(驱动配置,Figure 804)和 **Write-back format**(DMA 回写,Figure 805)

**TDES0 (Read)**:BUF1AP——Buffer 1 物理地址(TSO 时若 TSE+FD 置位则是 **TSO 头地址**)

**TDES1 (Read)**:BUF2AP——Buffer 2 物理地址(无对齐限制)

**TDES2 (Read)**:
- **IOC** (bit31):传输完成时置 DMACxSR.TI
- **TTSE** (bit30):使能该包的发送时间戳
- **B2L** (bits29:16):Buffer 2 长度
- **VTIR** (bits15:14):VLAN 标签插入/替换——00 不加、01 移除(仅 VLAN 包)、10 插入(MACVIR 或 context descriptor 的值)、11 替换(仅 VLAN 包)
- **HL/B1L** (bits13:0):TSE 置位时 = 头长(只取 bits9:0,**最大 1023 字节**,从以太网源地址到 TCP 头尾);否则 = Buffer 1 长度

**TDES3 (Read)**:
- **OWN** (bit31):1=DMA 拥有;DMA 传输完缓冲数据后清 0
- **CTXT** (bit30):normal 描述符必须为 0(context 描述符为 1)
- **FD** (bit29):包的第一个描述符
- **LD** (bit28):包的最后一个描述符(B1L/B2L 必须非零)
- **CPC** (bits27:26):CRC/Pad 控制(仅 FD 有效)——00 CRC+Pad 插入、01 只插 CRC(应用自己补 Pad)、10 全禁(应用自带 Pad+CRC)、11 CRC 替换(用重算的 CRC 覆盖最后 4 字节);**TSE 置位时忽略此字段**(分段时 CRC/Pad 总是自动做)
- **SAIC** (bits25:23):源地址插入控制——bit25 选 MACA1/MACA0,bit24:23:00 不加、01 插入(应用应给无 SA 的帧)、10 替换(应用应给有 SA 的帧)、11 保留;仅 FD 有效
- **SLOTNUM** (bits22:19):AV 槽号控制,与 DMACxSFCSR.RSN 比较匹配才取数据(仅 AV 通道);非 AV 时 = **THL**(TCP 头长,最小 5;UDP 为 2;仅 FD 有效)
- **TSE** (bit18):TCP 分段使能(FD 必须置位)
- **CIC/TPL** (bits17:16):校验和插入控制——00 禁、01 仅 IP 头校验和、10 IP+payload(伪头不硬件算)、11 IP+payload+伪头硬件算;TSE 置位时是 TCP payload 长度 bits[17:16](支持到 256 KB)
- **FL/TPL** (bits15:0):TSE 时 = payload 长度低 15 位(**不含以太网头/TCP-UDP-IP 头**)

**Write-back 格式(只在包的 LD 描述符回写)**：
- TDES0:TTSL——发送包时间戳低 32 位(TTSE 置位且 LS+TTSS 才写)
- TDES1:TTSH——时间戳高 32 位
- TDES2:保留
- TDES3:OWN 清 0 + Status[30:0](发送状态)

**TDES3 Write-back 状态字段 (Table 535)**：

| 位 | 名称 | 含义 |
|---|---|---|
| 31 | OWN | 传输完成后清 0(回写完必为 0) |
| 30 | CTXT | normal = 0 |
| 29 | FD | 首描述符 |
| 28 | LD | 末描述符——**只有 LD 描述符回写状态字段** |
| 17 | TTSS | 时间戳状态——已捕获发送时间戳,TDES0/1 有效(仅 LD) |
| 15 | **ES** | 错误汇总 = 下列位逻辑 OR:JT、FF、PCE、LoC、NC、LC、EC、ED、UF、IHE——**驱动先看 ES,为 1 再细查** |
| 14 | JT | Jabber 超时(仅 MACCR.JD=0 时置位) |
| 13 | FF | 包被 flush(软件 flush 命令) |
| 12 | PCE | payload 校验和错误——COE 失败未插入校验和。原因:IP 头 payload 长度字段不足;或存储转发模式下 Tx FIFO 深度 < 包长(防死锁,FIFO 满就转发,校验和还没算完);或传输中总线错误 |
| 11 | LoC | 载波丢失——半双工、无碰撞传输中 CRS 无效≥1 个发送时钟周期 |
| 10 | NC | 无载波——PHY 载波感知未断言 |
| 9 | LC | 晚碰撞——碰撞窗口之后碰撞(MII 64 字节时;GMII 512 字节时含前导+载波扩展);**UF 置位时无效** |
| 8 | EC | 过度碰撞——16 次连续碰撞中止;MACCR.DR=1 时首次碰撞即置位并中止 |
| 7:4 | CC | 碰撞计数(EC 置位时无效) |
| 3 | ED | 过度延迟——延迟超 24,288 bit 时间(1000M/Jumbo:155,680 bit 时间);仅 MACCR.DC=1 时 |
| 2 | UF | 下溢——内存数据迟到;空 Tx 缓冲;MTL FIFO 填充慢于发送速率;传输挂起并置 MTLISR 对应队列下溢位 |
| 1 | DB | 延迟发送(半双工,发送前检测到载波) |
| 0 | IHE | IP 头错误——COE 检测到;若以太网类型为 IPv4 仍会插入 IPv4 头校验和 |

**发送 Context 描述符**：
- 可在任意包描述符之前提供;**上下文对当前包及后续包都有效**(持续到被覆盖)
- 用途:one-step 时间戳校正的时间戳、VLAN 插入的 tag ID
- 回写只在 context 描述符上复位 OWN 位

**Context 描述符补充规则**：
- context 提供的 VLAN tag ID 和 MSS(带 Valid 位)被 **DMA 内部存储**
- 一旦有效,最后一个有效 VLAN tag **一直传给 MTL,应用无法使它失效**——只能被新的有效值覆盖
- **Inner VLAN tag 控制只对紧跟 context 的那个包生效**——每个要用内 VLAN 的包前都要放 context

**TDES0/TDES1 (Context)**:TTSL/TTSH——one-step 校正用的时间戳参考值(仅 TDES3.OSTC + TCMSSV 都置位时有效)

**TDES2 (Context)**：
- **IVT** (bits31:16):内 VLAN tag(仅 IVLTV=1 且 TCMSSV=OSTC=0 时)
- **MSS** (bits13:0):TCP/IP payload 分段大小(仅 TCMSSV=1 且 OSTC=0 时)

**TDES3 (Context)**：
- **OWN** (31)、**CTXT** (30)=1(区分 normal)
- **OSTC** (27):one-step 时间戳校正使能(用 TDES0/1 的值)
- **TCMSSV** (bits25:24):双用途——OSTC=1 时表示 TDES0/1 时间戳校正输入有效;OSTC=0 且后续 normal 描述符 TSE=1 时表示 TDES2.MSS 有效
- **CDE** (bit23):context 描述符错误——DMA 回写时置位。错误情形:context 出现在包首描述符之前(顺序错)、全 1、CTXT+LD+FD 同时置位
  - 发生时:Tx DMA 以 DE+LD 关闭描述符;对应首描述符 IOC=1 则置 TI
  - **后续描述符可能被当成 FD,发出部分包**
  - 归类为**异常事件,恢复只能软复位**(停止-重配-重启机制不支持!)
- **IVTIR** (bits21:20):内 VLAN 插入/替换——00 不加、01 移除、10 插入(MACIVIR 或 context 值)、11 替换;VLAN 修改时 MAC **自动重算并替换 CRC**
- **IVLTV** (18):IVT 字段有效
- **VLTV** (16):VT 字段有效
- **VT** (bits15:0):VLAN tag 值(仅 MACVIR.VLTI=0 时作为 tag 使用)

### 63.10.4 接收描述符

- DMA 只在**尾指针 ≠ 基指针/当前指针**时才读描述符
- **推荐环长度至少容纳 2 个完整包**,否则性能大降(描述符不足 → MTL RxFIFO 满 → 丢包)
- 三种接收描述符:normal(read/write-back 格式)、context
- 软件准备 normal 描述符 → DMA 收包后关闭描述符并回写状态
- **部分包状态超长** → Rx DMA 把扩展状态写到**下一个描述符**(不处理/不使用该描述符的缓冲指针)——即 Figure 809 的接收 context 描述符

**Read format (Figure 807)**：
- **RDES0**:BUF1AP——Buffer 1 地址(可字节对齐,LS 位可非零)。**包首**传输时 DMA 的写操作按对齐边界,但数据按实际偏移移位;包**中间/末尾**部分则忽略偏移,写满整个位置
- **RDES1**:保留
- **RDES2**:BUF2AP——Buffer 2 地址(规则同 RDES0)
- **RDES3**:OWN(31)、IOC(30)、保留(29:26)、BUF2V(25)、BUF1V(24)
  - **OWN**:DMA 收完包或缓冲填满时清 0
  - **IOC**:DMA 关闭该描述符时向应用发中断
  - **BUF1V/BUF2V**:缓冲地址有效位——**必须置位 DMA 才往那个地址写数据**
  - 若缓冲地址全 0 → MAC 不往该缓冲传数据,直接跳下一个缓冲/描述符

**Write-back 格式 (Figure 808)**——DMA 收完包回写:
- RDES0:内 VLAN tag[31:16] + 外 VLAN tag[15:0](RS0V 置位时有效;内 tag 还需双 VLAN + 剥离使能)
- RDES1:OAM code / MAC 控制 opcode[31:16] + 扩展状态[15:0]
- RDES2:MAC 过滤状态
- RDES3:OWN + 状态 + ES + 包长度

**RDES1 扩展状态 (Table 545)**：
- **OPC**[31:16]:OAM 子类型码(RDES3[18:16]=111 时)或 MAC 控制包 opcode(=110 时)
- **TD** (15):时间戳被丢弃(捕获了但 MTL RxFIFO 溢出丢掉了)
- **TSA** (14):时间戳可用——值在**下一个 context 描述符**的 RDES1/RDES2 里(仅 LD 有效)
- **PV** (13):PTP 版本(v2=1 / v1=0)
- **PFT** (12):PTP 消息直接走以太网
- **PMT**[11:8]:PTP 消息类型——0000 无、0001 SYNC、0010 Follow_Up、0011 Delay_Req、0100 Delay_Resp、0101 Pdelay_Req、0110 Pdelay_Resp、0111 Pdelay_Resp_Follow_Up、1000 Announce、1001 Management、1010 Signaling、1111 保留类型
- **IPCE** (7):IP payload 校验和错误(校验和不匹配/段长与 IP 头不符/段长小于最小);**⚠️ 此位置位时 RDES3.ES 不置位**
- **IPCB** (6):校验和卸载旁路
- **IPV6/IPV4** (5/4):检测到 IPv6/IPv4 头
- **IPHE** (3):IP 头错误(头校验和不符/版本与以太网类型不一致/头字节数不对);IPV4/6 置位时有效
- **PT**[2:0]:payload 类型——000 未知或未处理、001 UDP、010 TCP、011 ICMP、100 IGMP(IPv4 时)、101 AV untagged control、110 AV tagged data、111 AV tagged control;IP 头错误或分片时为 000

**RDES2 MAC 过滤状态 (Table 546)**：
- **L3L4FM**[31:29]:匹配的 L3/L4 过滤器号(0-7,多个匹配取最低号)
- **L4FM** (26):L4 端口匹配;仅"L3 未使能且所有使能 L4 匹配"或"全部 L3/L4 匹配"时给状态
- **L3FM** (25):L3 IP 地址匹配
- **MADRM**[24:19]:HF=0 时 = 匹配的 MAC 地址寄存器号(DAF=0 时有效);HF=1 时 = 哈希值
- **HF** (18):包通过 MAC 地址哈希过滤
- **DAF/SAF** (17/16):DA/SA 地址过滤失败
- **VF** (15):VLAN tag 通过 VLAN 过滤
- **ARPNR** (10):ARP 回复未生成(MAC 忙,一次只处理一个 ARP 请求——对应 63.5.7)

**RDES3 状态 (Table 547,部分)**：
- **OWN** (31)、**CTXT** (30)、**FD** (29)、**LD** (28)
- **CTXT+FD 组合**:00=中间描述符、01=首描述符、10=保留、**11=描述符错误(全 1)**
  - 描述符错误时:RxDMA 关闭并跳过该描述符,**不写缓冲**;置 DMACxSR.CDE 但**不置 RI**(即使 IOC 置位);后续有效描述符继续写数据
- **RS2V/RS1V/RS0V** (27:25):对应 RDES2/RDES1/RDES0 扩展状态有效
- **CE/GP/RWT/OE/RE/DE/LT** (24:16):CRC 错误、巨大包、接收看门狗超时、溢出、接收错误、描述符错误、L3/L4 过滤类型(细节下页)
- **ES** (15):错误汇总;**PL**[14:0]:包长度

**RDES3 状态位细节 (Table 547 续)**：
- **FD** (29):首缓冲;首缓冲大小为 0 则包从第二缓冲开始;两缓冲都为 0 则从下一描述符开始
- **LD** (28):末缓冲
- **RS2V/RS1V/RS0V** (27:25):RDES2/RDES1/RDES0 状态由 DMA 写入且有效(仅 LD 置位时)
- **CE** (24):CRC 错误(仅 LD)
- **GP** (23):巨大包——超过最大以太网长度 1518/1522/2000 字节(jumbo:9018/9022);**只报长度不截断**
- **RWT** (22):接收看门狗超时——当前包在超时后**被截断**
- **OE** (21):Rx FIFO 溢出,包损坏——仅阈值模式传部分包时置位;**存储转发模式下部分包全丢,不会置 OE**
- **RE** (20):接收错误——RX_ER 在 RX_DV 期间断言;含 GMII 半双工载波扩展错误
- **DE** (19):dribble bit 错误(字节非整数倍,奇半字节;仅 MII 模式)
- **LT** (18:16):长度/类型字段——000 长度包、001 类型包、011 **ARP 请求包**、100 类型包带 VLAN、101 类型包带双 VLAN、110 MAC 控制包、111 OAM 包
- **ES** (15):错误汇总 = DE、RE、OE、RWT、GP、CE 的逻辑 OR(**注意:不含 IPCE/协议层错误**;仅 LD 有效)
- **PL** (14:0):包长度(含 CRC)——LD 置位且 OE 清 0 时有效;**IP 校验和使能且非 MAC 控制包时长度 +2 字节**;LD 清 0 时 = 当前包**累积已传字节数**(多描述符包靠它跟踪进度)

**接收 Context 描述符 (Figure 809)**——**应用只读,DMA 独占写**:
- 提供最后收包的扩展状态(时间戳)
- 触发:normal 描述符的 RDES1.TSA=1 时,下一个描述符即 context 类型
- **RDES0**:RTSL——接收时间戳低 32 位
- **RDES1**:RTSH——高 32 位;**RTSL 与 RTSH 全为 1 时时间戳视为损坏**
- **RDES2**:保留
- **RDES3**:OWN(31)、CTXT(30)=1、其余保留
