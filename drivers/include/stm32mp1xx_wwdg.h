
typedef struct {
    uint32_t CR;
    uint32_t CFR;
    uint32_t SR;
} WwdgRegs_t;


typedef enum {
    WWDG_DIV_1      = 0,
    WWDG_DIV_2      = 1,
    WWDG_DIV_4      = 2,
    WWDG_DIV_8      = 3,
    WWDG_DIV_16     = 4,
    WWDG_DIV_32     = 5,
    WWDG_DIV_64     = 6,
    WWDG_DIV_128    = 7
} WwdgDivSel_t;


// 超时计算
// t_timeout = 4096 * (2 ^ div_sel) * (t + 1) / clk
typedef struct {
    uint32_t        wwdg_ew_en      : 1;
    uint32_t        wwdg_ew_comp    : 7;
    uint32_t        wwdg_t;         : 6;
    WwdgDivSel_t    wwdg_div_sel;
} WwdgCfg_t;


extern volatile WwdgRegs_t *const WWDG;
