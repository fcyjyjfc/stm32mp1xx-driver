## 29.4 ADC 功能描述

### 29.4.1 ADC 框图

ADC 框图的主要组成部分：

| 信号/模块 | 说明 |
|-----------|------|
| ADCx_INP[19:0] | 正相模拟输入（单端模式用此脚） |
| ADCx_INN[19:0] | 反相模拟输入（差分模式用） |
| VDDA / VREF+ / VREF- / VSSA | 模拟电源和参考电压 |
| BOOST | 根据 ADC 时钟频率设置的升压模式 |
| DEEPPWD | 深度掉电模式 |
| ADVREGEN | 内部 LDO 稳压器使能 |
| ADCAL / ADCALDIF | 单端/差分校准 |
| DIFSEL[19:0] | 每通道差分/单端选择 |
| PCSEL[19:0] | 通道预选寄存器 |
| SMPx[2:0] | 每通道采样时间 |

**AHB 接口：**
- 从机接口：控制/状态寄存器和数据读/写
- 主机接口：无（ADC 是 AHB 从机）
- 通过 DMA 传输数据（`adc_dma` 请求）
- 数据输出到 DFSDM 做后处理（`adc_dat[15:0]`）

**中断输出：**
ADRDY / EOSMP / EOC / EOS / OVR / JEOC / JEOS / JQOVF / AWDx

**触发输入：**
- 常规转换：`adc_ext_trg[20:0]`（21 个外部触发源，来自定时器等）
- 注入转换：`adc_jext_trg[20:0]`（21 个外部触发源）

---

### 29.4.2 ADC 引脚和内部信号

#### 外部引脚

| 引脚名 | 类型 | 说明 |
|--------|------|------|
| VREF+ | 模拟参考正极 | ADC 正参考电压，1.62~3.6V |
| VDDA | 模拟电源 | 等于 VDDA |
| VREF- | 模拟参考负极 | ADC 负参考电压 |
| VSSA | 模拟地 | 等于 VSS |
| ADCx_INP[0:19] | 外部模拟输入 | 正相通道（x=1,2），INP0~5 快速，INP6~19 慢速 |
| ADCx_INN[0:19] | 外部模拟输入 | 反相通道，用于差分模式 |

#### 内部信号

| 信号名 | 类型 | 说明 |
|--------|------|------|
| VINP[y] | 模拟输入 | 实际连接到 SAR ADC 的正相输入（外部或内部通道） |
| VINN[y] | 模拟输入 | 实际连接到 SAR ADC 的反相输入 |
| adc_ext_trgy | 输入 | 常规转换外部触发（最多 21 路，来自片内定时器），ADC 主从共享 |
| adc_jext_trgy | 输入 | 注入转换外部触发（最多 21 路），ADC 主从共享 |
| adc_awd1/2/3 | 输出 | 模拟看门狗输出，连接到片内定时器 |
| adc_it | 输出 | ADC 中断 |
| adc_hclk | 输入 | AHB 总线时钟 |
| adc_ker_ck_input | 输入 | ADC 内核时钟（来自 RCC） |
| adc_dma | 输出 | ADC DMA 请求 |
| adc_dat[15:0] | 输出 | ADC 数据输出 |

#### 内部通道连接

| 信号 | 来源/去向 |
|------|-----------|
| ADC2 VINP[12] | VSENSE（内部温度传感器） |
| ADC2 VINP[13] | VREFINT（内部参考电压） |
| ADC2 VINP[15] | VREFINT（内部参考电压） |
| ADC2 VINP[14] | VBAT/4（电池电压） |
| ADC2 VINP[16] | DAC 输出 ch1 |
| ADC2 VINP[17] | VDDCORE（内核电压） |
| adc_dat[15:0] | → DAC ch1, ch2 / DFSDM |

---

### 29.4.3 ADC 时钟

ADC 采用双时钟域架构：**ADC 时钟独立于 AHB 总线时钟**。

#### 两种时钟方案

**方案 1：独立时钟（`adc_ker_ck`）**
- CKMODE[1:0] = `00`（ADCx_CCR 寄存器）
- 独立于 AHB 时钟的专用时钟源
- 可以在 RCC 中配置 `adc_ker_ck` 时钟频率
- 优点：可以达到最高 ADC 时钟频率，不受 AHB 总线时钟限制
- ADC 时钟可进一步分频：`/1, /2, /4, /6, /8, /10, /12, /16, /32, /64, /128, /256`
  （通过 ADCx_CCR 的 PRESC[3:0] 配置）

**方案 2：AHB 时钟分频**
- CKMODE[1:0] ≠ `00`
- 从 AHB 时钟分频而来：`/1, /2, /4`
- 优点：无时钟域同步问题，定时器触发的 ADC 转换无额外抖动

#### 时钟约束

| 条件 | 时钟比要求 |
|------|-----------|
| 所有通道为 16/14/12/10 位 | Fadc_hclk ≥ Fadc_ker_ck / 4 |
| 存在 8 位通道 | Fadc_hclk ≥ Fadc_ker_ck / 3 |

多个 ADC 同时使用时必须使用**同一个时钟源**（不带预分频器）从 RCC 输入。

#### BOOST 控制

通过 ADC_CR 寄存器的 BOOST 位设置，必须根据 ADC 时钟频率正确配置。

---

### 29.4.4 ADC1/2 连接

ADC1 和 ADC2 紧密耦合，共享部分外部通道。

#### 关键连接规则

- **ADCx_INNy 信号只能在相应通道配置为差分模式时使用**
- 内部通道（VSENSE / VREFINT / VBAT / VDDCORE）仅在 **ADC2** 上可用
- ADC1 有最多 20 个外部输入通道
- ADC2 除了外部通道外还内置了温度传感器、参考电压、电池监测等内部通道

#### ADC1 通道列表

| GPIO/通道 | VINP[x] | 速度 | 注 |
|-----------|---------|------|----|
| ADC1_INP0 (与 ADC2 共用) | VINP[0] | 快速 | 含 VINN[0] |
| ADC1_INP1 (与 ADC2 共用) | VINP[1] | 快速 | 含 VINN[1] |
| ADC1_INP2 | VINP[2] | 快速 | 含 VINN[2] |
| ADC1_INP3 (与 ADC2 共用) | VINP[3] | 慢速 | 含 VINN[3] |
| ADC1_INP4 (与 ADC2 共用) | VINP[4] | 慢速 | 含 VINN[4] |
| ADC1_INP5 (与 ADC2 共用) | VINP[5] | 慢速 | 含 VINN[5] |
| ADC1_INP6 | VINP[6:7] | 慢速 | 含 VINN[6:7] |
| ADC1_INP7 (与 ADC2 共用) | VINP[8:9] | 慢速 | 含 VINN[8:9] |
| ADC1_INP8 (与 ADC2 共用) | VINP[10:11] | 慢速 | 含 VINN[10:11] |
| ADC1_INP9 (与 ADC2 共用) | VINP[12:13] | 慢速 | 含 VINN[12:13] |
| ADC1_INP10 (与 ADC2 共用) | VINP[14:15] | 慢速 | 含 VINN[14:15] |
| ADC1_INP11 (与 ADC2 共用) | VINP[16:17] | 慢速 | 含 VINN[16:17] |
| ADC1_INP12~19 | VINP[18:19] | 慢速 | 含 VINN |

#### ADC2 通道列表（含内部通道）

| GPIO/通道 | VINP[x] | 速度 | 注 |
|-----------|---------|------|----|
| ADC2_INP0 (与 ADC1 共用) | VINP[0] | 快速 | 含 VINN[0] |
| ADC2_INP1 (与 ADC1 共用) | VINP[1] | 快速 | 含 VINN[1] |
| ADC2_INP2 | VINP[2] | 快速 | 含 VINN[2] |
| ADC2_INP3 (与 ADC1 共用) | VINP[3] | 快速 | 含 VINN[3] |
| ADC2_INP4 (与 ADC1 共用) | VINP[4] | 快速 | 含 VINN[4] |
| ADC2_INP5 (与 ADC1 共用) | VINP[5] | 快速 | 含 VINN[5] |
| ADC2_INP6 | VINP[6] | 慢速 | 含 VINN[6] |
| ADC2_INP7 (与 ADC1 共用) | VINP[8:9] | 慢速 | 含 VINN[8:9] |
| ADC2_INP8 (与 ADC1 共用) | VINP[10:11] | 慢速 | 含 VINN[10:11] |
| ADC2_INP9 (与 ADC1 共用) | VINP[12:13] | 慢速 | 含 VINN[12:13] |
| ADC2_INP10 (与 ADC1 共用) | VINP[14:15] | 慢速 | 含 VINN[14:15] |
| ADC2_INP11 (与 ADC1 共用) | VINP[16:17] | 慢速 | 含 VINN[16:17] |
| ADC2_INP18 (与 ADC1 共用) | VINP[18:19] | 慢速 | 含 VINN[18:19] |

**ADC2 内部专用通道：**
- VINP[12] ← **VSENSE**（温度传感器）
- VINP[13] / VINP[15] ← **VREFINT**（内部参考电压）
- VINP[14] ← **VBAT/4**（电池电压监测）
- VINP[16] ← **DAC_OUT1**
- VINP[17] ← **VDDCORE**（内核电压监测）
- VINP[7] ← **DAC_OUT2**

---

### 29.4.5 AHB 从机接口

ADC 实现了一个 AHB 从机端口，用于控制/状态寄存器和数据访问。

- 字（32 位）访问
- 单周期响应
- 所有读写访问均为零等待
- 不支持 split/retry 请求，从不产生 AHB 错误

---

### 29.4.6 ADC 深度掉电模式（DEEPPWD）与电压调节器（ADVREGEN）

默认情况下，ADC 处于深度掉电模式，内部电源被切断以降低漏电流（复位后 ADC_CR 的 DEEPPWD=1）。

**启动顺序：**
1. 清除 DEEPPWD=0，退出深度掉电
2. 设置 ADVREGEN=1，使能内部 LDO 稳压器
3. 软件必须等待稳压器启动时间 TADCVREG_STUP（查数据手册）
4. 可检查 ADC_ISR 的 LDORDY 位确认 LDO 就绪

**关闭顺序：**
1. ADEN=0 禁用 ADC
2. ADVREGEN=0 关闭 LDO（可选省电）
3. DEEPPWD=1 重新进入深度掉电（进一步省电）

**注意事项：**
- DEEPPWD=1 会自动清除 ADVREGEN
- 写 DEEPPWD=1 时，写入顺序应该先设 DEEPPWD=1（它会自动清 ADVREGEN）
- 仅关闭 LDO（ADVREGEN=0 但 DEEPPWD=0）：**内部模拟校准值保留**
- 进入深度掉电（DEEPPWD=1）：**内部模拟校准值丢失**，需要重新校准或重新写入之前保存的系数

---

### 29.4.7 单端与差分输入通道

通过 **ADC_DIFSEL** 寄存器的 DIFSEL[19:0] 位逐通道配置，**必须在 ADC 禁用时写入**（ADEN=0）。

**单端模式：**
- 转换电压 = VINP[i]（正输入） - VREF-（负参考）
- 使用 INP 引脚即可

**差分模式：**
- 转换电压 = VINP[i]（正输入） - VINN[i]（负输入）
- 需要同时使用 INP 和 INN 引脚
- 输出数据为**无符号数**：
  - VINP[i] = VREF-，VINN[i] = VREF+ 时 → 0x0000（16位分辨率）
  - VINP[i] = VREF+，VINN[i] = VREF- 时 → 0xFFFF
- 转换值计算公式：
  Converted Value = ADC_Full_Scale × (VINP - VINN) / (VREF+ - VREF-) × (-1) + ADC_Full_Scale / 2
- 差分模式下两个输入端应偏置在 VREF+/2
- 输入信号应为差分信号（共模电压应固定）

**注意事项：**
- 通道 "i" 配置为差分时，其负输入 VINN[i] 被占用。连接到同一 VINN[i] 的通道 "i+n" 不能同时被不同的 ADC 转换
- 部分通道在 ADC1/ADC2 之间共享，差分配置可能使另一 ADC 的对应通道不可用

---

### 29.4.8 校准（ADCAL, ADCALDIF, ADCALLIN, ADC_CALFACT）

每个 ADC 提供自动校准流程，包含上电/下电序列。校准期间，ADC 计算校准系数（11 位偏移或 160 位线性），内部应用于 ADC 直到下次断电。

校准是任何 ADC 转换的前提，用于消除芯片间的系统误差，补偿偏移和线性偏差。

#### 偏移校准与线性校准

| 配置 | 说明 |
|------|------|
| ADCALDIF = 0 | 校准用于**单端输入**转换 |
| ADCALDIF = 1 | 校准用于**差分输入**转换 |
| ADCALLIN = 1 | 同时执行线性校准和偏移校准 |
| ADCALLIN = 0 | 仅执行偏移校准，不执行线性校准 |

线性校准**只需做一次**（不受单端/差分影响）。

#### 基本校准流程（6 步）

1. 确保 DEEPPWD=0、ADVREGEN=1，等待 LDO 稳定（检查 LDORDY）
2. 确保 ADEN=0（ADC 禁用）
3. 配置 ADCALDIF（单端/差分）和 ADCALLIN（是否线性校准）
4. 设置 ADCAL=1（启动校准）
5. 等待 ADCAL=0（硬件自动清零表示完成）
6. 读取校准系数：
   - 偏移系数 → ADC_CALFACT 寄存器的 CALFACT_S[10:0] 或 CALFACT_D[10:0]
   - 线性系数 → ADC_CALFACT2 寄存器（需遵循线性系数读取流程，且 ADEN=1）

#### 线性校准系数的读取流程（20 步）

校准完成后（ADCAL=0），ADC_CR 中的 6 个 LINCALRDYW1..6 位被置 1。读取需要 6 轮，每轮操作一个 LINCALRDYWx 位：

| 轮次 | 位 | 线性系数位域 | 有效位数 | 说明 |
|------|---|-------------|---------|------|
| 1 | LINCALRDYW6 | [159:150] | 10 位 | 注意：写 ADC_CALFACT2[9:0] |
| 2 | LINCALRDYW5 | [149:120] | 30 位 | |
| 3 | LINCALRDYW4 | [119:90] | 30 位 | |
| 4 | LINCALRDYW3 | [89:60] | 30 位 | |
| 5 | LINCALRDYW2 | [59:30] | 30 位 | |
| 6 | LINCALRDYW1 | [29:0] | 30 位 | |

每轮操作步骤：
1. 清除对应的 LINCALRDYWx 位（写 0）
2. 轮询该位直到硬件返回 0（表示数据已就绪）
3. 读取 ADC_CALFACT2[29:0]

**前提条件：** ADEN=1 且 ADSTART=0、JADSTART=0（ADC 使能且无转换进行中）

**注意：** 一次只能操作一个 LINCALRDYWx 位，不能同时改多个位。

#### 线性校准系数的写入流程（20 步）

用于恢复之前保存的线性校准系数（掉电后重新注入）：

1. 确保 DEEPPWD=0、ADVREGEN=1，检查 LDORDY
2. 设 ADEN=1，等 ADRDY=1
3. 每轮：先写 ADC_CALFACT2[29:0] → 设对应 LINCALRDYWx 位（写 1） → 轮询直到硬件返回 1（表示写入完成）

**前提条件：** ADEN=1 且 ADSTART=0、JADSTART=0

#### 混合单端和差分输入的校准策略

如果 ADC 同时使用单端和差分通道，需要做两次校准：

1. 禁用 ADC
2. **第一次校准**：ADCALDIF=0（单端）+ **ADCALLIN=1**（含线性校准）
   - 更新 CALFACT_S[10:0] 和 LINCALFACT[159:0]
3. **第二次校准**：ADCALDIF=1（差分）+ ADCALLIN=0（仅偏移，无需再做线性）
   - 更新 CALFACT_D[10:0]
4. 使能 ADC，配置通道，启动转换

之后，每次从单端通道切换到差分通道（或反之），ADC 硬件会自动注入对应的校准因子，无需软件干预。

#### 校准系数保持与恢复

| 状态 | 校准系数状态 |
|------|-------------|
| ADEN=0（仅禁用） | 内部校准值保留 |
| ADVREGEN=0（关 LDO） | 内部校准值保留 |
| DEEPPWD=1（深度掉电） | 内部校准值丢失 |
| STANDBY/VBAT 模式 | 内部校准值丢失 |

**恢复方法：**
- 方法一：重新执行校准流程
- 方法二：写入之前保存的校准系数（省时间）
  - 偏移系数写入 ADC_CALFACT（CALFACT_S / CALFACT_D）
  - 线性系数通过 LINCALRDYW1..6 流程写入 ADC_CALFACT2
  - ADC 使能且未转换状态写入（ADEN=1, ADSTART=0, JADSTART=0）
  - 下次启动转换时自动注入，**零延迟**

**VREF+ 变化超过 10%** 时建议重新校准。
