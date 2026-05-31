# GIC（全局中断控制器）— RM0436 第1261-1264页翻译

## 23.1 介绍

GIC 的更多细节参考以下 Arm 规范：
- Arm Generic Interrupt Controller Architecture Specification v2.0
- Cortex-A7 MPCore Technical Reference Manual r0p5

## 23.2 GIC 主要特性

集成的 GIC 汇集并仲裁大量中断源，提供：
- 中断屏蔽
- 中断优先级排序
- 中断分发到目标处理器
- 追踪中断状态
- 软件产生中断
- 支持安全扩展（Secure/Non-Secure）
- 支持虚拟化扩展

## 23.3 GIC 功能描述

GIC 位于 Cortex-A7 MPCore 内部，是一个独立功能单元，由**一个共享分发器（Distributor）**和**多个 CPU 接口**组成。每个处理器核对应：
- 一个 CPU 接口
- 一个虚拟接口控制
- 一个虚拟 CPU 接口

GIC 寄存器是**内存映射**的，基地址通过每个处理器核的 CBAR（配置基地址寄存器）可见。这些寄存器所在的内存区域必须在页表中标记为 **device** 或 **strongly-ordered**，标记为 normal memory 的区域无法访问 GIC 寄存器。

### 图 138 GIC 框图

```
                 ┌───────────── 分 发 器 ─────────────┐
                 │                                     │
    SGI(ID0-15)  │    PPI(ID16-31)    SPI(ID32-287)   │
    PPIn/SGIn    │                                     │
                 └─────────┬──────────────────────┬────┘
                           │                      │
                    CPU接口0                  CPU接口1
                    ┌──────┐                  ┌──────┐
                    │ GICC │                  │ GICC │
                    └──┬───┘                  └──┬───┘
                       │                        │
                    IRQ/FIQ                   IRQ/FIQ
                       │                        │
                    核0                        核1
```

## 23.4 中断源

每个中断源由唯一的 ID 标识。Cortex-A7 MPCore 有 3 类中断：

### 23.4.1 SGIs（软件产生中断）
- 通过写 `GICD_SGIR` 寄存器产生
- 最多 16 个 SGI，ID0 ~ ID15
- 边沿触发
- Arm 建议：ID0~ID7 用于 Non-Secure 中断，ID8~ID15 用于 Secure 中断

### 23.4.2 PPIs（私有外设中断）
每个 CPU 接口有 7 个 PPI，特定于单个处理器：

| PPI | 中断号 | 名称 | 说明 |
|-----|--------|------|------|
| PPI0 | ID28 | nFIQ 信号 | 不使用。GIC 旁路后 nFIQ 直连处理器；使能 GIC 后作为 ID28，低电平有效 |
| PPI1 | ID29 | Secure 物理定时器 | 电平触发 |
| PPI2 | ID30 | Non-Secure 物理定时器 | 电平触发 |
| PPI3 | ID31 | nIRQ 信号 | 不使用，同 nFIQ 逻辑 |
| PPI4 | ID27 | 虚拟定时器 | 电平触发 |
| PPI5 | ID26 | Hypervisor 定时器 | 电平触发 |
| PPI6 | ID25 | 虚拟维护中断 | 电平触发 |

### 23.4.3 SPIs（共享外设中断）
- 由外部中断输入线触发
- 最多 256 个 SPI，从 **ID32** 开始
- 可配置为边沿触发或高电平触发

### 23.4.4 中断优先级格式
Cortex-A7 使用 **5 位优先级字段**，共 32 级优先级。

## 23.5 GIC 分发器（GICD）

GICD 集中管理所有中断源，确定优先级，向 CPU 接口转发最高优先级中断。

GICD 提供以下编程接口：
- **全局使能**中断转发到 CPU 接口
- **使能/禁止**单个中断
- **设置**每个中断的**优先级**
- **设置**每个中断的**目标处理器**
- **设置**电平触发或边沿触发
- **设置** Group 0 或 Group 1
- **发送 SGI** 到一个或多个目标处理器
- **查看**每个中断的**状态**
- **软件置位/清除**外设中断的**挂起状态**

寄存器访问宽度：
- `GICD_IPRIORITYRx`、`GICD_ITARGETSRx`、`GICD_CPENDSGIRx`、`GICD_SPENDSGIRx` —— 支持字节/半字访问
- 其余寄存器 —— 仅支持字访问

### GICD_CTLR（控制寄存器）
- 偏移：0x000
- 复位值：0x0000_0000
- 只有低 2 位有效：
  - bit1 = **ENABLEGRP1**（使能 Group 1 中断转发）
  - bit0 = **ENABLEGRP0**（使能 Group 0 中断转发）
  - bit31~2 —— 保留
