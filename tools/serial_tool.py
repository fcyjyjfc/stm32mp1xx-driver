#!/usr/bin/env python3
"""
简易串口工具 — 发送文本, 实时显示接收
---------------------------------------
用法:
    python serial_tool.py                 # 默认 COM5 @ 115200
    python serial_tool.py COM8            # 指定端口
    python serial_tool.py COM5 -b 9600    # 指定波特率
    python serial_tool.py --eol CRLF      # 行结束符: CR / LF / CRLF / NONE

交互: 输入一行回车即发送(自动补 \\r 回车符, 与 Xshell 等终端一致),
串口接收内容实时打印, Ctrl+C 或输入 q 退出。
"""

import sys
import argparse
import threading

try:
    import serial
except ImportError:
    print("ERROR: pyserial required.  pip install pyserial")
    sys.exit(1)


def rx_loop(ser):
    """后台线程: 读取串口数据并打印"""
    while True:
        n = ser.in_waiting
        if n > 0:
            data = ser.read(n)
        else:
            data = ser.read(1)
        if data:
            sys.stdout.write(data.decode('utf-8', errors='replace'))
            sys.stdout.flush()


def main():
    ap = argparse.ArgumentParser(description='简易串口工具')
    ap.add_argument('port', nargs='?', default='COM5', help='串口号 (默认 COM5)')
    ap.add_argument('-b', '--baud', type=int, default=115200, help='波特率 (默认 115200)')
    args = ap.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except serial.SerialException as e:
        print(f'打开 {args.port} 失败: {e}')
        sys.exit(1)

    print(f'[Connected] {args.port} @ {args.baud} (Ctrl+C 或 q 退出)')
    threading.Thread(target=rx_loop, args=(ser,), daemon=True).start()

    try:
        while True:
            line = input()
            if line in ('q', 'quit', 'exit'):
                break
            ser.write((line + '\r').encode())
            ser.flush()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print('\n[Disconnected]')


if __name__ == '__main__':
    main()
