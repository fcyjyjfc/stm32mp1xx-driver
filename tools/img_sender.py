#!/usr/bin/env python3
"""
Image Stream Sender for STM32 LTDC RGB888 Display
-------------------------------------------------
Reads an image (JPG/PNG/BMP... via Pillow, or BMP via stdlib fallback),
streams raw pixel bytes to the board via the frame protocol
(AA 55 len cmd data xor). LTDC RGB888 memory order is B,G,R
(docs/ltdc_overview.md) — the image is converted to BGR once on the PC
side, the firmware does zero pixel conversion.

Non-target-size images are automatically scaled to --width x --height
(default 1280x720; 1080p test uses --width 1920 --height 1080).

Requirements: pip install pyserial pillow
  Without pillow, falls back to stdlib BMP parsing (24-bit only).

Usage:
    python img_sender.py image.jpg COM3 [--wait 3] [--width 1280 --height 720]
"""

import sys
import struct
import argparse
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial required.  pip install pyserial")
    sys.exit(1)

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

FRAME_CMD_IMG = 0x11
FRAME_HDR1    = 0xAA
FRAME_HDR2    = 0x55


def build_frame(data: bytes) -> bytes:
    """AA 55 len cmd data xor"""
    length = 1 + len(data)
    body = bytes([FRAME_CMD_IMG]) + data
    xor = length
    for b in body:
        xor ^= b
    return bytes([FRAME_HDR1, FRAME_HDR2, length]) + body + bytes([xor & 0xFF])


def read_bmp(path: str):
    """Parse 24-bit BMP → (width, height, pixel bytes in B,G,R order)."""
    with open(path, 'rb') as f:
        d = f.read()
    if d[:2] != b'BM':
        raise ValueError('Not a BMP file')
    off_bits = struct.unpack_from('<I', d, 10)[0]
    width    = struct.unpack_from('<i', d, 18)[0]
    height   = struct.unpack_from('<i', d, 22)[0]
    bpp      = struct.unpack_from('<H', d, 28)[0]
    if bpp != 24:
        raise ValueError(f'Only 24-bit BMP supported, got {bpp}bpp')

    h_abs   = abs(height)
    row_stride = (width * 3 + 3) & ~3     # rows are 4-byte aligned
    pixels  = bytearray()
    for y in range(h_abs):
        # height > 0: BMP rows are bottom-up, flip so first byte is the top row
        src_y = y if height < 0 else h_abs - 1 - y
        base  = off_bits + src_y * row_stride
        pixels += d[base:base + width * 3]
    return width, h_abs, bytes(pixels)


def scale_nearest(pix: bytes, sw: int, sh: int, w: int, h: int) -> bytes:
    """Nearest-neighbor scale to w x h (pure Python)."""
    out  = bytearray(w * h * 3)
    row  = bytearray(w * 3)
    xmap = [x * sw // w for x in range(w)]
    for y in range(h):
        sy   = y * sh // h
        base = sy * sw * 3
        for x in range(w):
            sx = xmap[x]
            row[x*3:x*3+3] = pix[base + sx*3:base + sx*3 + 3]
        out[y*w*3:(y+1)*w*3] = row
    return bytes(out)


def load_image(path: str, w: int, h: int):
    """Load image → (w, h, BGR bytes for LTDC). 非目标尺寸自动缩放."""
    if HAS_PIL:
        img = Image.open(path)
        iw, ih = img.size
        print(f'  Source: {iw}x{ih} {img.format} ({path.rsplit(".",1)[-1].upper()})')
        if (iw, ih) != (w, h):
            img = img.resize((w, h))
            print(f'  Scaled: {w}x{h}')
        rgb = img.convert('RGB').tobytes()
        # Pillow 输出 RGB 序, LTDC RGB888 内存序是 BGR: PC 端对调 R/B
        # (仅此一次预处理, 固件仍零转换)
        pix = bytearray(len(rgb))
        for i in range(0, len(rgb), 3):
            pix[i]     = rgb[i + 2]
            pix[i + 1] = rgb[i + 1]
            pix[i + 2] = rgb[i]
        print(f'  Output: {len(pix)/1024/1024:.1f} MB BGR bytes')
        return w, h, bytes(pix)

    # stdlib fallback: BMP only (native BGR, zero conversion)
    iw, ih, pix = read_bmp(path)
    print(f'  Source: {iw}x{ih} 24-bit BMP ({len(pix)/1024:.0f} KB)')
    if (iw, ih) != (w, h):
        pix = scale_nearest(pix, iw, ih, w, h)
        print(f'  Scaled: {w}x{h}')
    return w, h, pix


def main():
    ap = argparse.ArgumentParser(description='Image Stream Sender for STM32 LTDC')
    ap.add_argument('bmp',      help='Image file (JPG/PNG/BMP...; needs pillow, else BMP24)')
    ap.add_argument('port',     help='Serial port, e.g. COM3')
    ap.add_argument('--baud',   type=int, default=460800)
    ap.add_argument('--chunk',  type=int, default=240, help='Max 254 bytes/frame')
    ap.add_argument('--wait',   type=float, default=3.0,
                    help='Seconds to wait after opening port '
                         '(board needs to finish init first)')
    ap.add_argument('--width',  type=int, default=1280, help='Target width (1080p: 1920)')
    ap.add_argument('--height', type=int, default=720,  help='Target height (1080p: 1080)')
    args = ap.parse_args()
    if args.chunk > 254:
        args.chunk = 254

    # ---- load image (non-target-size auto-scaled) ----
    w, h, pix = load_image(args.bmp, args.width, args.height)

    # ---- connect ----
    print(f'  Opening {args.port} @ {args.baud}...')
    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    time.sleep(0.1)
    if args.wait > 0:
        print(f'  Waiting {args.wait:.0f}s for board init...')
        time.sleep(args.wait)

    # ---- send at full speed ----
    sent  = 0
    total = len(pix)
    start = time.time()
    try:
        while sent < total:
            chunk = pix[sent:sent + args.chunk]
            ser.write(build_frame(chunk))
            sent += len(chunk)
            if sent % (args.chunk * 20) == 0 or sent >= total:
                pct = sent * 100 / total
                print(f'\r  [{pct:5.1f}%]  {sent//1024:6d}/{total//1024} KB', end='')
        print()
    except KeyboardInterrupt:
        print('\n  [Aborted]')
    finally:
        ser.close()
        print(f'\n  Done — {sent/1024/1024:.1f} MB in {time.time()-start:.1f}s')

if __name__ == '__main__':
    main()
