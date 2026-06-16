#include <string.h>
#include "stm32mp1xx_iwdg.h"
#include "test_common.h"

static IwdgCfg_t iwdg_cfg;

void IwdgInit(void)
{
    // ~10s timeout: DIV_128 (div=5), RL=2499
    // t = (2499+1) * 2^(5+2) / 32000 = 2500 * 128 / 32000 = 10.0s
    iwdg_cfg.iwdg_div = IWDG_DIV_128;
    iwdg_cfg.iwdg_rl = 2499;
    iwdg_cfg.iwdg_ew_en = 0;
    iwdg_cfg.iwdg_win_en = 0;

    IwdgCfg(IWDG2, &iwdg_cfg);
}

static void KickTest(void)
{
    PRINT("Kick test running. Press any key to stop.\r\n");

    int count = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);

        PRINT("kick ");
        char buf[16];
        int i;
        int n = count;
        for (i = 0; i < 4; i++)
        {
            buf[3 - i] = '0' + n % 10;
            n /= 10;
        }
        buf[4] = 0;
        PRINT(buf);
        PRINT("...\r\n");
        count++;

        // wait ~5s (rough delay loops)
        volatile int d;
        for (d = 0; d < 12000000; d++);

        char ch;
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 1)
        {
            PRINT("Kick test stopped.\r\n");
            break;
        }
    }
}

static void ResetTest(void)
{
    PRINT("\r\n*** Reset Test ***\r\n");
    PRINT("Watchdog will NOT be kicked.\r\n");
    PRINT("System will reset in ~10s...\r\n");

    // 再喂一次狗，10s 倒计时从此刻开始
    IwdgKickDog(IWDG2);

    // 等待看门狗复位
    while (1);
}

static const MenuEntry_t iwdg_menu[] = {
    { "1", "Kick test (~5s)",              KickTest },
    { "2", "Reset test (stop kick, ~10s)", ResetTest },
};

void IwdgTest(void)
{
    RunSubMenu("IWDG Test", iwdg_menu, sizeof(iwdg_menu) / sizeof(iwdg_menu[0]));
}
