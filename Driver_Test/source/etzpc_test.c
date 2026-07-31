/*
 * etzpc_test.c — ETZPC 配置只读 dump
 *
 * ETZPC 本身 write-secure: 非安全侧只能读, 不能配置
 * decprot 编码: 0=RW secure, 1=W secure, 2=M4 隔离, 3=non-secure
 */

#include "common_test.h"
#include "stm32mp1xx_etzpc.h"


static void PrintHexLabel(const char *label, uint32_t val)
{
    char buf[48];
    int p = 0;
    while (*label) buf[p++] = *label++;
    buf[p++] = ':';
    buf[p++] = ' ';
    buf[p++] = '0';
    buf[p++] = 'x';
    PrintHex32(buf + p, val);
    while (buf[p]) p++;
    buf[p++] = '\r';
    buf[p++] = '\n';
    buf[p] = '\0';
    PRINT(buf);
}

static void PrintDecprotRow(uint32_t reg_idx)
{
    uint32_t base = reg_idx * 16;
    uint32_t hi   = base + 15;
    char buf[64];
    int p = 0;
    int i;

    buf[p++] = 'D';
    buf[p++] = '0' + reg_idx;
    buf[p++] = ' ';
    buf[p++] = '[';
    buf[p++] = '0' + base / 10;
    buf[p++] = '0' + base % 10;
    buf[p++] = '-';
    buf[p++] = '0' + hi / 10;
    buf[p++] = '0' + hi % 10;
    buf[p++] = ']';
    buf[p++] = ':';

    for (i = 0; i < 16; i++)
    {
        buf[p++] = ' ';
        buf[p++] = '0' + EtzpcDecprotGet(ETZPC, base + i);
    }
    buf[p++] = '\r';
    buf[p++] = '\n';
    buf[p] = '\0';
    PRINT(buf);
}


void EtzpcTest(void)
{
    uint32_t i;

    PRINT("\r\n=== ETZPC Dump ===\r\n");
    PRINT("decprot: 0=RW secure 1=W secure 2=M4 isol 3=non-secure\r\n");

    PRINT("\r\n-- TZMA --\r\n");
    PrintHexLabel("TZMA0_SIZE", ETZPC->TZMA0_SIZE);
    PrintHexLabel("TZMA1_SIZE", ETZPC->TZMA1_SIZE);

    PRINT("\r\n-- DECPROT (Table 94 indices) --\r\n");
    for (i = 0; i < 6; i++)
        PrintDecprotRow(i);

    PRINT("\r\n-- LOCK --\r\n");
    PrintHexLabel("DECPROT_LOCK0", ETZPC->DECPROT_LOCK0);
    PrintHexLabel("DECPROT_LOCK1", ETZPC->DECPROT_LOCK1);
    PrintHexLabel("DECPROT_LOCK2", ETZPC->DECPROT_LOCK2);

    PRINT("=== Dump Done ===\r\n");
}
