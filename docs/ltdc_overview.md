# LTDC 显示控制器

RM0436 Rev 7, Chapter 35, Pages 1771-1804

## 35.1 Introduction

LCD-TFT 显示控制器提供并行数字 RGB 输出及 HSYNC/VSYNC/DE/CLK 同步信号，可直接连接各种 LCD/TFT 面板。

## 35.2 LTDC 主要特性

- 24-bit RGB 并行像素输出 (RGB888)
- 2 个显示层，每层独立 FIFO (64x64-bit)
- CLUT 颜色查找表，每层最多 256 色 (256x24-bit)
- 可编程显示时序，适配不同面板
- 可编程背景色
- 可编程 HSYNC/VSYNC/DE 极性
- 每层支持 8 种输入颜色格式:
  - ARGB8888, RGB888, RGB565, ARGB1555, ARGB4444
  - L8 (8-bit 亮度或 CLUT 索引)
  - AL44 (4-bit alpha + 4-bit 亮度)
  - AL88 (8-bit alpha + 8-bit 亮度)
- 伪随机抖动输出 (每通道最多 2-bit)
- 两层 alpha 混合 (逐像素 alpha / 常量 alpha)
- Color keying (颜色透明)
- 可编程窗口位置和大小
- AXI master 接口，支持 16 double-word burst
- 最多 4 个可编程中断事件

## 35.3 功能描述

### 35.3.1 框图 (Figure 279)

LTDC 框图: AXI master → Layer1 FIFO + Layer2 FIFO → PFC(像素格式转换) → Blending unit → Dithering → LCD_RGB/CLK 输出; Timing generator 产生 HSYNC/VSYNC/DE; APB slave 配置寄存器。

### 35.3.2 引脚

外部引脚:

| 信号 | 类型 | 描述 |
|------|------|------|
| LCD_CLK | Output | 像素时钟输出 |
| LCD_HSYNC | Output | 水平同步 |
| LCD_VSYNC | Output | 垂直同步 |
| LCD_DE | Output | 数据使能 |
| LCD_R[7:0] | Output | 红色数据 (8-bit) |
| LCD_G[7:0] | Output | 绿色数据 (8-bit) |
| LCD_B[7:0] | Output | 蓝色数据 (8-bit) |

注: 若接更低色深屏 (如 RGB565 16bit)，数据线连到 MSB: R[4:0]→R[7:3], G[5:0]→G[7:2], B[4:0]→B[7:3]

内部信号:

| 信号 | 类型 | 描述 |
|------|------|------|
| ltdc_aclk | Input | LTDC AXI 时钟 |
| ltdc_ker_ck | Input | LTDC 内核时钟 (用于 LCD_CLK 生成) |
| ltdc_pclk | Input | LTDC APB 时钟 (寄存器访问) |
| ltdc_it | Output | LTDC 全局中断请求 |
| ltdc_err_it | Output | LTDC 全局错误中断请求 |

### 35.3.3 复位和时钟

3 个时钟域:

| 时钟域 | 功能 |
|--------|------|
| ltdc_aclk | AXI master 接口: 从内存读数据到 layer FIFO + 帧缓冲配置寄存器 |
| ltdc_pclk | APB 总线: 全局配置寄存器 + 中断寄存器 |
| ltdc_ker_ck | 像素时钟域: 像素生成、图层配置、接口信号发生器 → LCD_CLK |

寄存器时钟域分布:

| 时钟域 | 寄存器 |
|--------|--------|
| ltdc_aclk | LxCR, LxCFBAR, LxCFBLR, LxCFBLNR, SRCR |
| ltdc_pclk | IER, ISR, ICR |
| ltdc_ker_ck | SSCR, BPCR, AWCR, TWCR, GCR, BCCR, LIPCR, CPSR, CDSR, LxWHPCR, LxWVPCR, LxCKCR, LxPFCR, LxCACR, LxDCCR, LxBFCR, LxCLUTWR |

寄存器访问延迟 (Table 244):

| 访问类型 | AXI 域 | APB 域 | Pixel 域 |
|----------|--------|--------|----------|
| 读 | 7×pclk + 5×aclk | 7×pclk | 7×pclk + 5×ker_ck |
| 写 | 6×pclk + 5×aclk | 6×pclk | 6×pclk + 5×ker_ck |

LTDC 可通过 RCC 对应复位位置位来复位三个时钟域。

## 35.4 可编程参数

### 35.4.1 同步时序 (Figure 280)

时序参数:

| 参数 | 含义 | 单位 |
|------|------|------|
| HSW | HSYNC 脉宽 | LCD_CLK 周期 |
| HBP | 水平后肩 (HSYNC→有效数据) | LCD_CLK 周期 |
| HFP | 水平前肩 (有效数据→下一行HSYNC) | LCD_CLK 周期 |
| VSW | VSYNC 脉宽 | 行数 |
| VBP | 垂直后肩 | 行数 |
| VFP | 垂直前肩 | 行数 |
| Active Width | 每行有效像素数 | 像素 |
| Active Height | 有效行数 | 行 |

寄存器配置 (全部用**累积值减1**):

| 寄存器 | 低12位 | 高12位 |
|--------|--------|--------|
| SSCR | HSW - 1 | VSW - 1 |
| BPCR | HSW + HBP - 1 | VSW + VBP - 1 |
| AWCR | HSW + HBP + ActiveW - 1 | VSW + VBP + ActiveH - 1 |
| TWCR | Total Width - 1 | Total Height - 1 |

Total Width = HSW + HBP + ActiveW + HFP
Total Height = VSW + VBP + ActiveH + VFP

示例 (320x240): HSW=10, HBP=20, HFP=10, VSW=2, VBP=2, VFP=4
- SSCR = 0x00090001
- BPCR = 0x001D0003
- AWCR = 0x015D00F3
- TWCR/THCR 分别存总宽/总高

可编程极性 (GCR): HSYNC/VSYNC/DE/CLK 可选 active high 或 active low。

### 背景色 (BCCR)

RGB888 格式常量背景色，用于与底层混合。

### 抖动 (Dithering)

伪随机 LFSR 技术，将 24bit 数据输出到 18bit 面板时减少色带。每通道 2bit 抖动宽度，可通过 GCR 随时开关。LTDC 使能后 LFSR 从第一个有效像素开始运行。

### 影子寄存器重载

Layer1/Layer2 的所有寄存器 (除 LxCLUTWR) 均为影子寄存器。写入后通过 SRCR 触发重载:
- SRCR=0x01: 立即重载
- SRCR=0x10: 垂直消隐期重载 (防撕裂)

重载期间不可再次修改影子寄存器。

### 35.4.2 图层可编程参数

最多 2 层，顺序固定 bottom-up (Layer2 在 Layer1 上)。

窗口定位:
- LxWHPCR: WHSTPOS/WHSPPOS (水平起止像素)
- LxWVPCR: WVSTPOS/WVSPPOS (垂直起止行)

窗口必须在有效显示区内，可实现帧裁剪或分屏。

坐标系: 原点 (0,0) 为有效显示区左上角，X 向右，Y 向下。

计算公式:
- 窗口宽度 = WHSPPOS - WHSTPOS + 1
- 窗口高度 = WVSPPOS - WVSTPOS + 1
- WHSTPOS = 窗口左边界 X 坐标
- WHSPPOS = 窗口右边界 X 坐标
- WVSTPOS = 窗口上边界 Y 坐标
- WVSPPOS = 窗口下边界 Y 坐标

### 像素输入格式 (Table 245)

每层通过 LxPFCR 选择格式，硬件自动展开为内部 ARGB8888。

位扩展规则: <8bit 的通道通过位复制扩展到 8bit。如 RGB565 的 5bit 红色: 43210 → 43210432, 取高 8 位。

内存排列 (小端):

| 格式 | 每像素 | 字节排列 |
|------|--------|---------|
| ARGB8888 | 4B | @+0=B, @+1=G, @+2=R, @+3=A |
| RGB888 | 3B | @+0=B, @+1=G, @+2=R (无 A) |
| RGB565 | 2B | 低字节 G[2:0]B[4:0], 高字节 R[4:0]G[5:3] |
| ARGB1555 | 2B | 低字节 G[2:0]B[4:0], 高字节 A[0]R[4:0]G[4:3] |
| ARGB4444 | 2B | 低字节 G[3:0]B[3:0], 高字节 A[3:0]R[3:0] |
| L8 | 1B | 单字节亮度/CLUT 索引 |
| AL44 | 1B | 高半字节 A[3:0], 低半字节 L[3:0] |
| AL88 | 2B | 低字节 L[7:0], 高字节 A[7:0] |

### Color Look-Up Table (CLUT)

用于 L8/AL44/AL88 索引颜色模式，每层最多 256 色 (256x24-bit RGB)。

加载方式: 通过 LxCLUTWR 寄存器逐项写入:
```
LxCLUTWR = (地址 << 24) | (R << 16) | (G << 8) | B
```

不同模式的 CLUT 加载:
- L8/AL88: 256 色, 地址 0x00~0xFF
- AL44: 16 色, 地址按 4bit L 复制填充 (L0→0x00, L1→0x11, ..., L15→0xFF)

### 帧缓冲参数

- LxCFBAR: 帧缓冲起始地址 (DDR 地址)
- LxCFBLR: 低位 = 一行长度(字节), 高位 = pitch (行间跨度, 字节)
- LxCFBLNR: 帧缓冲总行数

行长度设太短 → FIFO underrun; 设太长 → 多余数据丢弃。

### 图层混合 (Figure 282)

顺序固定: BG → Layer1 → Layer2
- LxBFCR: 混合因子寄存器
- LxDCCR: 默认色 (ARGB), 窗口外或层禁用时显示
- LxCACR: 常量 alpha (当使用常量 alpha 而不是逐像素 alpha 时)

### Color Keying (LxCKCR)

设一个 RGB 透明色，像素匹配后 ARGB 全清 0。比较在格式转换到 ARGB8888 后进行，所以 LxCKCR 的值始终是 RGB888 格式。

## 35.5 LTDC 中断 (Table 247)

4 个可屏蔽中断事件，映射到两个中断向量:

| 中断事件 | 向量 | 说明 |
|----------|------|------|
| Line interrupt | ltdc_it | 到达 LIPCR 设定的行号时触发 |
| Register reload interrupt | ltdc_it | 影子寄存器重载完成 (垂直消隐期) |
| FIFO underrun interrupt | ltdc_err_it | 层 FIFO 为空但仍有像素请求 |
| Transfer error interrupt | ltdc_err_it | AXI 总线读错误 |

通过 IER 寄存器使能/屏蔽各中断，ISR 查询状态，ICR 清除。

### 中断事件 (Figure 283, Table 246)

| 中断源 | 标志位 | 使能位 | 向量 |
|--------|--------|--------|------|
| Line interrupt | LIF | LIE | ltdc_it |
| Register reload interrupt | RRIF | RRIEN | ltdc_it |
| FIFO underrun | FUDERRIF | FUDERRIE | ltdc_err_it |
| Transfer error | TERRIF | TERRIE | ltdc_err_it |

## 35.6 LTDC 编程步骤

1. RCC 使能 LTDC 时钟
2. 配像素时钟 (PLL 输出, 按面板规格)
3. 配同步时序: HSW/HBP/HFP/VSW/VBP/VFP (SSCR/BPCR/AWCR/TWCR)
4. GCR 配极性: HSYNC/VSYNC/DE/CLK
5. BCCR 配背景色
6. IER + LIPCR 配中断
7. 配置 Layer1/2:
   - 窗口位置 (LxWHPCR/LxWVPCR), 必须在有效显示区内
   - 像素输入格式 (LxPFCR)
   - 帧缓冲起始地址 (LxCFBAR)
   - 行长 + pitch (LxCFBLR)
   - 帧缓冲行数 (LxCFBLNR)
   - CLUT 加载 (LxCLUTWR)
   - 默认色 + 混合因子 (LxDCCR / LxBFCR)
8. LxCR 使能图层及 CLUT
9. GCR 使能抖动, LxCKCR 使能 color keying
10. SRCR 触发影子寄存器重载
11. GCR 使能 LTDC 控制器
12. 后续改参数 → 重新 SRCR 重载
