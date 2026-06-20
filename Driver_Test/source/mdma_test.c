#include "test_common.h"
#include "stm32mp1xx_mdma.h"
#include "stm32mp1xx_rcc.h"

static uint8_t g_src[131072] __attribute__((aligned(8)));
static uint8_t g_dst[131072] __attribute__((aligned(8)));

static void TestM2mSw(void)
{
    uint32_t i;
    uint16_t *src16 = (uint16_t *)g_src;
    uint16_t *dst16 = (uint16_t *)g_dst;
    const uint32_t count = 16384;  /* 16384 × 2B = 32KB */

    for (i = 0; i < count; i++)
        src16[i] = (uint16_t)i;
    for (i = 0; i < count; i++)
        dst16[i] = 0;

    MdmaMemcpyInit(0);
    MdmaMemcpy(g_dst, g_src, count * 2);

    while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF))
        IwdgKickDog(IWDG2);
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < count; i++)
    {
        if (dst16[i] != (uint16_t)i)
        {
            errors++;
            if (errors <= 5)
                PrintU32("Mismatch at word", i);
        }
    }

    if (errors == 0)
        PRINT("32KB M2M copy PASS\r\n");
    else
    {
        PRINT("32KB M2M copy FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static void TestBlockRepeat(void)
{
    uint32_t i;
    uint16_t *src16 = (uint16_t *)g_src;
    uint16_t *dst16 = (uint16_t *)g_dst;
    const uint32_t count = 65536;  /* 65536 × 2B = 128KB, 0x0000~0xFFFF */

    for (i = 0; i < count; i++)
        src16[i] = (uint16_t)i;
    for (i = 0; i < count; i++)
        dst16[i] = 0;

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch      = 0;
    cfg.ssize   = MDMA_DATA_64BIT;
    cfg.dsize   = MDMA_DATA_64BIT;
    cfg.sinc    = MDMA_INC_INCR;
    cfg.dinc    = MDMA_INC_INCR;
    cfg.sincos  = MDMA_DATA_64BIT;
    cfg.dincos  = MDMA_DATA_64BIT;
    cfg.sburst  = MDMA_BSIZE_128B;
    cfg.dburst  = MDMA_BSIZE_128B;
    cfg.tlen    = 127;  /* buffer = 128 bytes */
    cfg.trgm    = MDMA_TRGM_REP_BLOCK;
    cfg.swrm    = 1;
    cfg.ctcie   = 1;
    cfg.bndt    = 65536;
    cfg.brc     = 1;    /* 2 blocks (BRC+1=2), total 128KB */
    cfg.suv     = 0;    /* linear: address auto-increments, no extra offset */
    cfg.duv     = 0;
    cfg.src_addr = (uint32_t)g_src;
    cfg.dst_addr = (uint32_t)g_dst;
    MdmaCfg(MDMA, &cfg);
    MdmaEnable(MDMA, 0);
    MdmaSwTrig(MDMA, 0);

    while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF))
        IwdgKickDog(IWDG2);
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < count; i++)
    {
        if (dst16[i] != (uint16_t)i)
        {
            errors++;
            if (errors <= 5)
                PrintU32("Mismatch at word", i);
        }
    }

    if (errors == 0)
        PRINT("128KB block-repeat PASS\r\n");
    else
    {
        PRINT("128KB block-repeat FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static MdmaLinkNode_t g_nodes[3];

static void TestLinkedList(void)
{
    uint32_t i;
    uint16_t *src16 = (uint16_t *)g_src;
    uint16_t *dst16 = (uint16_t *)g_dst;
    const uint32_t count = 2560;  /* 2560 × 2B = 5KB */

    for (i = 0; i < count; i++)
        src16[i] = (uint16_t)i;
    for (i = 0; i < count; i++)
        dst16[i] = 0;

    /* TCR: 32-bit, incr 4B, 4-beat burst, buffer=16B, SW trig, TRGM=channel */
    uint32_t tcr = (1u << 30) | (3u << 28) | (15u << 18) |
                   (2u << 15) | (2u << 12) | (2u << 10) | (2u << 8) |
                   (2u << 6)  | (2u << 4)  | (2u << 2)  | (2u << 0);

    g_nodes[0].TCR   = tcr;
    g_nodes[0].BNDTR = 1024;
    g_nodes[0].SAR   = (uint32_t)&g_src[1024];
    g_nodes[0].DAR   = (uint32_t)&g_dst[1024];
    g_nodes[0].BRUR  = 0;
    g_nodes[0].LAR   = (uint32_t)&g_nodes[1];
    g_nodes[0].TBR   = 0;
    g_nodes[0].RSVD  = 0;
    g_nodes[0].MAR   = 0;
    g_nodes[0].MDR   = 0;

    g_nodes[1].TCR   = tcr;
    g_nodes[1].BNDTR = 1024;
    g_nodes[1].SAR   = (uint32_t)&g_src[2048];
    g_nodes[1].DAR   = (uint32_t)&g_dst[2048];
    g_nodes[1].BRUR  = 0;
    g_nodes[1].LAR   = (uint32_t)&g_nodes[2];
    g_nodes[1].TBR   = 0;
    g_nodes[1].RSVD  = 0;
    g_nodes[1].MAR   = 0;
    g_nodes[1].MDR   = 0;

    /* Node 2: 2KB via repeat (BRC=1, 2×1KB), end of list */
    g_nodes[2].TCR   = tcr;
    g_nodes[2].BNDTR = (1u << 20) | 1024;  /* BRC=1, BNDT=1024 */
    g_nodes[2].SAR   = (uint32_t)&g_src[3072];
    g_nodes[2].DAR   = (uint32_t)&g_dst[3072];
    g_nodes[2].BRUR  = 0;
    g_nodes[2].LAR   = 0;
    g_nodes[2].TBR   = 0;
    g_nodes[2].RSVD  = 0;
    g_nodes[2].MAR   = 0;
    g_nodes[2].MDR   = 0;

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch        = 0;
    cfg.ssize     = MDMA_DATA_32BIT;
    cfg.dsize     = MDMA_DATA_32BIT;
    cfg.sinc      = MDMA_INC_INCR;
    cfg.dinc      = MDMA_INC_INCR;
    cfg.sincos    = MDMA_DATA_32BIT;
    cfg.dincos    = MDMA_DATA_32BIT;
    cfg.sburst    = MDMA_BSIZE_16B;
    cfg.dburst    = MDMA_BSIZE_16B;
    cfg.tlen      = 15;   /* buffer = 16 bytes */
    cfg.trgm      = MDMA_TRGM_CHANNEL;
    cfg.swrm      = 1;
    cfg.ctcie     = 1;
    cfg.bndt      = 1024;
    cfg.src_addr  = (uint32_t)g_src;
    cfg.dst_addr  = (uint32_t)g_dst;
    cfg.link_addr = (uint32_t)&g_nodes[0];
    MdmaCfg(MDMA, &cfg);
    MdmaEnable(MDMA, 0);
    MdmaSwTrig(MDMA, 0);

    while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF))
        IwdgKickDog(IWDG2);
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < count; i++)
    {
        if (dst16[i] != (uint16_t)i)
        {
            errors++;
            if (errors <= 5)
                PrintU32("Mismatch at word", i);
        }
    }

    if (errors == 0)
        PRINT("5KB linked-list + repeat PASS\r\n");
    else
    {
        PRINT("5KB linked-list + repeat FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static void TestEndianExchange(void)
{
    uint32_t i;
    uint64_t *src64 = (uint64_t *)g_src;
    uint64_t *dst64 = (uint64_t *)g_dst;
    const uint32_t count = 512;  /* 512 double-words = 4KB */

    for (i = 0; i < count; i++)
        src64[i] = 0x0102030405060708ULL /*+ i * 0x0808080808080808ULL*/;
    for (i = 0; i < count; i++)
        dst64[i] = 0;

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch      = 0;
    cfg.ssize   = MDMA_DATA_64BIT;
    cfg.dsize   = MDMA_DATA_64BIT;
    cfg.sinc    = MDMA_INC_INCR;
    cfg.dinc    = MDMA_INC_INCR;
    cfg.sincos  = MDMA_DATA_64BIT;
    cfg.dincos  = MDMA_DATA_64BIT;
    cfg.sburst  = MDMA_BSIZE_128B;
    cfg.dburst  = MDMA_BSIZE_128B;
    cfg.tlen    = 127;  /* buffer = 128 bytes */
    cfg.trgm    = MDMA_TRGM_BLOCK;
    cfg.swrm    = 1;
    cfg.ctcie   = 1;
    cfg.bex     = 1;    /* byte swap within half-word */
    cfg.hex     = 1;    /* half-word swap within word */
    cfg.wex     = 1;    /* word swap within double-word */
    cfg.bndt    = count * 8;
    cfg.src_addr = (uint32_t)src64;
    cfg.dst_addr = (uint32_t)dst64;
    MdmaCfg(MDMA, &cfg);
    MdmaEnable(MDMA, 0);
    MdmaSwTrig(MDMA, 0);

    while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF))
        IwdgKickDog(IWDG2);
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < count; i++)
    {
        uint64_t expected = __builtin_bswap64(src64[i]);
        if (dst64[i] != expected)
        {
            errors++;
            if (errors <= 5)
                PrintU32("Mismatch at dword", i);
        }
    }

    if (errors == 0)
        PRINT("4KB endian exchange (64-bit) PASS\r\n");
    else
    {
        PRINT("4KB endian exchange (64-bit) FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static void TestReverse(void)
{
    uint32_t i;
    uint16_t *src16 = (uint16_t *)g_src;
    uint16_t *dst16 = (uint16_t *)g_dst;
    const uint32_t count = 65536;  /* 32768 × 4B = 128KB */

    for (i = 0; i < count; i++)
        src16[i] = i;
    for (i = 0; i < count; i++)
        dst16[i] = 0;

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch      = 0;
    cfg.ssize   = MDMA_DATA_16BIT;
    cfg.dsize   = MDMA_DATA_16BIT;
    cfg.sinc    = MDMA_INC_DECR;
    cfg.dinc    = MDMA_INC_INCR;
    cfg.sincos  = MDMA_DATA_16BIT;
    cfg.dincos  = MDMA_DATA_16BIT;
    cfg.sburst  = MDMA_BSIZE_32B;
    cfg.dburst  = MDMA_BSIZE_32B;
    cfg.tlen    = 31;   /* buffer = 32 bytes (>= dest burst 16×2B) */
    cfg.trgm    = MDMA_TRGM_REP_BLOCK;
    cfg.swrm    = 1;
    cfg.ctcie   = 1;
    cfg.bndt    = 65536;
    cfg.brc     = 1;    /* 2 blocks, total 128KB */
    cfg.suv     = 0;
    cfg.duv     = 0;
    cfg.src_addr = (uint32_t)&src16[count - 1];
    cfg.dst_addr = (uint32_t)g_dst;
    MdmaCfg(MDMA, &cfg);
    MdmaEnable(MDMA, 0);
    MdmaSwTrig(MDMA, 0);

    uint32_t isr;
    while (1)
    {
        IwdgKickDog(IWDG2);
        isr = MdmaGetChIsr(MDMA, 0);
        if (isr & MDMA_FLAG_CTCIF)
            break;
        if (isr & MDMA_FLAG_TEIF)
        {
            PrintU32("ISR", isr);
            PrintU32("ESR", MdmaGetChEsr(MDMA, 0));
            MdmaClearChIf(MDMA, 0, MDMA_FLAG_TEIF);
            PRINT("Transfer error, abort.\r\n");
            MdmaDisable(MDMA, 0);
            return;
        }
    }
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < count; i++)
    {
        if (dst16[i] != count - 1 - i)
        {
            errors++;
            if (errors <= 5)
                PrintU32("Mismatch at word", i);
        }
    }

    if (errors == 0)
        PRINT("128KB reverse copy PASS\r\n");
    else
    {
        PRINT("128KB reverse copy FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static void TestStrideExtract(void)
{
    uint32_t i, j;
    uint16_t *src16 = (uint16_t *)g_src;
    uint16_t *dst16 = (uint16_t *)g_dst;
    const uint32_t cols = 10;
    const uint32_t rows = 16;

    for (i = 0; i < rows * cols; i++)
        src16[i] = (uint16_t)i;
    for (i = 0; i < rows * 5; i++)
        dst16[i] = 0;

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch      = 0;
    cfg.ssize   = MDMA_DATA_16BIT;
    cfg.dsize   = MDMA_DATA_16BIT;
    cfg.sinc    = MDMA_INC_INCR;
    cfg.dinc    = MDMA_INC_INCR;
    cfg.sincos  = MDMA_DATA_32BIT;  /* 4B stride: skip one uint16_t */
    cfg.dincos  = MDMA_DATA_16BIT;
    cfg.sburst  = MDMA_BSIZE_2B;
    cfg.dburst  = MDMA_BSIZE_2B;
    cfg.tlen    = 1;    /* buffer = 2 bytes */
    cfg.trgm    = MDMA_TRGM_REP_BLOCK;
    cfg.swrm    = 1;
    cfg.ctcie   = 1;
    cfg.bndt    = 10;   /* 5 elements × 2B */
    cfg.brc     = rows - 1;
    cfg.suv     = 0;    /* 5×4B = 20B = row width, auto-aligned */
    cfg.duv     = 0;
    cfg.src_addr = (uint32_t)&src16[1];
    cfg.dst_addr = (uint32_t)g_dst;
    MdmaCfg(MDMA, &cfg);
    MdmaEnable(MDMA, 0);
    MdmaSwTrig(MDMA, 0);

    while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF))
        IwdgKickDog(IWDG2);
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < rows; i++)
    {
        for (j = 0; j < 5; j++)
        {
            uint16_t expected = (uint16_t)(i * cols + 1 + j * 2);
            if (dst16[i * 5 + j] != expected)
            {
                errors++;
                if (errors <= 5)
                    PrintU32("Mismatch at", i * 5 + j);
            }
        }
    }

    if (errors == 0)
        PRINT("Stride extract (16x10 odd cols) PASS\r\n");
    else
    {
        PRINT("Stride extract FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static void TestSubRegion(void)
{
    uint32_t i, j;
    uint16_t *src16 = (uint16_t *)g_src;
    uint16_t *dst16 = (uint16_t *)g_dst;
    const uint32_t total_cols = 32;
    const uint32_t total_rows = 32;
    const uint32_t sub_row = 4, sub_col = 4;
    const uint32_t sub_w = 8, sub_h = 8;

    for (i = 0; i < total_rows * total_cols; i++)
        src16[i] = (uint16_t)i;
    for (i = 0; i < sub_w * sub_h; i++)
        dst16[i] = 0;

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch      = 0;
    cfg.ssize   = MDMA_DATA_16BIT;
    cfg.dsize   = MDMA_DATA_16BIT;
    cfg.sinc    = MDMA_INC_INCR;
    cfg.dinc    = MDMA_INC_INCR;
    cfg.sincos  = MDMA_DATA_16BIT;
    cfg.dincos  = MDMA_DATA_16BIT;
    cfg.sburst  = MDMA_BSIZE_8B;
    cfg.dburst  = MDMA_BSIZE_8B;
    cfg.tlen    = 7;    /* buffer = 8 bytes */
    cfg.trgm    = MDMA_TRGM_REP_BLOCK;
    cfg.swrm    = 1;
    cfg.ctcie   = 1;
    cfg.bndt    = sub_w * 2;
    cfg.brc     = sub_h - 1;
    cfg.suv     = (total_cols - sub_w) * 2;  /* 48: skip to next row */
    cfg.duv     = 0;
    cfg.src_addr = (uint32_t)&src16[sub_row * total_cols + sub_col];
    cfg.dst_addr = (uint32_t)g_dst;
    MdmaCfg(MDMA, &cfg);
    MdmaEnable(MDMA, 0);
    MdmaSwTrig(MDMA, 0);

    while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF))
        IwdgKickDog(IWDG2);
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < sub_h; i++)
    {
        for (j = 0; j < sub_w; j++)
        {
            uint16_t expected = (uint16_t)((sub_row + i) * total_cols + sub_col + j);
            if (dst16[i * sub_w + j] != expected)
            {
                errors++;
                if (errors <= 5)
                    PrintU32("Mismatch at", i * sub_w + j);
            }
        }
    }

    if (errors == 0)
        PRINT("Sub-region (8x8 from 32x32) PASS\r\n");
    else
    {
        PRINT("Sub-region FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static MdmaLinkNode_t g_xpose_nodes[127];

static void TestTranspose(void)
{
    uint32_t i, j;
    uint32_t *src32 = (uint32_t *)g_src;
    uint32_t *dst32 = (uint32_t *)g_dst;
    const uint32_t rows = 128;
    const uint32_t cols = 256;

    for (i = 0; i < rows * cols; i++)
        src32[i] = i;
    for (i = 0; i < rows * cols; i++)
        dst32[i] = 0;

    uint32_t tcr = (1u << 30) | (3u << 28) | (3u << 18) |
                   (2u << 10) | (2u << 8) |
                   (2u << 6)  | (2u << 4) | (2u << 2) | (2u << 0);
    uint32_t bndtr = (255u << 20) | 4;     /* BRC=255 (256 blocks), BNDT=4 */
    uint32_t brur  = (508u << 16) | 0;     /* DUV=508 (128*4-4), SUV=0 */

    for (i = 0; i < 127; i++)
    {
        g_xpose_nodes[i].TCR   = tcr;
        g_xpose_nodes[i].BNDTR = bndtr;
        g_xpose_nodes[i].SAR   = (uint32_t)&src32[(i + 1) * cols];
        g_xpose_nodes[i].DAR   = (uint32_t)&dst32[i + 1];
        g_xpose_nodes[i].BRUR  = brur;
        g_xpose_nodes[i].LAR   = (i < 126) ? (uint32_t)&g_xpose_nodes[i + 1] : 0;
        g_xpose_nodes[i].TBR   = 0;
        g_xpose_nodes[i].RSVD  = 0;
        g_xpose_nodes[i].MAR   = 0;
        g_xpose_nodes[i].MDR   = 0;
    }

    MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
    cfg.ch        = 0;
    cfg.ssize     = MDMA_DATA_32BIT;
    cfg.dsize     = MDMA_DATA_32BIT;
    cfg.sinc      = MDMA_INC_INCR;
    cfg.dinc      = MDMA_INC_INCR;
    cfg.sincos    = MDMA_DATA_32BIT;
    cfg.dincos    = MDMA_DATA_32BIT;
    cfg.sburst    = MDMA_BSIZE_4B;
    cfg.dburst    = MDMA_BSIZE_4B;
    cfg.tlen      = 3;    /* buffer = 4 bytes */
    cfg.trgm      = MDMA_TRGM_CHANNEL;
    cfg.swrm      = 1;
    cfg.ctcie     = 1;
    cfg.bndt      = 4;
    cfg.brc       = cols - 1;
    cfg.suv       = 0;
    cfg.duv       = rows * 4 - 4;
    cfg.src_addr  = (uint32_t)src32;
    cfg.dst_addr  = (uint32_t)dst32;
    cfg.link_addr = (uint32_t)&g_xpose_nodes[0];
    MdmaCfg(MDMA, &cfg);
    MdmaEnable(MDMA, 0);
    MdmaSwTrig(MDMA, 0);

    while (!(MdmaGetChIsr(MDMA, 0) & MDMA_FLAG_CTCIF))
        IwdgKickDog(IWDG2);
    MdmaClearChIf(MDMA, 0, MDMA_FLAG_CTCIF);

    uint32_t errors = 0;
    for (i = 0; i < cols; i++)
    {
        for (j = 0; j < rows; j++)
        {
            uint32_t expected = j * cols + i;
            if (dst32[i * rows + j] != expected)
            {
                errors++;
                if (errors <= 5)
                    PrintU32("Mismatch at", i * rows + j);
            }
        }
    }

    if (errors == 0)
        PRINT("Transpose (128x256, link+repeat) PASS\r\n");
    else
    {
        PRINT("Transpose FAIL, errors=");
        char buf[12];
        NumToStr(buf, errors);
        PRINT(buf);
        PRINT("\r\n");
    }
}

static const MenuEntry_t mdma_menu[] = {
    { "1", "M2M software trigger",   TestM2mSw },
    { "2", "Block repeat (128KB)",   TestBlockRepeat },
    { "3", "Linked-list + repeat",   TestLinkedList },
    { "4", "Endian exchange",        TestEndianExchange },
    { "5", "Reverse copy (128KB)",   TestReverse },
    { "6", "Stride extract (odd cols)", TestStrideExtract },
    { "7", "Sub-region extract",     TestSubRegion },
    { "8", "Transpose (128x256)",    TestTranspose },
};

void MdmaTest(void)
{
    RCC->MP_AHB6ENSETR = (1u << 0);
    RunSubMenu("MDMA Test", mdma_menu, 8);
}
