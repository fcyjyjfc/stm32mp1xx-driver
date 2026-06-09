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

## I2C

| 测试项 | 状态 | 说明 |
|--------|------|------|
| I2C 初始化 | 通过 | PF14(AF5)=SCL, PF15(AF5)=SDA, OD, I2C1, TIMINGR=0x10707dbc |
| EEPROM (0xA0) 写入 | 通过 | AT24C_Write: 0..31→addr0, 0xAA→addr32, 32 字节/页 |
| EEPROM (0xA0) 读取 | 通过 | AT24C_Read: 回读校验通过 |
| Sensor (0x80) 温度读取 | 通过 | 命令 0xE3, WRITE+RESTART+READ, 26.80°C |
| Sensor (0x80) 湿度读取 | 通过 | 命令 0xE5, WRITE+RESTART+READ |
| 二级菜单交互 | 通过 | 1=EEPROM, 2=Sensor, 0=返回 |
| 器件分层 | 通过 | AT24CXX 器件驱动移至 devices/，I2cMstWrite/Read 为通用 I2C 接口 |
| AUTOEND 改手动 STOP | 通过 | 读/写函数统一禁止 AUTOEND，手动发 STOP，解决 RESTART 前多余 STOP 问题 |
| 单次读写 >255 字节 | 通过 | 919 字节写入（I2cMstWrite 921 字节含 RELOAD）+ AT24C_Read 回读校验通过 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| RESTART 前出现多余 STOP | AUTOEND 在 RESTART 场景中先于 START 设置，导致控制器在 NBYTES 传输完成后自动生成 STOP，再发起 RESTART，形成 STOP+RESTART 而非纯 RESTART | I2cMstWrite/I2cMstRead 统一禁止 AUTOEND，等待 TC 后手动发 STOP |
| RELOAD 段尾 TC 不置位 | RELOAD→非 RELOAD 过渡时 CR2 分多次写入（先改 RELOAD、再清 NBYTES、再设 NBYTES），中间态 RELOAD=0 & NBYTES=0 可能使硬件误清除 TC | CR2 一次性写 RELOAD + NBYTES + AUTOEND + START，消除中间态 |

## DTS

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 时钟使能 | 通过 | APB3ENSETR bit16 + APB3RSTCLRR bit16 |
| PCLK 模式 | 通过 | SMP_TIME=8，MFREQ≈1620，温度≈35°C（PCLK 频率待核实） |
| LSE 模式 | 通过 | SMP_TIME=15，MFREQ≈236，温度≈49°C |
| 主菜单入口 | 通过 | 按 7 进入 DTS 测试 |

- [ ] 代码未写

## DAC

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 基本输出 (SW 触发 + ADC2 内部回读) | 通过 | 6 组值 0/512/1024/2048/3072/4095，校准前后各一次 |
| 三角波 (ch2, TIM6 触发, DHR=1000, MAMP=2047) | 通过 | 4 周期，ADC2 每 256 步采样，波形正常 |
| LFSR 噪声 (ch2, TIM6 触发, DHR=1000, MAMP=2047) | 通过 | 任意键停止，ADC2 采样值随机分布 |
| 采样保持 (ch2, mode7, 长期保持 ~10s) | 通过 | THOLD=1 TREFRESH=30，20 次双采样波动<100，触发后值跳变>1500 |
| 双通道同步 (DHR12RD + ADC2 双通道回读) | 通过 | 4 组值对，单次写入 DHR12RD 同时更新两路 |
| sin/cos 双通道同步波形 (mode1, TIM6 触发) | 通过 | 16 点/周期 × 2，幅值 4000pp，ADC2 每 4 步回读，波峰偏差<2，波谷偏差~33 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| S&H 保持值异常 | SHHR/SHRR 在 EN=1 时写入被硬件忽略 | DacSampleHoldCfg 在 DacCfg(en=0) 之后、DacCfg(en=1) 之前调用 |
| TEN=1 模式下 DHR 写入不触发采样 | RM 描述与实测不符，触发模式下仅 SWTRGR/硬件触发有效 | 显式调用 DacSoftTrig 触发，DacWriteDhr 仅更新寄存器 |
| sin/cos 首点输出为 0 | DacWriteDualDhr 在 TIM6 启动后才写入，首个触发输出复位值 | 在 BasicTimerStart 前写入初始 DHR 值 |

## DMA

- [ ] 代码未写

## BTIMER

| 测试项 | 状态 | 说明 |
|--------|------|------|
| TIM6 时钟使能 | 通过 | RCC->MP_APB1ENSETR bit4 |
| BasicTimerCfg 配置 | 通过 | psc=15999, arr=5000, arpe=1, opm=0, urs=0, udis=0 |
| BasicTimerStart/Stop | 通过 | CEN 位单独启停，退出时自动 Stop |
| UIF 轮询 | 通过 | 检测 SR.0，清 UIF 后打印计数、翻转 GPIOZ6/Z7 LED |
| 串口退出 | 通过 | 收到任意字符即停止 |
| 主菜单入口 | 通过 | 按 8 进入 BTIMER 测试 |

## IRQ

| 测试项 | 状态 | 说明 |
|--------|------|------|
| TIM6 中断 (register + ISR 分发) | 通过 | GIC 使能、注册 ISR、GiccInit/GicdInit 正确，TIM6 ISR 翻转 Z6 |
| EXTI0 下降沿中断 | 通过 | PA0 按键下降沿触发，ISR 打印 "PA0 Pressed!"、翻转 Z7 |
| EXTI0 双边沿中断 | 通过 | FPR/RPR 分别处理按下/释放，打印 Pressed!/Released!，Z7 亮/灭 |
| 中断优先级抢占 (TIM6/TIM7) | **未通过** | cpsie i / MRS/MSR 均无法在 IRQ 模式下重新使能 IRQ，嵌套未发生 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| GICD 寄存器偏移错误 | GICD 基址 0xA0021000，寄存器偏移从 0x004 开始 | 修正 GicdRegs 结构体偏移 |
| GiccInit PMR 导致 TIM6 被屏蔽 | PMR=80，TIM6 priority 10<<3=80，掩码等于优先级 | PMR 改为 0xFF |
| 抢占测试：ISR 内重新使能 IRQ | cpsie i 已编译通过但未生效，MRS/MSR 方式同样无效 | 暂时标记为未完成，待进一步定位硬件层次问题 |
| 抢占测试 start.S 嵌套修改导致跑飞 | start.S 中 MRS/MSR 操作 CPSR 后无 ISB，SPSR 被嵌套覆盖 | 回退 start.S 为简单版本 |

| 测试项 | 状态 | 说明 |
|--------|------|------|
| STGENC 控制接口 (0x5C008000) | 阻塞 | 同步外部数据中止 (DFSR=0x1008)，APB5 总线返回错误 |
| STGENR 只读接口 (0x5A005000) | 阻塞 | 同上，总线无设备响应 |
| RCC 时钟使能 | 无效 | APB4ENSETR/APB5ENSETR bit20 已置位，复位已释放，仍无法访问 |
| CP15 CNTPCT 替换方案 | 失败 | CNTPCT 底层仍依赖 STGEN，STGEN 未使能时计数器不工作 |
| **结论** | **跳过** | STGEN 地址被 ETZPC 隔离，A7 侧不可访问，CP15 也无法读数。代码保留但无法测试 |

## ADC

| 测试项 | 状态 | 说明 |
|--------|------|------|
| ADC2 内部通道单次转换 (TIM6 触发) | 通过 | VSENSE/VREFINT/VDDCORE/VBAT/DAC1/DAC2，12bit 单次 |
| 连续模式 (软件触发) | 通过 | CONT=1，无 AUTDLY，有溢出风险，OVR 测试通过 |
| 软件触发单次模式 | 通过 | ADSTART=1 触发一轮后硬件自动停 |
| 间断模式 (DISCEN=2) | 通过 | 每次触发转 2 个通道，序列完成后重置 |
| OVR 阻塞演示 (PRESERVE) | 通过 | 不读 DR，FIFO 满后 OVR 置位，丢弃新数据 |
| 连续 + AUTDLY | 通过 | CONT=1 + AUTDLY=1，每次等待 DR 读取，无 OVR |
| 注入转换 (EXTI0 触发, OS=1024x) | 通过 | JEXTEN 下降沿触发 INJ 序列，过采样 1024x 右移 10 |
| 自动注入 (JAUTO=1, TIM6 触发) | 通过 | 常规序列完成后硬件自动开始注入序列 |
| 模拟看门狗 AWD1+AWD2 | 通过 | AWD1 1V~2V / AWD2 1.2V~1.8V，旋钮调节超范围置标志 |
| 双 ADC 常规同步 (DUAL=REG_SIMULT) | 通过 | ADC1 ch1 外部电位器 + ADC2 VREFINT，DUAL=0x06 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| JOVSE=1 + CONT=1 串口刷屏看不到注入 | 常规转换太快，注入结果被淹没 | 降低常规触发频率或存结果验证 |
| DUAL 组合模式误用导致 ADC1 读值异常 | `ADC_DUAL_REG_SIMULT=0x01` 实际是"常规+注入同步"组合模式 | 修正为 0x06（纯常规同步） |
