#include "test_common.h"


void NumToStr(char *buf, uint32_t val)
{
    char rev[12];
    int i = 0;
    do {
        rev[i++] = '0' + val % 10;
        val /= 10;
    } while (val);
    while (i > 0)
        *buf++ = rev[--i];
    *buf = '\0';
}


void PrintU32(const char *label, uint32_t val)
{
    char buf[48];
    int p = 0;
    while (*label) buf[p++] = *label++;
    buf[p++] = ':';
    buf[p++] = ' ';
    NumToStr(buf + p, val);
    while (buf[p]) p++;
    buf[p++] = '\r';
    buf[p++] = '\n';
    UsartWrite(USART4, (void *)buf, p);
}


void PrintDec(char *buf, int32_t val)
{
    if (val < 0)
    {
        *buf++ = '-';
        val = -val;
    }
    char *p = buf;
    do {
        *p++ = '0' + val % 10;
        val /= 10;
    } while (val > 0);
    *p = '\0';
    p--;
    while (buf < p)
    {
        char t = *buf;
        *buf++ = *p;
        *p = t;
        p--;
    }
}


void PrintHex8(uint8_t val)
{
    char buf[3];
    char hex[] = "0123456789ABCDEF";
    buf[0] = hex[val >> 4];
    buf[1] = hex[val & 0xF];
    buf[2] = '\0';
    PRINT(buf);
}


void PrintHex32(char *buf, uint32_t val)
{
    int i;
    for (i = 28; i >= 0; i -= 4)
    {
        uint32_t n = (val >> i) & 0xF;
        *buf++ = n < 10 ? '0' + n : 'A' + n - 10;
    }
    *buf = '\0';
}


int ReadLine(char *buf, int max_len)
{
    int pos = 0;
    char ch;
    while (pos < max_len - 1)
    {
        IwdgKickDog(IWDG2);
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 0)
            continue;
        if (ch == 'S' || ch == 's')
        {
            UsartWrite(USART4, (void *)"\r\n", 2);
            break;
        }
        UsartWrite(USART4, &ch, 1);
        buf[pos++] = ch;
    }
    buf[pos] = '\0';
    return pos;
}


void RunSubMenu(const char *title, const MenuEntry_t *items, int count)
{
    char buf[16];
    while (1)
    {
        IwdgKickDog(IWDG2);
        PRINT("\r\n===== ");
        PRINT(title);
        PRINT(" =====\r\n");
        int i;
        for (i = 0; i < count; i++)
        {
            PRINT(items[i].key);
            PRINT(". ");
            PRINT(items[i].desc);
            PRINT("\r\n");
        }
        PRINT("0. Back\r\n");
        PRINT("Select: ");

        ReadLine(buf, sizeof(buf));

        if (strcmp(buf, "0") == 0)
            return;

        for (i = 0; i < count; i++)
        {
            if (strcmp(buf, items[i].key) == 0)
            {
                items[i].func();
                break;
            }
        }
        if (i == count)
            PRINT("Invalid selection.\r\n");
    }
}
