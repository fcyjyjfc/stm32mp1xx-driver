#include "stm32mp1xx_iwdg.h"

static IwdgCfg_t iwdg2_cfg;

void IwdgInit(void)
{
    iwdg2_cfg.iwdg_div = IWDG_DIV_256;
    iwdg2_cfg.iwdg_rl = 4095;
    iwdg2_cfg.iwdg_ew_en = 0;
    iwdg2_cfg.iwdg_win_en = 0;

    IwdgCfg(IWDG2, &iwdg2_cfg);
}
