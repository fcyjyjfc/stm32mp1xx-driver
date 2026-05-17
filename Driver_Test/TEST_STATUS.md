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

## I2C

| 测试项 | 状态 | 说明 |
|--------|------|------|
| I2C 初始化 | 待测 | PF14(AF5)=SCL, PF15(AF5)=SDA, OD, I2C1 |
| EEPROM (0xA0) 写入 | 待测 | 0..31 写入地址0，0xAA 批量写入地址32 |
| EEPROM (0xA0) 读取 | 待测 | 回读校验，数据比对 |
| Sensor (0x80) 温度读取 | 待测 | 命令 0xE3，读 2 字节，计算公式 17572×raw/65536-4685 |
| Sensor (0x80) 湿度读取 | 待测 | 命令 0xE5，读 2 字节，计算公式 125×raw/65536-6 |
| 二级菜单交互 | 待测 | 1=EEPROM, 2=Sensor, 0=返回 |

## SPI

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
