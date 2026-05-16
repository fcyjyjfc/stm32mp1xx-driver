#include <string.h>
#include "stm32mp1xx_stgen.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

static void PrintHex32(char *buf, uint32_t val)
{
    int i;
    for (i = 28; i >= 0; i -= 4)
    {
        uint32_t n = (val >> i) & 0xF;
        *buf++ = n < 10 ? '0' + n : 'A' + n - 10;
    }
    *buf = '\0';
}

void StgenTest(void)
{
    char buf[48];
    uint64_t t0, t1;

    StgenCfg(&STGEN);

    if (StgenIsHalt(&STGEN))
    {
        PRINT("STGEN: halted (debug state)\r\n");
        return;
    }

    PRINT("\r\nSTGEN: CNTFID0=");
    PrintHex32(buf, STGEN.stgenc->CNTFID0);
    PRINT(buf);
    PRINT("\r\n");

    int i;
    for (i = 0; i < 5; i++)
    {
        IwdgKickDog(IWDG2);

        t0 = StgenTim(&STGEN);

        volatile int d;
        for (d = 0; d < 2000000; d++);

        t1 = StgenTim(&STGEN);

        PRINT("  t0_hi=");
        PrintHex32(buf, (uint32_t)(t0 >> 32));
        PRINT(buf);
        PRINT(" lo=");
        PrintHex32(buf, (uint32_t)t0);
        PRINT(buf);

        PRINT("  t1_hi=");
        PrintHex32(buf, (uint32_t)(t1 >> 32));
        PRINT(buf);
        PRINT(" lo=");
        PrintHex32(buf, (uint32_t)t1);
        PRINT(buf);
        PRINT("\r\n");

        char ch;
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 1)
        {
            PRINT("aborted.\r\n");
            break;
        }
    }

    PRINT("STGEN test done.\r\n");
}
