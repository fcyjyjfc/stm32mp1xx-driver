# Driver Test Status

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
