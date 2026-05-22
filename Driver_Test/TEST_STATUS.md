# Driver Test Status

## USART4

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 初始化配置 | 通过 | 115200/8/N/1，GPIOG11(AF6)=TX, GPIOB2(AF8)=RX |
| UsartWrite 发送 | 通过 | 字符串、整数格式化输出正常 |
| UsartReadOne 单字节读取 | 通过 | 轮询接收，串口助手交互正常 |
| UsartCfg 配置 | 通过 | 波特率、数据位、停止位、校验位配置正确 |
| 串口菜单交互 | 通过 | TestMenu_Run 循环打印菜单、接收选择、返回正确 |

## GPIO (Z5/Z6/Z7 LED, A0 按键)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 时钟使能 | 通过 | RCC 0x50000000+0x210 bit0 使能 GPIOZ |
| GpioMode 输出模式 | 通过 | Z5/Z6/Z7 配置为 GPIO_MODER_OUTPUT |
| GpioOtype 推挽输出 | 通过 | 配置为 GPIO_OTYPE_PUSH_PULL |
| GpioOspeed 高速 | 通过 | 配置为 GPIO_OSPEED_HI |
| GpioMode 输入模式 | 通过 | A0 配置为 GPIO_MODER_INPUT |
| GpioOutHi/GpioOutLow | 通过 | Z6/Z7 交替高低电平，LED 闪烁正常 |
| GpioToggle 翻转 | 通过 | Z5 随按键翻转 |
| GpioInData 输入检测 | 通过 | 轮询 A0 引脚电平，低电平有效 |
| 按键 + LED 联动 | 通过 | 按下 A0，Z5 翻转；Z6/Z7 持续交替闪烁 |
| 延时退出 | 通过 | 任意按键退出测试 |

## IWDG

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 配置 | **已修改** | DIV_128 (div=5), RL=2499，溢出时间 ~10.0s |
| 初始化 | 通过 | 开机自动 IwdgInit，DIV_128+RL=2499，10s 超时 |
| Kick test | 通过 | ~5s 喂狗一次，打印计数，任意键退出正常 |
| Reset test | 通过 | 停止喂狗后 ~10s 触发复位 |
| 二级菜单交互 | 通过 | 主菜单选 6 进入，1/2/0 功能正常 |

## SPI

| 测试项 | 状态 | 说明 |
|--------|------|------|
| SPI4 时钟使能 | 通过 | RCC MP_APB2ENSETR bit9，GPIOE11-14 AF5 |
| SpiCfg 初始化 | 通过 | GPIOE11-14 AF5，波特率分频 6，CPOL=0/CPHA=1，全双工，主机模式，MSB 先，8 位字长 |
| SS 软件管理 (SSM=1+SSI=1) | 通过 | 避免 MODF（Flash CS 内部上拉导致低电平→主机模式故障），通过 CFG2 bit29(SSOE) 驱动引脚 |
| SpiTxRx 全双工通信 | 通过 | 8-bit 访问 TXDR/RXDR 避免 packing（STRB 指令代替 STR），CSTART 前预写首字节防 UDR |
| 无条件 RX FIFO 弹出 | 通过 | rd_buf=NULL 时也读取 RXDR，避免 RxFIFO 满导致 EOT 永不置位 |
| 传输完成等待 (EOT) | 通过 | 等待 SR bit3 后清除 IFCR、关闭 SPI |
| 波特率配置 (CFG1 bit30:28) | 通过 | SPI_CLK = APB2 / (2 × div) |
| 字长配置 (DSIZE=7) | 通过 | CFG1 bit4:0=7（8 位），FTHLV=0（每帧一包） |
| 帧间隔 (MIDI=10) | 通过 | CFG2 bit7:4=10，16 位模式间隔 10 SCK |
| SS 有效到首数据延迟 (MSSI=8) | 通过 | CFG2 bit3:0=8，SS 有效后插入 8 SCK |

### W25Q64 Flash Test

| 测试项 | 状态 | 说明 |
|--------|------|------|
| JEDEC ID 读取 | 通过 | 0xEF 0x40 0x17（W25Q64） |
| 状态寄存器读取 (RDSR1) | 通过 | 2 字节事务，返回 rx[1] |
| 写使能 + 扇区擦除 | 通过 | 0x20 命令 + 3 字节地址，WaitBusy 轮询 |
| 页编程 (256 字节) | 通过 | 0x02 命令 + 3 字节地址 + 256 数据，Verify 0..255 通过 |
| 页读取 (256 字节) | 通过 | 0x03 命令 + 3 字节地址 + 256 哑字节，数据正确 |

### LED Display Test

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 74HC595 移位寄存器驱动 | 通过 | 2 字节 SPI 事务（位选 + 段码），4 位滚动显示 |
| 滚动显示 | 通过 | 16 进制数 0-F 循环滚动，任意键停止 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| MODF 主机模式故障 | SS 引脚被 Flash CS 电阻拉低，MODF 检测到低电平 | SSM=1 + SSI=1 软件管理 |
| SPI 数据偏移 6 字节 | TXDR 为 uint32_t，编译器生成 STR 指令→4 帧 packing | 8-bit 指针访问 (STRB)，每帧单包 |
| ReadData 系统复位 | 栈溢出：FlashTest(~560B) + W25Q_ReadData(~536B) > 1KB SYS 栈 | start.S 栈从 2KB 扩大到 4KB |
| SpiTxRx 卡死 (rd_buf=NULL) | RxFIFO 未弹出→FIFO 满→SPI 停滞→EOT 永不置位→看门狗复位 | 无条件读取 RXDR 再判断 rd_buf |

## I2C

| 测试项 | 状态 | 说明 |
|--------|------|------|
| I2C 初始化 | 待测 | PF14(AF5)=SCL, PF15(AF5)=SDA, OD, I2C1 |
| EEPROM (0xA0) 写入 | 待测 | 0..31 写入地址0，0xAA 批量写入地址32 |
| EEPROM (0xA0) 读取 | 待测 | 回读校验，数据比对 |
| Sensor (0x80) 温度读取 | 待测 | 命令 0xE3，读 2 字节，计算公式 17572×raw/65536-4685 |
| Sensor (0x80) 湿度读取 | 待测 | 命令 0xE5，读 2 字节，计算公式 125×raw/65536-6 |
| 二级菜单交互 | 待测 | 1=EEPROM, 2=Sensor, 0=返回 |

## DTS

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 时钟使能 | 通过 | APB3ENSETR bit16 + APB3RSTCLRR bit16 |
| PCLK 模式 | 通过 | SMP_TIME=8，MFREQ≈1620，温度≈35°C（PCLK 频率待核实） |
| LSE 模式 | 通过 | SMP_TIME=15，MFREQ≈236，温度≈49°C |
| 主菜单入口 | 通过 | 按 7 进入 DTS 测试 |

- [ ] 代码未写

## DMA

- [ ] 代码未写

## STGEN

| 测试项 | 状态 | 说明 |
|--------|------|------|
| STGENC 控制接口 (0x5C008000) | 阻塞 | 同步外部数据中止 (DFSR=0x1008)，APB5 总线返回错误 |
| STGENR 只读接口 (0x5A005000) | 阻塞 | 同上，总线无设备响应 |
| RCC 时钟使能 | 无效 | APB4ENSETR/APB5ENSETR bit20 已置位，复位已释放，仍无法访问 |
| CP15 CNTPCT 替换方案 | 失败 | CNTPCT 底层仍依赖 STGEN，STGEN 未使能时计数器不工作 |
| **结论** | **跳过** | STGEN 地址被 ETZPC 隔离，A7 侧不可访问，CP15 也无法读数。代码保留但无法测试 |
