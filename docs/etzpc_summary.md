# ETZPC — Extended TrustZone Protection Controller

## 介绍

ETZPC 配置 SoC 的 TrustZone 安全属性，控制对象：
- 片上 RAM/ROM 的可编程安全区域大小
- AHB/APB 外设设为 secure
- AHB/APB 外设设为隔离

## 特性

- 32-bit APB4 接口
- ETZPC 自身永远 write-secure
- 寄存器控制：
  - 片上 RAM/ROM secure 区域大小
  - AHB/APB 外设访问权限
  - 外设隔离到 Cortex-M4 域
  - 配置锁定
- 内存区域可按 4KB 步进配置

## TrustZone 架构

- Secure world (AxPROT[1]=0)：运行安全敏感应用
- Non-secure world (AxPROT[1]=1)：Linux 用户空间
- STM32MP157 将 TrustZone 扩展到 AHB/APB

### 外设分类

| 类型 | 说明 |
|------|------|
| secure | 硬件防火墙，非安全访问 Bus error（写忽略、读返回 0） |
| securable | ETZPC 控制是否 secure，同上规则 |
| non-secure | 直连，无限制 |
| TrustZone-aware | 外设自身实现 TrustZone 行为（如部分寄存器强制 secure） |

### AXI 主设备

- non-secure 主设备：AxPROT[1]=1
- TrustZone 能力主设备 (CA7, MDMA)：secure 时 AxPROT[1]=0
- AHB 主设备默认 non-secure

### 初始化

- ETZPC 在 secure boot 阶段配置，之后不更改
- TZMA 安全区域大小只能由安全软件改
- RAM 释放给 non-secure 前必须先清除内容

## 15.4 功能描述

两组控制位：
- **r0size[]** — 定义 TZMA 安全区域大小（管内存）
- **decprot[1:0]** — 每个可安全配置外设的访问权限（管外设）

### decprot 编码

| 值 | 读 | 写 | 说明 |
|----|----|----|------|
| 00 | 仅安全 | 仅安全 | 完全 secure |
| 01 | 都行 | 仅安全 | write secure |
| 10 | 都行 | 都行 | non-secure + 可隔离外设隔离到 M4 域 |
| 11 | 都行 | 都行 | 完全 non-secure |

- 复位默认：所有内存区域 secure，所有外设读写 secure
- r0size/decprot 可写锁，锁后直到下次复位才能改

### 要点

- 复位默认全 secure → FSBL 必须把 M4 用的外设改为 10/11，否则 M4 访问静默 Bus error（读返回 0，写被忽略，不报错）
- decprot=10 是给 M4 的标准配法：放权 + 隔离到 M4 域，防止 A7/M4 同时访问

## 15.5 安全架构

外设安全分类：P (securable) / S (永远读写 secure) / WS (写 secure) / TrustZone-aware / TrustZone capable

### Table 86 — secure 和 securable 外设

| 类型 | 外设 |
|------|------|
| P | DDRCTRL, DDRPHYC, CRYP1, HASH1, RNG1, SRAM1-4, RETRAM, BKPSRAM |
| P (APB5 安全用途) | I2C4, I2C6, SPI6, USART1, IWDG1, STGENC |
| S | TZC, AXIM |
| WS | ETZPC 自身 |

### Table 87 — TrustZone-aware 外设

| 外设 | 行为 |
|------|------|
| GPIOZ | 引脚级安全 |
| RCC/PWR/EXTI | 敏感寄存器写 secure（可读） |
| MDMA | 每通道独立安全属性 |
| BSEC | 部分寄存器 secure |
| RTC/TAMP | 部分寄存器 secure |

### Table 88 — 内存防火墙

| 区域 | 控制器 | 配置 |
|------|--------|------|
| ROM | TZMA0 | 4KB 步进，最大 128KB，复位后全 secure |
| SYSRAM | TZMA1 | 4KB 步进，最大 256KB，复位后全 secure |

释放标准做法：FSBL 调小 r0size，SYSRAM 大部分变 non-secure 给 M4。

## 15.6 MCU 资源隔离

ETZPC 可将资源限定为仅 Cortex-M4 域访问。隔离域除 M4 外，可包含 DMA1/DMA2/OTG/SDMMC3/ETH1 等主设备（通过各自 decprot 配置）。

资源分三种类型：
- **type 1** — 仅可安全配置 (Table 90)
- **type 2** — 仅可隔离 (Table 91)
- **type 3** — 既可安全配置又可隔离 (Table 92)

### Table 90 (type 1, 可安全配置)

| decprot | 行为 |
|---------|------|
| 00 | RW secure (默认) |
| 01 | 写 secure |
| 10 | 保留 (行为未定义但不削弱安全) |
| 11 | non-secure |

### Table 91 (type 2, 可隔离)

| decprot | 行为 |
|---------|------|
| 0x | 保留 |
| 10 | 仅 Cortex-M4 域可读写 (A7 无论安全与否都被拒) |
| 11 | non-secure (默认) |

### Table 92 (type 3, 两者)

| decprot | 行为 |
|---------|------|
| 00 | RW secure |
| 01 | 写 secure |
| 10 | 仅 M4 域 |
| 11 | non-secure (默认) |

### Table 93 — 可编程性汇总

| 类型 | 00 安全 | 01 写安全 | 10 M4隔离 | 11 非安全 |
|------|---------|-----------|-----------|-----------|
| 1: 可安全 | ✓(默认) | ✓ | ✗ | ✓ |
| 2: 可隔离 | ✗ | ✗ | ✓ | ✓(默认) |
| 3: 两者 | ✓ | ✓ | ✓ | ✓(默认) |

### 失败访问行为

- 写忽略、读返回 0 + Bus error（CPU 异常 / DMA 中断）
- 例外：GPIOZ 静默失败（Tz-aware 不报错）
- 写已锁定的 ETZPC 配置静默忽略

### Table 94 — DECPROT 分配 (前段)

DECPROT0[1:0]~[23:22]，全部默认 0b00 (RW secure)，全部 type 1：
STGENC, BKPSRAM, IWDG1, USART1, SPI6, I2C4, I2C6, RNG1, HASH1, CRYP1, DDRCTRL, DDRPHYC

(表 94 未完，后续有 GPIO/TIM/LTDC 等)

## 对本项目的影响

- LTDC、I2C5、GPIOA-K 不在任何安全表内 = 普通 non-secure 外设，M4 直接访问
- 代码在 DDR (0xC2000040)，DDR 地址空间由 TZC 过滤，FSBL 通常配成 non-secure
- 坑：FSBL 若把 SRAM1-4 配成 secure-only，M4 从 SYSRAM 启动会直接跑飞
