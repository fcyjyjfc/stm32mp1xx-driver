/*
 * stm32mp1xx_sai.c
 *
 * SAI 驱动 — RM0436 §55
 *
 * 当前实现：
 *   - SaiCfgFillI2S: 填充 I2S Philips 标准配置
 *   - SaiInit:       寄存器初始化（NODIV=0, MCLK=256/512×FS）
 *   - SaiStart/Stop: 使能/关闭
 *   - SaiSendBlocking / SaiRecvBlocking: 轮询收发
 */

#include "stm32mp1xx_sai.h"


/* SAI 外设基址 */
SaiRegs_t *const SAI1 = (void *)0x4400A000U;
SaiRegs_t *const SAI2 = (void *)0x4400B000U;
SaiRegs_t *const SAI3 = (void *)0x4400C000U;


/* ---- CR1 位域 ---- */
#define CR1_MCKEN      (1U << 27)
#define CR1_OSR        (1U << 26)
#define CR1_MCKDIV_POS 20
#define CR1_MCKDIV_Msk (0x3FU << CR1_MCKDIV_POS)
#define CR1_NODIV      (1U << 19)
#define CR1_DMAEN      (1U << 17)
#define CR1_SAIEN      (1U << 16)
#define CR1_OUTDRIV    (1U << 13)
#define CR1_MONO       (1U << 12)
#define CR1_SYNCEN_POS 10
#define CR1_SYNCEN_Msk (3U << CR1_SYNCEN_POS)
#define CR1_CKSTR      (1U << 9)
#define CR1_LSBFIRST   (1U << 8)
#define CR1_DS_POS     5
#define CR1_DS_Msk     (7U << CR1_DS_POS)
#define CR1_PRTCFG_POS 2
#define CR1_PRTCFG_Msk (3U << CR1_PRTCFG_POS)
#define CR1_MODE_POS   0
#define CR1_MODE_Msk   (3U << CR1_MODE_POS)

/* ---- CR2 位域 ---- */
#define CR2_COMP_POS   14
#define CR2_COMP_Msk   (3U << CR2_COMP_POS)
#define CR2_CPL        (1U << 13)
#define CR2_MUTECNT_POS 7
#define CR2_MUTECNT_Msk (0x3FU << CR2_MUTECNT_POS)
#define CR2_MUTEVAL    (1U << 6)
#define CR2_MUTE       (1U << 5)
#define CR2_TRIS       (1U << 4)
#define CR2_FFLUSH     (1U << 3)
#define CR2_FTH_POS    0
#define CR2_FTH_Msk    (7U << CR2_FTH_POS)

/* ---- FRCR 位域 ---- */
#define FRCR_FSOFF     (1U << 18)
#define FRCR_FSPOL     (1U << 17)
#define FRCR_FSDEF     (1U << 16)
#define FRCR_FSALL_POS 8
#define FRCR_FSALL_Msk (0x7FU << FRCR_FSALL_POS)
#define FRCR_FRL_POS   0
#define FRCR_FRL_Msk   (0xFFU << FRCR_FRL_POS)

/* ---- SLOTR 位域 ---- */
#define SLOTR_SLOTEN_Msk  0xFFFF0000U
#define SLOTR_NBSLOT_POS  8
#define SLOTR_NBSLOT_Msk  (0xFU << SLOTR_NBSLOT_POS)
#define SLOTR_SLOTSZ_POS  6
#define SLOTR_SLOTSZ_Msk  (3U << SLOTR_SLOTSZ_POS)
#define SLOTR_FBOFF_POS   0
#define SLOTR_FBOFF_Msk   (0x1FU << SLOTR_FBOFF_POS)

/* ---- SR 位域 ---- */
#define SR_FLVL_POS   16
#define SR_FLVL_Msk   (7U << SR_FLVL_POS)
#define SR_LFSDET     (1U << 6)
#define SR_AFSDET     (1U << 5)
#define SR_CNRDY      (1U << 4)
#define SR_FREQ       (1U << 3)
#define SR_WCKCFG     (1U << 2)
#define SR_MUTEDET    (1U << 1)
#define SR_OVRUDR     (1U << 0)

/* ---- CLRFR 位域 ---- */
#define CLRFR_CLFSDET   (1U << 6)
#define CLRFR_CAFSDET   (1U << 5)
#define CLRFR_CCNRDY    (1U << 4)
#define CLRFR_COVRUDR   (1U << 3)
#define CLRFR_CWCKCFG   (1U << 2)
#define CLRFR_CMUTEDET  (1U << 1)


/* ================================================================== */
/*  内部辅助                                                           */
/* ================================================================== */

/* DS 枚举值 → 实际位宽 */
static const uint8_t sai_ds_bits[] = {0, 0, 8, 10, 16, 20, 24, 32};


/**
 * @brief 用 I2S Philips 标准参数填充配置
 * @param cfg          配置结构体指针
 * @param ker_ck_hz    SAI 内核时钟频率 (Hz)
 * @param sample_rate  采样率 (Hz)
 * @param data_size    数据位宽 (SAI_DS_8/10/16/20/24/32)
 * @param mode         主/从, 收/发 模式
 * @param mclk_en      是否使能 MCLK 输出 (0=关, 1=开)
 */
void SaiCfgFillI2S(SaiCfg_t *cfg, uint32_t ker_ck_hz,
                   uint32_t sample_rate, SaiDataSize_t data_size,
                   SaiMode_t mode, uint8_t mclk_en)
{
    cfg->mode     = mode;
    cfg->protocol = SAI_PROTOCOL_FREE;

    /* 数据位宽 (先算, 后面需要 slot_bits 计算帧长) */
    cfg->data_size = data_size;

    /* 时钟 */
    cfg->ker_ck_hz   = ker_ck_hz;
    cfg->sample_rate = sample_rate;
    cfg->nodiv       = 0;
    cfg->osr         = 0;
    cfg->mcken       = mclk_en ? 1 : 0;

    /* 帧长: I2S 立体声 = 2 × slot_bits */
    {
        uint32_t slot_bits = sai_ds_bits[data_size & 7];
        cfg->frame_len = 2 * slot_bits;
    }

    /* 计算 MCKDIV: NODIV=0, MCLK = ker_ck / MCKDIV, FS = MCLK / 256
     * MCKDIV = ker_ck_hz / (sample_rate * 256) */
    {
        uint32_t mclk = sample_rate * 256UL;
        uint32_t div  = ker_ck_hz / mclk;

        if (div == 0)           div = 1;
        else if (div > 63)      div = 63;
        cfg->mckdiv = (uint8_t)div;
    }
    cfg->lsbfirst  = 0;            /* MSB 先 */
    cfg->ckstr     = 1;            /* SCK 下降沿改变, 上升沿采样 (I2S 标准) */

    /* 帧 — I2S: FSDEF=1, FSOFF=1, FSPOL=0 */
    cfg->fsall     = cfg->frame_len / 2; /* 半帧 */
    cfg->fspol     = 0;
    cfg->fsoff     = 1;            /* FS 在首 bit 前一个 SCK */
    cfg->fsdef     = 1;            /* FS = 帧起始 + 声道标识 */

    /* Slot */
    cfg->nbslot    = 2;
    cfg->slotsz    = SAI_SLOTSZ_EQ_DS;
    cfg->fboff     = 0;
    cfg->sloten    = 0x0003;       /* slot 0 + slot 1 使能 */

    /* 其他 */
    cfg->mono      = 0;
    cfg->tris      = 0;
    cfg->fth       = 0;            /* FIFO 空阈值 */
}


/**
 * @brief 使能 SAI DMA 请求
 */
void SaiDmaEnable(SaiRegs_t *sai, SaiBlock_t block)
{
    sai->Block[block].CR1 |= CR1_DMAEN;
}

/**
 * @brief 关闭 SAI DMA 请求
 */
void SaiDmaDisable(SaiRegs_t *sai, SaiBlock_t block)
{
    sai->Block[block].CR1 &= ~CR1_DMAEN;
}


/**
 * @brief SAI 寄存器初始化 (配置完后不使能)
 * @param sai    SAI 外设基址
 * @param block  子块 A/B
 * @param cfg    配置参数
 */
void SaiInit(SaiRegs_t *sai, SaiBlock_t block, const SaiCfg_t *cfg)
{
    volatile SaiBlockRegs_t *r = &sai->Block[block];
    uint32_t cr1, cr2, frcr, slotr;

    /* 确保关闭 */
    r->CR1 &= ~CR1_SAIEN;
    while (r->CR1 & CR1_SAIEN)
        ;

    /* ---- CR2 (先配, 不影响其他) ---- */
    cr2  = ((uint32_t)cfg->fth << CR2_FTH_POS) & CR2_FTH_Msk;
    cr2 |= cfg->tris ? CR2_TRIS : 0;
    r->CR2 = cr2;

    /* ---- FRCR ---- */
    frcr  = ((uint32_t)(cfg->frame_len - 1) << FRCR_FRL_POS) & FRCR_FRL_Msk;
    frcr |= ((uint32_t)(cfg->fsall - 1) << FRCR_FSALL_POS) & FRCR_FSALL_Msk;
    frcr |= cfg->fspol ? FRCR_FSPOL : 0;
    frcr |= cfg->fsoff ? FRCR_FSOFF : 0;
    frcr |= cfg->fsdef ? FRCR_FSDEF : 0;
    r->FRCR = frcr;

    /* ---- SLOTR ---- */
    slotr  = ((uint32_t)cfg->sloten << 16) & SLOTR_SLOTEN_Msk;
    slotr |= ((uint32_t)(cfg->nbslot - 1) << SLOTR_NBSLOT_POS) & SLOTR_NBSLOT_Msk;
    slotr |= ((uint32_t)cfg->slotsz << SLOTR_SLOTSZ_POS) & SLOTR_SLOTSZ_Msk;
    slotr |= ((uint32_t)cfg->fboff << SLOTR_FBOFF_POS) & SLOTR_FBOFF_Msk;
    r->SLOTR = slotr;

    /* ---- CR1 (最后配, 含 SAIEN=0) ---- */
    cr1  = cfg->mcken   ? CR1_MCKEN   : 0;
    cr1 |= cfg->osr     ? CR1_OSR     : 0;
    cr1 |= ((uint32_t)cfg->mckdiv << CR1_MCKDIV_POS) & CR1_MCKDIV_Msk;
    cr1 |= cfg->nodiv   ? CR1_NODIV   : 0;
    cr1 |= CR1_OUTDRIV;                         /* 使能后立即驱动输出 */
    cr1 |= cfg->mono    ? CR1_MONO    : 0;
    cr1 |= ((uint32_t)cfg->syncen << CR1_SYNCEN_POS) & CR1_SYNCEN_Msk;
    cr1 |= cfg->ckstr   ? CR1_CKSTR   : 0;
    cr1 |= cfg->lsbfirst ? CR1_LSBFIRST : 0;
    cr1 |= ((uint32_t)cfg->data_size << CR1_DS_POS) & CR1_DS_Msk;
    cr1 |= ((uint32_t)cfg->protocol << CR1_PRTCFG_POS) & CR1_PRTCFG_Msk;
    cr1 |= ((uint32_t)cfg->mode << CR1_MODE_POS) & CR1_MODE_Msk;
    r->CR1 = cr1;

    /* 清除残留标志 */
    r->CLRFR = CLRFR_COVRUDR | CLRFR_CAFSDET | CLRFR_CLFSDET
             | CLRFR_CCNRDY | CLRFR_CWCKCFG | CLRFR_CMUTEDET;
}


/* ================================================================== */
/*  启停                                                               */
/* ================================================================== */

/**
 * @brief 使能 SAI 子块 (开始传输)
 * @param sai    SAI 外设基址
 * @param block  子块 A/B
 */
void SaiStart(SaiRegs_t *sai, SaiBlock_t block)
{
    volatile SaiBlockRegs_t *r = &sai->Block[block];
    r->CR1 |= CR1_SAIEN;
}

/**
 * @brief 关闭 SAI 子块 (阻塞等待关闭完成)
 * @param sai    SAI 外设基址
 * @param block  子块 A/B
 */
void SaiStop(SaiRegs_t *sai, SaiBlock_t block)
{
    volatile SaiBlockRegs_t *r = &sai->Block[block];
    r->CR1 &= ~CR1_SAIEN;
    while (r->CR1 & CR1_SAIEN)
        ;
}


/* ================================================================== */
/*  阻塞收发                                                           */
/* ================================================================== */

/**
 * @brief 阻塞发送 (轮询 FREQ 标志)
 * @param sai    SAI 外设基址
 * @param block  子块 A/B
 * @param data   发送数据缓冲区 (32-bit word 数组)
 * @param cnt    word 数
 */
void SaiSendBlocking(SaiRegs_t *sai, SaiBlock_t block,
                     const uint32_t *data, uint32_t cnt)
{
    volatile SaiBlockRegs_t *r = &sai->Block[block];

    for (uint32_t i = 0; i < cnt; i++) {
        while (!(r->SR & SR_FREQ))
            ;
        r->DR = data[i];
    }
}

/**
 * @brief 阻塞接收 (轮询 FREQ 标志)
 * @param sai    SAI 外设基址
 * @param block  子块 A/B
 * @param data   接收数据缓冲区 (32-bit word 数组)
 * @param cnt    word 数
 */
void SaiRecvBlocking(SaiRegs_t *sai, SaiBlock_t block,
                     uint32_t *data, uint32_t cnt)
{
    volatile SaiBlockRegs_t *r = &sai->Block[block];

    for (uint32_t i = 0; i < cnt; i++) {
        while (!(r->SR & SR_FREQ))
            ;
        data[i] = r->DR;
    }
}
