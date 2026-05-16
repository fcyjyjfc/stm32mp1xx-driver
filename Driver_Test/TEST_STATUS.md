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
| 配置 | 待测 | 已改为 DIV_256，RL=4095，溢出时间 ~32.7s |
| IwdgCfg 初始化 | 待测 | 禁止 EW/EWIN，需烧录后确认 |
| IwdgKickDog 喂狗 | 待测 | 已在 ReadLine、GpioTest 主循环中插入 |

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

- [ ] 代码未写
