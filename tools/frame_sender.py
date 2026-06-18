#!/usr/bin/env python3
"""
STM32 Test Frame Sender
~~~~~~~~~~~~~~~~~~~~~~~
通过帧协议 (AA 55 LEN CMD DATA XOR) 与测试板交互。

用法:
    python frame_sender.py COM3
    python frame_sender.py COM3 -b 115200

交互指令:
    1, 2, 10 ...  -> 发送 CMD_MENU (0x01), DATA = ASCII 编号
    b             -> 发送 CMD_BACK (0x02), 返回上级菜单
    echo 48 69    -> 发送 CMD_ECHO (0x03), DATA = 十六进制字节
    q             -> 退出
"""

import sys
import serial
import threading
import argparse


CMD_MENU = 0x01
CMD_BACK = 0x02
CMD_ECHO = 0x03


def build_frame(cmd, data=b''):
    length = 1 + len(data)
    body = bytes([cmd]) + data
    xor = length
    for b in body:
        xor ^= b
    return bytes([0xAA, 0x55, length]) + body + bytes([xor])


def rx_thread(ser):
    while True:
        try:
            data = ser.read(ser.in_waiting or 1)
            if data:
                sys.stdout.write(data.decode('ascii', errors='replace'))
                sys.stdout.flush()
        except (serial.SerialException, OSError):
            break


def main():
    ap = argparse.ArgumentParser(description='STM32 Test Frame Sender')
    ap.add_argument('port', help='Serial port (COM3, /dev/ttyUSB0 ...)')
    ap.add_argument('-b', '--baud', type=int, default=115200)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)

    t = threading.Thread(target=rx_thread, args=(ser,), daemon=True)
    t.start()

    print(f'[Connected] {args.port} @ {args.baud}')
    print('[Commands]  <num>=select  b=back  echo <hex>=echo  q=quit')
    print()

    try:
        while True:
            line = input().strip()
            if not line:
                continue
            if line.lower() == 'q':
                break
            elif line.lower() == 'b':
                ser.write(build_frame(CMD_BACK))
            elif line.lower().startswith('echo '):
                try:
                    data = bytes.fromhex(line[5:].replace(' ', ''))
                except ValueError:
                    print('[ERROR] invalid hex')
                    continue
                ser.write(build_frame(CMD_ECHO, data))
            else:
                ser.write(build_frame(CMD_MENU, line.encode('ascii')))
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        ser.close()
        print('\n[Disconnected]')


if __name__ == '__main__':
    main()
