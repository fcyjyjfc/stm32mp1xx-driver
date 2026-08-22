#!/usr/bin/env python3
"""
Serial Typewriter for STM32 LTDC — 串口打字器
---------------------------------------------
每行输入 + Enter 发送到板子显示 (UTF-8 -> GB2312, 帧协议
AA 55 len cmd data xor, 命令 0x12)。中文英文混排均可。

命令:
  /clr   清屏
  /quit  退出 (发 BACK 帧, 板子返回菜单)

Usage:
    python typewriter.py COM3 [--baud 460800] [--wait 3]
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial required.  pip install pyserial")
    sys.exit(1)

FRAME_CMD_TXT  = 0x12
FRAME_CMD_BACK = 0x02
FRAME_HDR1     = 0xAA
FRAME_HDR2     = 0x55
CHUNK          = 240   # 帧 payload 上限 254


def build_frame(cmd, data):
    """AA 55 len cmd data xor"""
    length = 1 + len(data)
    body = bytes([cmd]) + data
    xor = length
    for b in body:
        xor ^= b
    return bytes([FRAME_HDR1, FRAME_HDR2, length]) + body + bytes([xor & 0xFF])


def main():
    ap = argparse.ArgumentParser(description='Serial Typewriter for STM32 LTDC')
    ap.add_argument('port', help='Serial port, e.g. COM3')
    ap.add_argument('--baud', type=int, default=460800)
    ap.add_argument('--wait', type=float, default=3.0,
                    help='Seconds to wait after opening port '
                         '(board needs to finish init first)')
    args = ap.parse_args()

    print(f'  Opening {args.port} @ {args.baud}...')
    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    time.sleep(0.1)
    if args.wait > 0:
        print(f'  Waiting {args.wait:.0f}s for board init...')
        time.sleep(args.wait)

    print('  Type text + Enter.  /clr = clear screen, /quit = exit, Ctrl+C = quit')
    try:
        while True:
            try:
                line = input('> ')
            except EOFError:
                break

            cmd = line.strip()
            if cmd == '/quit' or cmd == '/q':
                ser.write(build_frame(FRAME_CMD_BACK, b''))
                break
            if cmd == '/clr' or cmd == '/c':
                ser.write(build_frame(FRAME_CMD_TXT, b'\x0c'))
                continue

            # UTF-8 -> GB2312 (字库按 GB2312 区/位码索引), 行尾加换行
            data = line.encode('gb2312', 'replace') + b'\n'
            for i in range(0, len(data), CHUNK):
                ser.write(build_frame(FRAME_CMD_TXT, data[i:i + CHUNK]))
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print('\n  Done.')


if __name__ == '__main__':
    main()
