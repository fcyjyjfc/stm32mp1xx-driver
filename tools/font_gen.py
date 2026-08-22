#!/usr/bin/env python3
"""
font_gen.py — 从系统字体生成 16x16 点阵字库 C 文件
--------------------------------------------------
输出 (写入 Driver_Test/source/):
  - g_gbk16_font[]    GB2312 全字库 87 区 x 94 位, 每字 32B (逐行, 每行 2 字节, 左边 8 像素在高字节)
  - g_ascii8x16_font[]  ASCII 8x16, 256 项, 每字 16B (逐行 8 位)

字体: C:/Windows/Fonts/simsun.ttc (宋体)
用法: python tools/font_gen.py
"""

import os

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "C:/Windows/Fonts/simsun.ttc"
OUT_DIR   = os.path.normpath(os.path.join(os.path.dirname(__file__),
                                          "..", "Driver_Test", "source"))
CELL = 16


def render_glyph(ch, cell_w, cell_h):
    """渲染单字符到 cell_w x cell_h 单色位图, 返回逐行位 (每行 cell_w 位, MSB 为最左)"""
    font = ImageFont.truetype(FONT_PATH, CELL)

    # 先画 16x16 获得字形外框, 再居中 (汉字填满格, ASCII 按自然宽度)
    img = Image.new("L", (CELL, CELL), 0)
    d = ImageDraw.Draw(img)
    d.text((0, 0), ch, font=font, fill=255)
    bbox = img.getbbox()
    if bbox is None:
        return [0] * cell_h

    gw = bbox[2] - bbox[0]
    gh = bbox[3] - bbox[1]

    # ASCII 超过 8px 宽则整体缩到 8px (BOX 滤波), 保持比例
    if gw > 8:
        glyph = img.crop(bbox).resize((8, gh), Image.BOX)
    else:
        glyph = img.crop(bbox)

    out = Image.new("L", (cell_w, cell_h), 0)
    ox = (cell_w - min(gw, 8)) // 2
    oy = (cell_h - gh) // 2
    out.paste(glyph, (ox, oy))

    rows = []
    px = out.load()
    for row in range(cell_h):
        v = 0
        for col in range(cell_w):
            if px[col, row] > 127:
                v |= 1 << (cell_w - 1 - col)
        rows.append(v)
    return rows


def pack(rows, w):
    """逐行位图 -> 每行 ceil(w/8) 字节, MSB 在前 (左边像素在高位)"""
    data = bytearray()
    n = (w + 7) // 8
    for v in rows:
        for k in range(n):
            data.append((v >> (8 * (n - 1 - k))) & 0xFF)
    return bytes(data)


def gen_gb2312():
    """87 区 x 94 位, 已分配字位渲染, 空字位全零"""
    data = bytearray()
    assigned = 0
    for a in range(0xA1, 0xA1 + 87):          # 区码 0xA1..0xF7
        for p in range(0xA1, 0xFF):           # 位码 0xA1..0xFE
            try:
                ch = bytes([a, p]).decode('gb2312')
            except UnicodeDecodeError:
                data += bytes(32)
            else:
                data += pack(render_glyph(ch, CELL, CELL), CELL)
                assigned += 1
    return bytes(data), assigned


def gen_ascii():
    data = bytearray()
    for b in range(256):
        if 0x20 <= b <= 0x7E:
            data += pack(render_glyph(chr(b), 8, CELL), 8)
        else:
            data += bytes(16)
    return bytes(data)


def c_bytes(data, per_line=8):
    lines = []
    for i in range(0, len(data), per_line):
        lines.append('    ' + ', '.join('0x%02X' % v for v in data[i:i + per_line]))
    return ',\n'.join(lines)


def main():
    if not os.path.exists(FONT_PATH):
        print("ERROR: 找不到字体 %s" % FONT_PATH)
        return 1

    gb, assigned = gen_gb2312()
    ascii_tbl = gen_ascii()

    hdr = f"""/*
 * font_gbk16.h — 16x16 点阵字库 (GB2312 全字库 + ASCII 8x16)
 * 由 tools/font_gen.py 从 {FONT_PATH} 生成, 勿手改
 */

#ifndef FONT_GBK16_H_
#define FONT_GBK16_H_

#include <stdint.h>

#define GBK16_GB_AREAS    87
#define GBK16_GB_POS      94
#define GBK16_GLYPH_SIZE  32
#define GBK16_FONT_SIZE   (GBK16_GB_AREAS * GBK16_GB_POS * GBK16_GLYPH_SIZE)
#define ASCII8X16_SIZE    (256 * 16)

/* GB2312 双字节 (a=区码 0xA1-0xF7, p=位码 0xA1-0xFE) -> 字模偏移 */
#define GBK16_INDEX(a, p) \\
    ((((uint32_t)(a) - 0xA1) * GBK16_GB_POS + (uint32_t)(p) - 0xA1) * GBK16_GLYPH_SIZE)

extern const uint8_t g_gbk16_font[GBK16_FONT_SIZE];
extern const uint8_t g_ascii8x16_font[ASCII8X16_SIZE];

#endif /* FONT_GBK16_H_ */
"""

    src = f"""/*
 * font_gbk16.c — 16x16 点阵字库
 * 由 tools/font_gen.py 从 {FONT_PATH} 生成, 勿手改
 */

#include "font_gbk16.h"

/* GB2312 全字库: 87 区 x 94 位 x 32B, 共 {assigned} 字已分配
 * 索引: GBK16_INDEX(区码, 位码) */
const uint8_t g_gbk16_font[GBK16_FONT_SIZE] = {{
{c_bytes(gb)}
}};

/* ASCII 8x16: 256 项 x 16B, 索引 = 字符值 (0x20-0x7E 已渲染, 其余全零) */
const uint8_t g_ascii8x16_font[ASCII8X16_SIZE] = {{
{c_bytes(ascii_tbl)}
}};
"""

    with open(os.path.join(OUT_DIR, "font_gbk16.h"), "w", newline="\n", encoding="utf-8") as f:
        f.write(hdr)
    with open(os.path.join(OUT_DIR, "font_gbk16.c"), "w", newline="\n", encoding="utf-8") as f:
        f.write(src)

    print("GB2312: %d 字已分配, %d B" % (assigned, len(gb)))
    print("ASCII : %d B" % len(ascii_tbl))
    print("输出  : %s" % OUT_DIR)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
