
#include "stm32mp1xx_wwdg.h"


volatile WwdgRegs_t *const WWDG = (void *)0x4000A000;


void WwdgCfg(volatile WwdgRegs_t *const wwdg_reg, const WwdgCfg_t *const cfg)
{
    // 使能WWDG并写入计数值，T6也要写1避免立刻复位
    wwdg_reg->CR |= (3 << 6) | (cfg->wwdg_t);

    wwdg_reg->CFR |= cfg->wwdg_div_sel; // 计数分频

    // 设置提前中断
    if (cfg->wwdg_ew_en == 1)
    {
        wwdg_reg->CFR |= 1 << 9; // 使能
        wwdg_reg->CFR &= ~0x7F;
        wwdg_reg->CFR |= cfg->wwdg_ew_comp; // 中断比较值
    }
}


void WwdgKickDog(volatile WwdgRegs_t *const wwdg_reg, const uint32_t t)
{
    wwdg_reg->CR = (t & 0x3F) | (1 << 7);
}

