# GIC 寄存器要点总结

## GICD_CTLR — Group 使能开关

- **ENABLEGRP0**（bit0）：使能 Group 0 中断转发到 CPU 接口（安全/FIQ）
- **ENABLEGRP1**（bit1）：使能 Group 1 中断转发到 CPU 接口（非安全/IRQ）

ENABLEGRP0 只能由 Secure 状态访问，ENABLEGRP1 非安全代码也能访问。

**CTLR / CTLRNS** — 同一物理地址，通过总线安全属性判断访问来源：
- Secure 访问 → CTLR，所有位可读写
- Non-Secure 访问 → CTLRNS，只可读写 ENABLEGRP1

非安全裸机环境：全 Group1，只使能 ENABLEGRP1。

## IGROUPRx — 中断分组

每个中断 ID（0-287）占 1 bit：
- 0 = Group 0（安全）
- 1 = Group 1（非安全）

## ISENABLERx / ICENABLERx — 中断使能/除能

- ISENABLERx：写 1 使能对应中断（读回 1 表示已使能）
- ICENABLERx：写 1 除能对应中断（读回 1 表示已除能）

每个中断 1 bit，共 288 个中断。

## ISPENDRx / ICPENDRx — 挂起置位/清除

- ISPENDRx：写 1 置位挂起（手动触发中断）
- ICPENDRx：写 1 清除挂起（CPU 没响应就清掉，不会送给 CPU）

写 ISPENDR 不管中断是否使能都有效，使能只决定是否 forward 给 CPU。

**限制**：SGIs(ID0-15) 不能通过 ISPENDR 置位，只能通过 GICD_SGIR 触发。PPIs(ID16-31) 由外设硬件置位。

## ISACTIVERx / ICACTIVERx — Active 状态控制

中断状态机：`Inactive → Pending → Active → Inactive`

CPU 读 IAR 后，硬件自动将中断从 Pending 转为 Active。
- ISACTIVERx：手动将中断设为 Active（模拟 CPU 已读 IAR）
- ICACTIVERx：手动清除 Active

**不会触发中断**，只管状态标志。手动触发中断只能写 ISPENDR。

## IPRIORITYRx — 中断优先级

每个中断 1 字节，Cortex-A7 只用高 5 位（bits[7:3]），共 32 级。

**值越小优先级越高**。

## ITARGETSRx — 目标处理器

每个中断 1 字节，每 bit 对应一个 CPU：
- bit0 = CPU0，bit1 = CPU1
- 同时写 bit0+bit1 = 发给两个核

SGIs/PPIs 的 ITARGETSR 是硬件固定的，不能修改。只有 SPIs(ID32+) 可配。

## ICFGRx — 触发配置

每中断 2 bit：
- **00 = 电平触发（Level-sensitive）**：外设一直保持高电平，Pending 就持续；电平变低 Pending 自动消失。写 EOIR 时如果电平还高会立即再次触发。
- **01 = 边沿触发（Edge-triggered）**：检测到上升沿就锁存 Pending，外设信号变低也不影响。读 IAR+写 EOIR 后硬件自动清除。

## GICD_SGIR — 软件中断

### TARGETLISTFILTER（bit25:24）

| Filter | 含义 |
|--------|------|
| 0b00 | 发给 CPUTARGETLIST 指定的 CPU（位图，可以是多个） |
| 0b01 | 发给除自己外的所有 CPU |
| 0b10 | 仅发给自己 |

CPUTARGETLIST（bit17:16）在 filter=0b00 时有效，每 bit 对应一个 CPU。全 0 则不发给任何人。

"请求中断的 CPU" = 写 SGIR 的那个 CPU，由 TARGETLISTFILTER 决定是否排除自己。

### NSATT（bit15）
- 0 → 只有分配给 Group 0 的中断才会 forward
- 1 → 只有分配给 Group 1 的中断才会 forward

### SPENDSGIR / CPENDSGIR
和 ISPENDR/ICPENDR 对应，仅适用于 SGIs(ID0-15)。

## GICD 地址映射

- **GICD（Distributor）**：全局共享，两个 CPU 读写同一份
- **GICC / GICH / GICV**：每个 CPU 私有，地址相同但总线按源 CPU 自动路由
