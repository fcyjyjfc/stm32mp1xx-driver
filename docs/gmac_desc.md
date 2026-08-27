# GMAC 描述符 (RM0436 Ch. 63.10)

> 来源: RM0436 Rev 7 §63.10 Descriptors (pp3607-3636), Table 800~551 原文转写
> 记录日期: 2026-8-27
> 适用: ETH_DMAC (Synopsys QoS 世代) 普通描述符, 非 TSO/non-context 场景

## 63.10.1/63.10.2 结构与环规则

描述符 = 内存中连续 16 字节 = 4 个字 (DES0~DES3). Tx 与 Rx 分列独立描述符环.

- DSL 字段 (ETH_DMACxCR) 指定相邻描述符间隔 (单位 = 64bit 字数). 本库 DSL=0 → 紧密排列 16B 间距
- 描述符环自动回卷: 处理完最后一个回到基址
- **Owner 规则**: DES3[31]=1 归 DMA, =0 归应用. DMA 完成收/发后清 0 并写回状态
- **尾指针语义**: ETH_DMACxTXDTPR/RXDTPR 指向「最后一个有效描述符」的**下一个**位置,
  即指针到 N 则 N-1 及之前的都归 DMA; current == tail 时进 Suspend.
  单描述符启动例子: 手册直接把尾指针写成 Base+1 (=第一个描述符之后一格)
- 越过环尾编程尾指针 = 声明又一整圈可用; 不想这样就在写完最后一个描述符时把尾指回环首

## 63.10.3 发送描述符 (普通, 读格式 ← 软件填写)

| 字 | 内容 |
|---|---|
| DES0 | **BUF1AP** 缓冲1物理地址 |
| DES1 | BUF2AP 缓冲2地址 (单缓冲不用, 填 0) |
| DES2 | [31]IOC 完成 interrupt · [30]TTSE 时间戳 · **[29:16]B2L** · [15:14]VTIR VLAN · **[13:0]B1L** |
| DES3 | [31]**OWN** · [30]CTXT=0 · [29]**FD** · [28]**LD** · [27:26]CPC · [25:23]SAIC · [22:19]SLOTNUM · [18]TSE · [17:16]CIC · [15:0]FTL/THL |

关键位:
- **帧长承载**: 普通 (非 TSO) 帧, 字节数 = B1L(+B2L); TDES3[14:0] 在 TSE=0 时保留 — 别错填到 DES3
- CPC[27:26]: `00`=**MAC 自动补 Pad+CRC** (推荐默认; 帧<60 自动 padding); `01`只加CRC不加Pad;
  `10`都不加 (软件自己带齐); `11`替换尾部4字节重算
- SAIC[25:23]: `000`不动源地址 (软件自备 MAC 头, 默认用它)
- VTIR=00 无 VLAN; CIC=00 关闭校验和卸载
- 单缓冲整包: 一个描述符置 FD|LD 即可, B1L 必须非零 (LD 要求)

### Tx 写回格式 (仅 LD 描述符, 由硬件改写)

| 字 | 内容 |
|---|---|
| DES0/1 | TTSL/TTSH 时间戳 (TTSE 使能才有意义) |
| DES2 | 保留 |
| DES3 | [31]OWN=0 · [29]FD · [28]LD · [16]TTSS · **[15]ES 错误汇总** · [14:0]状态位 |

错误位含义: [14]JT Jabber超时 · [13]FF 冲刷 · [12]PCE 校验和失败 · [11]LoC/[10]NC 载波丢失/
无载波(半双工相关) · [9]LC 迟冲突 · [8]EC 过多碰撞(16次; DR=1 时首次即报) · [7:4]CC 碰撞计数 ·
[3]ED 过度推迟 · [2]UF 下溢(FIFO 断流——存储转发模式不该出现, 出现即查 PBL/FIFO 配置) ·
[1]DB 推迟发送 · [0]IHE IP头错.

## 63.10.4 接收描述符 (普通)

### 读格式 ← 软件预填

| 字 | 内容 |
|---|---|
| DES0 | **BUF1AP** 缓冲1地址 (可任意字节对齐, SOP 起始偏移以低 3 位计) |
| DES1 | 保留 |
| DES2 | BUF2AP |
| DES3 | [31]**OWN** · [30]IOC 描述符级完成中断 · [29:26]Res · [25]BUF2V · [24]**BUF1V** · 其余 Res |

预填最小集: `DES3 = OWN(1<<31) | BUF1V(1<<24) | IOC(1<<30 可选)`.
注意: 帧长预填位不存在; 且**地址全 0 的缓冲会被跳过**.

### 写回格式 (硬件写)

| 字 | 内容 |
|---|---|
| DES0 | [31:16]内层VLAN · [15:0]外层VLAN (RS0V 有效时) |
| DES1 | 校验和/PTP/ARP 状态: [11:8]PMT · [7]IPCE · [6]IPCB · [5]IPV6 · [4]IPV4 · [3]IPHE · [2:0]PT 载荷类型 |
| DES2 | 过滤命中: [31:29]L3L4FM 号 · [28]L4FM · [27]L3FM · [26:19]MADRM 匹配号/哈希值 · [18]HF · [17]DAF · [16]SAF · [15]VF |
| DES3 | [31]OWN=0 · [30]CTXT · [29]**FD** · [28]**LD** · [27]RS2V · [26]RS1V · [25]RS0V · [24]**CE CRC错** · [23]GP 巨包 · [22]RWT 收看门狗 · [21]OE 溢出 · [20]RE 收错误 · [19]DE 碎位 · [18:16]LT 类型 · [15]**ES** · **[14:0]PL 帧长(含CRC)** |

判读守则 (驱动取帧时用):
- 有效接收判定: `OWN==0 && LD==1`
- 错误帧判定: `ES==1` (位级看 CE/OE/RE/RWT), 错误帧照样占缓冲, 重灌 OWN 复用即可
- PL 只在 LD=1 且 OE=0 时可信; 含 4 字节 FCS; 若 MACCR.ACS=1 (剥 CRC) 硬件已把 CRC 剥掉
- LT 编码: 001=类型帧 · 011=ARP请求 · 100/101=VLAN · 110=MAC控制 · 111=OAM · 000=length帧
- RS1V/RS2V 标记 DES1/DES2 状态有效性

## 初始化/运行配方 (对应 63.9.3 启动序列)

```
前置: RCC 总线时钟/复位就绪, SYSCFG 已选 RGMII, PHY 已硬件复位并通过 MDIO 可达

① SWR 时机: 等 link 建立(RX_CLK 在场)再调 GmacSoftReset, 通过后所有寄存器为默认态
② MAC 配置: GmacMacInit(地址/协商速率双工/混杂) —— 保持 RE/TE=0
③ Rx 环: 软件预填 N 个描述符 (地址+OWN|BUF1V|IOC)
④ 寄存器序: RXDLAR=环基址 → RXRLR=N → RXCR=RBSZ|SR=1 → RXDTPR=&desc[N-1]
   (手册: DLAR 仅停止态可写, 启动命令前必须先写)
⑤ Tx 环同理 (TXDLAR→TXRLR→TXCR.ST=1→提交时写 TXDTPR)
⑥ 最后: GmacMacStart() 置 RE/TE —— 反了会灌爆 Rx FIFO 或发空包
⑦ 收包处理循环: 轮询 CHx.SR.RI (rc_w1 清) → 找 OWN==0&&LD==1 的描述符 → 取 PL/ES
   → 数据可直接读缓冲 → 重灌 DES3 (OWN|BUF1V|IOC) 并推进 RXDTPR 到其下一格
⑧ 发送流程: 填缓冲 → TDES0=addr, DES2=IOC|B1L, DES3=OWN|FD|LD → 写 TXDTPR=提交格下一格
   → 轮询该 DES3 OWN 自清 → 读 WB 状态 (重点 UF/NC/LC/ES)
```

注意事项:
- 描述符与缓冲放 DDR, 地址 = 物理地址 (裸机 MMU 关闭场景无需虚拟映射/cache 维护;
  一旦开启 D-Cache, 写回格式字段需按非缓存区域处理或手动维护)
- 环长度寄存器填的是描述符个数 (RXRLR 低 10 位), 不含 DSL 扩展
- 每描述符最大 32KB (单 B1L 上限 14 位计数 → 单缓冲最大 16383 字节)
