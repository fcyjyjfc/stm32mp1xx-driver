/*
 * font_gbk16.h — 16x16 点阵字库 (GB2312 全字库 + ASCII 8x16)
 * 由 tools/font_gen.py 从 C:/Windows/Fonts/simsun.ttc 生成, 勿手改
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
#define GBK16_INDEX(a, p) \
    ((((uint32_t)(a) - 0xA1) * GBK16_GB_POS + (uint32_t)(p) - 0xA1) * GBK16_GLYPH_SIZE)

extern const uint8_t g_gbk16_font[GBK16_FONT_SIZE];
extern const uint8_t g_ascii8x16_font[ASCII8X16_SIZE];

#endif /* FONT_GBK16_H_ */
