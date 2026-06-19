# frame_sender.py 使用说明

## 前置条件

```bash
pip install pyserial
```

## 启动

```bash
python tools/frame_sender.py COM3            # 默认 115200
python tools/frame_sender.py COM3 -b 9600    # 指定波特率
```

启动后脚本自动接收并打印板子的文本输出（菜单、测试结果等）。

## 指令

| 输入 | 含义 | 发送的帧 |
|------|------|----------|
| `1` | 选择菜单项 1 | `AA 55 02 01 31 32` |
| `10` | 选择菜单项 10 | `AA 55 03 01 31 30 23` |
| `b` | 返回上级菜单 | `AA 55 01 02 03` |
| `echo 48 65 6C 6C 6F` | 回显测试 | `AA 55 06 03 48 65 6C 6C 6F 2D` |
| `q` | 退出脚本 | — |

## 典型交互流程

```
$ python tools/frame_sender.py COM3

[Connected] COM3 @ 115200
[Commands]  <num>=select  b=back  echo <hex>=echo  q=quit

===== Main Menu =====
1. GPIO
2. BTIMER
3. I2C
4. SPI
5. IRQ
6. IWDG
7. STGEN
8. DMA
9. ADC
10. DAC
Select: 8                     <- 输入 8 回车
8
=== DMA TX + RX ===
TX: DMA ring buffer (global)
RX: DMA circular + frame parser (global)
Frame: AA 55 LEN CMD [DATA] XOR
CMD 01: Menu select (DATA=key)
CMD 02: Back/exit
CMD 03: Echo (DATA echoed as hex)
Send CMD 03 to test echo, CMD 02 to exit.
echo 41 42 43                 <- 输入 echo 命令
Echo: 414243
b                             <- 输入 b 返回
Back (exit)
DMA RX stopped.

===== Main Menu =====
...
q                             <- 输入 q 退出脚本
[Disconnected]
```

## 帧格式

```
AA 55 LEN CMD [DATA...] XOR

LEN = 1(CMD) + len(DATA)
XOR = LEN ^ CMD ^ DATA[0] ^ DATA[1] ^ ...
```

| CMD | 值 | DATA | 说明 |
|-----|----|------|------|
| MENU | 0x01 | ASCII 字符串 | 菜单选项编号 |
| BACK | 0x02 | 无 | 返回/退出当前测试 |
| ECHO | 0x03 | 任意字节 | 板子以十六进制回显 |

## 注意事项

- 板子上电后会自动打印主菜单，脚本启动即可看到
- 每次输入一行，回车发送，和以前手动输入体验一致
- `echo` 后跟的是十六进制字节，空格分隔或连写均可（`echo 4869` 等同 `echo 48 69`）
- Ctrl+C 可随时退出脚本
