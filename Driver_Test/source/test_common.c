#include "test_common.h"
#include "stm32mp1xx_dma.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_rcc.h"


/* ========== DMA TX ========== */

static uint8_t g_tx_ring[1024];
static UsartDmaCtx_t g_dma_tx_ctx;

static void DmaTxIsr(void)
{
    UsartDmaTxIsr(&g_dma_tx_ctx);
}


/* ========== DMA RX + 帧解析 ========== */

static uint8_t g_rx_ring[256];
static UsartDmaRxCtx_t g_dma_rx_ctx;
static FrameParser_t g_frame_parser;
static volatile int g_cmd_ready;
static Frame_t g_cmd_frame;

static void OnFrame(const Frame_t *frame)
{
    if (frame->cmd == FRAME_CMD_ECHO)
    {
        PRINT("Echo: ");
        uint8_t i;
        for (i = 0; i < frame->data_len; i++)
            PrintHex8(frame->data[i]);
        PRINT("\r\n");
        return;
    }
    g_cmd_frame = *frame;
    g_cmd_ready = 1;
}


int FramePoll(Frame_t *out)
{
    uint8_t b;
    while (UsartDmaRxReadOne(&g_dma_rx_ctx, &b))
        FrameParserFeed(&g_frame_parser, b);

    if (g_cmd_ready)
    {
        g_cmd_ready = 0;
        if (out)
            *out = g_cmd_frame;
        return 1;
    }
    return 0;
}


/* ========== Init ========== */

void TestCommonInit(void)
{
    RCC->MP_AHB2ENSETR |= (1u << 1) | (1u << 2);

    /* DMA TX: DMA2 Stream 0 */
    UsartDmaTxInit(&g_dma_tx_ctx, USART4, DMA2, 0,
                   DMAMUX_REQ_UART4_TX, g_tx_ring, sizeof(g_tx_ring));

    GicdSetGroup(GIC_DMA2_STR0);
    GicdSetPriority(GIC_DMA2_STR0, 5);
    GicdSetTarget(GIC_DMA2_STR0, 1);
    GicdSetTrigMode(GIC_DMA2_STR0, 0);
    GicRegisterIrq(GIC_DMA2_STR0, DmaTxIsr);
    GicdEnableInt(GIC_DMA2_STR0);
    GicdInit();
    GiccInit(10, 2);

    /* DMA RX: DMA2 Stream 1, 循环模式 */
    UsartDmaRxInit(&g_dma_rx_ctx, USART4, DMA2, 1,
                   DMAMUX_REQ_UART4_RX, g_rx_ring, sizeof(g_rx_ring));
    FrameParserInit(&g_frame_parser, OnFrame);
}


void PrintDma(const char *s)
{
    uint32_t len = strlen(s);
    const uint8_t *p = (const uint8_t *)s;
    while (len > 0)
    {
        int sent = UsartDmaSend(&g_dma_tx_ctx, p, len);
        if (sent > 0)
        {
            p += sent;
            len -= sent;
        }
        IwdgKickDog(IWDG2);
    }
}


void PrintFlush(void)
{
    while (g_dma_tx_ctx.tx_busy || g_dma_tx_ctx.tx_head != g_dma_tx_ctx.tx_tail)
    {
        IwdgKickDog(IWDG2);
    }
}


void NumToStr(char *buf, uint32_t val)
{
    char rev[12];
    int i = 0;
    do {
        rev[i++] = '0' + val % 10;
        val /= 10;
    } while (val);
    while (i > 0)
        *buf++ = rev[--i];
    *buf = '\0';
}


void PrintU32(const char *label, uint32_t val)
{
    char buf[48];
    int p = 0;
    while (*label) buf[p++] = *label++;
    buf[p++] = ':';
    buf[p++] = ' ';
    NumToStr(buf + p, val);
    while (buf[p]) p++;
    buf[p++] = '\r';
    buf[p++] = '\n';
    buf[p] = '\0';
    PRINT(buf);
}


void PrintDec(char *buf, int32_t val)
{
    if (val < 0)
    {
        *buf++ = '-';
        val = -val;
    }
    char *p = buf;
    do {
        *p++ = '0' + val % 10;
        val /= 10;
    } while (val > 0);
    *p = '\0';
    p--;
    while (buf < p)
    {
        char t = *buf;
        *buf++ = *p;
        *p = t;
        p--;
    }
}


void PrintHex8(uint8_t val)
{
    char buf[3];
    char hex[] = "0123456789ABCDEF";
    buf[0] = hex[val >> 4];
    buf[1] = hex[val & 0xF];
    buf[2] = '\0';
    PRINT(buf);
}


void PrintHex32(char *buf, uint32_t val)
{
    int i;
    for (i = 28; i >= 0; i -= 4)
    {
        uint32_t n = (val >> i) & 0xF;
        *buf++ = n < 10 ? '0' + n : 'A' + n - 10;
    }
    *buf = '\0';
}


int ReadLine(char *buf, int max_len)
{
    PrintFlush();
    Frame_t fr;

    while (!FramePoll(&fr))
        IwdgKickDog(IWDG2);

    if (fr.cmd == FRAME_CMD_BACK)
    {
        buf[0] = '0';
        buf[1] = '\0';
        PRINT("0\r\n");
        return 1;
    }

    int len = fr.data_len;
    if (len >= max_len)
        len = max_len - 1;
    int i;
    for (i = 0; i < len; i++)
        buf[i] = fr.data[i];
    buf[len] = '\0';
    PRINT(buf);
    PRINT("\r\n");
    return len;
}


void RunSubMenu(const char *title, const MenuEntry_t *items, int count)
{
    char buf[16];
    while (1)
    {
        IwdgKickDog(IWDG2);
        PRINT("\r\n===== ");
        PRINT(title);
        PRINT(" =====\r\n");
        int i;
        for (i = 0; i < count; i++)
        {
            PRINT(items[i].key);
            PRINT(". ");
            PRINT(items[i].desc);
            PRINT("\r\n");
        }
        PRINT("0. Back\r\n");
        PRINT("Select: ");

        ReadLine(buf, sizeof(buf));

        if (strcmp(buf, "0") == 0)
            return;

        for (i = 0; i < count; i++)
        {
            if (strcmp(buf, items[i].key) == 0)
            {
                items[i].func();
                break;
            }
        }
        if (i == count)
            PRINT("Invalid selection.\r\n");
    }
}
