# SYSCFG 系统配置控制器笔记

> 来源: RM0436 第 14 章 (pp1094-1113, Rev 7)
> 记录日期: 2026-8-26
> SYSCFG 基地址 = 0x50000000

## 14.1 I/O 补偿单元 (I/O compensation cell)

**作用**: IO 输出缓冲速度配到 50MHz 及以上时, 翻转瞬间 dI/dt 大, 会在电源上产生噪声.
补偿单元做压摆率控制 (slew rate control), 减小 IO 翻转时对电源的干扰.

**要点**:
- 默认不使用
- 只有 `GPIOx_OSPEEDRy = 0b10/0b11` (50MHz/100MHz 档) 的 IO 受它影响
- 使能后 READY 标志置位表示就绪, 补偿值随后持续自动更新
- 参考值来自 BSEC OTP fuses (RESTRIM)

**使能序列** (4 步):
1. 确保 CSI 振荡器使能且就绪 (在 RCC)
2. `CMPENSETR.MCU_EN` 或 `MPU_EN` = 1
3. 等 `CMPCR.READY` = 1
4. 置 `CMPCR.SW_CTRL` = 0

**禁用序列** (进 STOP 前, 保持当前补偿值):
1. 把 `CMPCR.APSRC[3:0]/ANSRC[3:0]` 拷贝到 `RAPSRC[3:0]/RANSRC[3:0]`
2. `SW_CTRL` = 1
3. `CMPENCLRR.MCU_EN` 和 `MPU_EN` = 1

**注意**: CSI 在 Stop 模式自动关闭 (没被其他用途请求时), Standby 模式恒关闭.

## 14.2 Ethernet 时钟源

ETH_CLK 按 PHY 类型有三种用法:

| PHY 类型 | 时钟用法 |
|---|---|
| GMII/RGMII | PG5 接收 PHY 的 125MHz, 或使用内部 ETH_CLK 125MHz (ETH_CLK 也可引到其他引脚) |
| RMII | ETH_CLK 50MHz 输出到 PA1, 同时给 PHY 做 REF_CLK |
| ALL | ETH_CLK 可用于 PHY 参考时钟 |

**Figure 126 时钟源图结构** (自上而下):

```
RCC ETH_CLK ─┬─> ETH_CLK_SEL ─> PG5-AF11 (CLK125 输入/输出)
             └─> ETH_REF_CLK_SEL ─┬─> PA1-AF11
                                  ├─> PA1-AF0
                                  └─> 其他 AFMUX 输出
```

**关键结论**:
- PG5-AF11 = CLK125, 受 `ETH_CLK_SEL` 控制 (0 = 外部 PHY 时钟, 1 = 内部 ETH_CLK)
- PA1 两个 AF: AF11 = GMII_RX_CLK, AF0 = ETH_CLK, 由 `ETH_REF_CLK_SEL` 决定
- 时钟源控制寄存器 = **SYSCFG_PMCSETR / SYSCFG_PMCCLRR**

## 14.3 SYSCFG 寄存器

| 寄存器 | 偏移 | 功能 |
|---|---|---|
| SYSCFG_BOOTR | 0x000 | BOOT 引脚 |
| SYSCFG_PMCSETR | 0x004 | 外设模式配置 set |
| SYSCFG_ICNR | 0x01C | AXI 互连控制 |
| SYSCFG_IOCTRLSETR | 0x018 | IO 控制 set |
| SYSCFG_CMPCR | 0x020 | 补偿单元控制 |
| SYSCFG_CMPENSETR | 0x024 | 补偿单元使能 set |
| SYSCFG_CMPENCLRR | 0x028 | 补偿单元使能 clear |
| SYSCFG_CBR | 0x02C | 定时器 break 控制 |
| SYSCFG_PMCCLRR | 0x044 | 外设模式配置 clear |
| SYSCFG_IOCTRLCLRR | 0x058 | IO 控制 clear |
| SYSCFG_VERR | 0x3F4 | 版本 |
| SYSCFG_IPIDR | 0x3F8 | 标识 |
| SYSCFG_SIDR | 0x3FC | 尺寸标识 |

### 14.3.1 SYSCFG_BOOTR (0x000)

复位值 0x0000 000X. BOOT 引脚状态读取 + 上拉控制 (省电用).

- Bit 6 **BOOT2_PD** (rw): BOOT2 下拉禁用 (BOOT2 接 VDD 时省电)
  - 0: 下拉使能 (悬空读 0), 1: 下拉禁用 (不能悬空)
- Bit 5 **BOOT1_PD** (rw): 同上, BOOT1
- Bit 4 **BOOT0_PD** (rw): 同上, BOOT0
- Bit 2 **BOOT2** (r): BOOT2 引脚电平 (0=低, 1=高)
- Bit 1 **BOOT1** (r): BOOT1 引脚电平
- Bit 0 **BOOT0** (r): BOOT0 引脚电平

### 14.3.2 SYSCFG_PMCSETR (0x004) — 外设模式配置 set

复位值 0x0000 0000. 写 1 置位, 写 0 无效.

- Bit 25 **ANA1_SEL**: ANA1 与 PA1 模拟连接
  - 0: 无连接, 1: ANA1 内部连到 PA1 (无 ANA1 引脚的封装才用)
- Bit 24 **ANA0_SEL**: ANA0 与 PA0 模拟连接 (同上)
- Bits 23:21 **ETH_SEL[2:0]**: ETH PHY 接口选择
  - 000: GMII 或 MII, 001: RGMII, 100: RMII, 其余保留
  - **Note: 必须在 ETH 处于复位期间、使能 ETH 时钟之前配置**
- Bit 20 **ETH_SELMII**: ETH_SEL=000 时选 MII/GMII
  - 0: GMII (由 GMAC 控制), 1: MII (强制 MII 模式)
- Bit 17 **ETH_REF_CLK_SEL**: 50MHz RMII 时钟选择
  - 0: 外部时钟 (需选 AFMux, 可用于所有 PHY)
  - 1: 内部 ETH_CLK1 (忽略 AFMux, 仅 RMII PHY)
- Bit 16 **ETH_CLK_SEL**: 125MHz 时钟选择 (仅 GMII/RGMII 有用)
  - 0: 外部时钟 (需选 AFMux), 1: 内部 ETH_CLK1 (忽略 AFMux)
- Bit 9 **ANASWVDD**: GPIO 模拟开关供电电压选择
  - 0: VDDA 供电 (EN_BOOSTER=0) 或 Booster 输出 (EN_BOOSTER=1)
  - 1: VDD 供电 (需清 EN_BOOSTER 避免额外功耗)
- Bit 8 **EN_BOOSTER**: Pad Booster 使能 (VDDA/VDD < 2.7V 时保证 AC 性能, 增加 VDD 电流)
- Bit 5 **I2C6_FMP** / Bit 4 **I2C5_FMP** / Bit 3 **I2C4_FMP** / Bit 2 **I2C3_FMP** / Bit 1 **I2C2_FMP** / Bit 0 **I2C1_FMP**: I2C Fast Mode Plus 使能
  - 0: 最高 400kHz, 1: FM+ 最高 1MHz (需选对应 AF)

### 14.3.3 SYSCFG_PMCCLRR (0x044) — 外设模式配置 clear

复位值 0x0000 0000. 与 PMCSETR 位定义完全相同, 但为 rc_w1 (写 1 清零, 写 0 无效).

ETH 相关位清零注意: ETH_SEL[2:0] 为 3 位字段, 按位清.

### 14.3.4 SYSCFG_IOCTRLSETR (0x018) — IO 控制 set

复位值 0x0000 0000. 写 1 置位.

- Bit 4 **HSLVEN_SPI**: SPIy_x 信号选 AFMUX 时 _h/_e pad 高速低电压模式使能
- Bit 3 **HSLVEN_SDMMC**: SDMMCy_x 信号同上
- Bit 2 **HSLVEN_ETH**: ETH_x 信号同上
- Bit 1 **HSLVEN_QUADSPI**: QUADSPI_x 信号同上
- Bit 0 **HSLVEN_TRACE**: TRACEx 信号同上

**危险警告** (手册 Danger 标记):
- OTP `product_below_2V5` = 0 (默认) 时该位不生效
- **VDD > 2.7V 时使能高速低电压模式会损坏芯片!** — 本项目 VDD=3.3V, 绝不能开

### 14.3.5 SYSCFG_IOCTRLCLRR (0x058) — IO 控制 clear

复位值 0x0000 0000. 与 IOCTRLSETR 位定义相同, rc_w1 写 1 清零.

### 14.3.6 SYSCFG_ICNR (0x01C) — 互连控制

复位值 0x0000 0000. 控制各 AXI master 走哪个 slave 端口访问 DDR (性能/延迟调优).

- Bit 10 **AXI_M10** / Bit 9 **AXI_M9** / Bit 8 **AXI_M8** / Bit 7 **AXI_M7** / Bit 6 **AXI_M6** / Bit 5 **AXI_M5** / Bit 3 **AXI_M3** / Bit 2 **AXI_M2** / Bit 1 **AXI_M1** / Bit 0 **AXI_M0** (rw):
  - 0: 经 Slave S0 访问 DDR, 1: 经 Slave S1 访问 DDR
- Bit 4 保留

### 14.3.7 SYSCFG_CMPCR (0x020) — 补偿单元控制

复位值 0xXX87 0000.

- Bits 31:28 **APSRC[3:0]** (r): PMOS 补偿值 (SW_CTRL=0 且 READY=1 时输出到 IO)
- Bits 27:24 **ANSRC[3:0]** (r): NMOS 补偿值 (同上)
- Bits 23:20 **RAPSRC[3:0]** (rw): SW_CTRL=1 时输出到 IO 的 PMOS 补偿值
  - 0001: fast 条件最大补偿 ... 1000: typical ... 1110: slow 条件最大补偿
- Bits 19:16 **RANSRC[3:0]** (rw): SW_CTRL=1 时输出到 IO 的 NMOS 补偿值
  - 0001: slow 条件最大补偿 ... 0111: typical ... 1110: fast 条件最大补偿
- Bit 8 **READY** (r): 补偿单元就绪标志 (1 = APSRC/ANSRC 有效)
- Bit 1 **SW_CTRL** (rw): 补偿软件控制
  - 0: 用 ANSRC/APSRC (自动补偿), 1: 用 RANSRC/RAPSRC (保持值)
  - Note: SW_CTRL=0 在 READY=1 前不生效; 补偿单元掉电时 (MCU_EN=0 且 MPU_EN=0) 恒用 RANSRC/RAPSRC

### 14.3.8 SYSCFG_CMPENSETR (0x024) — 补偿单元使能 set

复位值 0x0000 0000. 写 1 置位.

- Bit 1 **MCU_EN** (rs): M4 侧补偿单元使能
- Bit 0 **MPU_EN** (rs): A7 侧补偿单元使能
- Note: 必须先使能 CSI 振荡器 (RCC 控制) 才能置位; 只有 MCU_EN 和 MPU_EN 都为 0 才能关 CSI

### 14.3.9 SYSCFG_CMPENCLRR (0x028) — 补偿单元使能 clear

复位值 0x0000 0000. 位定义同 CMPENSETR, rc_w1 写 1 清零.

### 14.3.10 SYSCFG_CBR (0x02C) — 定时器 break 控制

复位值 0x0000 0000.

- Bit 2 **PVDL** (rs): PVD 锁定使能
  - 0: PVD 中断不连 TIM1/8/15/16/17 Break 输入, PVDE/PLS 可编程
  - 1: PVD 连 TIM break 输入, PVDE/PLS 只读 (系统复位才能清)
- Bit 0 **CLL** (rs): Cortex-M4 LOCKUP (Hardfault) 输出使能
  - 1: M4 LOCKUP 连到 TIM1/8/15/16/17 Break 输入 (系统复位才能清)

### 14.3.11 SYSCFG_VERR (0x3F4) — 版本

复位值 0x0000 0020.

- Bits 7:4 **MAJREV[3:0]** (r): 主版本 = 2
- Bits 3:0 **MINREV[3:0]** (r): 次版本 = 0

### 14.3.12 SYSCFG_IPIDR (0x3F8) — 标识

复位值 0x0003 0001. Bits 31:0 **ID[31:0]** (r): SYSCFG 标识值.

### 14.3.13 SYSCFG_SIDR (0x3FC) — 尺寸标识

复位值 0xA3C5 DD01. Bits 31:0 **SID[31:0]** (r): SYSCFG 寄存器分配的存储区域大小.

---

## 本项目要点总结

- **ETH_SEL=001 (RGMII)**: 写 PMCSETR, 时序: ETH 进复位 → 写 ETH_SEL → 释放复位 → 使能 ETH 时钟
- **ETH_CLK_SEL / ETH_REF_CLK_SEL**: 板级 PHY 晶振 25MHz, 用外部时钟 (0), 延后到 PHY 阶段确认
- **HSLVEN_ETH 不能开** (VDD=3.3V > 2.7V, 会损坏芯片)
- 补偿单元: 噪声优化, 暂不处理
