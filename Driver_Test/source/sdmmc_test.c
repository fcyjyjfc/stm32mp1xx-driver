#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_rcc.h"
#include "sd_card.h"
#include "test_common.h"

static SdCard_t g_card;
/* 多块测试用大缓冲区: 256 块 × 512B = 128KB */
static uint8_t g_buf[256 * 512] __attribute__((aligned(4)));


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


/* ================================
 *  1. 读 MBR 测试 — 检查 0x55AA 签名
 * ================================ */
static void ReadMbrTest(void)
{
    PRINT("\r\n----- Read MBR (Block 0) -----\r\n");

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

    PRINT("\r\n----- Write/Read Test (Block ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT(") -----\r\n");

    IwdgKickDog(IWDG2);

    /* 写入 pattern: 每字节 = 索引低8位 XOR 0xA5 */
    for (i = 0; i < 512; i++)
        g_buf[i] = (uint8_t)(i ^ 0xA5);

    PRINT("  Writing...\r\n");
    SdCardErr_t err = SdCardWriteBlocks(&g_card, test_block, g_buf, 1);
    if (err != SD_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 清缓冲区再读回 */
    memset(g_buf, 0, 512);
    PRINT("  Reading back...\r\n");
    err = SdCardReadBlocks(&g_card, test_block, g_buf, 1);
    if (err != SD_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    /* 校验 */
    int errors = 0;
    for (i = 0; i < 512; i++)
    {
        if (g_buf[i] != (uint8_t)(i ^ 0xA5))
        {
            if (errors < 4)
            {
                PRINT("  Mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": expect ");
                PrintHex8((uint8_t)(i ^ 0xA5));
                PRINT(" got ");
                PrintHex8(g_buf[i]);
                PRINT("\r\n");
            }
            errors++;
        }
    }

    if (errors == 0)
        PRINT("  Verify PASSED\r\n");
    else
    {
        PRINT("  Verify FAILED: ");
        PrintDec(dec, errors);
        PRINT(dec);
        PRINT(" errors\r\n");
    }
}


/* ================================
 *  3. 多块读写测试 (block 100000, 256 块)
 *     写连续的 uint16_t 0~65535, 回读校验
 * ================================ */
static void MultiBlockTest(void)
{
    char dec[16];
    uint32_t test_block = 100000;
    uint32_t n_blocks = 256;  /* 65536 个 uint16_t × 2B = 128KB */
    int i;

    PRINT("\r\n----- Multi-block Write/Read (");
    PrintDec(dec, (int32_t)n_blocks);
    PRINT(dec);
    PRINT(" blocks @ block ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT(") -----\r\n");

    IwdgKickDog(IWDG2);

    /* 填 pattern: uint16_t 0~65535 */
    uint16_t *p = (uint16_t *)g_buf;
    for (i = 0; i < 65536; i++)
        p[i] = (uint16_t)i;

    PRINT("  Writing ");
    PrintDec(dec, (int32_t)n_blocks);
    PRINT(dec);
    PRINT(" blocks...\r\n");
    SdCardErr_t err = SdCardWriteMulti(&g_card, test_block, g_buf, n_blocks);
    if (err != SD_OK)
    {
        PRINT("  Write FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    memset(g_buf, 0, n_blocks * 512);
    PRINT("  Reading back...\r\n");
    err = SdCardReadMulti(&g_card, test_block, g_buf, n_blocks);
    if (err != SD_OK)
    {
        PRINT("  Read FAILED\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    int errors = 0;
    for (i = 0; i < 65536; i++)
    {
        if (p[i] != (uint16_t)i)
        {
            if (errors < 4)
            {
                PRINT("  Mismatch @[");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT("]: expect ");
                PrintHex8((uint8_t)(i >> 8));
                PrintHex8((uint8_t)i);
                PRINT(" got ");
                PrintHex8((uint8_t)(p[i] >> 8));
                PrintHex8((uint8_t)p[i]);
                PRINT("\r\n");
            }
            errors++;
        }
    }

    if (errors == 0)
        PRINT("  Verify PASSED (65536 values)\r\n");
    else
    {
        PRINT("  Verify FAILED: ");
        PrintDec(dec, errors);
        PRINT(dec);
        PRINT(" errors\r\n");
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
    PRINT("  Write: 5 nodes (512+512+480+544+512), ULS mix\r\n");
    PRINT("  Read:  2 nodes (1024+1536)\r\n");
    PRINT("  Block: ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
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
    SdCardErr_t err = SdCardWriteMultiLL(&g_card, test_block, desc_wr, 9);
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
    err = SdCardReadMultiLL(&g_card, test_block, &desc_rd_pool[1], 9);
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

    PRINT("\r\n----- LL Interrupt Write/Read (1 node, ULA=0) -----\r\n");
    PRINT("  Block: ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
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

    /* 注册 SDMMC1 中断 */
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

    /* ---- 中断写 ---- */
    PRINT("  Writing via ISR...\r\n");
    SdCardStartWriteMultiLL(&g_card, test_block, &desc, 9);

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

    /* 写后卡内部编程 (BUSYD0=1), 等释放再回读, 否则 SdmmcStartLL 的 if 检查会拒绝 */
    while (g_card.sdmmc->STAR & SDMMC_STA_BUSYD0)
        ;
    IwdgKickDog(IWDG2);

    /* 清空 buffer */
    memset(buf, 0, buf_size);

    /* ---- 中断读 ---- */
    PRINT("  Reading via ISR...\r\n");
    SdCardStartReadMultiLL(&g_card, test_block, &desc, 9);

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

    /* 校验 */
    {
        int errors = 0;
        for (i = 0; i < 256; i++)
        {
            if (p[i] != (uint16_t)i)
            {
                if (errors < 4)
                {
                    PRINT("  Mismatch @");
                    PrintDec(dec, i);
                    PRINT(dec);
                    PRINT(": expect ");
                    PrintDec(dec, i);
                    PRINT(dec);
                    PRINT(" got ");
                    PrintDec(dec, p[i]);
                    PRINT(dec);
                    PRINT("\r\n");
                }
                errors++;
            }
        }

        if (errors == 0)
            PRINT("  Verify PASSED (256 values, ISR-driven)\r\n");
        else
        {
            PRINT("  Verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" errors\r\n");
        }
    }

cleanup:
    GicdDisableInt(GIC_SDMMC1);
    g_sdmmc1_it_done = 0;
}


/* ================================
 *  6. 中断 LL BTC 动态 BAR 测试 — 验证硬件"先查BAR再加载"的行为
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

    PRINT("\r\n----- LL IT BTC Dynamic BAR Test (2 nodes) -----\r\n");
    PRINT("  Theory: HW checks next desc ABR before loading\r\n");
    PRINT("  If correct: BTC=1, DATAEND OK\r\n");
    PRINT("  If wrong:   IDMATE fires (no IRQ), transfer hangs\r\n");
    PRINT("  Block: ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
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

    /* ======== GIC 中断注册 ======== */
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

    /* ---- 中断写: 只预填了节点0, 节点1 由前台在 BTC 后动态填 ---- */
    PRINT("  Writing 2 nodes via ISR (node1 BAR=0, fill on BTC)...\r\n");
    SdCardStartWriteMultiLL(&g_card, test_block, desc_wr, 9);

    {
        uint32_t last_btc = 0;
        uint32_t timeout = 0;
        uint16_t fill_val = 512;  /* 节点0 填了 0..511, 节点1 从 512 开始 */
        while (!g_sdmmc1_it_done)
        {
            /* IDMATE 无中断使能, 前台轮询检测 */
            if (g_card.sdmmc->STAR & SDMMC_STA_IDMATE)
            {
                PRINT("  WRITE: IDMATE detected! HW loads desc BEFORE checking ABR\r\n");
                g_card.sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
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
    while (g_card.sdmmc->STAR & SDMMC_STA_BUSYD0)
        ;
    IwdgKickDog(IWDG2);

    /* 清读缓冲区 */
    for (i = 0; i < 2; i++)
        memset(rd_buf[i], 0, rd_sizes[i]);

    /* ---- 中断读: 读不需要填数据, 前台只置 BAR ---- */
    g_sdmmc1_it_done = 0;
    g_sdmmc1_btc_cnt = 0;
    PRINT("  Reading 2 nodes via ISR (node1 BAR=0, set on BTC)...\r\n");
    SdCardStartReadMultiLL(&g_card, test_block, &desc_rd_pool[1], 9);

    {
        uint32_t last_btc = 0;
        uint32_t timeout = 0;
        while (!g_sdmmc1_it_done)
        {
            if (g_card.sdmmc->STAR & SDMMC_STA_IDMATE)
            {
                PRINT("  READ: IDMATE detected! HW loads desc BEFORE checking ABR\r\n");
                g_card.sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
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
    GicdDisableInt(GIC_SDMMC1);
    g_sdmmc1_it_done = 0;
}


/* ================================
 *  7. 中断 LL BTC 双缓冲测试 — 8节点×512B
 *     前2节点预填+BAR=1, 后续节点BAR=0
 *     前台轮询 BTC → 准备节点(N+2) → 置 BAR=1
 *     写/读各8节点=4096B, uint16_t 0~2047
 * ================================ */
static void LlItBtcMultiTest(void)
{
#define N_NODES 8
    char dec[16];
    uint32_t test_block = 400000;
    int i;

    PRINT("\r\n----- LL IT BTC Double-buffer (");
    PrintDec(dec, N_NODES);
    PRINT(dec);
    PRINT(" nodes x 512B) -----\r\n");
    PRINT("  Node0+1 pre-filled (BAR=1), BTC(N) prepares node(N+2)\r\n");
    PRINT("  Block: ");
    PrintDec(dec, (int32_t)test_block);
    PRINT(dec);
    PRINT("\r\n");

    IwdgKickDog(IWDG2);

    /* ======== 缓冲区: 写/读各 8×512=4096B ======== */
    uint32_t node_size = 512;
    uint8_t  *wr_buf[N_NODES];
    uint8_t  *rd_buf[N_NODES];
    for (i = 0; i < N_NODES; i++)
    {
        wr_buf[i] = g_buf + i * node_size;
        rd_buf[i] = g_buf + 8192 + i * node_size;
    }

    /* 预填节点 0,1: uint16_t 0..511 */
    uint16_t v = 0;
    for (i = 0; i < 2; i++)
    {
        uint16_t *p = (uint16_t *)wr_buf[i];
        uint32_t j;
        for (j = 0; j < 256; j++)
            p[j] = v++;
    }
    /* v = 512, 后续动态填 */

    /* ======== 写描述符 ======== */
    SdmmcIdmaDesc_t desc_wr[N_NODES];
    for (i = 0; i < N_NODES; i++)
    {
        desc_wr[i].buf_addr = (uint32_t)(uintptr_t)wr_buf[i];
        desc_wr[i].buf_size = node_size;
        uint32_t idmalar = SDMMC_IDMALAR_ULS;
        if (i < 2)
            idmalar |= SDMMC_IDMALAR_ABR;
        if (i < N_NODES - 1)
        {
            idmalar |= SDMMC_IDMALAR_ULA;
            idmalar |= (uint32_t)((i + 1) * sizeof(SdmmcIdmaDesc_t));
        }
        desc_wr[i].idmalar = idmalar;
    }

    /* ======== 读描述符 ======== */
    SdmmcIdmaDesc_t desc_rd[N_NODES];
    for (i = 0; i < N_NODES; i++)
    {
        desc_rd[i].buf_addr = (uint32_t)(uintptr_t)rd_buf[i];
        desc_rd[i].buf_size = node_size;
        uint32_t idmalar = SDMMC_IDMALAR_ULS;
        if (i < 2)
            idmalar |= SDMMC_IDMALAR_ABR;
        if (i < N_NODES - 1)
        {
            idmalar |= SDMMC_IDMALAR_ULA;
            idmalar |= (uint32_t)((i + 1) * sizeof(SdmmcIdmaDesc_t));
        }
        desc_rd[i].idmalar = idmalar;
    }

    /* ======== GIC 中断注册 ======== */
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

    /* ---- 中断写 ---- */
    PRINT("  Writing 8 nodes via ISR...\r\n");
    SdCardStartWriteMultiLL(&g_card, test_block, desc_wr, 9);

    {
        uint32_t last_btc = 0;
        uint32_t timeout = 0;
        uint16_t fill_val = 512;
        while (!g_sdmmc1_it_done)
        {
            if (g_card.sdmmc->STAR & SDMMC_STA_IDMATE)
            {
                PRINT("  WRITE: IDMATE detected!\r\n");
                g_card.sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
                goto cleanup;
            }
            uint32_t cur = g_sdmmc1_btc_cnt;
            if (cur != last_btc)
            {
                last_btc = cur;
                /* BTC=cur: 节点(cur-1)完成, 准备节点(cur+1) */
                uint32_t prep = cur + 1;
                if (prep < N_NODES)
                {
                    uint16_t *p = (uint16_t *)wr_buf[prep];
                    uint32_t j;
                    for (j = 0; j < 256; j++)
                        p[j] = fill_val++;
                    desc_wr[prep].idmalar |= SDMMC_IDMALAR_ABR;
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
    PRINT(" (expect 7)\r\n");

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Write FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    /* 写后等卡编程完成 */
    while (g_card.sdmmc->STAR & SDMMC_STA_BUSYD0)
        ;
    IwdgKickDog(IWDG2);

    /* 清读缓冲区 */
    for (i = 0; i < N_NODES; i++)
        memset(rd_buf[i], 0, node_size);

    /* ---- 中断读 ---- */
    g_sdmmc1_it_done = 0;
    g_sdmmc1_btc_cnt = 0;
    PRINT("  Reading 8 nodes via ISR...\r\n");
    SdCardStartReadMultiLL(&g_card, test_block, desc_rd, 9);

    {
        uint32_t last_btc = 0;
        uint32_t timeout = 0;
        while (!g_sdmmc1_it_done)
        {
            if (g_card.sdmmc->STAR & SDMMC_STA_IDMATE)
            {
                PRINT("  READ: IDMATE detected!\r\n");
                g_card.sdmmc->ICR = SDMMC_ICR_ALLFLAGS;
                goto cleanup;
            }
            uint32_t cur = g_sdmmc1_btc_cnt;
            if (cur != last_btc)
            {
                last_btc = cur;
                uint32_t prep = cur + 1;
                if (prep < N_NODES)
                    desc_rd[prep].idmalar |= SDMMC_IDMALAR_ABR;
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
    PRINT(" (expect 7)\r\n");

    if (g_sdmmc1_it_result != SDMMC_OK)
    {
        PRINT("  IT Read FAILED, err=");
        PrintDec(dec, (int32_t)g_sdmmc1_it_result);
        PRINT(dec);
        PRINT("\r\n");
        goto cleanup;
    }

    /* 校验: 跨 8 个读 buffer 逐一比对 */
    {
        int errors = 0;
        v = 0;
        for (i = 0; i < N_NODES; i++)
        {
            uint16_t *p = (uint16_t *)rd_buf[i];
            uint32_t j;
            for (j = 0; j < 256; j++)
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
            PRINT("  Verify PASSED (2048 values)\r\n");
        else
        {
            PRINT("  Verify FAILED: ");
            PrintDec(dec, errors);
            PRINT(dec);
            PRINT(" errors\r\n");
        }
    }

cleanup:
    GicdDisableInt(GIC_SDMMC1);
    g_sdmmc1_it_done = 0;
#undef N_NODES
}


static const MenuEntry_t sdmmc_menu[] = {
    { "1", "Read MBR",              ReadMbrTest },
    { "2", "Write/Read Test",       WriteReadTest },
    { "3", "Multi-block Write/Read",MultiBlockTest },
    { "4", "LL Multi-block Write/Read", LlMultiBlockTest },
    { "5", "LL Interrupt Write/Read",   LlItMultiBlockTest },
    { "6", "LL IT BTC Test (2-node)",   LlItBtcTest },
    { "7", "LL IT BTC Multi (8-node)",  LlItBtcMultiTest },
};

void SdmmcTest(void)
{
    PRINT("\r\n===== SDMMC Init =====\r\n");
    if (SdmmcHwInit() != 0)
        return;

    RunSubMenu("SDMMC Test", sdmmc_menu, sizeof(sdmmc_menu) / sizeof(sdmmc_menu[0]));
}
