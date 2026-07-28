# PWR — Power Control (RM0436 Ch.9, p.439-444)

## 9.1 PWR 主要功能

| 功能块 | 说明 |
|--------|------|
| 系统供电调节控制 | VDDCORE 请求信号 PWR_ON, 低功耗模式控制 PWR_LP |
| 外设供电调节 | DSI Regulator, 1.8V Regulator, 1.1V Regulator |
| 电源监控 | POR/PDR, BOR, PVD, AVD, VBAT 阈值, 温度阈值, VDDCORE 监控 |
| VBAT 充电 | 备份电池充电 |
| 电源管理 | 运行模式, 低功耗模式 |
| TrustZone 安全 | 寄存器访问安全控制 |

## 9.2 框图关键信号

### PWR ↔ RCC 互连信号

| 信号 | 方向 | 说明 |
|------|------|------|
| `rcc_tzen` | RCC → PWR | TrustZone 安全使能 (来自 RCC 的 TZEN 位) |
| `hclk4` | RCC → PWR | PWR AHB 总线时钟 |
| `pwrwake_mpu` | PWR → RCC | MPU 时钟唤醒请求 |
| `pwrwake_mcu` | PWR → RCC | MCU 时钟唤醒请求 |
| `pwrwake_sys` | PWR → RCC | 系统时钟唤醒请求 |
| `nrst_por` | PWR → RCC | VDD 上电复位 |
| `nrst_bor` | PWR → RCC | VDD 掉电复位 |
| `ready_run` | PWR → RCC | 运行就绪 |

### 其他 PWR 输出

| 信号 | 说明 |
|------|------|
| `pvdo` | 可编程电压检测器输出 (PVD) |
| `avdo` | 模拟电压检测器输出 (AVD) |
| `sys_ds_exit` | EXT I唤醒请求 (sys_wakeup) |
| `mpu_ds_exit` | MPU 唤醒请求 (c1_wakeup) |
| `mcu_ds_exit` | MCU 唤醒请求 (c2_wakeup) |
| `mpu_cstandby` | MPU CStandby 模式指示 |
| `mpu_int_wkup` | MPU 唤醒中断 |
| `mcu_int_wkup` | MCU 唤醒中断 |
| `async_wkup[6:1]` | 异步唤醒至 EXTI |

## 9.2.1 引脚概览

| 引脚 | 方向 | 说明 |
|------|------|------|
| VDD | P | IO 和 VDD 域供电输入 |
| VDD_PLL | P | PLL 供电, 接 VDD |
| VBAT | P | 备份电池供电输入 |
| VDDCORE | P | Core 域供电 |
| VDDA | P | 模拟域供电 (ADC/DAC) |
| VREF+ | P | ADC/DAC 外部参考电压 |
| PWR_ON | O | Core 供电使能输出 |
| PWR_LP | O | Core 低功耗 Stop 模式选择 |
| NRST | I/O | 应用复位输入/输出 |
| PDR_ON | I | VDD 上电复位使能 |
| PDR_ON_CORE | I | VDDCORE 上电复位使能 |
| NRST_CORE | I | VDDCORE 复位输入 (PDR 禁用时) |
| BYPASS_REG1V8 | I | 1.8V 稳压器旁路 |
| WKUP[6:1] | I | 唤醒输入 |
| PVD_IN | I | PVD 外部输入 |

## 9.3 供电架构

```
VDD ─┬─ IO + 振荡器/模拟 (HSI, CSI, HSE, LSI, WKUP, IWDG)
     ├─ Level Shifter ─┬─ MPU
     │                  ├─ MCU
     │                  ├─ 外设
     │                  └─ RAM
     └─ PLLs (VDD_PLL)

VDDA ─── ADC, DAC, REF_BUF

VBAT ─── 备份域: LSE, RTC, AWU, Tamper, 备份寄存器, 备份 RAM

VDD3V3_USBFS  ─── USB FS IOs
VDD3V3_USBHS  ─── USB HS PHY
VDD_DSI       ─── DSI Regulator
VDD1V2_DSI_REG/PHY ─── DSI 1.2V
VDD1V8_DSI    ─── DSI PHY 1.8V

VDDCORE       ─── Core domain
VSW           ─── Retention domain (低功耗保留)
```

### 电源域层次

- **VDD domain**: IO 逻辑、PLL、复位、唤醒逻辑
- **VSW domain**: 含电源开关，可切换供电
- **Retention domain**: 保留稳压器（VRET），低功耗下保持 Retention RAM
- **Backup domain**: VBAT 独立供电（LSE、RTC、Tamper、备份 RAM/寄存器）
- **Analog domain**: VDDA 供电（ADC/DAC 独立）

## 9.3.1 PWR Core Domain — 工作模式 (p.445)

VDDCORE 由 **外部稳压器** 供电，PWR 通过 PWR_ON/PWR_LP 两个引脚控制此稳压器。

| 信号 | 方向 | 作用 |
|------|------|------|
| PWR_ON | O → 外部稳压器 | 使能控制，高=开，低=关 |
| PWR_LP | O → 外部稳压器 | 模式选择，高=Main，低=Low-power |

### 五种系统模式

| 模式 | PWR_ON | PWR_LP | 稳压器状态 | 说明 |
|------|--------|--------|------------|------|
| **Run** | 高 | 高 | Main 全功率 | Core 域正常运行 |
| **Stop** | 高 | 高 | Main (快速唤醒) | 时钟关闭，电压不降，LPDS=0 |
| **LP-Stop** | 高/配LPCFG | 低 | Low-power | LPDS=1，电压降低 |
| **LPLV-Stop** | 高/配LPCFG | 低 | Low-power + 低压 | LPDS=1 + LVDS=1，VDDCORE 降压 |
| **Standby** | 低 | 低 | 关闭 | Core 域完全断电，数据丢失 |

### 关键控制位 (PWR_CR1)

| 位 | 说明 |
|----|------|
| **LPDS** | Low-Power Deep Sleep — 使能 LP-Stop/LPLV-Stop 的低功耗信令 |
| **LVDS** | Low-Voltage Deep Sleep — 使能 LPLV-Stop 降压模式 |
| **LPCFG** | PWR_ON 在低功耗模式下的行为（保持高/跟随 PWR_LP） |

### Standby 模式下 RAM 数据

- Core 域内的 **SRAM1/2/3 全部丢失**（VDDCORE 掉电）
- 保留区域仅限 **备份域（VBAT）**: 备份 RAM、RTC 计数器、备份寄存器
- 唤醒后 CPU 相当于冷启动：
  - **A7**: Boot ROM → FSBL (TF-A) → U-Boot → Kernel
  - **M4**: 需要 A7 端 remoteproc 重新加载固件 (DDR 也掉电)
- 唤醒后可通过 PWR 标志位判断是冷启还是从 Standby 恢复

## 9.3.2 系统上电启动时序 — Figure 29 (p.446)

```
VDD 上电 → POR 监测 VDD 超阈值 → PWR_ON 使能外部稳压器
  → VDDCORE 升至 vddcore_ok → 释放复位 → HSI 起振
  → HSI 稳定 → HW 系统初始化(加载选项字节) → Run 模式
```

**6 个阶段**:
1. POR 监测 VDD 超阈值 → PWR_ON 拉高使能外部稳压器
2. VDDCORE 低于 vddcore_ok，系统保持复位
3. VDDCORE 超阈值，释放复位
4. HSI 振荡器使能
5. HSI 稳定
6. 硬件初始化（选项字节加载），进入 Run

**图 29 信号**: VDD, nrst_por, PWR_ON, PWR_LP, VDDCORE, vddcore_ok, HSI

## 9.3.3 Stop 供电时序 — Figure 30 (p.447)

```
Run → hclk4 停止(振荡器可停) → PWR_ON/PWR_LP 保持高(Main)
  → 唤醒事件 → 时钟恢复 → HSI 稳定 → Run
```

| 阶段 | 说明 |
|------|------|
| ① 进 Stop | hclk4 停钟，振荡器可停，LPDS=0 |
| ② Stop 中 | PWR_ON=高, PWR_LP=高, 稳压器 Main |
| ③ 唤醒 | 时钟恢复序列 (RCC) |
| ④ Run | HSI 稳定后进入 Run |

**图 30 信号**: PWR_ON, PWR_LP, VDDCORE, vddcore_ok, hclk4

## 9.3.4 LP-Stop 供电时序 — Figure 31 (p.448)

```
Run → hclk4 停止 → PWR_LP 拉低(通知稳压器进 Low-power)
  → 唤醒 → PWR_LP 拉高 → 时钟恢复
  → PWRLP_TEMPO 延时等稳压器切回 Main → Run
```

| 阶段 | 说明 |
|------|------|
| ① 进 LP-Stop | hclk4 停钟，PWR_LP 拉低 |
| ② LP-Stop 中 | PWR_ON 保持高或跟随 PWR_LP (LPCFG 配置) |
| ③ 唤醒 | PWR_LP 拉高，时钟恢复 |
| ④ PWRLP_TEMPO | 等待外部稳压器切回 Main (RCC_PWRLPDLYCR 配置) |
| ⑤ Run | 进入运行模式 |

**图 31 信号**: PWR_ON, PWR_LP, VDDCORE, vddcore_ok, hclk4

## 9.3.5 LPLV-Stop 供电时序 — Figure 32 (p.449)

```
Run → hclk4 停止 → PWR_LP 拉低 + LVDS 使能 → VDDCORE 降压
  → 唤醒 → tSEL_VDDCORETEMPO 等 VDDCORE 恢复电压
  → HSI 起振 → 时钟恢复 → Run
```

| 阶段 | 说明 |
|------|------|
| ① 进 LPLV-Stop | LPDS=1 + LVDS=1，hclk4 停钟 |
| ② LPLV-Stop 中 | VDDCORE 降压，PWR_LP 拉低 |
| ③ 唤醒 tSEL | 等待 VDDCORE 恢复到运行电压 |
| ④ HSI 起振 + 时钟恢复 |
| ⑤ Run | 进入运行模式 |

**图 32 信号**: PWR_ON, PWR_LP, VDDCORE, vddcore_ok, hclk4

## 9.3.6 Standby 供电时序 — Figure 33 (p.450)

```
Run → PWR_ON 拉低关外部稳压器 → VDDCORE 掉电
  → 唤醒事件 → PWR_ON 拉高请求供电
  → VDDCORE 升至 vddcore_ok → HSI 起振
  → HW 系统初始化(选项字节加载) → Run
```

| 阶段 | 说明 |
|------|------|
| ① 进 Standby | PWR_ON 拉低，振荡器停，hclk4 停 |
| ② Standby 中 | VDDCORE 掉电，PWR_LP 跟随 PWR_ON |
| ③ 唤醒 | PWR_ON 拉高，等待 VDDCORE 升压 |
| ④ HSI 起振 | VDDCORE 超阈值后使能 HSI |
| ⑤ HW Init | 硬件初始化(选项字节加载) |
| ⑥ Run | 进入运行模式 |

**注意**:
- PWR_CR3.POPL 可配置 PWR_ON 最短低电平脉冲时间
- 若 PDR_ON_CORE 禁用了 vddcore_ok 监测，则 VDDCORE 掉电时必须激活 NRST_CORE
- **PDR_ON_CORE 在应用中必须接 VDD**（仅 ST 生产测试用）

### 四种低功耗模式对比

| 模式 | 控制位 | VDDCORE | 稳压器 | 唤醒延迟主因 | 图号 |
|------|--------|---------|--------|-------------|------|
| Stop | LPDS=0 | 正常 | Main (不停) | HSI 起振 | Figure 30 |
| LP-Stop | LPDS=1 | 正常/略降 | Low-power | HSI + PWRLP_TEMPO | Figure 31 |
| LPLV-Stop | LPDS=1 + LVDS=1 | 降压 | Low-power + 低压 | 电压恢复 + HSI | Figure 32 |
| Standby | — | 掉电 | 关闭 | 冷启全流程 | Figure 33 |

## 9.3.7 VBAT 模式时序 — Figure 34 (p.451)

从 Standby 进入 VBAT（VDD 也掉电，仅 VBAT 引脚供电）的流程：

```
Standby → VDD 掉电 → VBAT 模式
  → VDD 恢复 → POR 监测 → PWR_ON 拉高
  → VDDCORE 升压 → HSI 起振 → HW Init → Run
```

| 阶段 | 说明 |
|------|------|
| ① Standby | PWR_ON 低，VDDCORE 掉电 |
| ② VBAT 模式 | VDD 掉电，PWR_ON/LP 高阻，仅 VBAT 域保留 |
| ③ VDD 恢复 | POR 监测，PWR_ON 拉高 |
| ④ HSI 起振 | VDDCORE 超 vddcore_ok 阈值 |
| ⑤ HW Init | 选项字节加载 |
| ⑥ Run | 进入运行 |

**注意**: 若从 Run/Stop/LP-Stop/LPLV-Stop 进入 VBAT，VDD 和 VDDCORE **都必须断电**。

**图 34 信号**: VDD, nrst_por, PWR_ON, PWR_LP, VDDCORE, vddcore_ok, hclk4

## 9.3.8 VSW 域 (p.451)

- VSW 域包含: **RTC, 备份寄存器, 备份 RAM, Retention RAM**
- VDD 正常 → VSW 由 VDD 供电
- VDD 掉电 → 通过 VBAT 引脚（电池或其他电源）保持 VSW 域内容
- 本质是**双电源切换**：VDD 在吃 VDD，VDD 不在切 VBAT

### VBAT 切换注意事项 (p.452)

- `tRSTTEMPO`（VDD 启动暂态）期间电源开关仍连 VBAT
- 若 VDD 在 `tRSTTEMPO` 内建立且 `VDD > VBAT + 0.6V`，会通过内部二极管向 VBAT 注入电流
  → VBAT 电池不支持此电流时，需外接低压降二极管
- **无外部电池时，VBAT 直接连 VDD**

### VSW 域 I/O 引脚行为 (p.452)

| 引脚 | VDD 存在时 | VBAT 模式下 |
|------|-----------|-------------|
| PC14, PC15 | GPIO 或 LSE | 仅 LSE |
| PC13, PI8, PC1 | GPIO 或 RTC/TAMP | 仅 RTC/TAMP |

### VSW 域安全访问 (p.452)

系统复位后 RTC 和备份寄存器写保护 → 置 `PWR_CR1.DBP = 1` 开放

## Backup RAM (p.453)

| 特性 | 说明 |
|------|------|
| 大小 | **4 KB**，8/16/32 位访问 |
| Run/Stop/LP-Stop/LPLV-Stop | VDDCORE 供电 |
| Standby/VBAT | VSW 备份稳压器 (PWR_CR2.BREN + BRRDY) |
| Tamper 事件 | 读保护，需写全 0 擦除恢复 |

## Retention RAM (p.453)

| 特性 | 说明 |
|------|------|
| 大小 | **64 KB**，8/16/32 位访问 |
| Run/Stop/LP-Stop/LPLV-Stop | VDDCORE 供电 |
| Standby/VBAT | VSW 保留稳压器 (PWR_CR2.RREN + RRRDY) |
| 完整性检测 | RRRDY 在供电低于阈值时自动清零，需重新 RREN→RREN 置位 |

## 图 35 — Backup Domain 框图 (p.454)

```
VBAT / VDD / VDDCORE → VSW ─┬─ Backup Regulator → Backup RAM (4KB)
                             ├─ Retention Regulator → Retention RAM (64KB)
                             ├─ RTC + LSE
                             └─ Backup Interface → Backup I/Os (PC13/14/15, PI8, PC1)
```

## VBAT 电池充电 (p.454)

- VDD 存在时通过内部电阻给 VBAT 充电
- 电阻选择: 5 kΩ 或 1.5 kΩ（PWR_CR3.VBRS）
- 使能: `PWR_CR3.VBE = 1`，进 VBAT 模式自动禁用

## 模拟供电 (p.454)

- **VDDA** — 独立模拟电源引脚，隔离 PCB 噪声改善 ADC/DAC 精度
- **VREF+** — ADC/DAC 独立参考电压（VREF- 与 VSSA 共享）

## 内部稳压器 (p.455)

### VREFBUF — 电压参考缓冲器

- 使能: `VREFBUF_CSR.ENVR` → VREF+ 由内部参考缓冲器驱动
- 可对外提供参考电压
- 关闭时 VREF+ 作为外部参考电源输入

### 1.8V 稳压器

| 项 | 说明 |
|----|------|
| 输入 | VDD → 输出 VDDA1V8_REG |
| 负载 | USB HS PHY 直连, DSI PHY VDD1V8_DSI 需外接 |
| 硬件使能 | BYPASS_REG1V8 引脚 |
| 软件使能 | PWR_CR3.REG18EN |
| 就绪 | PWR_CR3.REG18RDY |
| 旁路 | BYPASS_REG1V8 或 REG18EN=0 → 外部供电 |

### 1.1V 稳压器

| 项 | 说明 |
|----|------|
| 输入 | VDD → 输出 VDDA1V1_REG |
| 负载 | USB HS PHY 直连 |
| 使能 | PWR_CR3.REG11EN |
| 就绪 | PWR_CR3.REG11RDY |

### DSI 稳压器 (DSI IP 内部)

| 项 | 说明 |
|----|------|
| 输入 | VDD_DSI → 输出 VDD1V2_DSI_REG |
| 负载 | DSI PLL + PHY |
| 使能 | DSI_WRPCR.REGEN (非 PWR 寄存器) |
| 旁路 | REGEN=0 → 外部 DSI 供电 |

## 9.4 电源监控总览 (p.456)

| 监控对象 | 机制 | 触发后果 | 标志位 |
|----------|------|---------|--------|
| VDD | POR/PDR | 系统复位 | RCC.PORRSTF |
| VDD | BOR (选项字节) | 系统复位 (NRST) | RCC.BORRSTF |
| VDD / PVD_IN | PVD | EXTI 中断 (不复位) | PWR_CSR1.PVDO |
| VDDA | AVD | EXTI 中断 (不复位) | PWR_CSR1.AVDO |
| VBAT | VBAT 阈值 | TAMP 事件 | PWR_CR2 使能 |
| VDDCORE | vddcore_ok | Core 域复位 | RCC.VCORERSTF / PWR_MCUCR.SBF |
| VSW | rst_vsw | VSW 域复位 | 无标志 |
| VBKP | 备份稳压器 | 状态标志 | PWR_CR2.BRRDY |
| VRET | 保留稳压器 | 状态标志 | PWR_CR2.RRRDY |
| VDD3V3_USBFS | USB 检测 | 状态标志 | PWR_CR3.USB33RDY |
| VDDA1V8_REG | 1.8V 稳压器 | 状态标志 | PWR_CR3.REG18RDY |
| VDDA1V1_REG | 1.1V 稳压器 | 状态标志 | PWR_CR3.REG11RDY |
| VDD1V2_DSI_REG | DSI 稳压器 | 状态标志 | DSI 模块内 RRS |

### 9.4.1 POR/PDR — Figure 36 (p.456)

- 芯片内部集成，无需外部复位电路
- VDD < VPOR → 系统保持复位
- VDD > VPOR → 延迟 `tRSTTEMPO` 后释放 nrst_por
- 同时触发 NRST（应用复位引脚）
- **PDR_ON 必须接 VDD**（ST 生产测试用）

### 9.4.2 BOR — Figure 37 (p.457)

- 通过系统选项字节配置，默认关闭
- 可选等级: VBOR0(关) / VBOR1(2.10V) / VBOR2(2.40V) / VBOR3(2.70V)
- 触发时产生 NRST
- 迟滞: Level 1-3 ~100mV, Level 0 ~40mV
- 要配置 BOR 必须先让 VDD > VBOR0

### 9.4.3 VDDCORE OK Reset — Figure 38 (p.458)

- VDDCORE < vddcore_ok 阈值 → Core 域保持复位
- PDR_ON_CORE 必须接 VDD（禁用后才可在 LPLV-Stop 降压）
- NRST_CORE 无论 PDR_ON_CORE 状态都有效

### 9.4.4 PVD — Figure 39 (p.459)

- 使能: `PWR_CR1.PVDEN`
- 监控 VDD 或 PVD_IN 引脚（相对内部 VREFINT）
- 阈值: `PWR_CR1.PLS[2:0]`
- 标志: `PWR_CSR1.PVDO`，连接到 EXTI（可配上升/下降沿中断）
- **只通知不复位** — 用于掉电前紧急处理
- Standby 模式不可用

### 9.4.5 AVD (p.460)

- 使能: `PWR_CR1.AVDEN`
- 监控 VDDA，阈值: `PWR_CR1.ALS[1:0]`
- 标志: `PWR_CSR1.AVDO`，连接到 EXTI
- Standby 模式不可用

### 监控机制总结

| 机制 | 监控对象 | 触发后果 | 可配方式 |
|------|---------|---------|---------|
| POR/PDR | VDD | 系统复位 | 硬件固定 |
| BOR | VDD | 系统复位 | 选项字节 |
| vddcore_ok | VDDCORE | Core 域复位 | 硬件固定 |
| PVD | VDD / PVD_IN | EXTI 中断 | 软件 |
| AVD | VDDA | EXTI 中断 | 软件 |
| VBAT 阈值 | VBAT | TAMP 事件 | 软件 |
| 稳压器就绪 | 各稳压器输出 | 状态标志 | 等待就绪 |

### 9.4.6 VBAT 电压阈值 — Figure 41 (p.461)

- 使能: `PWR_CR2.MONEN`（使能后功耗增加）
- 两个阈值: VBATL（低电压）、VBATH（高电压）
- 输出到 **TAMP** 防篡改信号
- 用于检测 VBAT 电池电压异常

### 9.4.7 温度阈值 — Figure 42 (p.462)

- 使能: `PWR_CR2.MONEN`（使能后功耗增加）
- 两个阈值: TEMPL（低温）、TEMPH（高温）
- 标志位: `PWR_CR2.TEMPL`、`PWR_CR2.TEMPH`
- 输出到 TAMP 信号
- **Standby 和 VBAT 模式下仍可用**
- 可用于温度控制任务

## 9.5 电源管理 (p.463-469)

STM32MP157 双核（CA7 + CM4），PWR 层面 CA7 双核视为一个 MPU。

### 两层状态机

系统状态由两层决定:
1. **处理器状态**: CRun / CSleep / CStop / CStandby（各自独立）
2. **系统状态**: Run / Stop / LP-Stop / LPLV-Stop / Standby（由两个处理器状态合成）

### PDDS 位 — 关键控制位

`PDDS`（Power Down Deep Sleep）决定处理器进 CStop 时系统怎么走:

- `PWR_MPUCR.PPDS` — MPU 的 PDDS
- `PWR_MCUCR.PPDS` — MCU 的 PDDS

```
PDDS = 0 → 系统进 Stop/LP-Stop/LPLV-Stop（Core 域有电）
PDDS = 1 → 系统进 Standby（Core 域掉电）
```

### MPU 子系统的处理器模式 (Table 30, p.466)

双核 CA7（P0, P1）可独立进不同模式:

| MPU 模式 | P0 | P1 | L2 Cache | 唤醒方式 |
|----------|-----|-----|----------|---------|
| CRun | Run | Run | Run | — |
| CSleep | Run/Stop | Stop/Run | Run | 对方中断/事件/复位 |
| CStop | Stop | Stop | Stop (STPREQ=b11) | MPU EXTI 中断或复位 |
| CStandby | — | — | — | EXTI 中断 → MPU 复位 |

- STPREQ_P[1:0] 位于 RCC，控制 L2 Cache 状态请求
- WFI/WFE 进入低功耗，WFI 允许中断唤醒，WFE 允许事件唤醒

### MCU 子系统的处理器模式 (Table 31, p.467)

| MCU 模式 | 进入条件 | 唤醒方式 |
|----------|---------|---------|
| CRun | — | — |
| CSleep | SCR.SLEEPDEEP=0 + WFI/WFE | 任意 MCU 中断/事件/复位 |
| CStop | SCR.SLEEPDEEP=1 + WFI/WFE | 任意 MCU EXTI 中断/事件/复位 |

### 系统模式 — 由 MPU + MCU 状态合成 (Table 32, p.468)

```
              MPU: CRun/CSleep    CStop          CStandby
MCU CRun   →  Run                Run            Run
MCU CSleep →  Run                Run            Run
MCU CStop  →  Run                Stop/Standby   Stop/Standby
```

**规则**: 只有两个核都停止时，系统才能进入低功耗模式。

### 五种系统模式详解 (p.464-465)

| 系统模式 | 条件 | 说明 |
|----------|------|------|
| **Run** | 任一核在 CRun 或 CSleep | 各子系统时钟按 RCC 配置运行 |
| **Stop** (LPDS=0) | 两核都在 CStop/CStandby + 至少一个 PDDS=0 | hclk4 停，PWR_ON/LP 保持高，振荡器可停 |
| **LP-Stop** (LPDS=1, LVDS=0) | 同上 | PWR_LP 拉低，通知外部稳压器切 Low-power |
| **LPLV-Stop** (LPDS=1, LVDS=1) | 同上 | VDDCORE 降压（需禁用 vddcore_ok 检测） |
| **Standby** | MPU=CStandby/CStop(CSTBYDIS=1) + MCU=CStop(PDDS=1) + 所有 PDDS=1 | Core 域完全掉电，冷启唤醒 |

### 各系统模式信号汇总 (Table 33, p.468-469)

| 系统模式 | hclk4 | 系统振荡器 | PWR_ON | PWR_LP |
|----------|-------|-----------|--------|--------|
| Run | ON | ON | 1 | 1 |
| Stop | OFF | ON/OFF (见 xxxKERON) | 1 | 1 |
| LP-Stop | OFF | ON/OFF | 1 (LPCFG) | 0 |
| LPLV-Stop | OFF | ON/OFF | 1 (LPCFG) | 0 |
| Standby | OFF | OFF | 0 | 0 |

### 设计思想

整个 PWR 模式管理不需要显式调用"进入 Stop 模式"的 API。**机制是**: 让两个核各自 WFI/WFE、配好 PDDS，PWR 硬件根据两个核的状态自动切换系统供电模式。Run 模式下可通过 RCC 的 PERxEN 门控未使用外设的时钟来降低功耗。

## 表 34 — 各模式下外设功能可用性 (p.470-471)

图例: Y=始终使能, O=可选, R=数据保留不工作, -=不可用

### 核心与存储

| 模块 | Sleep | Stop/LP-Stop | LPLV-Stop | Standby | VBAT |
|------|:-----:|:------------:|:---------:|:-------:|:----:|
| MPU | R | R | R | - | - |
| MCU | R | R | R | - | - |
| GPU | R | R | R | - | - |
| ROM / SYSRAM | R | - | - | - | - |
| SRAM1-3 | R | - | - | - | - |
| BKPSRAM (备份RAM) | R | R | R | O | O |
| RETSRAM (保留RAM) | R | R | R | O | O |
| 备份寄存器 | R | R | R | R | R |
| DDRCTRL | R | R | R | - | - |

### 电源监控

| 模块 | Sleep | Stop/LP-Stop | LPLV-Stop | Standby | VBAT |
|------|:-----:|:------------:|:---------:|:-------:|:----:|
| BOR | Y | Y | Y | Y | Y |
| PVD | O | O | O | - | - |
| AVD | O | O | O | - | - |
| VBAT 阈值 | - | - | - | - | O |
| 温度阈值 | O | O | O | O | O |

### 振荡器与时钟

| 模块 | Sleep | Stop/LP-Stop | LPLV-Stop | Standby | VBAT |
|------|:-----:|:------------:|:---------:|:-------:|:----:|
| HSI | O | O(可唤醒) | - | - | - |
| HSE | O | - | - | - | - |
| LSI | O | O | O | O | - |
| LSE | O | O | O | O | O |
| RTC/自动唤醒 | O | O | O | O | O |

### 外设

| 模块 | Sleep | Stop/LP-Stop | LPLV-Stop | Standby | VBAT |
|------|:-----:|:------------:|:---------:|:-------:|:----:|
| FDCAN | O | - | - | - | - |
| ETH | O | - | - | - | - |
| USART | O | O(可唤醒) | O | - | - |
| I2C | O | O(地址匹配唤醒) | O | - | - |
| SPI | O | O | O | - | - |
| LPTIM | O | O | O | O | - |
| IWDG | O | R | R | - | - |
| GPIO | O | O(部分可唤醒) | O | 5引脚可唤醒 | - |

## 9.5.2 PWR 电源模式 (p.472)

| 模式 | VDDCORE | PWR_LP | PWR_ON (LPCFG=0) | PWR_ON (LPCFG=1) |
|------|---------|--------|-------------------|------------------|
| Run | 正常 | H | H | H |
| Stop | 正常 | H | H | H |
| LP-Stop | 正常 | L | H | L |
| LPLV-Stop | 可能降压 | L | H | L |
| Standby | 关断 | L | L | L |

## 图 43 — 电源状态与外部稳压器控制 (p.473)

三种电源状态:

```
Standby (PWR_ON=0, PWR_LP=0)
    ↓ 唤醒
Run (PWR_ON=1, PWR_LP=1)  ←── MAIN 模式
    ↓ 进低功耗
Stop / LP-Stop / LPLV-Stop (PWR_LP=0, PWR_ON 看 LPCFG)  ←── LOW POWER 模式
```

从 Low-power 状态不能直接进 Standby，必须先唤醒回 Run。

## 表 35 — PDDS 低功耗模式控制 (p.473)

| CSTBYDIS | MPU PDDS | MCU PDDS | 系统状态 |
|:--------:|:--------:|:--------:|----------|
| x | 0 | x | Stop/LP-Stop/LPLV-Stop (MPU 在 CStop) |
| 0 | 1 | 0 | Stop/LP-Stop/LPLV-Stop (MPU 在 CStandby) |
| 0 | 1 | 1 | **Standby** (MPU 在 CStandby) |
| 1 | 1 | 1 | **Standby** (MPU 在 CStop) |

**规则**: 只有 **两个 PDDS 都为 1** 才进 Standby。CSTBYDIS=1 允许 MPU 在 CStop 就进 Standby（不需要 CStandby）。

## 图 44 — 详细状态图 (p.474)

状态图涉及三个维度:
- **CPU 模式**: CRun/CSleep/CStop/CStandby
- **总线模式**: AXI DRun/DStop, AHB DRun/DStop
- **系统模式**: Run(绿) / Stop/LP-Stop/LPLV-Stop(蓝) / Standby(红)

| 箭头颜色 | 含义 |
|---------|------|
| 绿色 | 系统保持 Run（某个核 CSleep 唤醒） |
| 蓝色 | 两核都 CStop → 进 Stop/LP-Stop/LPLV-Stop |
| 红色 | 进 Standby |
| 红色虚线 | MPU 进 CStandby（结果取决于 MCU 状态） |
| 橙色 | MCU 在 AXI 上分配/释放外设 |

## 表 36 — 低功耗模式退出标志 (p.475)

| 系统模式 | SBF | SBFMPU | STOPF | 含义 |
|----------|:---:|:------:|:-----:|------|
| Run | 0 | 0 | 0 | 内容保留 |
| Run (MPU CStandby 唤醒) | 0 | 1 | 0 | MPU 本地复位 |
| Stop / LP-Stop / LPLV-Stop | 0 | 0 | 1 | 时钟停止后恢复 |
| Stop/LP-Stop (MPU CStandby) | 0 | 1 | 1 | MPU 本地复位+时钟恢复 |
| **Standby** | **1** | **1** | **0** | **内容丢失** |
| VBAT | 0 | 0 | 0 | VSW 域内容保留 |

- **SBF**: Standby Wakeup Flag（PWR_MPUCR / PWR_MCUCR）
- **SBFMPU**: MPU 从 CStandby 唤醒
- **STOPF**: 从 Stop/LP-Stop/LPLV-Stop 唤醒
- **CSSF**: 清除自己的唤醒标志

**SBF=1 是最高优先级** — 表示内存可能已经丢失，需要完整重新初始化。

## 9.5.3 系统唤醒 (p.475)

从 Standby 唤醒流程:

```
1. 复位事件触发 (NRST/IWDG/WKUP 引脚/RTC 等)
2. PWR_ON 拉高 → 外部稳压器给 VDDCORE 供电
3. VDDCORE 超 vddcore_ok → 释放复位
4. HSI 起振 → HW 初始化 (选项字节加载)
5. MPU 从 Boot ROM 开始执行
6. MCU 保持在 HOLD_BOOT (等待 MPU 通过 remoteproc 放行)
```

MCU 必须 HOLD 的原因: MCU 固件通常在 DDR 中，DDR 需要 MPU 侧初始化完成后才能使用。

## 9.6 低功耗模式的进入与退出 (p.476-477)

### 省电三个层次

1. **降频** — Run 模式下降低 CPU/总线时钟（RCC 控制）
2. **门控外设时钟** — Run 下 PERxEN 停用未用外设，CSleep 下 PERxLPEN
3. **睡眠模式** — CSleep / CStop / CStandby / Stop / LP-Stop / LPLV-Stop / Standby

### 进入低功耗 (p.476)

所有核通过 **WFI（Wait For Interrupt）** 或 **WFE（Wait For Event）** 指令进入:

- MPU: WFI/WFE → CSleep/CStop/CStandby（取决于 STPREQ + PDDS 配置）
- MCU: WFI/WFE/SLEEPONEXIT → CSleep/CStop（取决于 SCR.SLEEPDEEP）

系统进 Stop/LP-Stop/LPLV-Stop/Standby 的条件:
- **所有 EXTI 唤醒源已清除**
- **MPU 和 MCU 都在 CStop 或 CStandby**

### 退出低功耗 (p.477)

| 当前模式 | 唤醒方式 | 唤醒后行为 |
|----------|---------|-----------|
| **CSleep** (WFI) | 任意 GIC/NVIC 中断 | 从下一条指令继续 |
| **CSleep** (WFE) | 任意事件（中断或 EXTI 事件模式） | 从下一条指令继续 |
| **CStop / Stop / LP-Stop / LPLV-Stop** | EXTI 中断或事件 | 从下一条指令继续 |
| **CStandby** | EXTI 中断 | **MPU 本地复位**，从复位向量开始 |
| **Standby** | NRST / IWDG / WKUP 引脚 / RTC 事件 | **冷启**（选项字节→Boot ROM） |

### WFI vs WFE 差异

| 指令 | 唤醒方式 | ISR 执行 |
|------|---------|---------|
| WFI | 中断必须被 GIC/NVIC 确认 | 必进 ISR，然后返回 |
| WFE | 事件（中断或 EXTI 事件模式） | 不进 ISR，直接执行下一条 |

### CStandby 唤醒

MPU 醒来走 **本地复位** 而非冷启:
- PWR_MPUCR.SBFMPU 被置位
- MPU 从复位向量开始执行（不是 WFI 下一条）
- **非 Standby**: Core 域有电，数据保留
- 程序需检查 SBFMPU 判断是否需要重新初始化

### Standby 唤醒

唯一一个 **MCU 保持 HOLD_BOOT** 的模式:
```
Standby → NRST/IWDG/WKUP/RTC → PWR_ON 拉高
  → VDDCORE 恢复 → HSI → HW Init → Boot ROM
  → MPU 启动 → MCU 在 HOLD 等待
```
此后 MCU 需要 MPU 端的 remoteproc 来释放。

## 9.6.5 CSleep 模式 (p.478-479)

特征: 内核时钟停，外设时钟由 PERxLPEN 控制。

### MPU CSleep — Table 37

| 项目 | 说明 |
|------|------|
| 进入 | WFI/WFE + STPREQ_P ≠ b11 + GIC 中断/事件清除 |
| 唤醒(WFI) | GIC 使能的任意中断 |
| 唤醒(WFE) | 任意中断（即使 GIC 禁用）或事件 |
| 唤醒延迟 | 无 |

### MCU CSleep — Table 38

| 项目 | 说明 |
|------|------|
| 进入 | WFI/WFE/SLEEPONEXIT + SCR.SLEEPDEEP=0 + NVIC 中断/事件清除 |
| 唤醒(WFI) | NVIC 使能的任意中断 |
| 唤醒(WFE+SEVONPEND=0) | 事件 |
| 唤醒(WFE+SEVONPEND=1) | 任意中断（即使 NVIC 禁用）或事件 |
| 唤醒延迟 | 无 |

## 9.6.6 CStop 模式 (p.479-480)

特征: 内核+外设时钟全停。有内核时钟请求的外设仍可申请（时钟源支持 CStop 时）。

### MPU CStop — Table 39

| 项目 | 说明 |
|------|------|
| 进入 | WFI + STPREQ_P=b11 + (PDDS=0 或 CSTBYDIS=1) + GIC + EXTI 唤醒源清除 |
| 唤醒 | GIC 使能的 EXTI 中断 |
| 唤醒延迟 | EXTI + RCC 唤醒同步 |

两种进入路径:
- `STPREQ=b11 + PDDS=0` → 普通 CStop
- `STPREQ=b11 + CSTBYDIS=1 + PDDS=1` → Standby 配合的 CStop

### MCU CStop — Table 40

| 项目 | 说明 |
|------|------|
| 进入 | WFI/WFE/SLEEPONEXIT + SCR.SLEEPDEEP=1 + NVIC 中断/事件清除 + EXTI 唤醒源清除 |
| 唤醒(WFI) | NVIC 使能的 EXTI 中断 |
| 唤醒(WFE+SEVONPEND=0) | EXTI 事件 |
| 唤醒(WFE+SEVONPEND=1) | EXTI 中断（即使 NVIC 禁用）或 EXTI 事件 |
| 唤醒延迟 | EXTI + RCC 唤醒同步 |

IWDG1/2 配置为系统调试唤醒事件时可触发 MPU+MCU 唤醒并触发 CTI。

## 9.6.7 CStandby 模式 — Table 41 (p.480-481)

仅 MPU 支持。MPU 时钟+外设时钟全停。有内核时钟请求的外设仍可申请（同 CStop）。

| 项目 | 说明 |
|------|------|
| 进入 | WFI + STPREQ_P=b11 + CSTBYDIS=0 + PDDS=1 + GIC + EXTI 唤醒源清除 |
| 唤醒 | GIC 使能的 EXTI 中断 |
| 唤醒延迟 | EXTI + RCC 唤醒同步 |
| 唤醒后行为 | **MPU 本地复位**，从复位向量开始 |

## 9.6.8 Stop 模式 — Table 42 (p.481-482)

| 项目 | 说明 |
|------|------|
| 进入条件 | MPU/MCU 都在 CStop/CStandby（或 MCU HOLD_BOOT）+ 无活跃 EXTI 唤醒源 + **至少一个 PDDS 选 Stop** + LPDS=0 |
| 唤醒 | EXTI 中断 |
| 唤醒延迟 | EXTI+RCC 同步 + PWRLP_TEMPO |
| I/O 状态 | 全部保持 |

Stop 模式下可保持运行的外设:
- IWDG, RTC (LSE), LSI, LSE
- 有内核时钟请求的外设
- HSI/CSI/HSE (由 RCC 的 xxxKERON 控制)
- ADC/DAC 可进入低功耗

## 9.6.9 LP-Stop 模式 — Table 43 (p.483)

| 项目 | 说明 |
|------|------|
| 与 Stop 区别 | LPDS=1 |
| 额外延迟 | **LP-Stop timeout**（外部稳压器从 Low-power 切回 Main）|

## LPLV-Stop 模式 — Table 44 (p.483-484)

| 项目 | 说明 |
|------|------|
| 进入条件 | 同上 + LPDS=1 + **LVDS=1** |
| 唤醒源 | WKUP 引脚, RTC alarm/wakeup/tamper/time stamp, NRST/IWDG |
| 唤醒延迟 | LPLV-Stop timeout（VDDCORE 恢复电压）+ EXTI+RCC 同步 |
| 限制 | 除 LPTIM(LSE/LSI)、RTC、Tamper、WKUP 引脚外，其他可唤醒外设**必须禁用**；DDR 保留需 PWR_CR3.DDRRETEN |

## 9.6.10 Standby 模式 — Table 45 (p.484-485)

### 进入条件

- MPU=CStandby 或 CStop + MCU=CStop 或 HOLD_BOOT
- **所有 PDDS 都选 Standby**
- 所有 WKUPF 清除 + RTC 唤醒事件清除

### 唤醒源

WKUP 引脚, RTC alarm/wakeup/tamper/time stamp, NRST, IWDG

### 掉电保留关系

| 保留 | 丢失 |
|------|------|
| VDD 域, VSW 域, 备份域 | SRAM1-3, 大部分寄存器 |
| RTC/备份寄存器, LSE, LSI | PLL, HSI, CSI, HSE |
| Retention RAM (可选) | |
| PWR_CR2, PWR_CR3, SBF, WKUPFR, MPUWKUPENR | 其余全部复位 |

### 唤醒后行为

- **MCU 保持 HOLD**，MPU Boot ROM 启动
- 用 `RCC_MP_RSTSCLRR.STDBYRSTF / CSTDBYRSTF` 判断 Standby/CStandby 唤醒
- ⚠ **SBF/SBFMPU 是 BOOTROM 专用，应用软件不要直接读**

### 六种模式对比

| 模式 | LPDS | LVDS | 唤醒延迟主要开销 | VDDCORE | 适合场景 |
|------|:----:|:----:|-----------------|---------|---------|
| **CSleep** | — | — | 无 | 正常 | 短暂空闲，毫秒级 |
| **CStop** | — | — | EXTI+RCC 同步 | 正常 | 等待特定外设事件 |
| **Stop** | 0 | — | EXTI+RCC+PWRLP_TEMPO | 正常 | 数十ms空闲，需快唤醒 |
| **LP-Stop** | 1 | 0 | 上 + 稳压器切换 | 正常 | 数百ms空闲 |
| **LPLV-Stop** | 1 | 1 | 电压恢复 + EXTI+RCC | 降压 | 较长空闲，省电优先 |
| **Standby** | — | — | 冷启全流程 | 掉电 | 数秒以上，不关心唤醒时间 |

## 9.6.11 Standby I/O 状态 (p.486)

Standby 下所有 I/O 高阻无上拉，除外:
- NRST, PWR_ON, PWR_LP — 仍然工作
- RTC_AF1 (配置为 tamper/time stamp/RTC alarm 时)
- WKUP 引脚 (上拉由 PWR_WKUPCR.WKUPPUPD 配置)

## 9.6.12 调试模式 (p.486)

| 调试模式 | 行为 |
|---------|------|
| Debug Stop | DBG_STOP 使能 → PWR 不拉低 PWR_ON/LP，Core 域保持供电 |
| Debug Standby | DBG_Standby 使能 → PWR 保持 PWR_ON 活跃，Core 域不掉电，CPU/总线可继续运行，退出时仍会复位 |

## 9.7 WKUP 引脚 (p.486)

- 极性: PWR_WKUPCR.WKUPP (改前必须先禁用 WKUPEN)
- 中断使能前先清 WKUPF (防误中断)

## 9.8 PWR_ON/PWR_LP 引脚 — Figure 45 (p.486-487)

- PWR_ON: LPCFG=0 输出 pwr_on (仅 Standby 低); LPCFG=1 输出 pwr_onlp (Standby/LP-Stop/LPLV-Stop 都低)
- PWR_LP: 输出 pwr_onlp，低功耗模式拉低

## 9.9 PWR 中断 — Table 46 (p.487)

| 标志 | 说明 | 中断 | 清除 |
|------|------|------|------|
| **STOPF** | Stop 模式退出 | Stop 唤醒事件 | CSSF |
| **SBF** | Standby 唤醒 | Standby 唤醒事件 (**BOOTROM 专用**) | CSSF |
| **SBFMPU** | CStandby 唤醒 | CStandby 唤醒 (**BOOTROM 专用**) | CSSF |
| WKUPF[6:1] | WKUP 引脚事件 | MPU/MCU 唤醒 | WKUPC |
| PVDO | PVD 检测 | EXTI | — |
| AVDO | AVD 检测 | EXTI | — |
| TEMPL/TEMPH | 温度监测 | EXTI | — |
| BRRDY | 备份稳压器就绪 | EXTI | — |
| RRRDY | 保留稳压器就绪 | **无** | — |
| USB33RDY | USB FS 3.3V 就绪 | **无** | — |
| REG18RDY | 1.8V 稳压器就绪 | **无** | — |
| REG11RDY | 1.1V 稳压器就绪 | **无** | — |

应用软件用 `RCC_MP_RSTSCLRR.STDBYRSTF/CSTDBYRSTF` 判断复位来源，不要直接读 SBF/SBFMPU。

## 9.10 TrustZone 安全 — Table 47 (p.488)

RCC_TZCR.TZEN 使能后保护以下配置不受非安全访问:
- VBAT/温度监控, PVD/AVD
- 低功耗及 RAM 设置
- 备份域写保护
- 各稳压器及 VBAT 充电
- MPU 电源控制及 Standby 唤醒

### 寄存器安全属性

| 类型 | 寄存器 |
|------|--------|
| 写安全 (TZEN=1 时非安全写 → bus error) | PWR_CR1, CR2, CR3, MPUCR, MCUCR, MPUWKUPENR, MCUWKUPENR |
| 非安全 | PWR_CSR1, WKUPCR, WKUPFR, VER, ID, SID |

## 9.11.1 PWR_CR1 — 控制寄存器 1 (p.489)

偏移 0x000，复位 0x0000 0000，任意系统复位复位。

| 位 | 名称 | 说明 |
|:--:|------|------|
| 20:19 | ALS[1:0] | AVD 阈值选择 |
| 18 | AVDEN | AVD 使能 |
| 8 | DBP | 备份域写保护禁用 |
| 7:5 | PLS[2:0] | PVD 阈值选择 |
| 4 | PVDEN | PVD 使能 |
| 2 | LVDS | 低电压 Deep Sleep (LPLV-Stop) |
| 1 | LPCFG | PWR_ON 低功耗配置 |
| 0 | LPDS | 低功耗 Deep Sleep (LP-Stop) |
