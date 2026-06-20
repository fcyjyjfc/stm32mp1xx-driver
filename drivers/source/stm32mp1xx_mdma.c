#include "stm32mp1xx_mdma.h"


volatile MdmaRegs_t *const MDMA = (void *)0x58000000;

const MdmaCfg_t MDMA_CFG_DEFAULT = {0};


/*
 * 配置 MDMA 通道, EN=0 时调用。
 * 一次性写入 TCR/BNDTR/SAR/DAR/BRUR/LAR/TBR/MAR/MDR/CR。
 */
void MdmaCfg(volatile MdmaRegs_t *mdma, const MdmaCfg_t *cfg)
{
    volatile MdmaChRegs_t *ch = &mdma->CH[cfg->ch];

    ch->CR &= ~1u;  /* 确保 EN=0, 寄存器可写 */

    ch->TCR = (cfg->bwm    << 31) | (cfg->swrm   << 30) |
              (cfg->trgm   << 28) | (cfg->pam    << 26) |
              (cfg->pke    << 25) | (cfg->tlen   << 18) |
              ((cfg->dburst - cfg->dsize) << 15) | ((cfg->sburst - cfg->ssize) << 12) |
              (cfg->dincos << 10) | (cfg->sincos << 8)  |
              (cfg->dsize  << 6)  | (cfg->ssize  << 4)  |
              (cfg->dinc   << 2)  | (cfg->sinc   << 0);

    ch->BNDTR = (cfg->brc   << 20) | (cfg->brdum << 19) |
                (cfg->brsum << 18) | (cfg->bndt  << 0);

    ch->SAR  = cfg->src_addr;
    ch->DAR  = cfg->dst_addr;
    ch->BRUR = ((uint32_t)cfg->duv << 16) | cfg->suv;
    ch->LAR  = cfg->link_addr;
    ch->TBR  = cfg->tsel;
    ch->MAR  = cfg->mask_addr;
    ch->MDR  = cfg->mask_data;

    /* CR 最后写: 设置优先级/中断/字节序, 不置 EN */
    ch->CR = (cfg->wex   << 14) | (cfg->hex    << 13) |
             (cfg->bex   << 12) | (cfg->pl     << 6)  |
             (cfg->tcie  << 5)  | (cfg->btie   << 4)  |
             (cfg->brtie << 3)  | (cfg->ctcie  << 2)  |
             (cfg->teie  << 1);
}


/* 使能通道, 配置完成后调用 */
void MdmaEnable(volatile MdmaRegs_t *mdma, uint32_t ch)
{
    mdma->CH[ch].CR |= 1u;
}


/* 禁用通道, 当前 buffer 传输完成后停止, CTCIF 置位确认 */
void MdmaDisable(volatile MdmaRegs_t *mdma, uint32_t ch)
{
    mdma->CH[ch].CR &= ~1u;
}


/* 软件触发, SWRM=1 时用此发起传输 */
void MdmaSwTrig(volatile MdmaRegs_t *mdma, uint32_t ch)
{
    mdma->CH[ch].CR |= (1u << 16);
}


/* 读全局中断状态, bit[x]=1 表示通道 x 有中断待处理 */
uint32_t MdmaGetGisr(volatile MdmaRegs_t *mdma)
{
    return mdma->GISR0;
}


/* 读通道中断状态, 确定具体事件 (TCIF/BTIF/BRTIF/CTCIF/TEIF) */
uint32_t MdmaGetChIsr(volatile MdmaRegs_t *mdma, uint32_t ch)
{
    return mdma->CH[ch].ISR;
}


/* 读通道错误状态, TEIF 置位时调用以获取错误类型和地址 */
uint32_t MdmaGetChEsr(volatile MdmaRegs_t *mdma, uint32_t ch)
{
    return mdma->CH[ch].ESR;
}


/* 清除通道中断标志, flags 用 MDMA_FLAG_xxx 组合 */
void MdmaClearChIf(volatile MdmaRegs_t *mdma, uint32_t ch, uint32_t flags)
{
    mdma->CH[ch].IFCR = flags;
}


/* ========== MdmaMemcpy: 非阻塞线性拷贝 ========== */

static uint32_t g_mdma_cpy_ch;

/*
 * 初始化 memcpy 使用的通道, 配置为 64-bit M2M 软件触发。
 * 调用一次即可, 之后多次调用 MdmaMemcpy。
 */
void MdmaMemcpyInit(uint32_t ch)
{
    g_mdma_cpy_ch = ch;

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch      = ch;
    cfg.ssize   = MDMA_DATA_64BIT;
    cfg.dsize   = MDMA_DATA_64BIT;
    cfg.sinc    = MDMA_INC_INCR;
    cfg.dinc    = MDMA_INC_INCR;
    cfg.sincos  = MDMA_DATA_64BIT;
    cfg.dincos  = MDMA_DATA_64BIT;
    cfg.sburst  = MDMA_BSIZE_128B;
    cfg.dburst  = MDMA_BSIZE_128B;
    cfg.tlen    = 127;  /* buffer 长度 = 127+1 = 128 字节 */
    cfg.trgm    = MDMA_TRGM_BLOCK;
    cfg.swrm    = 1;    /* 软件触发模式 */
    cfg.ctcie   = 1;    /* channel 完成中断使能 */
    MdmaCfg(MDMA, &cfg);
}


/*
 * 非阻塞拷贝, 启动后立即返回。
 * size <= 65536, 须为 8 的倍数。
 * 完成判断: MdmaGetChIsr(MDMA, ch) & MDMA_FLAG_CTCIF
 */
void MdmaMemcpy(void *dst, const void *src, uint32_t size)
{
    volatile MdmaChRegs_t *c = &MDMA->CH[g_mdma_cpy_ch];

    c->BNDTR = size;
    c->SAR   = (uint32_t)src;
    c->DAR   = (uint32_t)dst;
    c->CR   |= 1u;
    c->CR   |= (1u << 16);
}
