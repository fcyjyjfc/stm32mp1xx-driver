---
name: BTIMER basic timer knowledge
description: STM32MP15x 基本定时器 TIM6/TIM7 特性总结，用于参考和驱动设计
type: reference
originSessionId: 14bc5778-da7e-4305-b39b-9b092720d5d5
---
# BTIMER (TIM6/TIM7) 基本定时器总结

来源于用户对 STM32MP157 基本定时器手册的阅读总结。

## 硬件特性

1. **PSC 分频**：对输入 CLK 进行 1~65536 分频，得到计数时钟 CK_CNT
2. **16 位计数器**：16 位可自动加载的递增计数器
3. **单次/连续计数**：OPM 位可选择单脉冲模式（一次）或连续计数
4. **ARR 自动重载**：连续计数时，更新事件产生时从 ARR 加载新的计数值
5. **更新事件来源**：计数溢出时自动产生，或软件写 UG 位主动产生
6. **ARR 阴影寄存器**：
   - ARPE=1（启用阴影）：ARR 值等到更新事件发生时才更新，当前周期计数值不变
   - ARPE=0（不启用）：ARR 值立刻生效，当前周期计数立即改变
7. **中断和 DMA**：可产生更新中断（UIE）和 DMA 请求（UDE）
8. **TRGO 输出**：可作为 ADC 采样时钟（需配 CR2.MMS）

## 软件注意事项

- **UIFREMAP**（CR1 bit11）：可将 UIF 标志映射到 CNT bit31，读 CNT 时可同时获得 UIF 状态
- **UG 与 URS 关系**：URS=1 时软件写 UG 不会产生更新事件，ARR/PSC 不会被更新
- **UIF 清除**：写 0 到 SR.0 清除 UIF
- **PSC/ARR 配置**：配前应先停 CEN，配完再启
- **时钟使能**：TIM6/TIM7 在 APB1，`RCC->MP_APB1ENSETR`，TIM6=bit4，TIM7=bit5

## 寄存器结构

| 偏移 | 寄存器 | 说明 |
|------|--------|------|
| 0x00 | CR1 | 控制寄存器1（CEN, UDIS, URS, OPM, ARPE, UIFREMAP） |
| 0x04 | CR2 | 控制寄存器2（MMS, TRGO 输出） |
| 0x0C | DIER | 中断/DMA 使能（UIE, UDE） |
| 0x10 | SR | 状态寄存器（UIF） |
| 0x14 | EGR | 事件生成寄存器（UG） |
| 0x24 | CNT | 计数器值 |
| 0x28 | PSC | 预分频器 |
| 0x2C | ARR | 自动重载值 |
