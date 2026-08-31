# Driver Test Status

## 测试框架

| 项目 | 说明 |
|------|------|
| 启动状态初始化 | 确保每项外设从复位状态开始。重新 F5 仿真或从其他程序跳转时，外设保持上次状态，中断线/标志位可能残留。 |
| 防护策略 | ① start.S 开头 `msr cpsr,#0xdF` (I=1, F=1) 屏蔽 IRQ/FIQ ② `TestCommonInit` 开头 `GicdInit` 禁用+清除所有 SPI 中断挂起 ③ 外设初始化前 RCC 硬件复位 ④ 所有外设 + GIC 配置完成后才 `GicdEnableInt` + `EnableCpuIrq` |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| 第二次 F5 DMA TC 中断残留 | 调试器附着时不复位硬件，DMA 保留上次 TCIF，`UsartDmaRxInit` 使能 TCIE 后立即触发 | RCC AHB2 硬件复位 DMA + GicdInit 清所有 SPI 挂起 + CPSR.I=1 启动 + 中断使能放末尾 |

## USART4

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 初始化配置 | 通过 | 115200/8/N/1，GPIOG11(AF6)=TX, GPIOB2(AF8)=RX |
| UsartCfg 配置 | 通过 | 波特率、数据位、停止位、校验位配置正确 |
| UsartWrite 阻塞发送 | 通过 | 逐字节等待 TXE，字符串输出正常 |
| UsartReadOne 阻塞单字节读取 | 通过 | 轮询 RXNE，串口助手交互正常 |
| DMA TX 全局发送 (PrintDma) | 通过 | 环形缓冲 + TC 中断续传，替代 UsartWrite |
| DMA RX 全局接收 (FramePoll) | 通过 | 循环模式 + 帧协议 (AA 55 LEN CMD DATA XOR)，替代 UsartReadOne |
| 帧协议菜单交互 | 通过 | CMD_MENU 选择、CMD_BACK 返回、CMD_ECHO 回显，frame_sender.py 配合使用 |

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

### DMA TX（全局，DMA2 Stream 0）

| 测试项 | 状态 | 说明 |
|--------|------|------|
| UsartDmaTxInit | 通过 | DMA2 Stream 0, DMAMUX ch8→UART4_TX(64), M→P, 8bit, MINC, TCIE |
| 环形缓冲区 (1024B) | 通过 | head/tail 管理，留 1 字节区分满/空 |
| UsartDmaSend 非阻塞入队 | 通过 | 拷入环形缓冲，DMA 空闲时自动 Kick |
| UsartDmaTxIsr (TC 中断) | 通过 | 推进 tail，有剩余续传，否则关 DMAT |
| PrintDma 自旋重试 | 通过 | 缓冲区满时循环等待，不丢数据 |
| PrintFlush 等待完成 | 通过 | 阻塞直到 busy=0 且 head==tail |
| 全局替换 UsartWrite | 通过 | 所有测试文件的 PRINT 宏统一走 DMA TX |

### DMA RX（全局，DMA2 Stream 1）

| 测试项 | 状态 | 说明 |
|--------|------|------|
| UsartDmaRxInit | 通过 | DMA2 Stream 1, DMAMUX ch9→UART4_RX(63), P→M, 8bit, MINC, CIRC, TCIE |
| 循环模式 + TC 计圈 | 通过 | TC 中断递增 rx_wr_wrap，配合 NDTR 精确定位写指针 |
| UsartDmaRxReadOne | 通过 | 原子读 wr_wrap+NDTR，wrap_diff 判断空/正常/满/溢出 |
| 溢出检测 | 通过 | wrap_diff>1 或 (==1 且 wr>rx_rd) 时置 rx_ovf，sync wrap，仍返回有效字节 |
| UsartDmaRxStop | 通过 | 清 DMAR，关 DMA Stream，排空 RDR |
| 帧协议 (AA 55 LEN CMD DATA XOR) | 通过 | 状态机解析：IDLE→HDR2→LEN→BODY→XOR |
| CMD_MENU (0x01) | 通过 | DATA 作为菜单选项键，替代原 ReadLine 字符输入 |
| CMD_BACK (0x02) | 通过 | 等同输入 "0"，返回上级菜单 |
| CMD_ECHO (0x03) | 通过 | 全局自动回显 DATA 的十六进制，用于连通性测试 |
| FramePoll 非阻塞轮询 | 通过 | 消费 DMA RX 缓冲区，喂帧解析器，溢出时复位解析器 |
| 全局替换 UsartReadOne | 通过 | 所有测试文件退出循环改为 FramePoll(0) |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| DMA TX 乱码 | D-Cache 使 CPU 写入停留在 cache，DMA 从物理 RAM 读到旧数据 | start.S 全量 clean+invalidate L1 D-Cache 后禁用 |
| PrintFlush 死锁 | ADC 注入测试退出时 CPSR.I=1 关全局 IRQ，DMA TC 中断无法触发 | 删除 ADC 测试中的全局 IRQ 禁用 |
| 菜单文本丢失 | 400B 缓冲区不够，PrintDma 满时截断 | 缓冲区扩至 1024B + 自旋重试不丢数据 |
| DMA RX 间歇失效 | RXNE 在 DMAR 置位前已为 1，DMA 请求为边沿触发无法感知 | 先启动 DMA + 置 DMAR，最后清 RXNE 保证下一字节产生 0→1 边沿 |
| 环形缓冲满误判空 | 纯 NDTR 轮询无法区分空和绕了 N 圈 | TC 中断计圈 + wrap_diff 判断，精确检测溢出 |

## BTIMER

| 测试项 | 状态 | 说明 |
|--------|------|------|
| TIM6 时钟使能 | 通过 | RCC->MP_APB1ENSETR bit4 |
| BasicTimerCfg 配置 | 通过 | psc=15999, arr=5000, arpe=1, opm=0, urs=0, udis=0 |
| BasicTimerStart/Stop | 通过 | CEN 位单独启停，退出时自动 Stop |
| BasicTimerClkEnable 接口 | 通过 | 驱动新增接口，PSC=63999 → CNT 1kHz；幻灯片轮播定时用（STGEN 安全世界不可访问） |
| UIF 轮询 | 通过 | 检测 SR.0，清 UIF 后打印计数、翻转 GPIOZ6/Z7 LED |
| UIF 清除封装 | 通过 | BasicTimerClrUif/UifGet: UIF 是 rc_w0 必须写 0 (原 SR|=1 无效), btimer/dac/irq 测试统一改走封装 |
| BasicTimerDelayMs | 通过 | 自包含 1ms 忙等 (PSC=6399/ARR=9 @64MHz, 每次调用重配不依赖静态标志); PHY 复位/协商轮询用 |
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

## SDIO (AP6212 / CYW43438)

硬件配置: SDMMC3, ker_ck=HCLK2(64MHz), PD0(CMD,AF10)+PG15(CLK,AF10)+PF0/4(D0/D1,AF9)+PD5/7(D2/D3,AF10)

### 初始化 & 识别

| 测试项 | 状态 | 说明 |
|--------|------|------|
| GPIO 初始化 | 通过 | PD0/PG15/PF0/PF4/PD5/PD7, AF10/AF9, 上拉 |
| WL_REG_ON 上电时序 | 通过 | PD4 拉高后等待 ~50ms |
| RCC 时钟使能 | 通过 | SDMMC3CKSELR=3(HCLK2), MP_AHB2ENSETR bit16 |
| SdioCardIdentify | 通过 | CMD0 → CMD5(arg=0 探测) → CMD5(poll C=1) → CMD3 → CMD7 |
| NF (I/O 功能数) | 通过 | R4 返回 NF=2 (F1=WiFi, F2=BT) |
| S18A (1.8V 切换) | 通过 | 卡支持 1.8V, 当前在 3.3V 工作 |

### CCCR & CIS

| 测试项 | 状态 | 说明 |
|--------|------|------|
| CCCR Read (0x00~0x14) | 通过 | CMD52 Func=0 读全部 CCCR 寄存器 |
| CMD52 RAW Loopback | 通过 | INT_ENABLE 写+RAW 读回验证 |
| CIS Dump | 通过 | 逐元组扫描, CISTPL_MANFID/CISTPL_SDIO_STD/CISTPL_FUNCE 解析正常 |
| 卡信息 | 通过 | CCCR/SDIO 版本, CIS 指针, 制造商 ID, F1 最大块大小等 |

### I/O 功能使能 & 总线配置

| 测试项 | 状态 | 说明 |
|--------|------|------|
| IO_ENABLE F1 写+轮询 | 通过 | IO_ENABLE=0x02, 轮询 IO_READY bit1 就绪 |
| 4-bit 总线切换 | 通过 | BUS_CTRL bit0=1 + SdmmcSetBusWidth(4BIT) |
| 高速模式 (SDR25) | 通过 | BUS_SPEED=1, 时钟 25MHz |
| CD Disable | 通过 | BUS_CTRL bit7=1 |

### FBR (Function Basic Register) 访问

| 测试项 | 状态 | 说明 |
|--------|------|------|
| F1 CSI (F0 地址空间 0x100) | 通过 | 返回值 0x00 (CSA=0, 无 CMD53 支持声明) |
| F1 CSI (Fn 直接访问, Func=1 addr=0) | 通过 | 返回值 0xA6 (CSA=0, 但 Linux brcmfmac 无视此位) |
| F1 块大小 (F0 地址空间 0x110-0x111) | 通过 | 写 64 回读验证 0x0040 正确 |

### CMD53 数据传输

| 测试项 | 状态 | 说明 |
|--------|------|------|
| byte mode (F1, cnt=4, addr=0) | **未通过** | R5=0x1800 (ERROR + IO_CURRENT_STATE=CMD) |
| block mode (F1, 1×64B, addr=0) | **未通过** | R5=0x1800 (ERROR) + SdmmcReadData DTIMEOUT |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| RESPCMDR 误判 CMD53 为 CMD52 | R5 响应回显命令索引 52 (CMD52 的索引) 而非 53, 导致 SDMMC_ERR_RESP_MISMATCH | 删除 SdmmcCmdPhase 中 RESPCMDR 校验 (sdmmc.c:211~216) |
| CMD53 数据阶段 DTIMER 过长 | DTIMER=0xFFFFFFFF @25MHz ≈ 171s, 加上命令错误后无限等待 DATAEND 导致看门狗复位 | DTIMER=ker_ck_hz/10 (~100ms) + R5 ERROR 提前返回 |
| SdioCardReadMultiB DTMODE 配置错误 | 使用 SDMMC_DCTRL_SDMODE (DTMODE=01 电压切换模式) 而非 DTMODE=00 | 改用 DTMODE=00 + DBLOCKSIZE=0 |

### 未解决问题

- R5=0x1800 (ERROR) 表示卡拒接 CMD53. 可能原因：
  - F1 CSI CSA=0 (但 Linux brcmfmac 无视此位仍用 CMD53 收发所有数据)
  - F1 需额外初始化步骤 (如 F1 内部窗口/backplane 配置, CYW43438 的 SBSDIO 有 4KB 窗口机制)
  - CMD53 参数有误 (地址/计数/功能号等)
  需参照 Linux brcmfmac 源码 (`brcmf_sdiod_ramrw`, `brcmf_sdiod_send_pkt`) 确定正确用法.

## MDMA

| 测试项 | 状态 | 说明 |
|--------|------|------|
| M2M 软件触发 (32KB) | 通过 | MdmaMemcpy 封装，64-bit 数据宽度，16-beat burst，源填 uint16_t 递增 0~16383 |
| Block repeat (128KB) | 通过 | BNDT=65536, BRC=1 (2×64KB)，64-bit，TRGM=REP_BLOCK，源填 0~65535 |
| Linked-list + repeat (5KB) | 通过 | 初始配置 1KB + 3 节点（其中 node[2] 用 BRC=1 做 2×1KB），TRGM=CHANNEL，一次触发 |
| 字节序交换 (4KB, 64-bit) | 通过 | BEX+HEX+WEX 全开，64-bit 全字节反转，常量填充避免 64-bit 乘法 |
| 反向拷贝 (128KB) | 通过 | SINC=DECR 源递减读 + DINC=INCR 目的递增写，16-bit 数据，block repeat |
| Stride 抽取 (16×10 奇数列) | 通过 | SINCOS=4B > SSIZE=2B 跳读，SUV=0（步长自然对齐行宽），提取 5 列 |
| 子区域抽取 (8×8 from 32×32) | 通过 | Block repeat + SUV=48 跳到下一行，DUV=0 目的连续 |
| 矩阵转置 (128×256 uint32) | 通过 | 128 行用 127 个链表节点，BNDT=4（1 int/block），DUV=508 跨行写列 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| 64-bit 乘法导致 CPU 复位 | uint64_t 乘法调用库函数可能使用未使能的 NEON 指令 | 避免 64-bit 乘法，改用常量赋值 |
| DINCOS < DSIZE 触发 ASE | 硬件配置校验：DINCOS 必须 >= DSIZE | 确保 DINCOS 与 DSIZE 匹配 |
| Decrement + burst 误报 ASE | 实为 DINCOS/DSIZE 不匹配，非 burst 方向问题 | 修正 DINCOS 后 burst + decrement 正常工作 |

## SDMMC (SD 卡)

硬件配置: SDMMC1, ker_ck=HSI(64MHz), PC8~12+PD2 AF12, 4-bit 模式

### 基本功能

| 测试项 | 状态 | 说明 |
|--------|------|------|
| RCC 时钟配置 | 通过 | SDMMC12CKSELR=3(HSI), MP_AHB6ENSETR bit16 |
| GPIO 配置 | 通过 | PC8=DAT0, PC9=DAT1, PC10=DAT2, PC11=DAT3, PC12=CLK, PD2=CMD, AF12, VERY_HI |
| SdCardInit 卡识别 | 通过 | CMD0→CMD8→ACMD41→CMD2→CMD3→CMD9→CMD7→ACMD6 全序列 |
| CID 解析 | 通过 | Samsung SDU1 v1.0, SN=0x14A11A21, 2019/1 |
| CSD 解析 (容量/速率) | 通过 | 61440MB (64GB), Max Clock 25MHz |
| 4-bit 总线切换 | 通过 | ACMD6(arg=2) + SdmmcSetBusWidth(4BIT) |
| Read MBR (Block 0) | 通过 | 签名 55 AA 有效 |
| Write/Read (Block 2048) | 通过 | XOR pattern 512 字节，回读校验通过 |
| Multi-block Write/Read (256 blocks @ block 100000) | 通过 | CMD23+CMD25/CMD18 single buffer, uint16_t 0~65535, 128KB |

### IDMA 链表模式 (阻塞)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| LlMultiBlockTest 链表多块读写 | 通过 | 写5节点(512+512+480+544+512) ULS混用 + 读2节点(1024+1536) 非连续描述符, uint16_t 0~1279 |
| SdmmcReadDataLL (驱动层) | 通过 | scatter-gather, 3-word 可变大小节点, LlValidate 校验 |
| SdmmcWriteDataLL (驱动层) | 通过 | 同上, 写后轮询 BUSYD0 |

### 中断模式 (非阻塞)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| LlItMultiBlockTest 单节点中断读写 | 通过 | UL A=0 单 buffer, 512B, 中断写+读, GIC SDMMC1 IRQ 81, uint16_t 0~255 |
| LlItBtcTest 2节点动态BAR | **预期失败** | 节点0 BAR=1, 节点1 BAR=0, IDMATE 后自动 RCC 复位+重初始化恢复 |
| LlItBtcMultiTest 32节点动态BAR | **通过** | 24预填+8动态, 32×4096B=128KB, 降低时钟至8MHz, 记录BTC_start/end, 回读memcmp校验 |
| LlItPreparedTest 全预准备 | 通过 | 所有节点 ABR=1 预先, 5节点写+2节点读, BTC 仅计数, 中断路径正常 |

### 擦除功能

| 测试项 | 状态 | 说明 |
|--------|------|------|
| EraseTest 16块写+擦除前8块 | 通过 | 写16块(uint16_t 0~4095) → SdCardErase前8块 → 回读校验后8块保留 + 前8块全0 |

### 中断模式 ISR 架构

| 项目 | 说明 |
|------|------|
| ISR 实例绑定 | Sdmmc1IrqHandler(SDMMC1+g_sdmmc1_irq) / Sdmmc2IrqHandler(SDMMC2+g_sdmmc2_irq), 编译时硬编码, 无全局指针 |
| 状态机 | CMDREND(清单标志)→DATAEND(完成,关MASKR+IDMA) |
| BUSYD0 策略 | DATAEND 即完成, 不在 ISR 等 Busy; 下次 SdmmcStartLL 入口 if(BUSYD0)→ERR_BUSY |
| IDMABTC | ISR 递增 g_sdmmc[1|2]_irq.btc_cnt, 前台轮询变化→填数据+置 ABR |
| IDMATE | 无中断使能位(实测 MASKR 无效), 仅 STAR 标志+ICR 清除; ISR 被动检测 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| SdmmcReadData/WriteData 卡在 CPSM 轮询 | DCTRL 设置 DTEN=1 绕过 CPSM 直接启动 DPSM, RM0436: "DTEN must not be used with SD or eMMC cards" | DCTRL 不设 DTEN, 由 CMDR CMDTRANS 位触发 DPSM |
| SdmmcWriteData 卡在 BUSYD0END 轮询 | DATAEND 后 DPSM 回 idle, BUSYD0END 不可达 | 改为轮询 STAR.BUSYD0 电平 |
| BUSYD0END 中断不可达 | BUSYD0END 仅用于 R1b 命令响应 Busy, 与数据写后的卡编程 Busy 无关 | ISR 在 DATAEND 完成, 下次启动前 if(BUSYD0) 拒绝 |
| IDMATE 不产生中断 | IDMATE 无中断使能位, 仅标志; ABR=0 时硬件立即停止 IDMA→TXUNDERR/RXOVERR | 双缓冲: 前2节点预置 ABR=1, BTC 准备 N+2 号节点 |

## eMMC

硬件配置: SDMMC2, ker_ck=PLL4P(200MHz), PG6(CMD,AF10)+PE3(CLK,AF9)+PB14/15/3/4(D0~D3,AF9)+PA8(D4,AF9)+PA9(D5,AF10)+PE5(D6,AF9)+PD3(D7,AF9), 8-bit HS 模式

### 基本功能

| 测试项 | 状态 | 说明 |
|--------|------|------|
| SdmmcPowerOn 上电 | 通过 | POWER cycle→off→on, CLKDIV=251 → CK≈398kHz |
| EmmcGpioInit 配置 | 通过 | PG6(CMD,AF10)+PE3(CLK,AF9)+PB14/15/3/4(D0~D3,AF9)+PA8(D4,AF9)+PA9(D5,AF10)+PE5(D6,AF9)+PD3(D7,AF9), HI 速, 上拉 |
| EmmcCardIdentify 卡识别 | 通过 | CMD0→CMD1→CMD2→CMD3, 卡进 Stand-by |
| EmmcCardSelect 选卡 | 通过 | CMD7, 400kHz 1-bit 下完成, 卡进 Transfer |
| EmmcCardSetClock 切 HS 时序 | 通过 | CMD6 EXT_CSD[185]=1 + SdmmcSetClock 52MHz |
| EmmcCardSetBusWidth 切 8-bit | 通过 | CMD6 EXT_CSD[183]=2 + SdmmcSetBusWidth 8BIT |
| EmmcCardGetInfo 读卡信息 | 通过 | CMD9(CSD) + CMD8(EXT_CSD 512B), 解析容量/版本 |
| CID 解析 | 通过 | 制造商/产品名/版本/序列号/日期打印正常 |
| EXT_CSD 版本/容量 | 通过 | 容量打印正常, DEVICE_TYPE/HS_TIMING/BUS_WIDTH 显示正确 |

### 单块读写

| 测试项 | 状态 | 说明 |
|--------|------|------|
| Single Block R/W @ block 15000000 | 通过 | CMD17 读 → CMD24 写 0xA5 → CMD17 回读, memcmp 512B 校验 |
| Multi Block R/W @ block 15000128 | 通过 | CMD23+CMD25 写 128 块 uint16_t 0~32767 → CMD23+CMD18 回读, memcmp 64KB 校验 |

### 擦除功能

| 测试项 | 状态 | 说明 |
|--------|------|------|
| EraseTest @ 2 groups 1MB apart | **通过** | CMD35+CMD36+CMD38 擦除 Group A (block 15000000~15000007), 回读全 0; Group B (block 15002048~15002055) 原数据保留 — 验证擦除组边界正确 |

### DDR52 模式 (8-bit DDR, ~52MHz, 104 MB/s)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| EmmcCardSetDdrMode 切换 | **通过** | 规范顺序: HS_TIMING=1 → BUS_WIDTH=6 (8-bit DDR), SWITCH_ERROR 无, CLKCR DDR=1 BUSSPEED=1 |
| CMD13 卡状态 | **通过** | 切换后卡在 Transfer (Tran=4), 状态正确 |
| 单块写 + 回读校验 | **通过** | CMD24 写 0xA5 pattern → CMD17 回读, memcmp 512B 校验通过 (之前 BUS_WIDTH=4 非法值导致 DTIMEOUT, 修正为 6 后正常) |
| 切回 SDR 8-bit | **通过** | BUS_WIDTH=2 + CLEAR DDR, CLKCR 恢复 SDR 配置 |
| DDR52 + 中断 LL 多节点 (8wr+2rd) | **通过** | 8-bit DDR 52MHz 下 CMD23+CMD25/18 中断 LL, 8×4096B 写 + 2×16384B 读非连续 desc, uint16_t 0~16383 全校验 |

### 写保护

| 测试项 | 状态 | 说明 |
|--------|------|------|
| CMD28 SET_WRITE_PROT | 通过 | 设 WP 成功后, CMD24 写该组块被拒绝 (BUSYD0), 数据保留完好 |
| CMD29 CLR_WRITE_PROT | 通过 | 清 WP 后再 CMD24 写入正常, 数据回读校验通过 |
| CMD30 SEND_WRITE_PROT | 通过 (状态全 0) | 读 32 bits (4 字节, 32 组), 硬件响应正常但此 eMMC 返回的 WP 状态全 0 |
| CMD31 SEND_WRITE_PROT_TYPE | 通过 (状态全 0) | 读 64 bits (8 字节, 32 组 × 2 bits), 硬件响应正常但此 eMMC 返回的 WP 类型全 0 |

### 中断模式 (非阻塞)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| DDR52 + 中断 LL 多节点 | **通过** | 8-bit DDR 52MHz, CMD23 轮询+CMD25/18 中断, 8 节点写+2 节点读非连续 desc, 全 ABR=1 |

### Sleep/Awake

| 测试项 | 状态 | 说明 |
|--------|------|------|
| CMD5 Sleep (Standby → Slp) | 通过 | Sleep 前 Tran, 后 Slp(10), CMD13 验证状态切换正确 |
| CMD5 Awake (Slp → Standby → Tran) | 通过 | Awake 后回到 Tran(4), 状态恢复 |
| Read during sleep (状态检查) | 通过 | 状态检查拒绝 (WRONG_STATE), DPSM 不受影响 |
| R/W after wake | 通过 | 唤醒后 CMD24 写 + CMD17 回读, memcmp 512B 校验通过 |
| CMD5 参数 | bit15=1 进入 Sleep, bit15=0 唤醒 (符合 JESD84-B51 规范) |

### 新增接口

| 函数 | 说明 |
|------|------|
| `EmmcCardSetDdrMode(card, width, target_hz)` | 封装 DDR 切换规范顺序: EmmcCardSetClock(HS_TIMING=1) → EmmcCardSetBusWidth(BUS_WIDTH=5/6) |
| `EmmcCardReadStatus(card, &sts)` | 一站式获取: CMD13 卡状态 + CLKCR 实时配置(位宽/DDR/时钟) + 缓存值 |
| `EmmcCardSleep(card)` | CMD5 Sleep (Standby→Slp), R1b, 含 CMD7 deselect + WaitBusy |
| `EmmcCardAwake(card)` | CMD5 Awake (Slp→Standby→Tran), R1b, 含 WaitBusy + CMD7 select |
| `EmmcCardSendStatus(card, &sts)` | CMD13 SEND_STATUS, 自动解析 R1 到 EmmcCardStatus_t |
| `EmmcCardSetWriteProtect(card, block)` | CMD28 SET_WRITE_PROT, R1b |
| `EmmcCardClearWriteProtect(card, block)` | CMD29 CLR_WRITE_PROT, R1b |
| `EmmcCardGetWriteProtect(card, start_group, &wp[4])` | CMD30 SEND_WRITE_PROT, 32 bits (4 字节) bitmap |
| `EmmcCardGetWriteProtectType(card, start_group, &type[8])` | CMD31 SEND_WRITE_PROT_TYPE, 64 bits (8 字节, 2 bits/组) |

### 初始化流程

```
SdmmcPowerOn (400kHz, 1-bit)
  → EmmcCardIdentify (CMD0→CMD1→CMD2→CMD3, Stand-by)
  → EmmcCardSelect (CMD7, Transfer)
  → EmmcCardSetClock (CMD6 HS_TIMING=1, 52MHz)
  → EmmcCardSetBusWidth (CMD6 BUS_WIDTH=8)
  → EmmcCardGetInfo (CMD9+CMD8)
```

### 已知问题

| 问题 | 说明 |
|------|------|
| CMD30/31 返回 WP 状态全 0 | 此 eMMC (NCard) 的 CMD30 SEND_WRITE_PROT 和 CMD31 SEND_WRITE_PROT_TYPE 返回全 0, 但 CMD28/29 设/清 WP 功能正常, 写保护生效已验证 |
| Sleep 期间误读写后 DPSM 挂死无法恢复 | Sleep 态下发 CMDTRANS 命令 (CMD17/18/24/25) 会触发 DPSM, 即使命令 CTIMEOUT, DPSM 仍滞留活跃态 (DPSMACT=1), FIFORST 复位无效, 后续所有命令失败。当前通过读写前状态检查预防, 恢复方法尚未找到 |
| ~~EmmcGpioInit 全配 PG6(CMD) 导致初始化失败~~ | **已修复**: PG6(CMD) 应配 AF10 而非 AF9, PA9(D5) 应配 AF10 而非 AF9. 两个 AF 配置错误导致 eMMC 无法识别 |

## FDCAN

硬件配置: FDCAN1, ker_ck=24MHz(HSE=24MHz→FDCANCKSELR=0), PH13(AF9)=TX, PI9(AF9)=RX, SN65HVD230 收发器

### 经典 CAN 回环

| 测试项 | 状态 | 说明 |
|--------|------|------|
| Init 125kbit/s | 通过 | FdcanInit + NBTP 自动计算 |
| 标准帧过滤 (Range, FIFO 0) | 通过 | 0x000~0x7FF, ANFS/ANFE=REJECT |
| 发送 (FIFO) | 通过 | 4 帧 0x100/0x200/0x300/0x400, DLC=8 |
| 接收 + 校验 | 通过 | FdcanReadFifo0 轮询, ID/DLC/数据全比对 |

### CAN FD 回环

| 测试项 | 状态 | 说明 |
|--------|------|------|
| Init 1M/2M bit/s | 通过 | 仲裁 1M, 数据相位 2M, DSIZE_64 |
| 扩展帧过滤 (Range, FIFO 0) | 通过 | 0x00000000~0x1FFFFFFF |
| 发送 (FIFO, FD) | 通过 | 6 帧扩展 ID, DLC 9/10/12/13/14/15 |
| 接收 + 校验 | 通过 | FdcanReadFifo0 轮询, ID/XTD/DLC/数据全比对 |

### 正常模式发送 (DAR = 1, 需总线 ACK)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| GPIO 初始化 | 通过 | PH13(AF9)=TX, PI9(AF9)=RX, 推挽输出, 高速, RX 上拉 |
| 500kbit/s 连续发送 (串口停止) | 通过 | 64-bit 递增 (uint32_t 拆分防复位), USB-CAN 模块 + PC 上位机验证, 总线 ACK 正常 |

### 过滤组合测试 (正常模式, 需 PC USB-CAN 配合发帧)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 标准帧 Range (0x100~0x1FF→FIFO0) | 通过 | Fidx=0, ID+边界验证 |
| 标准帧 Dual (0x200/0x201→FIFO0) | 通过 | Fidx=1, 双 ID 精确匹配 |
| 标准帧 Classic (0x300/0x7F0→FIFO0) | 通过 | Fidx=2, 0x300~0x30F 范围 |
| 标准帧 Range→REJECT (0x500~0x5FF) | 通过 | 拒收帧不进 FIFO, 软件不可见 |
| 标准帧 RxBuf (0x400→RxBuf0) | 通过 | Fidx=4, SFEC=7 单 ID 专用缓冲 |
| 扩展帧 Range (0x10000000~FF→FIFO0) | 通过 | Fidx=0 |
| 扩展帧 Dual (0x10000100/101→FIFO0) | 通过 | Fidx=1 |
| 扩展帧 Classic (0x10000200/0x1FFFFFF0→FIFO0) | 通过 | Fidx=2, 低 4 位任意 |
| 扩展帧 Range→REJECT (0x1FFF0000~FF) | 通过 | 拒收生效 |
| 扩展帧 RxBuf (0x10001000→RxBuf1) | 通过 | Fidx=4 |
| ANFS=REJECT + ANFE=FIFO1 | 通过 | 未匹配扩展帧→FIFO1, 打印 ANMF |
| 实时监控循环 | 通过 | 串口实时打印帧信息(来源/ID/DLC/Fidx/ANMF/数据), 任意键停止 |

### 专用 Tx Buffer + Tx Event FIFO (正常模式, DAR=1)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 4 dedicated buffer 发送 | 通过 | FdcanSendBuf 逐 buffer 发送 ID 0x100~0x103 |
| TXBTO 完成确认 | 通过 | TXBTO=0x0F 全部发送完成 |
| Tx Event FIFO | 通过 | 4 events, ET=2(单次发送), MM=0, TXTS 正常计数 |
| 时间戳 (TSS_INC, prescaler=0) | 通过 | TXTS 随帧递增 |
| Tx Event 结构体打印 | 通过 | FdcanTxEvent_t 硬件位域直接映射, ID/ET/MM/TXTS |

### CAN 监控 + 周期发送

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 全帧接收 | 通过 | 标准+扩展全接受, FdcanRxFrame_t 结构体 |
| 接收帧实时打印 | 通过 | PrintRxFrame 公用函数 (来源/ID/DLC/Fidx/ANMF/数据) |
| 周期发送 (每秒 1 帧) | 通过 | ID=0x7E0, 数据含 32-bit 发送计数+16-bit 时间戳 |
| 时间戳回绕处理 | 通过 | prescaler=15(32us/tick), 16-bit 回绕检测+累加 |
| ECR 错误计数 (TEC/REC) | 通过 | 每秒打印 RX/TX/TEC/REC/秒数 |
| 统计打印 | 通过 | 停止后打印总耗时/总接收/总发送 |

### IRQ 接收 + 过滤优先级 (测试 7)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| GIC 配置 | 通过 | GicdSetGroup/SetPriority/SetTarget/SetTrigMode/GicdEnableInt + GicRegisterIrq |
| IRQ 线使能 (ILE_EINT0) | 通过 | RF0NE + TEFNE + DRXE + TSWE |
| ISR 分发 | 通过 | FdcanIrqCore 统一入口, 读 IR 后分发 |
| IR 标志 w1c 清除 | 通过 | ISR 末尾 FdcanIrqClear 统一清除, 此前不清导致中断风暴→看门狗复位 |
| Rx FIFO 0 中断接收 | 通过 | 打印帧信息 (来源/ID/DLC/Fidx/ANMF/数据) |
| Tx Event FIFO 中断 | 通过 | EFC=1, 打印发送事件 (ID/ET/MM/TXTS) |
| 过滤优先级演示 | 通过 | 3 个 RxBuf 过滤在 range 之前优先命中, 0x400 在 range 之后永不被匹配 (dead filter) |
| NDAT 扫描 4 buffer | 通过 | for(i=0; i<4 && ndat; i++) 覆盖全部 Rx Buffer |
| CCE 保护 SRAM 写入 | 通过 | FdcanEnterCfg/FdcanExitCfg 封装过滤配置 |
| 周期发送 (每秒 1 帧) | 通过 | TSW 中断触发, ID=0x7E0, 与上位机数据匹配 |

### CAN FD 发送 (测试 8)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| Init 1M/3M bit/s | 通过 | 仲裁 1M, 数据相位 3M, DSIZE_64, FDOE+BRSE |
| DLC 变化发送 | 通过 | 8→12→16→20→24→32→48→64 字节循环 (sent&7 查表) |
| DAR=1 单次发送 | 通过 | 禁止自动重传, TXBTO 确认 |
| 串口停止 + 统计 | 通过 | FramePoll(0) 检测, 打印发送帧数和 LEC |

### 驱动接口改进

| 改进 | 说明 |
|------|------|
| FdcanRxFrame_t | union+bit-field 硬件格式直接映射 (R0: ID/RTR/XTD/ESI, R1: RXTS/DLC/BRS/FDF/FIDX/ANMF), 接收函数直接 SRAM 赋值 |
| FdcanTxEvent_t | union+bit-field 硬件格式 (R0: ID/RTR/XTD/ESI, R1: TXTS/DLC/BRS/EDL/ET/MM), FdcanTxEventRead 一步读 |
| FdcanFilterStdRxBuf / FdcanFilterExtRxBuf | 专用 Rx Buffer 过滤, 去掉冗余的 SFT/EFT 参数 |
| PrintRxFrame | 测试 3 和测试 6 共用的帧打印函数 |
| EFC=1 | FdcanWriteTxElement T1 bit23 置 1, 发送成功后写 Tx Event FIFO |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| 标准帧 ID 全为 0 | T0 写 ID 未左移 18 位, 硬件从 bit[28:18] 读到全 0 | 标准帧 ID 左移 18 位, 读时右移回 11 位 |
| GFC 硬编码 ACCEPT_F0 | 未匹配帧行为不可配置 | 新增 anfs/anfe 配置项 |
| memcpy RAM 访问 | 非 4 字节对齐, 多余局部变量 ram | 改 uint32_t 直接访问 FDCAN_SRAM[] + DLC→word 查表 |
| NBTP 计算溢出 | sample_point_pct=875→unsigned 溢出→NTSEG2 无效 | 修正为 87 |
| NSJW ≥ NTSEG2 | nsjw=ntseg2 违反 RM 约束 (NSJW < NTSEG2) | nsjw = ntseg2 > 0 ? ntseg2 - 1 : 0 |
| HSE 频率误配 | 代码假设 HSE=12MHz, 实际 HSE=24MHz → 实际波特率 2Mbps | FDCAN_KER_CK_HZ=24000000U |
| uint64_t 自增复位 | yagarto 4.6.2 工具链缺陷, counter++ 导致 CPU 复位 | 拆为两个 uint32_t + 手动进位 |
| Tx Event FIFO 无事件 | T1 bit23 EFC(Event FIFO Control)=0, 硬件不写事件 | EFC=1, 发送成功时写 Tx Event |
| FdcanTimestampInit 无效 | TSCC 受保护寄存器, 需 CCE=1+INIT=1 才可写 | 在 FdcanTimestampInit 中进入/退出配置模式 |
| 时间戳计数停止 | 无 DAR 且无 ACK → 自动重传 → TEC 飙升 → Bus-Off → INIT=1 停计数器 | 配 DAR=1 避免自动重传 |

## SAI + CS42L51 音频

硬件配置: SAI2 Block A, I2S 主机, MCLK=64MHz, 8kHz 采样率, 16-bit. CODEC: CS42L51, I2C5 控制 (0x94), PC0=RST.

### 基本功能

| 测试项 | 状态 | 说明 |
|--------|------|------|
| GPIO/时钟初始化 | 通过 | PE0(MCLK,AF10), PI7(FS,AF10), PI5(SCK,AF10), PI6(SD,AF10), HI 速. SAI2CKSELR=3(PLLQ), CPERCKSELR=0 |
| I2C 通信 | 通过 | cs42l51_i2c_write/read, MAP 地址 + Restart, CS42L51 应答正常 |
| SAI I2S 初始化 | 通过 | SaiCfgFillI2S(64MHz→8kHz, 16-bit, Master TX) |
| CODEC 初始化 | 通过 | RESET↑→PDN=1→配寄存器→PDN=0→50ms 等待 |
| 1kHz 正弦波输出 | 通过 | 16 点 × 1/4 幅值, 8kHz 播放心跳 |
| Interactive 命令 | 通过 | FramePoll 非阻塞接收, +/-/v/p/m/s/q 全功能 |
| 停止/退出 | 通过 | SaiStop + Cs42l51Deinit (PDN=1, RESET↓), 返回菜单 |

### 音量控制

| 测试项 | 状态 | 说明 |
|--------|------|------|
| AOUT 增益 (v 命令) | 通过 | 0~24 half-db = 0~+12dB, 寄存器 0x00~0x18 |
| PCMMIX 衰减 (p 命令) | 通过 | -102~+24 half-db = -51~+12dB, 7-bit signed 编码 |
| AOUT + / - 快调 | 通过 | 4 步/次 (2dB), 上限 0x18 |
| Mute 切换 (m 命令) | 通过 | Cs42l51SetVolume(0x80), 恢复上次音量 |
| 状态显示 (s 命令) | 通过 | 读回 AOUT/PCMMIX/09h 寄存器原始值 |

### 子测试

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 1. Interactive I2S play + volume control | 通过 | 正弦波, +/-/v/p/m/s/q 交互命令 |
| 2. Happy Birthday melody | 通过 | 12 音符旋律表, SaiSendBlocking 轮询 |
| 3. PCM Stream (DMA ping-pong 460800bd) | **待实测** | DMA 乒乓缓冲, UART 流式接收, 需 PC 端 pcm_sender.py |
| 4. Self-loopback (MIC→ADC→DAC→HP) | 通过 | DATA_SEL=ADC, MIC2+30dB, 无需 SAI Block B |
| 5. Digital loopback (A RX → B TX) | 通过 | Block A Master RX + Block B Slave TX sync, MCU 中转 |

### 驱动接口

| 函数 | 说明 |
|------|------|
| `Cs42l51AoutHalfDbToReg(half_db)` | half-dB → AOUT 寄存器值 (0~24 → 0x00~0x18) |
| `Cs42l51PcmmixHalfDbToReg(half_db)` | half-dB → PCMMIX 寄存器值 (正=0x00~0x18 增益, 负=0x19~0x7F 衰减) |
| `Cs42l51Reg_t` 枚举 | CS42L51 全部寄存器地址 (02h~21h) 统一枚举 |
| `CS42L51_REG_MIC_CTL` 位域 | 05h MIC 控制: `CS42L51_MIC_GAIN_30dB` / `CS42L51_MIC2_SEL` / `CS42L51_BIAS_EN` |
| `CS42L51_REG_ADC_INPUT_SEL` 位域 | 07h ADC 输入选择: `CS42L51_AINA_SEL(s)` / `CS42L51_AINB_SEL(s)` / `CS42L51_IN_SEL_MIC2` |
| `CS42L51_REG_ADCMIXA/B_VOL` | 0Eh/0Fh ADCMIX 音量: `CS42L51_ADCMIX_VOL_12dB` / `CS42L51_ADCMIX_MUTE` |
| `SaiCfg_t.syncen` | SAI 子块同步模式 (0=async, 1=sync_int, 2=sync_ext), 通过 SaiInit 配置 |
| `SaiDmaEnable/Disable` | SAI DMA 请求使能/关闭 (CR1_DMAEN) |
| `UsartSetBaud` | USART 波特率单独设置 (BRR 寄存器) |

### CS42L51 编码细节

**AOUT (寄存器 16h/17h)** — unsigned binary:
| half-db | 寄存器 | dB |
|---------|--------|----|
| 24 | 0x18 | +12 (最大) |
| 8 | 0x08 | +4 |
| 0 | 0x00 | 0 |
| >0x18 或 0x19~0x33 | — | **无效 (无声)** |

**PCMMIX (寄存器 10h/11h)** — 7-bit signed (补码):
| half-db | 寄存器 | dB |
|---------|--------|----|
| +24 | 0x18 | +12 (最大) |
| 0 | 0x00 | 0 (参考) |
| -1 | 0x7F | -0.5 |
| -64 | 0x40 | -32 |
| -96 | 0x20 | -48 |
| -102 | 0x19 | -51 (最小) |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| DATA_SEL 双移位导致音量失控 | `CS42L51_DATA_SEL_DSP 1<<6` (0x40) 传入 `CS42L51_DATA_SEL(s) ((s)&3)<<6` → 0x00 (PCM 模式), AOUT/PCMMIX 音量写寄存器不生效 | _PCM/_DSP/_ADC 改为未移位值 (0/1/2) |
| I2C 连续写入寄存器值不对 | CS42L51 需 ~1ms 间隔, 连续写入时后一帧被忽略 | cs42l51_i2c_write 末尾加 cs42l51_delay_ms(1) |
| AOUT 寄存器 0x19~0x33 无效 | unsigned binary 编码在 0x19~0x33 区间无有效映射, 写入后芯片静音 | Cs42l51AoutHalfDbToReg 截断至 0x18, 不支持 AOUT 衰减 |
| PCMMIX 20 比 10 和 40 都小 | PCMMIX 用 7-bit signed 编码, 0x20=(-96)×0.5=-48dB, 0x10=(+16)×0.5=+8dB(增益), 0x40=(-64)×0.5=-32dB | 驱动提供 Cs42l51PcmmixHalfDbToReg 统一换算 |
| 某些寄存器用魔术数字 | sai_test.c 中 0x05/0x07/0x0E/0x0F 等寄存器地址直接写数字 | 补全 `Cs42l51Reg_t` 枚举 + 位域宏定义, 全部替换为命名常量 |

## RCC (Reset and Clock Control)

硬件配置: RCC 基址 0x50000000, RM0436 §10. HSE=24MHz(晶振), HSI=64MHz, CSI=4MHz, LSE=32768Hz, LSI=32000Hz.

> **注意** 当前阶段仍运行在 U-Boot 配好的时钟树之上，以下"通过"均指代码逻辑正确、寄存器读写正确，但尚未在纯裸机下实际切换时钟并验证系统稳定运行。真正裸机阶段再做完整时钟配置验证。

### 时钟树回读 (菜单 20) — 实测通过

| 测试项 | 状态 | 说明 |
|--------|------|------|
| RccClkDump 全部 25 个节点 | **实测通过** | 振荡器×5 + PLL×12 + 系统×3 + AHB×5 + APB×5 |
| 振荡器回读 (HSE/HSI/CSI/LSE/LSI) | 实测通过 | OCRDYR + BDCR + RDLSICR |
| PLL 回读 (4×P/Q/R) | 实测通过 | PLLRDY→读 CFGR1/CFGR2/FRACR |
| 小数模式回读 (PLL3) | 实测通过 | boot ROM 配的小数频率 208877929Hz 正确 |
| 系统时钟回读 (MPU/AXI/MCU) | 实测通过 | 源选择 + MPUDIV/AXIDIV/MCUDIV |
| AHB 总线回读 (AHB2~6) | 实测通过 | AHB2/3/4=MCU, AHB5/6=AXI |
| APB 总线回读 (APB1~5) | 实测通过 | APB1/2=MCU/2, APB3=MCU/2, APB4=AXI/2, APB5=AXI/4 |
| 64 位除法 | 实测通过 | libgcc 已链接, 不再复位 |

### 复位原因 — 实测通过

| 测试项 | 状态 | 说明 |
|--------|------|------|
| RccResetCauseGet + Clear | 实测通过 | 读 MP_RSTSCLRR, 写 1 清 0 |
| PrintResetCause (开机打印) | 实测通过 | main() 早期调用, POR/PDR + VDDCORE + MPU CPU0 Boot |

### 时钟配置 API — 代码完成, 待裸机实测

全部函数寄存器逻辑已实现, 约束检查、超时等待、返回值完整。当前仅回读验证了寄存器位定义正确 (dump 值与 U-Boot 配置吻合), 未实际切换时钟。

| 模块 | 函数 | 说明 |
|------|------|------|
| 振荡器 | RccHseInit/Disable (3 模式), RccHsiInit/Disable, RccCsiInit/Disable | OCENSETR/OCENCLRR/OCRDYR |
| 振荡器 | RccLseInit/Disable (晶振+旁路, 3×DRV), RccLsiInit/Disable | BDCR (LSE), RDLSICR (LSI) |
| PLL 源 | RccPll12SrcSelect, RccPll3SrcSelect, RccPll4SrcSelect | RCK12/3/4SELR |
| PLL1/2 | RccPll1/2Init (整数+小数), RccPll1/2Disable | DIVN 25~100, DIVP 奇数拒绝, FRACLE |
| PLL3/4 | RccPll3/4Init (整数+小数), RccPll3/4Disable | DIVN 25~200, IFRGE 输入范围 |
| 系统时钟 | RccMpuClkCfg, RccAxiSrcSelect, RccMcuSrcSelect | 枚举编码分频 |
| 分频器 | RccAxiDivSet, RccMcuDivSet, RccApb1~5DivSet | 各 DIVR 寄存器 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| LSI 回读始终 0 | `RccLsiInit` / `RccClkDump` 操作 BDCR, 但 LSI 位于 RDLSICR (0x144) | 改为 RDLSICR, bit0=LSION, bit1=LSIRDY |
| 64 位除法导致复位 | libgcc 未链接, `__aeabi_uldivmod` 跳飞 | 修复链接后正常 |

## LTDC + SiI9022A HDMI 显示 (菜单 21)

硬件配置: LTDC @0x5A001000, ker_ck=PLL4_Q(74.25MHz), 720p60 (1650×750, HSW40/HBP220/HFP110, VSW5/VBP20/VFP5). 引脚: HSYNC=PI12, VSYNC=PI13, CLK=PI14, DEN=PK7, R[7:1]=PJ[6:0], R0=PI15, G[4:0]=PJ[11:7], G[7:5]=PK[2:0], B[3:0]=PJ[15:12], B[7:4]=PK[6:3] (全 AF14). SiI9022A: I2C5 (PA11/PA12 AF4), TPI 地址 0x72, 设备 ID=0xB0, HDMI_RST=PA13.

分辨率档位: `DisplayHdmiInit(tim, sii, res)` 按 DisplayRes_t 参数化 — 720p60 (AVI VIC=4) / 1080p30 (VIC=20, 总 2200×1125, 刷新率 74.25M/2200/1125 = 30.00Hz)。LTDC 像素时钟上限 90MHz (datasheet)，1080p60 需 148.5MHz 不可达；1080p30/25/24 同为 74.25MHz 可行，PLL4 无需改动，只换时序/VIC/帧缓冲。`DisplayHdmiVideoStart(sii, vm, avi_vic)` 统一视频模式+AVI InfoFrame+D0+TMDS 使能启动流程。

### 基本功能

| 测试项 | 状态 | 说明 |
|--------|------|------|
| PLL4_Q 像素时钟 (74.25MHz) | 通过 | DIVN=147 + fracv=4096 → VCO=594MHz, DIVQ=7 → PLL4_Q=74.25MHz; LTDC ker_ck 来自 PLL4_Q (RM0436 时钟树), 不是 PLL4_P |
| LTDC 时序配置 (720p60) | 通过 | SSCR/BPCR/AWCR/TWCR, 总时序 1650×750, 刷新率 = 74.25M/1650/750 = 60.00Hz |
| GCR 极性 | 通过 | HSYNC+/VSYNC+/DE+ (depol=0)/PCPOL=1, 与 SiI9022A 上升沿锁存 (0x08[4]=1) 匹配 |
| 图层配置 (Layer 1, RGB565) | 通过 | 全屏窗口 (0,0)-(1279,719), const_alpha=255 不透明, 单图层 (Layer 2 未用) |
| 帧缓冲 8 色竖条 | 通过 | 彩虹 7 色 (赤橙黄绿青蓝紫) + 白, 1280×720 RGB565 |
| SiI9022A 探测 | 通过 | TPI 设备 ID 回读 0xB0 |
| SiI9022A 初始化 | 通过 | 源端接 SRC_TERM、输入总线 TCLK_X1/FULL、RGB 8-bit、外部同步 |
| 视频模式设置 | 通过 | 像素时钟 74.25MHz, htotal 1650, vtotal 750, 60Hz |
| AVI InfoFrame | 通过 | VIC=4 (720p60), RGB, 校验和正确 |
| TMDS 输出使能 | 通过 | SYS_CTRL bit0 (HDMI 模式), 空闲窗口后写入 + 回读确认 |
| 显示器识别 | 通过 | 识别为 1280×720@60Hz (4K 显示器整屏缩放显示) |
| 实际输出验证 | 通过 | 色条满屏正常; 引脚 DE 有效占比实测 73.1% ≈ 720/750 = 74.5% |

### 测试 2: UART 图片流 (RGB888 零转换)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 帧协议扩展 (FRAME_CMD_IMG_DATA 0x11) | 通过 | PC 发原始像素字节, LTDC 图层选 RGB888 直显 |
| 内存字节序 (RGB888 = BGR) | 通过 | LTDC 内存序 @+0=B (docs/ltdc_overview.md), 与 BMP24 原生匹配; PC 端 Pillow 加载后一次 R/B 对调, 固件零像素转换 |
| tools/img_sender.py | 通过 | 任意格式 (JPG/PNG/BMP) 加载, 非目标尺寸自动缩放 (--width/--height), 帧协议流式发送 |
| 460800 baud DMA RX 边收边显 | 通过 | DMA RX 环形缓冲 + FramePoll 处理, 传输中实时显示 |

### 测试 3: 板载幻灯片 (6 张图案 500ms 轮播)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 固件自生成 6 图案 | 通过 | 彩虹条/对角渐变/棋盘格/纯色/几何图形/同心圆环 (手写整数开方, 不用 libm) |
| VBlank 零拷贝切换 | 通过 | SRCR.VBR 重载图层 fb 指针, 无撕裂 |
| 轮播定时 | 通过 | TIM6 PSC=63999 → CNT 1kHz (STGEN 安全世界不可访问), 应用层只用驱动接口 |
| 看门狗喂狗 | 通过 | 6 个 SlideGen* 循环内每行 IwdgKickDog, 轮播不再复位 |

### 测试 4: 串口打字器 1080p30 (16x16 汉字 + 8x16 ASCII)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 字库生成管线 | 通过 | tools/font_gen.py 从宋体 simsun.ttc 渲染 16×16 单色点阵 → source/font_gbk16.c/.h (生成物, 换字体/加字重跑脚本, 勿手改) |
| GB2312 全字库 | 通过 | 87 区×94 位 (7445 字, 261,696B), 空字位补零, GBK16_INDEX(区码,位码) 直接查表 |
| ASCII 8×16 半宽 | 通过 | 256 项 (4,096B), 超宽字符 BOX 滤波缩放 |
| 文本引擎 | 通过 | 光标/换行/退格/清屏/满屏上滚, 白字黑底; 尺寸驱动 TxtScreenSet(fb,w,h,bpp) 绑定缓冲+清屏 (bpp=3/4 支持 ARGB 透明底) |
| FRAME_CMD_TXT_DATA 0x12 | 通过 | 载荷为 GB2312 字节流, UTF-8→GB2312 在 PC 端 (tools/typewriter.py, /clr 清屏 /quit 返回菜单) |
| 1080p30 显示效果 | 通过 | 4K 显示器点对点, 16px 字 2× 上采样, 锯齿较 720p (3×) 明显减少 |

### 测试 5: HDMI 1080p30 图片 (UART 真图)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 1080p30 时序 | 通过 | 总 2200×1125 (HSW44/HBP148/HFP88, VSW5/VBP36/VFP4), CEA-861 VIC 20, 刷新率 74.25M/2200/1125 = 30.00Hz |
| FHD 帧缓冲 | 通过 | g_fhd_fb 6,220,800B (1920×1080×3) 32B 对齐, 图层 RGB888 全屏 |
| 参数化初始化 | 通过 | DisplayHdmiInit(DISP_RES_1080P30) + DisplayHdmiVideoStart(AVI_VIC_1080P30), 与 720p60 共用流程 |
| UART 收真图 | 通过 | 64000000 源时钟分频 UsartSetBaud, 460800 baud 边收边显, BACK/'0' 返回菜单 |

### 测试 6: 双图层 (照片 + 文字条 + BCR 背景色)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 背景色 BCCR | 通过 | 深蓝 0x3030A0 打底, 四边露出; BCCR 非影子寄存器, 直接生效; DisplayHdmiInit 默认 0x000000, 测试内改 tim.bg_rgb888 后重调 LtdcInit |
| 图层窗口 (LxWHPCR/LxWVPCR) | 通过 | Layer1 照片 1280×720 居中 (320,180)-(1599,899) RGB888, 复用 g_img_fb 2.7MB; Layer2 文字条 1280×160 (320,740)-(1599,899) 贴照片底部, ARGB8888 0.8MB |
| 逐像素 alpha 混合 | 通过 | 混合顺序固定 BG→Layer1→Layer2; Layer2 BF1=逐像素×常量, BF2=1-逐像素×常量 (常量 255): 白字 0xFFFFFFFF 不透明/底全 0 透明, 照片上浮字 |
| 启动占位背景 | 通过 | SlideGenGradient 预填照片层, UART 图片 (img_sender 默认 1280×720, 无需参数) 流入后覆盖 |
| 文本引擎 bpp 参数化 | 通过 | TxtScreenSet(fb,w,h,bpp): 绘制/换行/上滚/退格全部按 g_txt_bpp; ARGB 白字 4 字节全 0xFF, 字节序无关; 测试 4 改传 3 无回归 |
| 内存占用 | 通过 | 2.7MB + 0.8MB = 3.5MB, 对比全屏 ARGB 叠层 8.3MB 省 4.8MB |
| 上板验证 | 通过 | img_sender 发图 → typewriter 打字: 照片上浮字 + 四边露底色正常 |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| LTDC 寄存器无法读写 | 看错时钟树: LTDC ker_ck 来自 PLL4_Q 而非 PLL4_P, Q 未使能 → 外设无时钟"没电", 寄存器访问失败 | 重配 PLL4: VCO 提至 594MHz (同时拉回 400-800MHz 规格内), DIVQ=7 + en_q=1 输出 74.25MHz, en_p=0 |
| DE 生成器寄存器回读 0 / SYS_CTRL 写不生效 | SiI9022A TPI 忙窗口内背靠背写被静默丢弃 | 每笔写间加 SiiSettle 稳定延时; TMDS 使能写前留空闲窗口, 写后回读确认 |
| 外部 DE 显示黑屏 (曾误判 DE 通路损坏) | LTDC GCR DEPOL 语义误读: RM0436 该位是 "Not data enable polarity", 1=DE 反相 | depol=0, 头文件注释改正 (RM0436 p1787 验证); 内部 DE 生成器方案废弃 |
| 调试中整板看门狗复位 | I2C 驱动无超时等待卡死 + 长循环不喂狗超 IWDG2 10s | 长循环内 IwdgKickDog; 移除会卡死的 EDID 读取 |
| AVI InfoFrame VIC 码错误 | VIC 写成 0x10 (1080p60), 实际 720p60 应为 4 | VIC=4 |
| HDMI/DVI 模式未设 | SYS_CTRL bit0 未置位 → DVI 模式, AVI InfoFrame 不发送 | SYS_CTRL 写 0x01 开启 HDMI 模式 |
| EDID 读取未实现 | I2C 驱动缺陷 (BSF 忙检查静默返回/NACK 无 STOP/无超时等待), 0x50 事务卡死整板 | 暂缓, 待 I2C 驱动修复 |
| 幻灯片轮播整板复位 | MPU 未配时钟跑 24MHz HSI, Rings 生成 (92 万次开方) 约 6s > IWDG2 10s 窗口 → 看门狗复位 | 6 个 SlideGen* 生成循环内每行 IwdgKickDog (720 次/图, 开销可忽略), 轮播正常 |

### 排查思路 1: LTDC 寄存器无法读写 → 时钟源选错 (PLL4_P vs PLL4_Q)

| 步骤 | 操作 | 结论 |
|------|------|------|
| 1 | 现象: LTDC 寄存器读写全失败 | 外设"没电" (无时钟), 寄存器访问直接失败 |
| 2 | 读同 APB4 总线上其他外设寄存器 | 可正常读写 → 总线没问题 |
| 3 | 固化 u-boot 启动 Linux, 显示器正常 | LTDC 硬件 IP + 外围电路没问题 (注意: Linux 显示走 MIPI DSI 屏, 只证明 LTDC 链路, **不覆盖** SiI9022A HDMI 路径) |
| 4 | 怀疑 ETZPC 把 LTDC 配成安全世界 | 查手册: LTDC 不在 DECPROT 可配置列表, 根本无法配成安全 → 排除 |
| 5 | 回到时钟树查 RM0436 | LTDC ker_ck 来自 **PLL4_Q** 不是 PLL4_P → 看错手册 |

**教训**: 外设寄存器访问异常, 第一嫌疑是**外设时钟没配** (无时钟外设像断电, 寄存器访问直接失败); 硬件层面用 Linux 当参照物一锤定音, 避免在好硬件上反复排查软件。

### 排查思路 2: 黑屏 → DE 极性 (DEPOL) 误配

| 步骤 | 操作 | 结论 |
|------|------|------|
| 1 | 外部 DE (PK7) 直连时画面消失 | 怀疑 SiI9022A 外部 DE 通路损坏 |
| 2 | 改用 SiI9022A 内部 DE 生成器绕过 | 画面恢复 → **误判**"外部 DE 坏了" (A/B 开关测试加深误判) |
| 3 | 质疑 "PK7 配置了" → 回头核查 | GPIO mux AF14 正确; 引脚采样 PK7 电平正常翻转; GCR 回读与配置一致 → 引脚/复用/寄存器写入全排除 |
| 4 | 唯一矛盾: DE 反相, 而 HSYNC/VSYNC 同为 "1=高有效" 语义却正常 | 查 RM0436 p1787: DEPOL 是 **"Not data enable polarity"**, 1 = Not DE 高有效 = **DE 低有效 (反相)** |
| 5 | 头文件注释 "DE 极性: 1=高有效" 是误读 | depol=0 → 画面正常, DE 占比实测 73.1% ≈ 720/750, DE 生成器方案废弃 |

**教训**: 位语义以手册原文为准, 不能信项目内注释; 怀疑"硬件通路损坏"前, 先穷尽"配置语义"的可能性; 实测手段 (引脚采样/寄存器回读) 用于收窄矛盾, 最终收敛到唯一解释。

## GMAC 以太网 (菜单 23)

硬件配置: RGMII (全部 AF11), ETH ker_ck=PLL4_P(125MHz, ETHCKSELR.ETHSRC=0), MDC=PC1/MDIO=PA2 (AF11, 分频 124→MDC≈2.15MHz, IEEE ≤2.5MHz 合规), PHY=MAE0621A (ID 0x7B74/0x4411, 复位脚 PH2, strap 地址 1 配置/实测应答在 0). ETH 与 LTDC 互斥 (PLL4: P=125MHz 给 ETH 或 Q=74.25MHz 给 LTDC, 不能同时精确).

三层分层: MAC MDIO 控制器 (drivers/stm32mp1xx_gmac.c) / GPIO 位冲击总线 (devices/mdio_gpio.c) / PHY 设备驱动 (devices/phy_mae0621a.c, 黑盒).

### MDIO 总线 (测试 1/8)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| MDIO 控制器读 ID (测试 1) | 通过 | addr0 reg2/reg3 稳定 0x7B74/0x4411, MDC≈2.15MHz 合规 |
| 首次读 REG2 0xFFFF 重试 | 通过 | PHY 特性 (实测复现), MaePhyReadRetry 重试×3, 对上层黑盒; GB 超时 (-1) 不重试 |
| GPIO 位冲击读 ID (测试 8) | 通过 | 绕过控制器直读 0x7B74/0x4411, 定位故障层级 (控制器/MAC 或 PHY/硬件) |
| 采样点 (MDC 低电平期间) | 通过 | 此 PHY 数据更新贴近上升沿, 高电平采样左移一位 (0x4411→0x8823), 改低电平采样修复 |
| 命令阶段推挽强驱动 | 通过 | 开漏+40k 上拉 RC 上升沿 1.2µs vs MDC 位宽 1.5µs, 弱驱动 PHY 采不到命令 (全 0xFFFF) |
| 帧末恢复 AF | 通过 | MDC/MDIO 与 MAC 共用引脚, 帧结束后恢复 AF11 还给控制器 (OTYPER/PUPDR 由下次初始化重置) |

### PHY 驱动 (devices/phy_mae0621a)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| MaePhyReset | 通过 | 低脉冲 20ms + 释放 200ms (≥10/30ms 留起振余量), 引脚配置在驱动内自完成 |
| MaePhyGetId | 通过 | reg2/reg3, 含首次读 0xFFFF 重试 |
| MaePhyFind | 通过 | 期望地址优先, 否则扫 0~31, 命中回填 dev->phy_addr |
| MaePhyRegRead/RegWrite | 通过 | 0xFFFF 自动重试×3; GB 超时返回 -1 |
| MaePhyWaitAnComplete | 待实测 | 轮询 BMSR.AN_COMPLETE, 20ms 间隔 |
| 外设隔离 | 通过 | 驱动内只调 GmacMdio*/Gpio*/BasicTimerDelayMs, 不碰 MAC PHYCSR (link/速率判定留测试层) |

### 链路激活 + 收发 (测试 4/5/6/7)

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 测试 4 完整激活 | 待实测 | MaePhyFind → AN → link → SWR → MAC 配置 → DMA 环启动 |
| 测试 5 RX 监控 (~12s) | 待实测 | 轮询 RI (rc_w1), 回显帧长/状态/头部 16 字节 |
| 测试 6 TX 广播帧 | 待实测 | 60 字节最小帧 (dst=FF*6, ethertype 0x88B5), CPC=00 自动补 Pad+CRC, 双槽交替 |
| 测试 7 MAC 内部回环 | 待实测 | MACCR.LM + SYSCFG 选内部 ETH_CLK 作 eth_rx_clk, 免网线验证描述符管线, 逐字节比对 |
| GmacSoftReset | 待实测 | 需 link 后 RX_CLK 在场才完成; 无 link 必超时 (RGMII RX 时钟来自 PHY PA1) |

### 已知问题/修复记录

| 问题 | 原因 | 修复 |
|------|------|------|
| SWR 无 link 必超时 | RGMII RX_CLK 来自 PHY 引脚 (PA1), 无 link 时 PHY 不输出; 复位完成需所有激活时钟域的时钟在场 | SWR 放链路建立后 (测试 4); 超时仅警告不中止, MDIO 在 CSR 时钟域不受影响 |
| SWR 清掉 MDC 分频 | 软复位复位 MDIOAR | 复位后重新 GmacMdioInit |
| 位冲击高电平采样数据左移 | PHY 数据更新贴近 MDC 上升沿, 高电平期间采到"下一位" | 采样点移到 MDC 低电平期间 |
| 位冲击命令阶段全 0xFFFF | 开漏+内部 40k 上拉 RC 上升沿 1.2µs, 弱驱动下 PHY 采不到合法命令 | 命令阶段 MDIO 推挽强驱动 |
| 首次读 REG2 返回 0xFFFF | PHY 特性 (实测复现) | 重试×3 后稳定 0x7B74 |
| PHY ID 与预期不符 | 最初误判为 RTL8211F (ID 0x001C/0xC843) | 实测 0x7B74/0x4411 = MAE0621A, 删除全部 RTL8211 代码 |
