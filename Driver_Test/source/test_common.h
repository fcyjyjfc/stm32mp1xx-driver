#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "frame_parser.h"

#define PRINT(s)  PrintDma(s)

void TestCommonInit(void);
void PrintDma(const char *s);
void PrintFlush(void);
void NumToStr(char *buf, uint32_t val);
void PrintU32(const char *label, uint32_t val);
void PrintDec(char *buf, int32_t val);
void PrintHex8(uint8_t val);
void PrintHex32(char *buf, uint32_t val);
int  ReadLine(char *buf, int max_len);
int  FramePoll(Frame_t *out);

typedef struct {
    const char *key;
    const char *desc;
    void (*func)(void);
} MenuEntry_t;

void RunSubMenu(const char *title, const MenuEntry_t *items, int count);

#endif /* TEST_COMMON_H_ */
