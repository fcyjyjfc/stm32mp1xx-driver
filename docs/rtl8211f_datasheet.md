# RTL8211F-CG 以太网 PHY 手册笔记 (全文转写)

> 来源: Realtek RTL8211F(I)/RTL8211FD(I)/RTL8211FI(I)/RTL8211FDI(I) Datasheet, Rev 1.1, 2014-02-21
> 记录日期: 2026-8-27
> 型号家族: F-CG = 内置开关稳压器 / FD-CG = 内置 LDO; I 后缀 = 工业级 (-40~85°C)
> 封装: 40-Pin QFN 5x5mm, 55nm 工艺, Green Package (料号尾部 G)
> 用途: STM32MP157 GMAC 的 RGMII PHY (板载 PHY, MDIO 扫描调试验证用)

## 1. 概述

符合 IEEE 802.3 / 802.3u (100Base-TX) / 802.3ab (1000Base-T) 的以太网收发器, 通过 CAT.5 UTP 收发以太网包.
MAC↔PHY 数据接口 = **RGMII**, 支持 RGMII 信令电压 3.3/2.5/1.8/1.5V.

内部技术: DSP + AFE (模拟前端), 实现交叉检测自动纠正 (MDI/MDIX), 极性纠正, 自适应均衡,
串扰消除, 回波消除, 时钟恢复, 纠错 — 覆盖 10/100/1000Mbps.

## 2. 特性列表

| 类别 | 特性 |
|---|---|
| 标准 | 1000Base-T (802.3ab) / 100Base-TX (802.3u) / 10Base-T (802.3) |
| 接口 | RGMII, 信令电压可选 3.3/2.5/1.8/1.5V |
| EEE | 支持 IEEE 802.3az-2010 能效以太网 |
| WOL | 内置 Wake-on-LAN + 中断功能 (共享引脚) |
| 链路 | 支持并行检测 (Parallel Detection), 交叉检测自动纠正, 极性自动纠正 |
| 其他 | Baseline Wander 纠正; 千兆下 CAT.5 支持 120m; 支持 PHYRSTB 核心电源关断 |
| 时钟 | 25MHz 外部晶振或 OSC; 给 MAC 提供 125MHz 参考时钟 (CLKOUT 引脚) |
| 省电 | Link Down 功率节省 (ALDPS); Green Ethernet (千兆/百兆模式) |
| 电源 | 内置开关稳压器 (F 型) 或 LDO (FD 型) |
| 封装 | 40-pin QFN |

## 3. 系统应用

DTV / MAU / CNR / 游戏机 / 打印机办公设备 / DVD 录放机 / Hub / Switch,
以及任何带以太网 MAC、需要 UTP 物理连接的嵌入式系统.

**Figure 1/2 应用图要点**: 3.3/2.5/1.8/1.5V 指 IO pad 外部供电情形 (非内置 LDO 供电);
磁口 (RJ45+变压器) 经 MDI0~MDI3 四对差分接入.

## 4. 框图

**Figure 3 框图结构** (自左向右): MDI 口 ↔ AFE/混合电路 + DSP 均衡回波消除 ↔ PCS 编解码 ↔ RGMII
接口 ↔ MAC. 管理侧: MDC/MDIO 寄存器组 + 中断/WOL 逻辑 + LED 驱动; PLL 由 25MHz 晶振产生.

## 5. 引脚分配

40-Pin QFN, Figure 4. Green 封装以料号 GXXXV 中 `G' 标识.

引脚号速查 (来自 §6 各表):

| 组 | 引脚号 | 名称 |
|---|---|---|
| Transceiver (MDI) | 1,2 (PAIR0) / 4,5 (PAIR1) / 6,7 (PAIR2) / 9,10 (PAIR3) | MDIP/N[3:0] |
| Clock | 36 XTAL_IN, 37 XTAL_OUT/EXT_CLK, 35 CLKOUT | 见 Table 2 |
| RGMII | 20 TXC, 18~15 TXD0~TXD3, 19 TXCTL, 27 RXC, 25~22 RXD0~RXD3, 26 RXCTL | 见 Table 3 |
| Management | 13 MDC, 14 MDIO, 31 INTB/PMEB | 见 Table 4 |
| Reset | 12 PHYRSTB | 见 Table 5 |
| Mode/LED (strap 复用) | 22,27,26,23,24,25 = PHYAD2..0 等; 32,33,34 = CFG_EXT/CFG_LDO0/CFG_LDO1 | 见 Table 6/7 |
| Regulator | 39 RSET, 30 REG_OUT, 28 DVDD_RG | 见 Table 8 |
| Power/GND | 29 DVDD33, 21 DVDD10, 11/40 AVDD33, 3/8/38 AVDD10, 41 GND(含散热焊盘) | 见 Table 9 |

## 6. 引脚描述

引脚类型缩写:
`I`=输入, `O`=输出, `IO`=双向, `P`=电源, `G`=地;
**`LI`=复位/上电期间锁存输入 (strap!)**, **`PD`/`PU`=上电复位期间的内部下拉/上拉**; OD=开漏.

### 6.1 Transceiver 接口 (Table 1)

MDI0~3 四对差分双向 IO.
- MDI 模式: 1000Base-T 的四对 = BI_DA/BI_DB/BI_DC/BI_DD; 10/100Base-T 只用 PAIR0 收发、PAIR1 对收发
- MDI crossover 模式: 发送对与接收对互换

### 6.2 Clock (Table 2)

| 引脚 | 名称 | 类型 | 说明 |
|---|---|---|---|
| 36 | XTAL_IN | I | 25MHz 晶振输入 |
| 37 | XTAL_OUT/EXT_CLK | O/I | 晶振输出; 外部 25MHz OSC 时接 OSC 输出 |
| 35 | CLKOUT | O | 内部 PLL 生成的 125/25MHz 参考时钟输出, 频率由 Reg25.11 选; MAC 不用时悬空 |

### 6.3 RGMII (Table 3) — 注意 strap 类型标注

| 引脚 | 名称 | 类型 | 说明 |
|---|---|---|---|
| 20 | TXC | I | TX 参考时钟: 千兆 125MHz / 百兆 25MHz / 十兆 2.5MHz, 由 MAC 提供 |
| 18~15 | TXD[3:0] | I | 发送数据 (MAC→PHY), 双沿采样 |
| 19 | TXCTL | I | 发送控制 (MAC→PHY) |
| 27 | RXC | **O/LI/PD** | RX 参考时钟: 同样按速率 125/25/2.5MHz |
| 25 | RXD0 | **O/LI/PU** | 接收数据 (PHY→MAC) |
| 24 | RXD1 | **O/LI/PD** | " |
| 23 | RXD2 | **O/LI/PD** | " |
| 22 | RXD3 | **O/LI/PU** | " |
| 26 | RXCTL | **O/LI/PD** | 接收控制 |

RXC 由 PHY 产生并驱动. **RXD/RXCTL/RXC 在硬件复位期间是 strap 锁存输入** → 见 §7.8.

### 6.4 Management Interface (Table 4)

| 引脚 | 名称 | 类型 | 说明 |
|---|---|---|---|
| 13 | MDC | I | 管理时钟 (MAC 提供) |
| 14 | MDIO | IO/**PU** | 管理数据, 板上需外部上拉 |
| 31 | INTB/PMEB | O/**OD** | 中断/PME 共享脚, 低有效电平触发; 默认 INTB. 可由 Page 0xd40 Reg22 bit5 切换. 不用则悬空 |

INTB 与 PMEB 都支持 3.3V 上拉.

### 6.5 Reset (Table 5)

引脚 12 PHYRSTB, 低有效. **完整复位要求拉低至少 10ms**, 复位后所有寄存器恢复默认值.

### 6.6 Mode Selection (Table 6)

strap 引脚汇总 (类型 = 复位期间锁存输入 + 内部弱上/下拉):

| 引脚 | strap 名 | 类型(内部默认) | 功能 |
|---|---|---|---|
| 22 (RXD3) | PHYAD0 | PU→1 | PHY 地址 bit0 |
| 27 (RXC) | PHYAD1 | PD→0 | PHY 地址 bit1 |
| 26 (RXCTL) | PHYAD2 | PD→0 | PHY 地址 bit2 |
| 23 (RXD2) | PLLOFF | PD→0 | ALDPS 进低功耗时是否停 PLL: 1=停 PLL (4.7kΩ 上拉) / 0=PLL 继续走 |
| 24 (RXD1) | TXDLY | PD→0 | RGMII TX 时钟时序控制: **上拉给 TXC 增加 2ns 内部延时** |
| 25 (RXD0) | RXDLY | PU→1 | RGMII RX 时钟时序控制: 上拉给 RXC 增加 2ns 内部延时 |
| 32 (LED0) | CFG_EXT | PD→0 | IO pad 电源模式: 1=外部供电 / 0=内置 LDO 变换 |
| 33 (LED1) | CFG_LDO0 | PU→1 | LDO 输出电压选择 bit0 / 外部电源电压选择 bit0 |
| 34 (LED2) | CFG_LDO1 | PD→0 | 同上 bit1 |

CFG_EXT=0 时 CFG_LDO[1:0] = LDO 输出电压: 00=保留, 01=2.5V, 10=1.8V, 11=1.5V;
CFG_EXT=1 时 CFG_LDO[1:0] = 外部 pad 电源选择: 00=3.3V, 01=2.5V, 10=1.8V, 11=1.5V.

> ⚠️ 仅按内部上下拉推算的"全悬空默认配置": PHYAD=001(地址1)、TXDLY=0、RXDLY=1(默认加 RX 延时 2ns)、
> PLLOFF=0、CFG_EXT=0+LDO=01(2.5V?!) — 实际板卡普遍通过外接电阻重设这些 strap,
> **必须以原理图上的 4.7kΩ 上/下拉为准**, 尤其 PHYAD 三位决定 MDIO 扫描的目标地址.
> CFG_LDO 组的解读需结合板卡 LED 脚是否有上拉电阻到 3.3V (见 §7.9).

### 6.7 LED 默认行为 (Table 7)

| 引脚 | 高电平含义 | 其他 |
|---|---|---|
| 32 LED0 | 10Mbps Link Up 时常亮 | 收发闪烁 |
| 33 LED1 | 100Mbps Link Up 时**低**亮 | (高低有效取决于 §7.14 配置) |
| 34 LED2 | 1000Mbps Link Up 时常亮 | " |

注意: 高/低有效取决于硬件 strap 配置 (§7.9): strap 电阻拉高 → 该路 LED 驱动为低有效; 拉低 → 高有效.

### 6.8 Regulator 与基准 (Table 8)

- 39 RSET: 基准电流外接电阻
- 30 REG_OUT: F 型=开关稳压器 1.0V 输出 (接 2.2µH/4.7µH 电感); FD 型=LDO 1.0V 输出

### 6.9 Power/Ground (Table 9)

29 DVDD33 数字 3.3V; 28 DVDD_RG IO pad 电源(外供模式时接外部电源); 21 DVDD10 核心 1.0V;
11/40 AVDD33; 3/8/38 AVDD10; 41 GND + 散热焊盘同地.

## 7. 功能描述

### 7.1 发送器

- **1000M**: PCS 从 RGMII 收字节流, 经 4D-PAM5 编码生成连续码组 → 波形成型滤波(降 EMI) → DAC 以 125MBaud/s 送四对线缆
- **100M**: RGMII 每 25MHz(TXCLK) 送 4bit nibble → 4B/5B 编码 → 加扰串行化 → 125MHz NRZ/NRZI → MLT3 编码 → DAC
- **10M**: 每 2.5MHz(TXCLK) 送 nibble → 串行化为 10Mbps 曼彻斯特码流 → DAC 上线

### 7.2 接收器

- **1000M**: 入线先过混合电路消近端回波, 再自适应均衡 + BLW(基线漂移)纠正 + 串扰/回波消除 + 时钟恢复 + 纠错 + 4D-PAM5 解码, 还原 8bit 数据 @125MHz 送 RGMII
- **100M**: MLT3 → ADC → 均衡器 → BLW → 时钟恢复 → MLT3/NRZI 解码 → 解扰 → 4B/5B 解码 → 25MHz 4bit nibble 送 RGMII
- **10M**: 差分信号 → 曼彻斯特解码 → 反串行化 → 2.5MHz 4bit nibble 送 RGMII

### 7.3 EEE 能效以太网

支持 802.3az-2010 @10/100/1000M. 无包可发时进 Low Power Idle 省电 (关闭大部分电路),
发包时快速唤醒回正常态 — 过程不改变链路状态、不丢帧不坏帧. EEE 自带协商机制判定双方能力
(MMD 设备 3/7 寄存器, 见 §8.2).

### 7.4 WOL 唤醒

监视网络中的 Magic Packet 或 Wakeup Frame, 触发时把 PMEB 脚 (pin31) 拉低通知系统. PMEB 建议 4.7kΩ 上拉 3.3V.

**Magic Packet 触发条件**: 目的地址被本芯片接受 (广播/组播/单播自身) + 无 CRC 错 +
payload 匹配 `6×FFh + MISC(可无) + 16×DID(目的 ID)` 出现在包内任意位置.

**Wakeup Frame 触发条件**: 目的地址被接受 + 无 CRC 错 + 包的 16bit CRC 匹配 OS 下发的样例帧 CRC
(或配置成直接网络包唤醒).

注意 2 细节: 支持 8 个长唤醒帧, 每个 128 字节掩码 (覆盖包头偏移 0~127); CRC16 多项式 = x^16+x^12+x^5+1.

注意 1: INTB/PMEB 共享 pin31, 由 Page 0xd40 Reg22 bit5 选择功能.

### 7.5 中断

媒体状态变化 → INTB 拉低 → MAC 读 Page0 Register19 (INSR, 0x13) 响应 → **读完自动撤销中断**.
省掉轮询 MDIO.

- 电平触发机制
- 与 PMEB 共享 pin31 (切换方式同上)

### 7.6 INTB/PMEB 使用

- 默认 = INTB 模式 (Page 0xd40 Reg22 bit5=0). 此模式下 WOL 事件也被并入中断事件之一
  (**Page0 Reg18(INER) bit7 = PME of WOL** 使能后触发). 但此共享模式下不支持"低脉冲"波形, 只有低电平持续
- 选 PMEB 模式 (bit5=1) 后 pin31 为完整 PMEB 功能, 中断功能停用

### 7.7 MDI 接口

四对差分双向, **片内集成端接电阻** (省 BOM/PCB). 1000Base-T 四对同时双向收发; 10/100 与自协商只使用 A/B 两对.

### 7.8 硬件配置 (strap → 配置项)

CONFIG 引脚 → 配置矢量映射 (Table 10), 用 **4.7kΩ 外部上/下拉电阻**设定 (不许直接接电源/地):

| CONFIG 脚 | 配置项 | | CONFIG 脚 | 配置项 |
|---|---|---|---|---|
| RXD3 | PHYAD[0] | | RXD2 | PLLOFF |
| RXC | PHYAD[1] | | RXD1 | TXDLY |
| RXCTL | PHYAD[2] | | RXD0 | RXDLY |
| LED0 | CFG_EXT | | LED1/LED2 | CFG_LDO[0]/[1] |

Table 11 定义:

| 项 | 说明 |
|---|---|
| PHYAD[2:0] | PHY 地址, **合法范围 00001~00111**. Note1: 地址 0 的 MDIO 命令是广播, 每个 PHY 都应答 (可由 Reg24.13=0 关闭). Note2: strap=000 时可用 Reg24.6=1 使芯片记住首个非零地址 |
| PLLOFF | ALDPS 模式进低功耗时停不停 PLL |
| TXDLY | 1: TXC 加 2ns 延时 (TXD 锁存用) / 0: 不加 *(手册此处正文笔误写成"RXC", 以 Table 6 的 TXC 为准)* |
| RXDLY | 1: RXC 加 2ns 延时 (RXD 锁存用) / 0: 不加 |
| CFG_EXT | IO pad 电源模式 (内置 LDO vs 外部供电) |
| CFG_LDO[1:0] | 内置 LDO 输出电压 / 或外部 pad 电源电压选择 (见 §6.6) |

**关键结论 (调试相关)**:
- 广播地址 0 默认会被响应 (PHYCR1.13 复位默认 =1) — MDIO 扫描在 addr0 读到数据不等于 PHY 就挂在 addr0
- MDIO 扫描应覆盖地址 1~7 并比对原理图 strap

### 7.9 LED 与 PHY 地址复用约束

LED0/1/2 与 CFG_EXT/CFG_LDO0/CFG_LDO1 双向复用: 复位期间采样电平决定配置; 运行期作 LED 驱动.
采样电平同时决定该路 LED 驱动的有效极性:

- CFG 脚被电阻拉高 → 对应 LED 输出为**低有效**驱动
- CFG 脚被电阻拉低 → 对应 LED 输出为**高有效**驱动

硬件要求: 这些脚必须经电阻 (4.7kΩ 例) 上/下拉, **禁止直连 VCC/GND**. 若不用 LED 指示,
可去掉 LED+510Ω 支路元件.

### 7.10 Green Ethernet (仅千兆/百兆模式)

- 原理: 动态检测线缆长度, 按长度动态调整发送功耗
- **默认为使能**; 手册给出的关断序列: `Reg27 ← 0x8011`, `Reg28 ← 0x573f`
  (再次使能序列: `Reg27 ← 0x8011`, `Reg16 ← 0xd73f`)
- 注: 该两寄存器属于 Realtek 内部分页空间, 本 Rev 数据手册未给出页号归属细节

### 7.11 MAC/PHY 接口

#### 7.11.1 RGMII

| 模式 | TXC/RXC 频率 | 时钟来源 |
|---|---|---|
| 1000Base-T | 125MHz | TXC 由 **MAC** 提供; RXC 由 **PHY** 提供 |
| 100Base-TX | 25MHz | " |
| 10Base-T | 2.5MHz | " |

TXD/TXD 都是时钟双沿采样. 时序细节见 §10.6.2.

#### 7.11.2 Management Interface (MDC/MDIO)

- 标准 IEEE 802.3u Clause 22 帧; MDC 由 MAC 给出, MDIO 与之同步
- **MDIO 线需要 1.5kΩ 上拉电阻**维持空闲/turnaround 为高
- 多 PHY 共享一条 MDIO 时, 各占唯一地址 (复位锁存). 一条 MDIO 总线最多挂 8 个 PHY

帧格式 (Table 12):

```
         Preamble     ST   OP   PHYAD    REGAD    TA      DATA(16b)   IDLE
读 C22 :  32×'1'       01   10   AAAAA    RRRRR    Z0   DDDDDDDDDDDDDDDD   Z
写 C22 :  32×'1'       01   01   AAAAA    RRRRR    10   DDDDDDDDDDDDDDDD
```

字段说明 (Table 13):
- Preamble: 32 个连续 '1', 用于同步; 写操作同样要有 (Reg24.4 复位默认=1 表示校验前导码)
- ST='01'; OP: 读=10, 写=01
- TA (Turnaround): 读帧为 Z0 (第一 bit 时 STA/PHY 都高阻, 第二 bit 由 PHY 驱动 0); 写帧为 10 (MAC 驱动)
- IDLE: 高阻态, 被 MDIO 上拉电阻保持逻辑 1

Figure 6 示例波形: `Preamble ... ST=01 OP=10 PHYAD=00001 REGAD=00000(BMCR) TA Z0 DATA=0x1340 IDLE-Z` —— 即对地址 1 的 PHY 读 BMCR 的实例.

#### 7.11.3 换页 (Page Select)

RTL8211F 寄存器分页访问:
1. `Reg31 ← 0x00XY` 切到 Page XY
2. 访问目标寄存器
3. `Reg31 ← 0x0000` 切回 Page 0

#### 7.11.4 MMD 访问 (Clause 45 设备的兼容访问)

通过 Reg13 (MACR) + Reg14 (MAADR) 两级窗口访问更大的 MMD 地址空间:

1. `Reg13`: Function=00 (address 模式), DEVAD=目标设备地址
2. `Reg14` ← 目标 MMD 内部地址
3. `Reg13`: Function=01 (data 无自增; 另有 10/11 自增变体), DEVAD 同前
4. 读: 第 5 步 / 写: 第 6 步
5/6. 对 `Reg14` 进行读或写

### 7.12 自协商 (Auto-Negotiation)

机制: 决定双方最快连接速率. 经 FLP burst 内发送 16bit Link Code Word (LCW); 接收方须收到 **3 个相同的 LCW**
才算认证生效. 广告千兆能力需要额外的 Next Page 交换 (1000Base-T Message Page + Unformatted Page1/2).

**自协商自动启动时机**: 上电、硬件复位、软件复位 (Reg0.15)、Restart_AN (Reg0.9)、掉电→上电迁移 (Reg0.11)、进入 link fail 态.

#### 优先级仲裁 (最高优先)

1. 1000Base-T 全双工
2. 1000Base-T 半双工
3. 100Base-TX 全双工
4. 100Base-TX 半双工
5. 10Base-T 全双工
6. 10Base-T 半双工

#### Master/Slave 仲裁 (建立千兆链路必经)

- Master 优先权: 多端口 > 单端口; 手动指定 > 自动
- 判定式 (Unformatted Page1 位): Manual_MASTER = U0·U1; Manual_SLAVE = U0·!U1;
  单端口 = !U0·!U2; 多端口 = !U0·U2
- 同配置冲突时比 Unformatted Page2 的 seed SB[10:0], 大者为 Master
- 成功: GBSR.10.15=0 且 10.14=1(Master)/0(Slave); 失败: 自动重启自协商;
  双方都手动 Master/Slave 或 seed 法 7 次失败 → 10.15 故障位置 1

#### PAUSE / ASYMMETRIC PAUSE 仲裁

PHY 只负责在自协商中广告/报告能力 (ANAR bit10/bit11 ↔ ANLPAR bit10/bit11),
最终流控解析由 **MAC 完成**并执行相应动作.

### 7.13 交叉检测与自动纠正 (Auto-MDI/X)

概念: 缺省先按 MDI 试链, 超时未 link 则切 MDI-X 重试, 直到建链. 用 11bit 伪随机定时器控制切换间隔.

与自协商的关系:
- 交叉检测不属于自协商流程本身, 但借用其过程交换 MDI/MDI-X 配置
- 固定速率为 100Base-TX 或 10Base-T 时: 只有交叉检测也被关闭, 自协商才真正关闭;
  否则仍开自协商但只广告该速率 (半双工只广告半双工; 全双工则全双工+半双工都广告)
- 手动开关交叉检测: `Reg24.[9:8]`

**生效时机规则** (调试易踩): 硬件复位后, Reg0.12 (ANE)/0.13,0.6 (speed)/0.8 (duplex) 的改写必须等以下事件之一才生效:
软件复位 (0.15) / Restart_AN (0.9) / 掉电→上电 (0.11).
Reg4/Reg9 在自协商状态机进入 ABILITY DETECT 时内部锁存一次 — 开始发 FLP 后再写无效.
自协商完成后更新寄存器 1/5/6/10; 速度双工等实时状态也在 reg26 (PHYSR) 和 reg29 (INSR? 见手册原文注:
"The speed...also available in registers 26 and 29").

### 7.14 LED 配置

三根 LED 脚可直接驱动 LED. 功能由 Page 0xd04 Reg16 (LCR) 定义. Table 16 位映射
(PDF 分栏还原后, 每 LED 一组: 10M/100M/1000M 三个 LINK 选择位 + 一个 ACT 位):

```
          LINK 10M   LINK 100M   LINK 1000M   ACT(Tx/Rx)
LED0      bit0       bit1        bit3         bit4
LED1      bit5       bit6        bit8         bit9
LED2      bit10      bit11       bit13        bit14
```

16 种组合编码见 Table 17 (LINK 位 [2:0]: 10/100/1000 任意组合; +ACT 位构成 Link+Active).

换页提醒: `Reg31 ← 0x0d04` 进 Page 0xd04; 设置完 `Reg31 ← 0x0000` 回 Page 0.

本手册 LCR 出厂默认对应 Table 7 行为: LED0=10M Link+Act, LED1=100M, LED2=1000M.
具体位值 (per Table 48 的复位默认列): LED2_ACT=1, LED2_LINK_1000=1, LED2_LINK_100=0, LED2_LINK_10=0;
LED1_ACT=1, LED1_LINK_1000=0, LED1_LINK_100=1, LED1_LINK_10=0;
LED0_ACT=1, LED0_LINK_1000=0, LED0_LINK_100=0, LED0_LINK_10=1.

EEE LED: EEE Idle = 连续慢闪; EEE Active = 快+慢闪. 由 Page 0xd04 Reg17 (EEELCR) 使能,
出厂三个 LED 的 EEE 指示位 (17.2/17.1/17.0) 默认全 =1 (使能).

### 7.15 极性纠正

接收对极性错误自动纠正: 千兆按 idle 符号序列判定, 解扰锁定后四对极性一起锁定 (失锁才解锁);
10Base-T 按 link pulse 间隔检测, 建 10M 链路时锁定; 百兆无极性问题.

### 7.16 电源

- 片内稳压器产生核心电源: F 型 = 开关稳压器, FD 型 = LDO. 系统需提供 **3.3V/1A 稳定源**
- 也可外部直接供 1.0V 核心 (须满足电流规格, 0.95~1.05V)
- RGMII pad 电源标准 3.3V, 可降至 2.5/1.8/1.5V (降 EMI), 由内置 LDO 或外电源供给 (strap §6.6/§7.8)

### 7.17 PHY 复位 (硬件复位时序) ★

- PHYRSTB 低电平至少 **10ms** (内部稳压器完全起效)
- 释放后**再等待 30ms** 让内部电路稳定, 之后才能访问 PHY 寄存器
- 硬件复位后所有寄存器回默认值
- 详细参见 RTL8211xx-xx_Power_Sequence_App_Note

> 对照 gmac_test.c: GmacPhyReset 用两个 busy-wait 循环近似这些延时, 实际时长依赖 CPU 频率,
> 需实测确认 ≥10ms 拉低 + ≥30ms 释放窗口.

## 8. 寄存器描述

访问类型 (Table 18):
`RO` 只读 / `RW` 读写 / `WO` 只写 / `RC` 读清除 / `LH` 锁存高 (置 1 后保持) / `SC` 自清零 (写 1 触发后自动回 0).

位地址记法 `N.b` = 寄存器地址 N (十进制同一地址) 的 bit b.

### 8.1 寄存器映射与定义

| 地址 (hex=dec) | 名称 | 类型 | 功能 |
|---|---|---|---|
| 0x00 = 0 | BMCR | RW | 基本模式控制 |
| 0x01 = 1 | BMSR | RO | 基本模式状态 |
| 0x02/0x03 = 2/3 | PHYID1/2 | RO | 出厂标识 |
| 0x04 = 4 | ANAR | RW | 自协商广告能力 |
| 0x05 = 5 | ANLPAR | RO | 对端广告能力 |
| 0x06 = 6 | ANER | RO | 自协商扩展状态 |
| 0x07 = 7 | ANNPTR | RW | Next Page 发送 |
| 0x08 = 8 | ANNPRR | RO | Next Page 接收 |
| 0x09 = 9 | GBCR | RW | 1000Base-T 控制 |
| 0x0A = 10 | GBSR | RO | 1000Base-T 状态 |
| 0x0D = 13 | MACR | WO/RW | MMD 访问控制 (Function+DEVAD) |
| 0x0E = 14 | MAADR | RW | MMD 访问数据窗口 |
| 0x0F = 15 | GBESR | RO | 千兆扩展状态 |
| 0x12 = 18 | INER | RW | 中断使能 |
| 0x13 = 19 | INSR | RO, RC | 中断状态 (**读自动撤 INTB**, 见 §7.5) |
| 0x18 = 24 | PHYCR1 | RW | PHY 特定控制 1 (广播响应/MDI 模式/Jabber/Preamble 校验/ALDPS) |
| 0x19 = 25 | PHYCR2 | RW | PHY 特定控制 2 (CLKOUT 频率/SSC/**RXC 使能**) |
| 0x1A = 26 | PHYSR | RO | PHY 特定状态 (速度双工/实时 link/crossover/ALDPS 态) |
| 0x1F = 31 | EPAGSR(PAGSR) | RW | 扩展页选择 (默认 Page 0) |

其余地址 (0x0B~0x0C, 0x10~0x11, 0x14~0x17, 0x1B~0x1E) 本手册标 RSVD
(但 §7.10 的 Green Ethernet 操作序列会写 Reg27/Reg28/Reg16 → 属 Realtek 内部分页空间).

各寄存器完整位域表见 §8.4 (编号 8.4.1~8.4.28 与手册一一对应).

### 8.2 MMD 寄存器映射与定义

经 reg13(MACR)/reg14(MAADR) 两级窗口访问 (操作流程见 §7.11.4). 手册 Table 20 定义的设备空间:

| DEVAD | 偏移(dec/hex) | 名称 | 类型 | 功能 |
|---|---|---|---|---|
| 3 | 0x00 | PC1R | RW | PCS 控制 1 寄存器 |
| 3 | 0x01 | PS1R | RO | PCS 状态 1 寄存器 |
| 3 | 0x14 (20) | EEECR | RO | EEE 能力寄存器 |
| 3 | 0x16 (22) | EEEWER | RC | EEE 唤醒错误寄存器 |
| 7 | 0x3C (60) | EEEAR | RW | EEE 广告寄存器 |
| 7 | 0x3D (61) | EEELPAR | RO | 对端 EEE 能力 |

详细位域见 §8.4.21~§8.4.26.

### 8.3 其他页寄存器映射与定义

手册 Table 21: 仅 Page 0xd04 上定义了两个寄存器 —

| Page | 偏移(hex=dec) | 名称 | 类型 | 功能 |
|---|---|---|---|---|
| 0xd04 | 0x10 = 16 | LCR | RW | LED 控制寄存器 |
| 0xd04 | 0x11 = 17 | EEELCR | RW | EEE LED 控制寄存器 |

换页方法: `Reg31 ← 0x0d04` 切入, 操作完成后 **必须** `Reg31 ← 0x0000` 回 Page 0.
详细位域见 §8.4.27/~§8.4.28.

### 8.4.1 BMCR — 基本模式控制 (reg0)

| 位 | 字段 | 类型 | 复位值 | 说明 |
|---|---|---|---|---|
| 0.15 | Reset | RW,SC | 0 | 1=软复位. 复位后 reg0/reg1 回默认. SC 位由硬件自动清 0 |
| 0.14 | Loopback | RW | 0 | PCS 环回: RGMII 发送数据环到 RGMII 接收路径 |
| 0.13 | Speed[0] | RW | 0 | 强制模式速率选择 LSB (与 0.6 组合, 见下表) |
| 0.12 | ANE | RW | 1 | 自协商使能 |
| 0.11 | PWD | RW | 0 | Power Down: 仅管理接口存活, 链路 down |
| 0.10 | Isolate | RW | 0 | RGMII 与 MAC 隔离 (忽略 TXD/TXCTL 输入, TXC/RXC/RXCTL/RXD 全高阻); **MDC/MDIO 不受影响** |
| 0.9 | Restart_AN | RW,SC | 0 | 重启自协商 |
| 0.8 | Duplex | RW | 1 | 强制模式全双工选择 (仅 NWay 关闭时有效) |
| 0.7 | Collision Test | RW | 0 | 碰撞测试 |
| 0.6 | Speed[1] | RW | 1 | 强制模式速率选择 MSB |
| 0.5 | Uni-directional enable | RW | 0 | 无链路时允许发包 (不等 linkok) |
| 0.4:0 | RSVD | RO | 00000 | 保留 |

强制速率编码 (Speed[1]=bit6, Speed[0]=bit13): `11`=保留, `10`=1000M, `01`=100M, `00`=10M.

**Note 1**: 从掉电态切回正常, 即使不写 0.15/0.9 也自动执行软复位+重启自协商.
**Note 2**: 速率设为千兆时自协商强制开启; Auto-MDI-X (交叉检测开启时) 优先于 ANE=0.
ANE 关闭时按 0.13/0.6/0.8 广告能力; 开启时以 reg4 (百兆/十兆) 与 reg9.9:8 (千兆) 为准.
**Note 3**: 硬件/软件复位后自协商自动重启 (不管是否写 Restart_AN).

> 验证点: 按 Table 复位默认拼出的 BMCR 上电值 ≈ **0x1140** (仅 0.12/0.8/0.6 为 1).

### 8.4.2 BMSR — 基本模式状态 (reg1)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 1.15 | 100Base-T4 | RO | 0 | 不支持 T4, 恒 0 |
| 1.14 | 100Base-TX 全双工能力 | RO | 1 | " |
| 1.13 | 100Base-TX 半双工能力 | RO | 1 | " |
| 1.12 | 10Base-T 全双工能力 | RO | 1 | " |
| 1.11 | 10Base-T 半双工能力 | RO | 1 | " |
| 1.10 | 10Base-T2 全双工 | RO | 0 | 不支持恒 0 |
| 1.9 | 10Base-T2 半双工 | RO | 0 | " |
| 1.8 | 1000Base-T Ext Status | RO | 1 | 支持扩展状态寄存器 0x0F, 恒 1 |
| 1.7 | Uni-directional ability | RO | 1 | 可无链路发包能力 |
| 1.6 | Preamble Suppression | RO | 1 | 恒接受无前导码帧 ("Permanently On") |
| 1.5 | Auto-Negotiation Complete | RO | 0 | 完成后为 1, 同时 reg5/6/8/10 才有效 |
| 1.4 | Remote Fault | RC,LH | 0 | 读清除/复位清除 |
| 1.3 | Auto-Negotiation Ability | RO | 1 | 具备自协商能力 |
| 1.2 | Link Status | RO | 0 | 无链路时 0. **锁存语义**: 该位反映"上次读取以来是否曾断链", 取当前实时值要么连读两次, 要么读 PHYSR (reg26) 实时位 |
| 1.1 | Jabber Detect | RC,LH | 0 | " |
| 1.0 | Extended Capability | RO | 1 | 恒 1 |

### 8.4.3/8.4.4 PHYID1/PHYID2 (reg2/reg3) ★ 芯片身份

- **PHYID1 (0x02) = 0x001C**: OUI 位 [3:18], 手册标注 Always 0000000000011100b.
  附注: Realtek 的 OUI = 0x000732
- **PHYID2 (0x03)** 组成:

| 位 | 字段 | 默认(二进制) |
|---|---|---|
| 3.15:10 | OUI_LSB | 110010 |
| 3.9:4 | Model Number | 010001 |
| 3.3:0 | Revision Number | 0110 |

拼合: **ID2 = 110010_010001_0110b = 0xC916**

> ★ 结论: RTL8211F-CG (Rev1.1 手册) 正确出厂 ID = **PHYID1=0x001C, PHYID2=0xC916**
> (Linux 内核亦按 0x001cc916 匹配该型号). MDIO 扫描时以此为判定基准;
> 注意不同封装批次 revision 字段可能异于 0110.

### 8.4.5 ANAR — 自协商广告 (reg4)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 4.15 | NextPage | RW | 0 | 是否还有后续 next page |
| 4.13 | Remote Fault | RW | 0 | 广告远端故障 |
| 4.11 | Asymmetric PAUSE | RW | 0 | 广告支持非对称流控 |
| 4.10 | PAUSE | RW | 0 | 广告支持对称流控 |
| 4.9 | 100Base-T4 | RO | 0 | 不支持 |
| 4.8 | 100Base-TX 全双工 | RW | 1 | 广告 |
| 4.7 | 100Base-TX 半双工 | RW | 1 | 广告 |
| 4.6 | 10Base-T 全双工 | RW | 1 | 广告 |
| 4.5 | 10Base-T 半双工 | RW | 1 | 广告 |
| 4.4:0 | Selector Field | RO | 00001 | IEEE 802.3 设备, 恒定 |

Note1: 写 Reg4 不生效, 直到软复位/Restart_AN/上电迁移. Note2: 广告千兆时必要的 next page 会自动发,
不需要额外 next page 则把 4.15 清 0.

> 复位默认拼合 ≈ 0x01E1 (此 Rev 版不含 pause 广告).

### 8.4.6 ANLPAR — 对端能力 (reg5, RO)

结构 = 收到的对端 LCW 位域:

| 位 | 字段 | 说明 |
|---|---|---|
| 5.15 | Next Page | 对端后续页指示 (收到的 bit15) |
| 5.14 | Ack | 收到对端 LCW 的应答 |
| 5.13 | Remote Fault | 对端报告远端故障 |
| 5.12:5 | Technology Ability Field | 支持的技术能力 (对 selector 值语境) |
| 5.4:0 | Selector Field | " |

**1.5 自协商完成前无效**.

### 8.4.7 ANER — 自协商扩展 (reg6)

| 位 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 6.6 | RX NP location ability | RO | 对端 next page 存放位置是否可选 |
| 6.5 | RX NP location | RO | 1: 存 reg8 / 0: 存 reg5 |
| 6.4 | Parallel Detection Fault | RC,LH | 并行检测出故障 |
| 6.3 | Link Partner Next Page Able | RO | 对端支持 next page 交换 |
| 6.2 | Local Next Page Able | RO | 恒 1 |
| 6.1 | Page Received | RC,LH | 收到新 LCW |
| 6.0 | Link Partner AN capable | RO | 对端具备自协商能力 |

### 8.4.8 ANNPTR — Next Page 发送 (reg7)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 7.15 | Next Page | RW | 0 | 1=还有后续 next page |
| 7.14 | Acknowledge | RW | 0 | 对收到 LCW 的应答位 (发送侧) |
| 7.13 | Message Page | RW | **1** | 1=Message 页 / 0=Unformatted 页 |
| 7.12 | Acknowledge 2 | RW | 0 | 1=本机具备遵从所收 message 的能力 |
| 7.11 | Toggle | RO | 0 | NWay 仲裁同步用翻转位 |
| 7.10:0 | Message/Unformatted Field | RW | 0x001 | 页内容. 千兆的 Message Code 固定为 8 |

### 8.4.9 ANNPRR — Next Page 接收 (reg8, RO)

结构 = 收到的对端 Link Code Word:

| 位 | 字段 |
|---|---|
| 8.15 / 8.14 / 8.13 / 8.12 / 8.11 | Next Page / Acknowledge / Message Page / Acknowledge 2 / Toggle |
| 8.10:0 | Message/Unformatted Field (对端页内容) |

自协商完成 (1.5) 前无效.

### 8.4.10 GBCR — 1000Base-T 控制 (reg9)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 9.15:13 | Test Mode | RW | 000 | 000=正常; 001/010/011=发送抖动测试模式 1/2(MASTER)/3(SLAVE); 100=失真测试模式 4 |
| 9.12 | Master/Slave Manual Enable | RW | 0 | 1=手动主从 |
| 9.11 | 主从配置值 | RW | 0 | 9.12=1 时有效: 1=MASTER, 0=SLAVE |
| 9.10 | Port Type | RW | 0 | 1=倾向多端口(MASTER), 0=倾向单端口(SLAVE) |
| 9.9 | 1000Base-T 全双工广告 | RW | **1** | 广告千兆全双工能力 |
| 9.8 | RSVD | RW | 0 | 保留 |

Note: 9.12:9 在软重启前不生效; 9.12=0 时 9.11/9.10 被忽略.

### 8.4.11 GBSR — 1000Base-T 状态 (reg10)

| 位 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 10.15 | Master/Slave 配置故障 | RO,RC,LH | 1=协商失败 (双方同设或 seed 七次未决) |
| 10.14 | 主从结果 | RO | 1=本地为 MASTER / 0=SLAVE |
| 10.13 | 本地接收机 OK | RO | Local Receiver OK |
| 10.12 | 远端接收机 OK | RO | Remote Receiver OK |
| 10.11 | 对端千兆全双工能力 | RO | 须 6.1 (Page Received)=1 后有效 |
| 10.10 | 对端千兆半双工能力 | RO | " |
| 10.7:0 | Idle Error Count | RO,RC | 空闲错误计数, 到 0xFF 自动停 |

自协商完成 (1.5) 前 reg10 整体无效.

### 8.4.12/8.4.13 MACR + MAADR — MMD 访问窗口 (reg13/reg14)

- MACR (reg13): bit15:14 Function (`00`=address 模式 / `01`=data 无自增 / `10`=读写后自增 / `11`=仅写后自增);
  bit4:0 DEVAD 设备地址
- MAADR (reg14): address 模式下写入的是 MMD 内部地址寄存器; data 模式下是该地址处的数据

操作流程见 §7.11.4 (两步一地址、两步一数据). EEE 相关的 MMD 映射见下文 Table 20.

### 8.4.14 GBESR — 千兆扩展状态 (reg15)

| 位 | 字段 | 默认 | 说明 |
|---|---|---|---|
| 15.15 | 1000Base-X FD | 0 | 不支持 |
| 15.14 | 1000Base-X HD | 0 | 不支持 |
| 15.13 | 1000Base-T FD | 1 | 支持 |
| 15.12 | 1000Base-T HD | 0 | 不支持 (本型号仅全双工千兆铜缆) |

### 8.4.15 INER — 中断使能 (reg18)

| 位 | 字段 | 说明 |
|---|---|---|
| 18.10 | Jabber | 使能 Jabber 中断 |
| 18.9 | ALDPS State Change | 使能 ALDPS 状态变化中断 |
| 18.7 | PME (WOL) | 使能 WOL 唤醒事件中断 (**§7.6 所述共享模式下的 WOL 入口**) |
| 18.5 | PHY Register Access | 寄存器访问可达性中断 |
| 18.4 | Link Status Change | **链路状态变化** |
| 18.3 | Auto-Negotiation Completed | 自协商完成 |
| 18.2 | Page Received | 新 LCW 页到达 |
| 18.0 | Auto-Negotiation Error | 自协商错误 |

屏蔽语义: 置 0 只在 INT 引脚上屏蔽该事件; reg29 (INSR) 对应位始终照常反映事件. 全部默认 = 0 (禁用).

### 8.4.16 PHYCR1 — PHY 特定控制 1 (reg24) ★

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 24.13 | PHYAD_0 Enable | RW | **1** | 地址 0 广播响应: 1=MAC 用地址 0 发命令时本 PHY 应答 (§7.8 Note1) |
| 24.9 | MDI 模式手动配置使能 | RW | 0 | 见下 Note |
| 24.8 | MDI Mode | RW | 1 | MDI=1 / MDIX=0, 仅 24.9=1 时生效 |
| 24.7 | TX CRS Enable | RW | 0 | 发送时是否拉 CRS |
| 24.6 | PHYAD Non-zero Detect | RW | 0 | strap 为 000 时锁存首个非零地址作为自身地址 (§7.8 Note2) |
| 24.4 | Preamble Check Enable | RW | 1 | 接收 MDC/MDIO 命令时校验前导码 |
| 24.3 | Jabber Detection Enable | RW | 1 | Jabber 检测使能 |
| 24.2 | ALDPS Enable | RO(原文如此) | 0 | Link Down 省电模式使能 |

强制 MDI/MDIX 流程: Reg24.9=1 + Reg24.8 选 1(MDI)/0(MDIX) → 软复位 (Reg0.15=1).

### 8.4.17 PHYCR2 — PHY 特定控制 2 (reg25) ★★

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 25.11 | CLKOUT Frequency Select | RW | **1** | CLKOUT 输出频率: 1=125MHz / 0=25MHz |
| 25.7 | CLKOUT SSC Enable | RW | 0 | CLKOUT 扩频时钟 |
| 25.3 | RXC SSC Enable | RW | 0 | RXC 扩频时钟 |
| 25.1 | RXC Enable | RO(原文如此) | **0** | "1: RXC clock output enabled" — 手册印刷的复位默认值 |
| 25.0 | CLKOUT Enable | RW | **1** | CLKOUT 输出使能 |

> ★ 调试线索: 若 reg25.1 的"默认 0"在硬件上表现为复位后 RXC 不翻转, 则与 gmac_test 观测
> "无 link 时 RX_CLK 不在场 → DMA SWR 等不到自清零" 相吻合. 该位类型标 RO 但描述为功能位,
> 需要实测写读验证 (或核对更新版手册) 后再用于驱动.

### 8.4.18 PHYSR — PHY 特定状态 (reg26)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 26.13 | ALDPS State | RO | 0 | 1=进入省电态 (拔线后约 3s 反映); 0=插线正常 |
| 26.8 | MDI Plug | RO | 0 | 1=网线已插入 |
| — | NWay Enable | RO | 1 | 自协商进行中标志 |
| — | Master Mode | RO | 0 | 当前主从角色: 1=Master |
| — | EEE capability | RO | 0 | 当前速率下双方均具 EEE 能力时为 1 |
| — | Rxflow/Txflow Enable | RO | 0 | 收/发流控使能 |
| [3:2] | Speed | RO | 00 | 00=10M / 01=100M / 10=1000M / 11=保留 |
| — | Duplex | RO | 0 | 1=全双工 |
| — | Link (Real Time) | RO | 0 | **实时链路状态** (无锁存语义, 单读即当前值) |
| 26.1 | MDI Crossover Status | RO | 1 | 1=MDI / 0=MDI-X |
| 26.0 | Jabber (Real Time) | RO | 0 | 实时 jabber |

(表内位号被 PDF 分栏打散的字段按手册行序转录; Speed/Duplex/Link 三字段即驱动取实时状态的入口.
Note: 26.11 仅千兆有效; 26.8 在 10M 且本地具 EEE 能力时置位.)

### 8.4.19 INSR — 中断状态 (reg19/0x13)

所有位 RO, RC (读清除), 读寄存器同时撤销 INTB:

29.10 Jabber / 29.9 ALDPS 变化 / 29.7 PME(WOL) / 29.5 寄存器可访问性 /
29.4 Link Status Change / 29.3 AN Completed / 29.2 Page Received / 29.0 AN Error; 其余 RSVD.

### 8.4.20 EPAGSR — 扩展页选择 (reg31)

bit11:0 ExtPageSel, RW, 默认 0x000 = Page 0. 换页流程见 §7.11.3.

### 8.4.21 PC1R — PCS 控制 1 (MMD dev3, addr 0x00)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 3.0.15:11 | RSVD | RW | 0 | 保留 |
| 3.0.10 | Clock Stop Enable | RW | 0 | 1=LPI 时允许 PHY 停发 RXC / 0=RXC 不可停 |
| 3.0.9:0 | RSVD | RW | 0 | 保留 |

### 8.4.22 PS1R — PCS 状态 1 (MMD dev3, addr 0x01)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 3.1.15:12 | RSVD | RO | 0 | 保留 |
| 3.1.11 | TX LPI Received | RO,LH | 0 | TX PCS 收到过 LPI |
| 3.1.10 | RX LPI Received | RO,LH | 0 | RX PCS 收到过 LPI |
| 3.1.9 | TX LPI Indication | RO | 0 | TX PCS 当前正在收 LPI (实时) |
| 3.1.8 | RX LPI Indication | RO | 0 | RX PCS 当前正在收 LPI (实时) |
| 3.1.7 | RSVD | RO | 0 | 保留 |
| 3.1.6 | Clock Stop Capable | RO | 1 | LPI 时 MAC 可停 TXC (能力位) |
| 3.1.5:0 | RSVD | RO | 0 | 保留 |

### 8.4.23 EEECR — EEE 能力 (MMD dev3, addr 0x14)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 3.20.15:3 | RSVD | RO | 0 | 保留 |
| 3.20.2 | 1000Base-T EEE | RO | **1** | 支持千兆 EEE |
| 3.20.1 | 100Base-TX EEE | RO | **1** | 支持百兆 EEE |
| 3.20.0 | RSVD | RO | 0 | 保留 |

### 8.4.24 EEEWER — EEE 唤醒错误 (MMD dev3, addr 0x16)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 3.22.15:0 | EEE Wake Error Counter | RC | 0 | PHY 未在规定时间内完成唤醒序列的故障计数, 读清除 |

### 8.4.25 EEEAR — EEE 广告 (MMD dev7, addr 0x3C)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 7.60.15:3 | RSVD | RW | 0 | 保留 |
| 7.60.2 | 1000BASE-T EEE 广告 | RW | **1** | 向对端广告千兆 EEE 能力 |
| 7.60.1 | 100BASE-TX EEE 广告 | RW | **1** | 向对端广告百兆 EEE 能力 |
| 7.60.0 | RSVD | RW | 0 | 保留 |

### 8.4.26 EEELPAR — 对端 EEE 能力 (MMD dev7, addr 0x3D)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 7.61.15:3 | RSVD | RO | 0 | 保留 |
| 7.61.2 | LP 1000BASE-T EEE | RO | 0 | 对端支持千兆 EEE (链路建立后有效) |
| 7.61.1 | LP 100BASE-TX EEE | RO | 0 | 对端支持百兆 EEE |
| 7.61.0 | RSVD | RO | 0 | 保留 |

### 8.4.27 LCR — LED 控制 (Page 0xd04, reg16)

按 Table 16 组内基址 + 偏移规律还原的位映射 ⚠️(PDF 分栏有错位, 组结构可信, 个别位号请以原表复核):

| LEDn | LINK 10M | LINK 100M | LINK 1000M | ACT (Tx/Rx) |
|---|---|---|---|---|
| LED0 | bit0 (=1 复位默认) | bit1 | bit3 | bit4 (复位默认=1) |
| LED1 | bit5 | bit6 (=1 默认) | bit8 | bit9 (=1 默认) |
| LED2 | bit10 | bit11 | bit13 (=1 默认) | bit14 (=1 默认) |

即出厂行为 = LED0 指示 10M 链路+活动 / LED1 指 100M / LED2 指 1000M.
把某 LED 同时配多个速率档 = 该速率链路亮; 再叠 ACT 位 = 收发闪烁.

### 8.4.28 EEELCR — EEE LED 控制 (Page 0xd04, reg17)

| 位 | 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|---|
| 17.15:3 | RSVD | RO | 0 | 保留 |
| 17.2 | LED2 EEE Enable | RW | **1** | LED2 的 EEE 指示使能 |
| 17.1 | LED1 EEE Enable | RW | **1** | " |
| 17.0 | LED0 EEE Enable | RW | **1** | " |

EEE LED 行为: Idle 态连续慢闪; Active 态快+慢闪 (含收发包时).

## 9. 稳压器与电源时序

### 9.1 开关稳压器 (F 型) — PCB 布局硬规则

REG_OUT (1.0V) **只允许接 DVDD10/AVDD10**, 禁止给其他器件供电:

- DVDD33 输入走线 ≥40mil; Cin1/Cin2 电容距 DVDD33 ≤200mil (0.5cm), 防输入过冲
- REG_OUT 输出走线 ≥60mil; 功率电感 Lx (2.2µH/4.7µH) 距 REG_OUT ≤200mil
- Cout1/Cout2 距 Lx ≤200mil; **输入输出电容必须陶瓷 X5R** (Cin 建议陶瓷)
- Lx 与 Cin1 与芯片同层; DVDD33/REG_OUT 走线禁止打孔
- 违反上述规则会损坏 IC

选型表 (Table 50): 电感 GLK2510P-2R2M(1000mA)/GLK2510P-4R7M(750mA)/GTSD32P-2R2M(1500mA), ±20% 允差;
电感须耐 600mA; ESR@1MHz <0.8Ω 时效率一般可过 75%; 不达要求可能损坏稳压器.
实测纹波参考: GTSD32P-2R2M+4.7µF=9.2mV ~ GLK2510P-4R7M+10µF=11.2mV.
电容: 4.7µF 0805 X5R(TDK, ESR≈4.8mΩ) 或 10µF 0603 X5R(YAGEO). 低 ESR → 低纹波.

测量判据:
- 输入: 在 **DVDD33 引脚处**测 (不是电容端子), 过冲不得超 **4V**; Cin 靠近引脚来压低过冲
- 输出: REG_OUT 处纹波 ≤**100mVpp**
- 选型避坑: Y5V 陶瓷会直接让稳压器失效; 电解并联可降纹波但不如陶瓷;
  1µH 磁珠代替功率电感会导致千兆时电流不足波形畸变 (Table 21 对比图)
- 效率公式 `η = Vcpb·Icpb / Vcpa·Icpa` (Vcpa=3.3V, Vcpb=1.05V, 千兆流量下测),
  建议 η>75%: 例 GOTREND GTSD32-4R7M 实测 82.3%

### 9.2 LDO (FD 型)

LDO_OUT (1.0V) 同样只接 DVDD10/AVDD10.

### 9.3 上电时序 (Table 51)

| 符号 | 含义 | Min | Max |
|---|---|---|---|
| Rt1 | 3.3V 上升时间 | **0.5ms*** | 100ms |
| Rt1 | 2.5/1.8/1.5V RGMII 电源上升 | - | 100ms |
| Rt2 | 3.3V 关断保持时间 | 100ms | - |
| Rt3 | 核心逻辑就绪时间 | 20ms | - |
| Rt4 | LDO 就绪时间 | 1.5ms | - |

Notes:
1. 不支持快速上电: 3.3V 上升须 >0.5ms (*0.1~0.5ms 仅当 3.3V 电源预算充足保证 OCP 不触发); <0.1ms 会在
   DVDD33 感应尖峰, 可能永久损坏稳压器
2. 连续上下电切换: 双路 (3.3V 与输出 1.05V) 都要落到 0V, 且间隔 >100ms
3. 外部 OSC 供电场合: 停时钟源前必须先给 PHY 掉电
4. IO pad 电压由 CFG_EXT/CFG_LDO strap 选择内置 LDO 或外供
5. 用外部 pad 电源时, 2.5/1.8/1.5V 必须与 3.3V 同时或略早上升; 晚于 3.3V 会出错

## 10. 特性参数

### 10.1 绝对最大额定值

| 参数 | Min | Max |
|---|---|---|
| VDD33/AVDD33 | -0.3V | 3.6V |
| AVDD10/DVDD10 | -0.3V | 1.2V |
| 2.5/1.8/1.5V RGMII 电源 | -(0.2/0.2/0.2)V | 2.8/2.3/2.0V |
| 3.3V DC 输入输出 | -0.3V | 3.6V |
| 存储温度 | -55°C | +125°C |

### 10.2 推荐工作条件

电源典型值: 3.3V (2.97~3.63) / 1.0V 核心 (0.95~1.05) / RGMII 按所选档位 ±10%(2.5V 档等).
温度: 商业级 0~70°C, 工业级 -40~85°C; 结温上限 125°C.

### 10.3 晶振要求 / 10.4 外部时钟要求

晶振 (XTAL_IN/OUT): 25MHz 基频 AT-cut 并联谐振, 频差 ±50ppm (含首年老化与 PCB), 占空比 40~60%,
ESR≤50Ω, 激励 ≤0.5mW, 相位抖动宽带 pp ≤200ps; XTAL_OUT 电平门限 ViH≥1.4V/ViL≤0.4V.

外部 OSC/时钟 (**支持 25MHz 或 50MHz**): ±50ppm, 占空 40~60%, 抖动 pp≤200ps,
摆幅 3.15~3.45Vpp, 上升/下降 ≤10ns. 注意: 用外部时钟源的系统, 关时钟前先给 PHY 断电.

### 10.5 直流特性 (要点)

各电源档对应的 VOH/VOL/VIH/VIL 门限 (例 3.3V: VOH≥2.4V, VOL≤0.4V, VIH≥2.0V, VIL≤0.8V;
1.8V 档: VIH≥1.2V, VIL≤0.5V, VOL≤0.1×VDD18); 输入漏流 Iin≤0.5µA. 未列出的引脚均按 3.3V 档.

### 10.6.1 MDC/MDIO 交流时序 (Table 57)

| 符号 | 含义 | Min |
|---|---|---|
| t1/t2 | MDC 高/低脉宽 | 各 ≥32ns |
| t3 | MDC 周期 | ≥80ns (=≤12.5MHz) |
| t4 | MDIO 相对 MDC 上升沿建立 | ≥10ns |
| t5 | MDIO 保持 | ≥10ns |
| t6 | PHY 数据相对 MDC 上升沿有效延迟 | 0~300ns |

对照 MP157 侧: eth_hclk 分频 124 → MDC ≈2.15MHz (周期 ~465ns), 远低于速率上限与 IEEE 的 2.5MHz 建议,
t6 的 300ns 采样余量充裕.

### 10.6.2 RGMII 时序模式 (Figure 27/28 + Table 58)

概念: 发送方把 clock 与 data 同时发出, 二者相对偏斜 (skew) 是链路成败关键. 可通过内部延时把采样点
移到数据窗口中央 — 手册 Figure 27/28 展示对 TXC/RXC 分别加/不加延时的四种形态;
延时来源 = strap TXDLY/RXDLY (§7.8).

Table 58 要点:

| 参数 | 数值 |
|---|---|
| 时钟周期 (千兆) | 7.2~8.8ns (125MHz±标称) |
| 时钟周期 (百兆/十兆) | 36~44ns / 360~440ns |
| 占空比 | 千兆 45~55%; 十/百兆 40~60% |
| 上升/下降沿 (20%~80%) | ≤0.75ns |
| 发送侧 (集成延时后) setup/hold | ≥1.2ns |
| 接收侧 (集成延时后) setup/hold | ≥1.0ns |
| Data-Clock Output Skew (发送端不加延时) | -0.5~+0.5ns |
| Data-Clock Input Skew (接收端不补偿时) | 1~2.6ns |

手册推论: 未加内部延时时, 要求 PCB 上时钟走线额外引入 **1.5~2.0ns** 的布线延时来居中采样窗;
若 PHY 已配 RXDLY 内部 +2ns, 则不再依赖长绕线.

> 结合 strap 默认值推导: RXD0(RXDLY) 内部默认弱上拉 → 出厂倾向 RX 延时开启.

## 11. 机械尺寸

QFN 5×5mm (JEDEC MO-220): A=0.85mm 高(0.80~0.90), A3=0.20REF, D/E=5.00BSC,
D2/E2=3.45~3.95mm 散热焊盘, 引脚间距 e=0.40BSC, b=0.15~0.25, L=0.30~0.50. 控制尺寸单位 mm.

## 12. 订购信息

| 型号 | 说明 |
|---|---|
| RTL8211F-CG | QFN Green, 开关稳压器版 (MP 2014-03) |
| RTL8211FD-CG | QFN Green, LDO 版 (MP 2014-03) |
| RTL8211FI-CG | 工业级, 开关稳压器版 |
| RTL8211FDI-CG | 工业级, LDO 版 |

---

# 附录 A: 寄存器复位默认速查 (硬件复位后 Page 0)

| 地址 | 名称 | 上电预期值 | 备注 |
|---|---|---|---|
| 0x00 | BMCR | 0x1140 | ANE=1 + Duplex=1 + Speed[1]=1 |
| 0x01 | BMSR | ~0x79C9* | 能力位置 1; 无链路时 Link/ANComplete=0 (*按本 Rev 位表拼合) |
| 0x02/0x03 | PHYID1/2 | **0x001C / 0xC916** | 身份寄存器, 判型基准 |
| 0x04 | ANAR | 0x01E1 | 10/100 能力全广告, 无 pause 广告 |
| 0x09 | GBCR | 0x0200 | 仅 9.9 千兆 FD 广告=1 |
| 0x15 | GBESR | 0x2000 | 仅 15.13 千兆 FD=1 (X 口全/半双工与千兆半双工均为 0) |
| 0x18 | PHYCR1 | 按 Table37 | 24.13 广播响应=1, 24.8 MDI 态=1(手动配置未开), 24.4/24.3 校验与 Jabber=1 |
| 0x19 | PHYCR2 | 25.11=1, 25.0=1, **25.1(RXC)=0** | CLKOUT=125MHz 输出开; RXC 使能印刷复位值 0 |
| 0x1A | PHYSR | 实时态 | 仅 26.1 crossover=MDI 有固定默认 |
| 0x1F | EPAGSR | 0x0000 | 处于 Page 0 |

# 附录 B: 调试对照 (GMAC 联调场景)

| 现象 | 对应检查点 |
|---|---|
| MDIO 扫描 addr0 读到稳定非 FF 数据 | RTL8211F 对地址 0 广播命令本来就会应答 (PHYCR1.13 默认=1). 该响应≠设备真实地址; **真正地址按 strap =1~7 中某值, 扫描应以 1..7 为准并查原理图 strap 电阻** |
| 读回 ID ≠ 0x001C/C916 | 先排除"读到别的寄存器": 连读 reg0/reg1 看 BMCR/BMSR 是否形似正常默认值; 再做写读环测 (写 Reg4=0xA5A5→回读). 写读一致=帧格式正确, 问题只剩身份判定; 不一致=MDIO 帧格式/电气问题 |
| GMAC DMA SWR 不自清零 | 手册印制 reg25.1 (RXC Enable) 复位默认=0; 若板上确认无 RX_CLK 输出, 复位完成条件缺一个时钟域即挂起. 先建自协商/插网线拉起 RXC 后再补 SWR, 或核实该位可写性 |
| 上电 30ms 内读不到 ID | §7.17: 复位脉冲 ≥10ms + 释放后再等 30ms 才能访问寄存器; 依此校准 GmacPhyReset 延时 |
| RGMII 收包持续误码/CRC 错 | 对照 Table 58: 确认 RXDLY/TXDLY strap 与 MAC 端预期一致; PHYCR2 SSC (25.7/25.3) 是否被意外打开 |
| 主从协商悬空失败 | GBSR.10.15 看 fault; 两端都手动同角色或 seed 冲突 7 次会报故障 |
| 流控无效 | 注意本 Rev ANAR 默认不含 pause 广告 (bit10/11=0), 需 MAC 侧在 reg4 写入期望广告后重启 AN; 最终解析由 MAC 完成 |
| CLKOUT 给 MAC 做参考的板卡 | reg25.11 默认 125MHz、reg25.0 使能=1; MP157 侧 PG5/ETH_CLK_SEL 配置需与此对应 |

# 附: 原文图表索引 (正文引用)

Figure 1/2 应用图 · Figure 3 框图 · Figure 4 引脚图 · Figure 5 LED 复用极性 · Figure 6/7 MDIO 读/写时序 ·
Figure 8 EEE LED · Figure 9 PHY 复位时序 · Figure 10~23 开关稳压器应用/测量波形组 ·
Figure 24 上电时序 · Figure 25/26 MDC/MDIO 参数定义 · Figure 27/28 RGMII 加延时模式 ·
Table 1~59 见对应章节 (编号保留原文便于回查 PDF).
