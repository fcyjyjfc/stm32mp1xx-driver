#include <string.h>
#include "stm32mp1xx_stgen.h"
#include "stm32mp1xx_rcc.h"
#include "test_common.h"

static inline uint64_t CpuTimRead(void)
{
    uint32_t lo, hi;
    /* 0xFC0E0F11 = MRRC p15, 0, r0, r1, c14 (CNTPCT) */
    __asm__ volatile(
        ".word 0xFC0E0F11\n\t"
        "mov %0, r0\n\t"
        "mov %1, r1\n\t"
        : "=r"(lo), "=r"(hi) : : "r0", "r1");
    return ((uint64_t)hi << 32) | lo;
}

static inline uint32_t CpuTimFreq(void)
{
    uint32_t freq;
    /* 0xFE1E0F10 = MRC p15, 0, r0, c14, c0, 0 (CNTFRQ) */
    __asm__ volatile(
        ".word 0xFE1E0F10\n\t"
        "mov %0, r0\n\t"
        : "=r"(freq) : : "r0");
    return freq;
}

void StgenTest(void)
{
    char buf[64];

    PRINT("\r\n===== STGEN (CP15) Test =====\r\n");

    uint32_t freq = CpuTimFreq();
    PRINT("  cntfreq=");
    PrintHex32(buf, freq);
    PRINT(buf);
    PRINT("\r\n");

    uint64_t t0, t1;
    int i;

    for (i = 0; i < 5; i++)
    {
        IwdgKickDog(IWDG2);

        t0 = CpuTimRead();
        volatile int d;
        for (d = 0; d < 2000000; d++);
        t1 = CpuTimRead();

        PRINT("  cnt_hi=");
        PrintHex32(buf, (uint32_t)(t1 >> 32));
        PRINT(buf);
        PRINT(" lo=");
        PrintHex32(buf, (uint32_t)t1);
        PRINT(buf);
        PRINT(" delta=");
        PrintHex32(buf, (uint32_t)(t1 - t0));
        PRINT(buf);
        PRINT("\r\n");

        if (FramePoll(0))
        {
            PRINT("aborted.\r\n");
            break;
        }
    }

    PRINT("STGEN test done.\r\n");
}
