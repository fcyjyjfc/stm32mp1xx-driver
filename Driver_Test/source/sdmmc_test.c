#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_rcc.h"
#include "sd_card.h"
#include "test_common.h"

static SdCard_t g_card;
/* 多块测试用大缓冲区: 256 块 × 512B = 128KB */
static uint8_t g_buf[256 * 512] __attribute__((aligned(4)));
#define WR_BUF  (g_buf)                /* 写缓冲区 0..65535 (64 KB) */
#define RD_BUF  (g_buf + 65536)        /* 读缓冲区 65536..131071 (64 KB) */


/* SDMMC1 GPIO: PC8~PC12(AF12), PD2(AF12), 高速+上拉 */
static void SdmmcGpioInit(void)
{
    RCC->MP_AHB4ENSETR |= (1U << 2) | (1U << 3) | (1U << 7);  /* GPIOC, D, H */

    /* PC8=DAT0, PC9=DAT1, PC10=DAT2, PC11=DAT3, PC12=CLK */
    int pin;
    for (pin = 8; pin <= 12; pin++)
    {
        GpioMode(GPIO_C, pin, GPIO_MODER_AF);
        GpioAf(GPIO_C, pin, 12);
        GpioOtype(GPIO_C, pin, GPIO_OTYPE_PUSH_PULL);
        GpioOspeed(GPIO_C, pin, GPIO_OSPEED_VERY_HI);
        GpioPullUpDown(GPIO_C, pin, (pin == 12) ? GPIO_PUPDR_NO : GPIO_PUPDR_PULL_UP);
    }

    /* PD2=CMD */
    GpioMode(GPIO_D, 2, GPIO_MODER_AF);
    GpioAf(GPIO_D, 2, 12);
    GpioOtype(GPIO_D, 2, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_D, 2, GPIO_OSPEED_VERY_HI);
    GpioPullUpDown(GPIO_D, 2, GPIO_PUPDR_PULL_UP);
}

static void SdmmcClkInit(void)
{
    /* ker_ck 选 HSI (64MHz), 频率已知不依赖 PLL 配置 */
    RCC->SDMMC12CKSELR = 3;
    /* 使能 SDMMC1 (AHB6 bit16) */
    RCC->MP_AHB6ENSETR = (1U << 16);
}

/* 统一初始化: GPIO + 时钟 + 卡识别, 成功返回0 */
static int SdmmcHwInit(void)
{
    char dec[16];

    SdmmcGpioInit();
    SdmmcClkInit();

    g_card.sdmmc = SDMMC1;
    g_card.ker_ck_hz = 64000000;

    SdCardErr_t err = SdCardInit(&g_card, 16);
    if (err != SD_OK)
    {
        PRINT("SdCardInit FAILED, err=");
        PrintDec(dec, (int32_t)err);
        PRINT(dec);
        PRINT("\r\n");
        return -1;
    }

    PRINT("Init OK!\r\n");
    PRINT("  Product: ");
    PRINT(g_card.product);
    PRINT(" v");
    PrintDec(dec, g_card.prv_major);
    PRINT(dec);
    PRINT(".");
    PrintDec(dec, g_card.prv_minor);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  OEM: ");
    PRINT(g_card.oem_id);
    PRINT("  MFR: 0x");
    PrintHex8(g_card.mfr_id);
    PRINT("\r\n");

    PRINT("  Serial: 0x");
    {
        char hex[12];
        PrintHex32(hex, g_card.serial);
        PRINT(hex);
    }
    PRINT("\r\n");

    PRINT("  Date: ");
    PrintDec(dec, g_card.mfr_year);
    PRINT(dec);
    PRINT("/");
    PrintDec(dec, g_card.mfr_month);
    PRINT(dec);
    PRINT("\r\n");

    PRINT("  Type: ");
    PRINT(g_card.is_sdhc ? "SDHC/SDXC" : "SDSC");
    PRINT("\r\n");

    PRINT("  Capacity: ");
    PrintDec(dec, (int32_t)g_card.capacity_mb);
    PRINT(dec);
    PRINT(" MB\r\n");

    PRINT("  Max Clock: ");
    PrintDec(dec, (int32_t)(g_card.max_clk_hz / 1000000));
    PRINT(dec);
    PRINT(" MHz\r\n");

    PRINT("  S18A: ");
    PRINT(g_card.s18a ? "yes (1.8V UHS-I supported)" : "no");
    PRINT("\r\n");

    return 0;
}


/* GIC 中断统一配置: SDMMC1 IRQ 81, priority 5, enable IRQ */
static void SdmmcGicConfig(void)
{
    GicdSetGroup(GIC_SDMMC1);
    GicdSetPriority(GIC_SDMMC1, 5);
    GicdSetTarget(GIC_SDMMC1, 1);
    GicdSetTrigMode(GIC_SDMMC1, 0);
    GicRegisterIrq(GIC_SDMMC1, Sdmmc1IrqHandler);
    GicdEnableInt(GIC_SDMMC1);
    GicdInit();
    GiccInit(10, 2);
    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        :
        :
        : "r0"
    );
}

/* 中断清理: 关 SDMMC1 + 重置 done 标志 */
static void SdmmcGicCleanup(void)
{
    GicdDisableInt(GIC_SDMMC1);
    g_sdmmc1_it_done = 0;
}

/* 测试间扫尾: SDMMC1 外设复位 + 卡重初始化, 确保下一个测试在干净环境中运行 */
static void SdmmcTestCleanup(void)
{
    RCC->AHB6RSTSETR = (1U << 16);  /* 断言 SDMMC1 复位 */
    RCC->AHB6RSTCLRR = (1U << 16);  /* 释放复位, 所有寄存器回到默认值 */
    SdCardInit(&g_card, 16);        /* 重新上电 + 卡识别 */
    g_sdmmc1_it_done = 0;
    g_sdmmc1_it_result = SDMMC_OK;
}


/* ================================
 *  1. 读 MBR 测试 — 检查 0x55AA 签名
 * ================================ */
static void ReadMbrTest(void)
{
    PRINT("\r\n----- Read MBR (Block 0) -----\r\n");
    PRINT("  Purpose: verify SD card MBR signature (0x55AA)\r\n");
    PRINT("  Mode:    blocking | single-block | no LL\r\n");
    PRINT("  Read:    block 0, 512B via CMD17\r\n");
    PRINT("  Expect:  byte[510]=0x55, byte[511]=0xAA\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);
    SdCardErr_t err = SdCardReadBlocks(&g_card, 0, g_buf, 1);
    if (err != SD_OK)
    {
        PRINT("Read FAILED\r\n");
        return;
    }

    PRINT("  Block 0 first 16 bytes:\r\n    ");
    int i;
    for (i = 0; i < 16; i++)
    {
        PrintHex8(g_buf[i]);
        PRINT(" ");
    }
    PRINT("\r\n");

    PRINT("  Signature [510..511]: ");
    PrintHex8(g_buf[510]);
    PRINT(" ");
    PrintHex8(g_buf[511]);
    if (g_buf[510] == 0x55 && g_buf[511] == 0xAA)
        PRINT(" -> MBR Valid\r\n");
    else
        PRINT(" -> No MBR\r\n");
}


/* ================================
 *  2. 写回读测试 — 写入 pattern 并校验
 * ================================ */
static void WriteReadTest(void)
{
    char dec[16];
    uint32_t test_block = 2048;
    int i;

    PRINT("\r\n----- Write/Read Test -----\r\n");
    PRINT("  Purpose: single-block write + read-back verify\r\n");
    PRINT("  Mode:    blocking | single-block | no LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Data:    512B, byte[i] = i XOR 0xA5\r\n");
    PRINT("  Write:   CMD24,  Read: CMD17\r\n");
    PRINT("  Expect:  all 512 bytes match\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 填充 WR_BUF: byte[i] = i XOR 0xA5 */
    for (i = 0; i < 512; i++)
        WR_BUF[i] = (uint8_t)(i ^ 0xA5);

    PRINT("  Writing...\r\n");
    SdCardErr_t err = SdCardWriteBlocks(&g_card, test_block, WR_BUF, 1);
    if (err != SD_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 读入独立缓冲区 */
    memset(RD_BUF, 0, 512);
    PRINT("  Reading back...\r\n");
    err = SdCardReadBlocks(&g_card, test_block, RD_BUF, 1);
    if (err != SD_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF, RD_BUF, 512) == 0)
        PRINT("  Verify PASSED\r\n");
    else
    {
        for (i = 0; i < 512; i++)
        {
            if (WR_BUF[i] != RD_BUF[i])
            {
                PRINT("  Mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": expect ");
                PrintHex8(WR_BUF[i]);
                PRINT(" got ");
                PrintHex8(RD_BUF[i]);
                PRINT("\r\n");
                break;
            }
        }
    }
}


/* ================================
 *  3. 多块读写测试 (block 100000, 128 块)
 *     写 uint16_t 0~32767 → RD_BUF 读回 → memcmp
 * ================================ */
static void MultiBlockTest(void)
{
    char dec[16];
    uint32_t test_block = 100000;
    uint32_t n_blocks = 128;  /* 32768 个 uint16_t × 2B = 64KB, WR_BUF+RD_BUF 各半 */
    uint32_t n_vals = n_blocks * 256;  /* 32768 */
    uint32_t n_bytes = n_blocks * 512; /* 65536 */
    int i;

    PRINT("\r\n----- Multi-block Write/Read -----\r\n");
    PRINT("  Purpose: multi-block (CMD23+CMD25/CMD18) write+read\r\n");
    PRINT("  Mode:    blocking | multi-block | no LL (single-buffer IDMA)\r\n");
    PRINT("  Blocks:  ");
    PrintDec(dec, (int32_t)n_blocks);
    PRINT(dec);
    PRINT(" (");
    PrintDec(dec, (int32_t)(n_bytes / 1024));
    PRINT(dec);
    PRINT(" KB) @ block ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Data:    uint16_t 0~");
    PrintDec(dec, (int32_t)(n_vals - 1));
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Write:   CMD25,  Read: CMD18\r\n");
    PRINT("  Expect:  memcmp(WR_BUF, RD_BUF) == 0\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 填 WR_BUF: uint16_t 0~32767 */
    uint16_t *p = (uint16_t *)WR_BUF;
    for (i = 0; i < (int32_t)n_vals; i++)
        p[i] = (uint16_t)i;

    PRINT("  Writing ");
    PrintDec(dec, (int32_t)n_blocks);
    PRINT(dec);
    PRINT(" blocks...\r\n");
    SdCardErr_t err = SdCardWriteMulti(&g_card, test_block, WR_BUF, n_blocks);
    if (err != SD_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    memset(RD_BUF, 0, n_bytes);
    PRINT("  Reading back...\r\n");
    err = SdCardReadMulti(&g_card, test_block, RD_BUF, n_blocks);
    if (err != SD_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF, RD_BUF, n_bytes) == 0)
    {
        PRINT("  Verify PASSED (");
        PrintDec(dec, (int32_t)n_vals);
        PRINT(dec);
        PRINT(" values)\r\n");
    }
    else
    {
        for (i = 0; i < (int32_t)n_vals; i++)
        {
            if (p[i] != ((uint16_t *)RD_BUF)[i])
            {
                PRINT("  Mismatch @[");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT("]: expect ");
                PrintHex8((uint8_t)(i >> 8));
                PrintHex8((uint8_t)i);
                PRINT(" got ");
                PrintHex8((uint8_t)(((uint16_t *)RD_BUF)[i] >> 8));
                PrintHex8((uint8_t)((uint16_t *)RD_BUF)[i]);
                PRINT("\r\n");
                break;
            }
        }
    }
}


/* ================================
 *  4. 链表多块读写测试 — 写/读各用独立描述符和独立缓冲区, 互不连续
 *     写: 5节点 (512+512+480+544+512)=2560B, ULS混用 (2-word+3-word)
 *     读: 2节点 (1024+1536)=2560B, 全3-word
 *     数据: uint16_t 从 0 连续递增, 共 1280 个值
 * ================================ */
static void LlMultiBlockTest(void)
{
    char dec[16];
    uint32_t test_block = 200000;
    int i;

    PRINT("\r\n----- LL Multi-block Write/Read -----\r\n");
    PRINT("  Purpose: linked-list scatter-gather write+read\r\n");
    PRINT("  Mode:    blocking | multi-block | IDMA LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Write:   5 nodes (512+512+480+544+512=2560B), ULS mix\r\n");
    PRINT("  Read:    2 nodes (1024+1536=2560B), non-contiguous desc\r\n");
    PRINT("  Data:    uint16_t 0~1279 (2560B), separate wr/rd buffers\r\n");
    PRINT("  Expect:  1280 values match, wr=5nodes rd=2nodes\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* ======== 写缓冲区 ========
     * 5块连续放在 g_buf 低地址区, 共2560B */
    uint32_t wr_sizes[5] = {512, 512, 480, 544, 512};
    uint8_t  *wr_buf[5];
    uint32_t  wr_off = 0;
    for (i = 0; i < 5; i++)
    {
        wr_buf[i] = g_buf + wr_off;
        wr_off += wr_sizes[i];
    }

    /* ======== 读缓冲区 ========
     * 2块放在 g_buf 高地址区, 与写区远离, 且两两之间有间隔 */
    uint32_t rd_sizes[2] = {1024, 1536};
    uint8_t  *rd_buf[2];
    rd_buf[0] = g_buf + 4096;                       /* 1024B, 与写区末尾(g_buf+2560)有间隔 */
    rd_buf[1] = g_buf + 6144;                       /* 1536B, 与rd_buf[0]末尾(g_buf+5120)有间隔 */

    /* ---- 填充递增 uint16_t 到写缓冲区 ---- */
    uint16_t v = 0;
    for (i = 0; i < 5; i++)
    {
        uint16_t *p = (uint16_t *)wr_buf[i];
        uint32_t count = wr_sizes[i] / 2;
        uint32_t j;
        for (j = 0; j < count; j++)
            p[j] = v++;
    }

    /* ======== 写描述符链 (5节点) ========
     * desc_wr[0] ULS=0: 第2节点按2-word加载, 复用前一个size (buf0/1同512B)
     * desc_wr[1..4] ULS=1: 3-word加载各自独立size */
    SdmmcIdmaDesc_t desc_wr[5];
    for (i = 0; i < 5; i++)
    {
        desc_wr[i].buf_addr = (uint32_t)(uintptr_t)wr_buf[i];
        desc_wr[i].buf_size = wr_sizes[i];
        uint32_t idmalar = SDMMC_IDMALAR_ABR;       /* bit29: buffer 就绪 */
        if (i >= 1)
            idmalar |= SDMMC_IDMALAR_ULS;            /* bit30: desc_wr[0]除外(U=0) */
        if (i < 4)
        {
            idmalar |= SDMMC_IDMALAR_ULA;            /* bit31: 继续下一节点 */
            idmalar |= (uint32_t)((i + 1) * sizeof(SdmmcIdmaDesc_t));
        }
        desc_wr[i].idmalar = idmalar;
    }

    /* ======== 读描述符 (同数组, 但不连续: pool[1]→pool[8]) ========
     * IDMALA 无符号偏移, 从低地址往高地址跳: pool[1] → pool[8] */
    SdmmcIdmaDesc_t desc_rd_pool[10];
    uint32_t idmala_rd = (8 - 1) * (uint32_t)sizeof(SdmmcIdmaDesc_t);  /* 84 */
    /* 只初始化和使用 [1] 和 [8], 其余 8 个位置空置 */

    desc_rd_pool[1].buf_addr = (uint32_t)(uintptr_t)rd_buf[0];
    desc_rd_pool[1].buf_size = rd_sizes[0];
    desc_rd_pool[1].idmalar  = SDMMC_IDMALAR_ULS     /* bit30: 3-word 可变大小 */
                            | SDMMC_IDMALAR_ABR     /* bit29: buffer 就绪 */
                            | SDMMC_IDMALAR_ULA     /* bit31: 继续下一节点 */
                            | idmala_rd;            /* IDMALA → pool[8] */

    desc_rd_pool[8].buf_addr = (uint32_t)(uintptr_t)rd_buf[1];
    desc_rd_pool[8].buf_size = rd_sizes[1];
    desc_rd_pool[8].idmalar  = SDMMC_IDMALAR_ULS     /* bit30 */
                            | SDMMC_IDMALAR_ABR;    /* bit29: ULA=0 最后一个节点 */

    /* ---- 写阶段 ---- */
    PRINT("  Writing 5 nodes via SdCardWriteMultiLL...\r\n");
    SdCardErr_t err = SdCardWriteMultiLL(&g_card, test_block, desc_wr, SDMMC_BLKSIZ_512B);
    IwdgKickDog(IWDG2);
    if (err != SD_OK)
    {
        PRINT("  LL Write FAILED, err=");
        PrintDec(dec, (int32_t)err);
        PRINT(dec);
        PRINT("\r\n");
        return;
    }
    PRINT("  LL Write OK\r\n");

    /* 清空读缓冲区 (与写缓冲区在不同地址, 验证数据确实从卡读回) */
    for (i = 0; i < 2; i++)
        memset(rd_buf[i], 0, rd_sizes[i]);

    /* ---- 读阶段: 用独立的 2 节点 desc_rd + rd_buf ---- */
    PRINT("  Reading 2 nodes via SdCardReadMultiLL...\r\n");
    err = SdCardReadMultiLL(&g_card, test_block, &desc_rd_pool[1], SDMMC_BLKSIZ_512B);
    IwdgKickDog(IWDG2);
    if (err != SD_OK)
    {
        PRINT("  LL Read FAILED, err=");
        PrintDec(dec, (int32_t)err);
        PRINT(dec);
        PRINT("\r\n");
        return;
    }
    PRINT("  LL Read OK\r\n");

    /* 校验: 跨 2 个读 buffer 逐一比对 */
    int errors = 0;
    v = 0;
    for (i = 0; i < 2; i++)
    {
        uint16_t *p = (uint16_t *)rd_buf[i];
        uint32_t count = rd_sizes[i] / 2;
        uint32_t j;
        for (j = 0; j < count; j++)
        {
            if (p[j] != v)
            {
                if (errors < 4)
                {
                    PRINT("  Mismatch: rd_buf[");
                    PrintDec(dec, i);
                    PRINT(dec);
                    PRINT("][");
                    PrintDec(dec, j);
                    PRINT(dec);
                    PRINT("] expect ");
                    PrintDec(dec, v);
                    PRINT(dec);
                    PRINT(" got ");
                    PrintDec(dec, p[j]);
                    PRINT(dec);
                    PRINT("\r\n");
                }
                errors++;
            }
            v++;
        }
    }

    if (errors == 0)
        PRINT("  Verify PASSED (1280 values, wr=5nodes rd=2nodes)\r\n");
    else
    {
        PRINT("  Verify FAILED: ");
        PrintDec(dec, errors);
        PRINT(dec);
        PRINT(" errors\r\n");
    }
}


/* ===== Submenu ===== */

/* ================================
 *  5. 中断模式 LL 读写测试 — 单节点 ULA=0, 验证 ISR 链路
 *     写512B递增 uint16_t → 中断写 → 清buffer → 中断读 → 校验
 * ================================ */
static void LlItMultiBlockTest(void)
{
    char dec[16];
    uint32_t test_block = 300000;
    int i;

    PRINT("\r\n----- LL Interrupt Write/Read -----\r\n");
    PRINT("  Purpose: interrupt-driven LL single-node write+read\r\n");
    PRINT("  Mode:    interrupt | multi-block | IDMA LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Desc:    1 node (512B), ULA=0, ABR=1\r\n");
    PRINT("  Data:    uint16_t 0~255 (512B)\r\n");
    PRINT("  ISR:     CMDREND → DATAEND finish, BTC counts\r\n");
    PRINT("  Expect:  256 values match, ISR link verified\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 单节点: 512B, ULA=0 (最后一个也是唯一一个 IDMA buffer) */
    uint8_t *buf = g_buf;
    uint32_t buf_size = 512;

    SdmmcIdmaDesc_t desc;
    desc.buf_addr = (uint32_t)(uintptr_t)buf;
    desc.buf_size = buf_size;
    desc.idmalar  = SDMMC_IDMALAR_ULS      /* bit30: 3-word */
                  | SDMMC_IDMALAR_ABR;      /* bit29: buffer 就绪 */
                  /* ULA=0: 单节点, 传输完停止 */

    /* 填充递增 uint16_t */
    uint16_t *p = (uint16_t *)buf;
    for (i = 0; i < 256; i++)
        p[i] = (uint16_t)i;

    SdmmcGicConfig();

    /* ---- 中断写 ---- */
    PRINT("  Writing via ISR...\r\n");
    SdCardStartWriteMultiLL(&g_card, test_block, &desc, SDMMC_BLKSIZ_512B);

    while (!g_sdmmc1_it_done)
        ;
    IwdgKickDog(IWDG2);

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Write FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }
    PRINT("  IT Write OK\r\n");

    /* 写后卡内部编程 (BUSYD0=1), 等释放再回读 */
    SdCardWaitBusy(&g_card);
    IwdgKickDog(IWDG2);

    /* ---- 中断读 (读入 RD_BUF) ---- */
    memset(RD_BUF, 0, buf_size);
    desc.buf_addr = (uint32_t)(uintptr_t)RD_BUF;

    g_sdmmc1_it_done = 0;
    g_sdmmc1_btc_cnt = 0;
    PRINT("  Reading via ISR...\r\n");
    SdCardStartReadMultiLL(&g_card, test_block, &desc, SDMMC_BLKSIZ_512B);

    while (!g_sdmmc1_it_done)
        ;
    IwdgKickDog(IWDG2);

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Read FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }
    PRINT("  IT Read OK\r\n");

    if (memcmp(buf, RD_BUF, buf_size) == 0)
        PRINT("  Verify PASSED (256 values, ISR-driven)\r\n");
    else
    {
        for (i = 0; i < 256; i++)
        {
            if (p[i] != ((uint16_t *)RD_BUF)[i])
            {
                PRINT("  Mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": expect ");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(" got ");
                PrintDec(dec, ((uint16_t *)RD_BUF)[i]);
                PRINT(dec);
                PRINT("\r\n");
                break;
            }
        }
    }

cleanup:
    SdmmcGicCleanup();
}


/* ================================
 *  6. 中断 LL BTC 动态 BAR 测试 — IDMA ABR 行为验证
 *     目的: 验证 IDMA 硬件"先查 BAR 再加载下一节点"的顺序
 *     性质: 硬件行为验证, 非功能测试
 *     写: 2节点 (1024+1536), 节点0 BAR=1 预填数据, 节点1 BAR=0
 *         前台轮询 BTC=1 → 填节点1数据 → 置 BAR=1
 *     读: 2节点 (1024+1536), 同理
 *     如果硬件"先查BAR再加载": BTC=1 后前台来得及置BAR → DATAEND 正常
 *     如果硬件"加载后查BAR": 节点1 BAR=0 → IDMATE → 传输卡死
 * ================================ */
static void LlItBtcTest(void)
{
    char dec[16];
    uint32_t test_block = 350000;
    int i;

    PRINT("\r\n----- LL IT BTC Dynamic BAR Test -----\r\n");
    PRINT("  Purpose: verify HW checks ABR before loading next desc\r\n");
    PRINT("  Mode:    interrupt | multi-block | IDMA LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Write:   2 nodes (1024+1536B), node0 ABR=1 pre-filled\r\n");
    PRINT("  Read:    2 nodes (1024+1536B), same dynamic-BAR logic\r\n");
    PRINT("  Data:    uint16_t 0~1279 (2560B)\r\n");
    PRINT("  Theory:  HW checks ABR before loading desc\r\n");
    PRINT("    OK:     BTC=1 → fill node1 → BAR=1 → DATAEND\r\n");
    PRINT("    FAIL:   IDMATE before BAR set (no IRQ, transfer hangs)\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* ======== 写缓冲区: 2块连续, 共2560B, 1280个uint16_t ======== */
    uint32_t wr_sizes[2] = {1024, 1536};
    uint8_t  *wr_buf[2];
    wr_buf[0] = g_buf + 0;
    wr_buf[1] = g_buf + 1024;

    /* 读缓冲区: 远离写区, 防止被写阶段残留数据影响 */
    uint32_t rd_sizes[2] = {1024, 1536};
    uint8_t  *rd_buf[2];
    rd_buf[0] = g_buf + 4096;
    rd_buf[1] = g_buf + 6144;

    /* 只预填节点0: uint16_t 0..511 (1024B = 512个值) */
    uint16_t *p0 = (uint16_t *)wr_buf[0];
    for (i = 0; i < 512; i++)
        p0[i] = (uint16_t)i;
    /* 节点1不填 — 等 BTC 中断后才填 */

    /* ======== 写描述符 (2节点) ========
     * desc_wr[0]: BAR=1, ULA=1, IDMALA→desc_wr[1]   (预准备)
     * desc_wr[1]: BAR=0, ULA=0                        (前台动态准备) */
    SdmmcIdmaDesc_t desc_wr[2];
    desc_wr[0].buf_addr = (uint32_t)(uintptr_t)wr_buf[0];
    desc_wr[0].buf_size = wr_sizes[0];
    desc_wr[0].idmalar  = SDMMC_IDMALAR_ABR             /* BAR=1: 预填数据已就绪 */
                        | SDMMC_IDMALAR_ULS              /* 3-word 可变大小 */
                        | SDMMC_IDMALAR_ULA              /* 继续下一节点 */
                        | (uint32_t)sizeof(SdmmcIdmaDesc_t); /* IDMALA → desc_wr[1] */

    desc_wr[1].buf_addr = (uint32_t)(uintptr_t)wr_buf[1];
    desc_wr[1].buf_size = wr_sizes[1];
    desc_wr[1].idmalar  = SDMMC_IDMALAR_ULS;             /* BAR=0! ULA=0 最后节点 */

    /* ======== 读描述符 (2节点, 非连续位置) ======== */
    SdmmcIdmaDesc_t desc_rd_pool[10];
    uint32_t idmala_rd = (8 - 1) * (uint32_t)sizeof(SdmmcIdmaDesc_t);

    desc_rd_pool[1].buf_addr = (uint32_t)(uintptr_t)rd_buf[0];
    desc_rd_pool[1].buf_size = rd_sizes[0];
    desc_rd_pool[1].idmalar  = SDMMC_IDMALAR_ABR         /* BAR=1: 缓冲区就绪 */
                             | SDMMC_IDMALAR_ULS
                             | SDMMC_IDMALAR_ULA
                             | idmala_rd;                /* → pool[8] */

    desc_rd_pool[8].buf_addr = (uint32_t)(uintptr_t)rd_buf[1];
    desc_rd_pool[8].buf_size = rd_sizes[1];
    desc_rd_pool[8].idmalar  = SDMMC_IDMALAR_ULS;        /* BAR=0! ULA=0 */

    SdmmcGicConfig();

    /* ---- 中断写: 只预填了节点0, 节点1 由前台在 BTC 后动态填 ---- */
    PRINT("  Writing 2 nodes via ISR (node1 BAR=0, fill on BTC)...\r\n");
    SdCardStartWriteMultiLL(&g_card, test_block, desc_wr, SDMMC_BLKSIZ_512B);

    {
        uint32_t last_btc = 0;
        uint32_t timeout = 0;
        uint16_t fill_val = 512;  /* 节点0 填了 0..511, 节点1 从 512 开始 */
        while (!g_sdmmc1_it_done)
        {
            /* IDMATE 无中断使能, 前台轮询检测 */
            if (SdmmcCheckIdmate(g_card.sdmmc))
            {
                PRINT("  WRITE: IDMATE detected! HW loads desc BEFORE checking ABR\r\n");
                SdmmcClearFlags(g_card.sdmmc);
                goto cleanup;
            }
            uint32_t cur = g_sdmmc1_btc_cnt;
            if (cur != last_btc)
            {
                last_btc = cur;
                /* BTC=1: 节点0 传输完毕, 硬件即将加载节点1
                 * 如果硬件"先查 BAR 再加载": 节点1 BAR=0 → 硬件等待, 此时代码有时机置 BAR=1
                 * 如果硬件"加载后查 BAR": 可能在 BTC 之前就已经 IDMATE 了 (上面已检测) */
                if (cur == 1)
                {
                    uint16_t *p1 = (uint16_t *)wr_buf[1];
                    uint32_t count = wr_sizes[1] / 2;  /* 1536/2 = 768 */
                    uint32_t j;
                    for (j = 0; j < count; j++)
                        p1[j] = fill_val++;
                    desc_wr[1].idmalar |= SDMMC_IDMALAR_ABR;  /* BAR=1: 数据就绪 */
                }
            }
            if (++timeout > 50000000)
            {
                IwdgKickDog(IWDG2);
                timeout = 0;
            }
        }
    }
    IwdgKickDog(IWDG2);

    PRINT("  Write done, BTC=");
    PrintDec(dec, (int32_t)g_sdmmc1_btc_cnt);
    PRINT(dec);
    if (g_sdmmc1_btc_cnt == 1)
        PRINT(" (OK)\r\n");
    else
        PRINT(" (unexpected)\r\n");

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Write FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    /* 写后等卡编程完成 */
    SdCardWaitBusy(&g_card);
    IwdgKickDog(IWDG2);

    /* 清读缓冲区 */
    for (i = 0; i < 2; i++)
        memset(rd_buf[i], 0, rd_sizes[i]);

    /* ---- 中断读: 读不需要填数据, 前台只置 BAR ---- */
    g_sdmmc1_it_done = 0;
    g_sdmmc1_btc_cnt = 0;
    PRINT("  Reading 2 nodes via ISR (node1 BAR=0, set on BTC)...\r\n");
    SdCardStartReadMultiLL(&g_card, test_block, &desc_rd_pool[1], SDMMC_BLKSIZ_512B);

    {
        uint32_t last_btc = 0;
        uint32_t timeout = 0;
        while (!g_sdmmc1_it_done)
        {
            if (SdmmcCheckIdmate(g_card.sdmmc))
            {
                PRINT("  READ: IDMATE detected! HW loads desc BEFORE checking ABR\r\n");
                SdmmcClearFlags(g_card.sdmmc);
                goto cleanup;
            }
            uint32_t cur = g_sdmmc1_btc_cnt;
            if (cur != last_btc)
            {
                last_btc = cur;
                if (cur == 1)
                    desc_rd_pool[8].idmalar |= SDMMC_IDMALAR_ABR;  /* BAR=1 */
            }
            if (++timeout > 50000000)
            {
                IwdgKickDog(IWDG2);
                timeout = 0;
            }
        }
    }
    IwdgKickDog(IWDG2);

    PRINT("  Read done, BTC=");
    PrintDec(dec, (int32_t)g_sdmmc1_btc_cnt);
    PRINT(dec);
    if (g_sdmmc1_btc_cnt == 1)
        PRINT(" (OK)\r\n");
    else
        PRINT(" (unexpected)\r\n");

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Read FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    /* 校验: 跨 2 个读 buffer 逐一比对 uint16_t */
    {
        int errors = 0;
        uint16_t v = 0;
        for (i = 0; i < 2; i++)
        {
            uint16_t *p = (uint16_t *)rd_buf[i];
            uint32_t count = rd_sizes[i] / 2;
            uint32_t j;
            for (j = 0; j < count; j++)
            {
                if (p[j] != v)
                {
                    if (errors < 4)
                    {
                        PRINT("  Mismatch: rd_buf[");
                        PrintDec(dec, i);
                        PRINT(dec);
                        PRINT("][");
                        PrintDec(dec, j);
                        PRINT(dec);
                        PRINT("] expect ");
                        PrintDec(dec, v);
                        PRINT(dec);
                        PRINT(" got ");
                        PrintDec(dec, p[j]);
                        PRINT(dec);
                        PRINT("\r\n");
                    }
                    errors++;
                }
                v++;
            }
        }

        if (errors == 0)
            PRINT("  Verify PASSED (1280 values)\r\n");
        else
        {
            PRINT("  Verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" errors\r\n");
        }
    }

cleanup:
    SdmmcGicCleanup();
    SdmmcTestCleanup();         /* 测试6触发 IDMATE, 需要外设复位 + 重初始化 */
}


/* ================================
 *  7. 中断 LL BTC 诊断测试 — 32节点×4096B, 24预填+8动态
 *     记录每个动态节点填充开始和结束时的 BTC
 *     调试跟踪: ISR(IDMABTC→1, TXUNDERR→3, IDMATE→4), 主循环(BAR置1→2)
 * ================================ */
static void LlItBtcMultiTest(void)
{
#define N_NODES      32        /* 32×4096=128KB 正好填满 g_buf */
#define N_PREPARED   24        /* 前 24 节点 ABR=1, 8 个动态 (24~31) */
    char dec[16];
    uint32_t test_block = 400000;
    int i;

    PRINT("\r\n----- LL IT BTC Multi -----\r\n");
    PRINT("  Purpose: measure CPU fill speed vs IDMA transfer rate\r\n");
    PRINT("  Mode:    interrupt | multi-block | IDMA LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Nodes:   32 × 4096B = 128KB, 24 prepared + 8 dynamic\r\n");
    PRINT("  Clock:   reduced to 8 MHz (32 MHz → IDMA ~5× faster than CPU fill)\r\n");
    PRINT("  Data:    uint16_t fill on BTC, 2048 values/node\r\n");
    PRINT("  Trace:   1=BTC(ISR) 2=BAR(main) 3=TXUNDERR 4=IDMATE\r\n");
    PRINT("  Expect:  BTC=32, err=0, all 8 dynamic nodes filled\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 先擦除目标区域 (阻塞式, 不需要中断), 消除写过程中 Flash 页擦除带来的 BTC 间隔差异 */
    {
        uint32_t erase_end = test_block + (N_NODES * 8) - 1;  /* 4096/512=8 blocks/node */
        SdCardErr_t er_err = SdCardErase(&g_card, test_block, erase_end);
        if (er_err != SD_OK)
        {
            PRINT("  Erase FAILED, err=");
            PrintDec(dec, (int32_t)er_err);
            PRINT(dec);
            PRINT("\r\n");
            return;
        }
        PRINT("  Erase done, range ");
        PrintDec(dec, (int32_t)test_block);
        PRINT(dec);
        PRINT("~");
        PrintDec(dec, (int32_t)erase_end);
        PRINT(dec);
        PRINT("\r\n");
    }

    uint32_t node_size = 4096;
    uint8_t *wr_buf[N_NODES];
    for (i = 0; i < N_NODES; i++)
        wr_buf[i] = g_buf + i * node_size;

    /* 只预填前 N_PREPARED 个节点 */
    uint16_t v = 0;
    for (i = 0; i < N_PREPARED; i++)
    {
        uint16_t *p = (uint16_t *)wr_buf[i];
        uint32_t j;
        for (j = 0; j < 2048; j++)
            p[j] = v++;
    }

    /* 写描述符: 仅前 N_PREPARED 节点 ABR=1, 其余 ABR=0 */
    SdmmcIdmaDesc_t desc_wr[N_NODES];
    for (i = 0; i < N_NODES; i++)
    {
        desc_wr[i].buf_addr = (uint32_t)(uintptr_t)wr_buf[i];
        desc_wr[i].buf_size = node_size;
        uint32_t idmalar = SDMMC_IDMALAR_ULS;
        if (i < N_PREPARED)
            idmalar |= SDMMC_IDMALAR_ABR;
        if (i < N_NODES - 1)
        {
            idmalar |= SDMMC_IDMALAR_ULA;
            idmalar |= (uint32_t)((i + 1) * sizeof(SdmmcIdmaDesc_t));
        }
        desc_wr[i].idmalar = idmalar;
    }

    /* 降低 SDMMC 时钟, 减慢 IDMA 传输速率, 验证 CPU 填充速度是否足够 */
    SdCardSetSpeed(&g_card, 4);  /* 4 MBps × 4-bit / 8 = 8 MHz (原 32 MHz) */
    PRINT("  Clock reduced to 8 MHz\r\n");

    /* ---- 中断写 ---- */
    g_it_sdmmc = g_card.sdmmc;
    SdmmcGicConfig();
    g_sdmmc1_it_done  = 0;
    g_sdmmc1_btc_cnt = 0;
    g_sdmmc1_dbg_idx = 0;
    PRINT("  Starting ISR write (24 prepared + 8 dynamic)...\r\n");
    SdCardStartWriteMultiLL(&g_card, test_block, desc_wr, SDMMC_BLKSIZ_512B);

    /* 每个 BTC 填一个动态节点, 只记录不打印 */
    {
        uint32_t last_btc = 0;
        uint32_t next_dyn = N_PREPARED;
        uint32_t timeout = 0;
        uint32_t fill_cnt = 0;
        uint32_t rec_btc_start[N_NODES - N_PREPARED];  /* 填充开始时的 BTC */
        uint32_t rec_btc_end[N_NODES - N_PREPARED];    /* BAR 置完时的 BTC */
        while (!g_sdmmc1_it_done)
        {
            if (SdmmcCheckIdmate(g_card.sdmmc))
            {
                SdmmcClearFlags(g_card.sdmmc);
                break;
            }
            uint32_t cur = g_sdmmc1_btc_cnt;
            if (cur != last_btc)
            {
                last_btc = cur;
                if (next_dyn < N_NODES)
                {
                    uint32_t idx = next_dyn;
                    rec_btc_start[fill_cnt] = cur;  /* 进分支时的 BTC */

                    uint16_t *p = (uint16_t *)wr_buf[idx];
                    uint32_t j;
                    for (j = 0; j < 2048; j++)
                        p[j] = v++;
                    desc_wr[idx].idmalar |= SDMMC_IDMALAR_ABR;

                    rec_btc_end[fill_cnt] = g_sdmmc1_btc_cnt;  /* BAR后的 BTC */
                    fill_cnt++;

                    if (g_sdmmc1_dbg_idx < SDMMC_DBG_TRACE_SIZE)
                        g_sdmmc1_dbg_trace[g_sdmmc1_dbg_idx++] = 2;
                    next_dyn++;
                }
            }
            if (++timeout > 50000000)
            {
                IwdgKickDog(IWDG2);
                timeout = 0;
            }
        }
        IwdgKickDog(IWDG2);

        /* 事后统一打印 */
        PRINT("  Write done, BTC=");
        PrintDec(dec, (int32_t)g_sdmmc1_btc_cnt);
        PRINT(dec);
        PRINT(" (expect ");
        PrintDec(dec, N_NODES);
        PRINT(dec);
        PRINT(") err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");

        for (i = 0; i < (int32_t)fill_cnt; i++)
        {
            PRINT("  fill[");
            PrintDec(dec, i);
            PRINT(dec);
            PRINT("]: node=");
            PrintDec(dec, (int32_t)(N_PREPARED + i));
            PRINT(dec);
            PRINT(" BTC_start=");
            PrintDec(dec, (int32_t)rec_btc_start[i]);
            PRINT(dec);
            PRINT(" BTC_end=");
            PrintDec(dec, (int32_t)rec_btc_end[i]);
            PRINT(dec);
            PRINT(" delta=");
            PrintDec(dec, (int32_t)(rec_btc_end[i] - rec_btc_start[i]));
            PRINT(dec);
            PRINT("\r\n");
        }

        PRINT("  dbg_trace[0..");
        PrintDec(dec, (int32_t)g_sdmmc1_dbg_idx);
        PRINT(dec);
        PRINT("]: ");
        {
            uint32_t dbg_cnt = g_sdmmc1_dbg_idx;
            if (dbg_cnt > 64) dbg_cnt = 64;
            for (i = 0; i < (int32_t)dbg_cnt; i++)
            {
                PrintDec(dec, (int32_t)g_sdmmc1_dbg_trace[i]);
                PRINT(dec);
                PRINT(" ");
            }
        }
        PRINT("\r\n");
    }

    /* ---- 读回校验 (阻塞多块读, 分两半: RD_BUF 只有 64KB) ---- */
    {
        SdCardErr_t r_err;
        int errors = 0;
        uint32_t half = N_NODES * node_size / 2;  /* 65536 */

        SdCardWaitBusy(&g_card);
        IwdgKickDog(IWDG2);

        PRINT("  Reading back (first half, 128 blocks)...\r\n");
        r_err = SdCardReadMulti(&g_card, test_block, RD_BUF, 128);
        if (r_err != SD_OK)
        {
            PRINT("  Read first half FAILED\r\n");
            errors = -1;
        }
        else if (memcmp(g_buf, RD_BUF, half) != 0)
        {
            PRINT("  First half mismatch\r\n");
            errors++;
        }
        IwdgKickDog(IWDG2);

        PRINT("  Reading back (second half, 128 blocks)...\r\n");
        r_err = SdCardReadMulti(&g_card, test_block + 128, RD_BUF, 128);
        if (r_err != SD_OK)
        {
            PRINT("  Read second half FAILED\r\n");
            if (errors >= 0) errors = -1;
        }
        else if (memcmp((uint8_t *)g_buf + half, RD_BUF, half) != 0)
        {
            PRINT("  Second half mismatch\r\n");
            errors++;
        }
        IwdgKickDog(IWDG2);

        if (errors == 0)
            PRINT("  Verify PASSED (128KB, 32 nodes)\r\n");
        else if (errors > 0)
            PRINT("  Verify FAILED\r\n");
    }

    SdmmcGicCleanup();
#undef N_PREPARED
#undef N_NODES
}


/* ================================
 *  8. 中断 LL 全预准备测试 — 所有节点 ABR=1, 数据提前写好
 *     写: 5节点 (512+512+480+544+512), ULS混用
 *     读: 2节点 (1024+1536), 非连续描述符
 *     BTC 仅计数, 不干预 — 验证"全准备"也能走通中断路径
 * ================================ */
static void LlItPreparedTest(void)
{
    char dec[16];
    uint32_t test_block = 450000;
    int i;

    PRINT("\r\n----- LL IT Pre-prepared -----\r\n");
    PRINT("  Purpose: verify ISR path with all ABR=1 (no dynamic fill)\r\n");
    PRINT("  Mode:    interrupt | multi-block | IDMA LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");
    PRINT("  Write:   5 nodes (512+512+480+544+512=2560B), all ABR=1\r\n");
    PRINT("  Read:    2 nodes (1024+1536=2560B), non-contiguous desc\r\n");
    PRINT("  Data:    uint16_t 0~1279 (2560B)\r\n");
    PRINT("  Expect:  BTC=5 (wr) + 2 (rd), 1280 values match\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* ======== 写缓冲区 ======== */
    uint32_t wr_sizes[5] = {512, 512, 480, 544, 512};
    uint8_t  *wr_buf[5];
    uint32_t  wr_off = 0;
    for (i = 0; i < 5; i++)
    {
        wr_buf[i] = g_buf + wr_off;
        wr_off += wr_sizes[i];
    }

    uint32_t rd_sizes[2] = {1024, 1536};
    uint8_t  *rd_buf[2];
    rd_buf[0] = g_buf + 4096;
    rd_buf[1] = g_buf + 6144;

    /* 全量填数据 */
    uint16_t v = 0;
    for (i = 0; i < 5; i++)
    {
        uint16_t *p = (uint16_t *)wr_buf[i];
        uint32_t count = wr_sizes[i] / 2;
        uint32_t j;
        for (j = 0; j < count; j++)
            p[j] = v++;
    }

    /* 写描述符: 全部 ABR=1 */
    SdmmcIdmaDesc_t desc_wr[5];
    for (i = 0; i < 5; i++)
    {
        desc_wr[i].buf_addr = (uint32_t)(uintptr_t)wr_buf[i];
        desc_wr[i].buf_size = wr_sizes[i];
        uint32_t idmalar = SDMMC_IDMALAR_ABR;
        if (i >= 1)
            idmalar |= SDMMC_IDMALAR_ULS;
        if (i < 4)
        {
            idmalar |= SDMMC_IDMALAR_ULA;
            idmalar |= (uint32_t)((i + 1) * sizeof(SdmmcIdmaDesc_t));
        }
        desc_wr[i].idmalar = idmalar;
    }

    /* 读描述符: pool[1]→pool[8], 全部 ABR=1 */
    SdmmcIdmaDesc_t desc_rd_pool[10];
    uint32_t idmala_rd = (8 - 1) * (uint32_t)sizeof(SdmmcIdmaDesc_t);

    desc_rd_pool[1].buf_addr = (uint32_t)(uintptr_t)rd_buf[0];
    desc_rd_pool[1].buf_size = rd_sizes[0];
    desc_rd_pool[1].idmalar  = SDMMC_IDMALAR_ULS | SDMMC_IDMALAR_ABR
                             | SDMMC_IDMALAR_ULA | idmala_rd;

    desc_rd_pool[8].buf_addr = (uint32_t)(uintptr_t)rd_buf[1];
    desc_rd_pool[8].buf_size = rd_sizes[1];
    desc_rd_pool[8].idmalar  = SDMMC_IDMALAR_ULS | SDMMC_IDMALAR_ABR;

    SdmmcGicConfig();

    /* ---- 中断写: 所有节点 ABR=1, BTC 仅计数 ---- */
    PRINT("  Writing via ISR (all pre-prepared)...\r\n");
    SdCardStartWriteMultiLL(&g_card, test_block, desc_wr, SDMMC_BLKSIZ_512B);

    while (!g_sdmmc1_it_done)
    {
        IwdgKickDog(IWDG2);
    }

    PRINT("  Write done, BTC=");
    PrintDec(dec, (int32_t)g_sdmmc1_btc_cnt);
    PRINT(dec);
    PRINT(" (expect 5)\r\n");

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Write FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    SdCardWaitBusy(&g_card);
    IwdgKickDog(IWDG2);

    for (i = 0; i < 2; i++)
        memset(rd_buf[i], 0, rd_sizes[i]);

    /* ---- 中断读 ---- */
    g_sdmmc1_it_done = 0;
    g_sdmmc1_btc_cnt = 0;
    PRINT("  Reading via ISR (all pre-prepared)...\r\n");
    SdCardStartReadMultiLL(&g_card, test_block, &desc_rd_pool[1], SDMMC_BLKSIZ_512B);

    while (!g_sdmmc1_it_done)
    {
        IwdgKickDog(IWDG2);
    }

    PRINT("  Read done, BTC=");
    PrintDec(dec, (int32_t)g_sdmmc1_btc_cnt);
    PRINT(dec);
    PRINT(" (expect 2)\r\n");

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Read FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    /* 校验 */
    {
        int errors = 0;
        v = 0;
        for (i = 0; i < 2; i++)
        {
            uint16_t *p = (uint16_t *)rd_buf[i];
            uint32_t count = rd_sizes[i] / 2;
            uint32_t j;
            for (j = 0; j < count; j++)
            {
                if (p[j] != v)
                {
                    if (errors < 4)
                    {
                        PRINT("  Mismatch: rd_buf[");
                        PrintDec(dec, i);
                        PRINT(dec);
                        PRINT("][");
                        PrintDec(dec, j);
                        PRINT(dec);
                        PRINT("] expect ");
                        PrintDec(dec, v);
                        PRINT(dec);
                        PRINT(" got ");
                        PrintDec(dec, p[j]);
                        PRINT(dec);
                        PRINT("\r\n");
                    }
                    errors++;
                }
                v++;
            }
        }

        if (errors == 0)
            PRINT("  Verify PASSED (1280 values)\r\n");
        else
        {
            PRINT("  Verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" errors\r\n");
        }
    }

cleanup:
    SdmmcGicCleanup();
}


/* ================================
 *  9. 擦除测试 — 写16块, 擦除前8块, 验证后8块保留
 * ================================ */
static void EraseTest(void)
{
    char dec[16];
    uint32_t test_block = 500000;
    int i;

    PRINT("\r\n----- Erase Test -----\r\n");
    PRINT("  Purpose: verify SD card erase (CMD32+CMD33+CMD38 R1b)\r\n");
    PRINT("  Mode:    blocking | multi-block | no LL\r\n");
    PRINT("  Block:   ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT(" (total 16 blocks)\r\n");
    PRINT("  Write:   CMD25, 16 blocks, uint16_t 0~4095 (8 KB)\r\n");
    PRINT("  Erase:   blocks ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("~");
    PrintDec(dec, (int32_t)(test_block + 7));
    PRINT(dec);
    PRINT(" (first 8 blocks) via SdCardErase\r\n");
    PRINT("  Expect:  blocks 0~7 = 0x00 (erased)\r\n");
    PRINT("           blocks 8~15 = original data preserved\r\n");
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* 填充 WR_BUF: uint16_t 0~4095 */
    uint16_t *p = (uint16_t *)WR_BUF;
    for (i = 0; i < 4096; i++)
        p[i] = (uint16_t)i;

    /* 写 16 块 */
    PRINT("  Writing 16 blocks...\r\n");
    SdCardErr_t err = SdCardWriteMulti(&g_card, test_block, WR_BUF, 16);
    if (err != SD_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 读回独立缓冲区校验 */
    memset(RD_BUF, 0, 8192);
    err = SdCardReadMulti(&g_card, test_block, RD_BUF, 16);
    if (err != SD_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    if (memcmp(WR_BUF, RD_BUF, 8192) == 0)
        PRINT("  Write verify PASSED\r\n");
    else
    {
        for (i = 0; i < 4096; i++)
        {
            if (p[i] != ((uint16_t *)RD_BUF)[i])
            {
                PRINT("  Write verify mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": expect ");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(" got ");
                PrintDec(dec, ((uint16_t *)RD_BUF)[i]);
                PRINT(dec);
                PRINT("\r\n");
                break;
            }
        }
        return;
    }

    /* 擦除前 8 块 */
    PRINT("  Erasing blocks ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("~");
    PrintDec(dec, (int32_t)(test_block + 7));
    PRINT(dec);
    PRINT("...\r\n");
    err = SdCardErase(&g_card, test_block, test_block + 7);
    if (err != SD_OK)
    {
        PRINT("  Erase FAILED\r\n");
        return;
    }
    PRINT("  Erase done\r\n");
    IwdgKickDog(IWDG2);

    /* 重新读回 RD_BUF */
    memset(RD_BUF, 0, 8192);
    err = SdCardReadMulti(&g_card, test_block, RD_BUF, 16);
    if (err != SD_OK)
    {
        PRINT("  Post-erase read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 校验后 8 块 (byte 4096~8191) 保留不变 */
    if (memcmp(WR_BUF + 4096, RD_BUF + 4096, 4096) == 0)
        PRINT("  Preserved blocks (8~15) verify PASSED\r\n");
    else
    {
        for (i = 2048; i < 4096; i++)
        {
            if (p[i] != ((uint16_t *)RD_BUF)[i])
            {
                PRINT("  Preserved mismatch @[");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT("]: expect ");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(" got ");
                PrintDec(dec, ((uint16_t *)RD_BUF)[i]);
                PRINT(dec);
                PRINT("\r\n");
                break;
            }
        }
        return;
    }

    /* 校验前 8 块擦除后全为 0x00 */
    {
        int errors = 0;
        for (i = 0; i < 4096; i++)
        {
            if (RD_BUF[i] != 0x00)
            {
                if (errors < 4)
                    PRINT("  Erased byte mismatch\r\n");
                errors++;
            }
        }

        if (errors == 0)
            PRINT("  Erased blocks (0~7) verify: all 0x00 PASSED\r\n");
        else
        {
            PRINT("  Erased blocks verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" non-00 bytes\r\n");
        }
    }
}


static const MenuEntry_t sdmmc_menu[] = {
    { "1", "Read MBR",              ReadMbrTest },
    { "2", "Write/Read Test",       WriteReadTest },
    { "3", "Multi-block Write/Read",MultiBlockTest },
    { "4", "LL Multi-block Write/Read", LlMultiBlockTest },
    { "5", "LL Interrupt Write/Read",   LlItMultiBlockTest },
    { "6", "LL IT BTC Test (2-node)",   LlItBtcTest },
    { "7", "LL IT BTC Multi (32-node)", LlItBtcMultiTest },
    { "8", "LL IT Pre-prepared",        LlItPreparedTest },
    { "9", "Erase Test (16 blk, erase 8)", EraseTest },
};

void SdmmcTest(void)
{
    PRINT("\r\n===== SDMMC Init =====\r\n");
    if (SdmmcHwInit() != 0)
        return;

    RunSubMenu("SDMMC Test", sdmmc_menu, sizeof(sdmmc_menu) / sizeof(sdmmc_menu[0]));
}
