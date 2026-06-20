# STM32MP157 Bare-Metal Driver Project

STM32MP157 Cortex-A7 裸机外设驱动库。不依赖 HAL/Linux，从寄存器层直接操作硬件。项目目标是开发一套精简、可复用的驱动层，`Driver_Test` 工程用于验证每个驱动的正确性。

## 硬件平台

- MCU: STM32MP157 (Cortex-A7 dual core)
- 运行模式: Bare-metal (无 OS，无 Linux)
- 启动方式: U-Boot 加载 bin 到 DDR 执行
- 调试串口: USART4 (115200/8/N/1)

## 目录结构

```
project/
├── drivers/            外设驱动 (寄存器级)
│   ├── include/        驱动头文件 (寄存器定义 + API)
│   └── source/         驱动实现
├── devices/            器件驱动 (基于 drivers 层)
│   ├── include/        器件头文件
│   └── source/         器件实现
├── Driver_Test/        测试工程
│   ├── asm/            启动文件 (start.S)
│   ├── source/         测试源码
│   ├── Makefile        构建脚本
│   └── Driver_Test.lds 链接脚本
├── tools/              辅助工具
│   └── frame_sender.py 串口帧发送工具
└── docs/               文档
```

## 驱动模块

| 模块 | 头文件 | 说明 |
|------|--------|------|
| GPIO | stm32mp1xx_gpio.h | 模式/速度/上下拉/AF 配置 |
| USART | stm32mp1xx_usart.h | 串口收发，DMA TX/RX |
| DMA | stm32mp1xx_dma.h | DMA1/DMA2 + DMAMUX |
| MDMA | stm32mp1xx_mdma.h | Master DMA，32 通道，2D/链表 |
| SPI | stm32mp1xx_spi.h | SPI 主机，全双工 |
| I2C | stm32mp1xx_i2c.h | I2C 主机，RELOAD 支持 |
| ADC | stm32mp1xx_adc.h | ADC1/ADC2，注入/看门狗/双 ADC |
| DAC | stm32mp1xx_dac.h | 双通道，三角波/噪声/S&H |
| TIM | stm32mp1xx_btimer.h | TIM6/TIM7 基本定时器 |
| IWDG | stm32mp1xx_iwdg.h | 独立看门狗 |
| DTS | stm32mp1xx_dts.h | 片上温度传感器 |
| GIC | stm32mp1xx_gic.h | 中断控制器 |
| EXTI | stm32mp1xx_exti.h | 外部中断 |
| RCC | stm32mp1xx_rcc.h | 时钟使能 |

## 器件驱动

| 器件 | 头文件 | 接口 |
|------|--------|------|
| W25QXX Flash | w25qxx.h | SPI NOR Flash |
| AT24CXX EEPROM | at24cxx.h | I2C EEPROM |
| Si7006 温湿度 | si7006.h | I2C 传感器 |
| 74HC595 移位寄存器 | m74hc595.h | SPI LED 显示 |

## 构建

工具链: `arm-none-eabi-gcc` (YAGARTO 4.6.2)

```bash
cd Driver_Test
make clean && make all
```

输出:
- `out/Driver_Test.elf` — ELF 文件
- `out/Driver_Test.bin` — 二进制，U-Boot 加载用
- `out/Driver_Test.map` — 链接器 map (含交叉引用)
- `out/Driver_Test.sym` — 符号表 (按地址排序)
- `out/Driver_Test.dis` — 反汇编

## 运行

1. U-Boot 通过 TFTP/SD 加载 `Driver_Test.bin` 到 DDR
2. 串口连接 USART4，115200/8/N/1
3. 交互式菜单驱动，支持两种输入方式:
   - 直接键入数字选择
   - 帧协议 (`AA 55 LEN CMD DATA XOR`)，配合 `tools/frame_sender.py`

## 测试菜单

```
===== Driver Test Menu =====
1. GPIO
2. SPI
3. I2C
4. DMA
5. STGEN
6. IWDG
7. DTS
8. BTIMER
9. IRQ
10. ADC
11. DAC
12. MDMA
0. Exit
============================
```

各模块含二级子菜单，详见 `Driver_Test/TEST_STATUS.md`。

## 文档

- `docs/mdma_overview.md` — MDMA 手册笔记 (RM0436)
- `docs/mdma_driver.md` — MDMA 驱动 API 使用说明
- `Driver_Test/TEST_STATUS.md` — 各模块测试状态与修复记录
- `tools/README.md` — 工具使用说明
