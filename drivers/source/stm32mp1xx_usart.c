#include "stm32mp1xx_usart.h"


volatile UsartRegs_t *const USART1 = (void *)0x5C000000;
volatile UsartRegs_t *const USART6 = (void *)0x44003000;
volatile UsartRegs_t *const USART8 = (void *)0x40019000;
volatile UsartRegs_t *const USART7 = (void *)0x40018000;
volatile UsartRegs_t *const USART5 = (void *)0x40011000;
volatile UsartRegs_t *const USART4 = (void *)0x40010000;
volatile UsartRegs_t *const USART3 = (void *)0x4000F000;
volatile UsartRegs_t *const USART2 = (void *)0x4000E000;


void UsartCfg(volatile UsartRegs_t *const usart_reg, const UsartCfg_t *const cfg)
{
    usart_reg->CR1 &= ~1;

    // 超采样及波特率设置
    if (cfg->usart_sample_mode == 16)
    {
        usart_reg->CR1 &= ~(1 << 15);
    }
    else
    {
        usart_reg->CR1 |= 1 << 15;
    }
    usart_reg->BRR = 556;

    uint32_t word_len = cfg->usart_word_len;
    if (cfg->usart_parity != 0)
    {
        word_len += 1;
    }
    // 字长设置 7~9bit
    if (word_len == 8)
    {
        usart_reg->CR1 &= ~(1 << 28);
        usart_reg->CR1 &= ~(1 << 12);
    }
    else if (word_len == 9)
    {
        usart_reg->CR1 &= ~(1 << 28);
        usart_reg->CR1 |= 1 << 12;
    }
    else
    {
        usart_reg->CR1 |= 1 << 28;
        usart_reg->CR1 &= ~(1 << 12);
    }

    // 校验设置
    if (cfg->usart_parity == 1)
    {
        // 奇校验
        usart_reg->CR1 |= 1 << 10;
        usart_reg->CR1 |= 1 << 9;
    }
    else if (cfg->usart_parity == 2)
    {
        // 偶校验
        usart_reg->CR1 |= 1 << 10;
        usart_reg->CR1 &= ~(1 << 9);
    }
    else
    {
        // 无校验
        usart_reg->CR1 &= ~(1 << 10);
    }

    // 停止位
    usart_reg->CR2 &= ~(3 << 12);
    if (cfg->usart_stop_bit == 1)
    {
        usart_reg->CR2 &= ~(3 << 12);
    }
    else if (cfg->usart_stop_bit == 2)
    {
        usart_reg->CR2 |= 2 << 12;
    }
    else if (cfg->usart_stop_bit == 0)
    {
        usart_reg->CR2 |= 1 << 12;
    }
    else
    {
        usart_reg->CR2 |= 3 << 12;
    }

    // 采样bit设置
    if (cfg->usart_one_sample == 1)
    {
        usart_reg->CR3 |= 1 << 11;
    }
    else
    {
        usart_reg->CR3 &= ~(1 << 11);
    }

    // 硬件流控制
//    usart_reg->CR3 |= 1 << 10;
//    usart_reg->CR3 |= 1 << 9; // CTS
//    usart_reg->CR3 |= 1 << 8; // RTS

    // FIFO及中断设置
    if (cfg->usart_fifo_en == 1)
    {
        usart_reg->CR1 |= 1 << 29; // 使能FIFO

        usart_reg->CR3 |= cfg->usart_txff_tl << 29; // 发送FIFO阈值
        usart_reg->CR3 |= cfg->usart_txff_tl << 25; // 接收FIFO阈值

        usart_reg->CR1 |= 1 << 31; // rxff接收FF满中断使能
        usart_reg->CR1 |= 1 << 30; // txfe发送FF空中断使能
        usart_reg->CR1 |= 1 << 8;  // PE中断
        usart_reg->CR1 |= 1 << 7;  // 发送FF未满中断
        usart_reg->CR1 |= 1 << 6;  // TC发送完成中断
        usart_reg->CR1 |= 1 << 5;  // 接收FF非空中断
        usart_reg->CR1 |= 1 << 4;  // IDLE空闲中断
        usart_reg->CR3 |= 1 << 28; // 接收FIFO阈值中断
        usart_reg->CR3 |= 1 << 23; // 发送FIFO阈值中断
        usart_reg->CR3 |= 1 << 22; // 从低功耗唤醒中断
    }
    else
    {
        usart_reg->CR1 &= ~(1 << 29); // 禁止FIFO

        usart_reg->CR1 |= 1 << 8;  // PE中断
        usart_reg->CR1 |= 1 << 7;  // 发送寄存器空中断
        usart_reg->CR1 |= 1 << 6;  // TC发送完成中断
        usart_reg->CR1 |= 1 << 5;  // 接收寄存器非空中断
        usart_reg->CR1 |= 1 << 4;  // IDLE空闲中断
    }
    usart_reg->CR3 |= 1; // 错误中断使能

    usart_reg->CR1 |= 1 << 3; // 发送使能
    usart_reg->CR1 |= 1 << 2; // 接收使能
    usart_reg->CR1 |= 1; // 使能USART
}


// 阻塞CPU直至最后一个数写入发送寄存器
void UsartWrite(volatile UsartRegs_t *const usart_reg, const uint8_t *dat, const uint32_t len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        // 等待TXE发送寄存器为空（无FIFO）或TXFNF发送FIFO不满（FIFO模式）
        while ((usart_reg->ISR & (1 << 7)) == 0)
        {

        }
        usart_reg->TDR = dat[i];
    }
}


void UsartRead(volatile UsartRegs_t *const usart_reg, uint8_t *dat, const uint32_t len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        // 等待RXNE接收寄存器不为空（无FIFO）或RXFNE接收FIFO不为空（FIFO模式）
        while ((usart_reg->ISR & (1 << 5)) == 0)
        {

        }
        *dat++ = usart_reg->RDR;
    }
}


uint32_t UsartReadOne(volatile UsartRegs_t *const usart_reg, uint8_t *dat)
{

    if ((usart_reg->ISR & (1 << 5)) != 0)
    {
        *dat = usart_reg->RDR;
        return 1;
    }

    return 0;
}


uint8_t UsartReadAll(volatile UsartRegs_t *const usart_reg)
{
    uint8_t dat;
    while ((usart_reg->ISR & (1 << 5)) != 0)
    {
        dat = usart_reg->RDR;
    }

    return dat;
}


/*
 * 从环形缓冲区取一段连续数据启动 DMA 传输。
 * 回绕时只取 tail 到末尾的部分, TC 中断里会再次 Kick 取剩余部分。
 */
static void UsartDmaKick(UsartDmaCtx_t *ctx)
{
    if (ctx->tx_busy || ctx->tx_head == ctx->tx_tail)
        return;

    ctx->tx_busy = 1;

    uint32_t tail = ctx->tx_tail;
    uint32_t head = ctx->tx_head;
    uint32_t len;

    /* 取 tail 到 head 或 tail 到缓冲区末尾, 取较短的连续段 */
    if (head > tail)
        len = head - tail;
    else
        len = ctx->tx_size - tail;

    ctx->tx_dma_len = len;
    ctx->dma_cfg.dma_mem0_addr = (uint32_t)&ctx->tx_buf[tail];
    ctx->dma_cfg.dma_ndtr      = len;

    DmaCfg(ctx->dma, &ctx->dma_cfg);   /* 配置并使能 DMA Stream */

    ctx->usart->CR3 |= (1u << 7);      /* DMAT=1, 开启 USART DMA 发送请求 */
}


/*
 * 一次性初始化: 绑定 USART/DMA/环形缓冲区, 准备 DMA 配置模板, 路由 DMAMUX。
 * 不启动传输, 首次 UsartDmaSend 时才触发 DMA。
 */
void UsartDmaTxInit(UsartDmaCtx_t *ctx, volatile UsartRegs_t *usart,
                    volatile DmaRegs_t *dma, uint32_t stream,
                    DmaMuxReqId_t req_id, uint8_t *buf, uint32_t size)
{
    ctx->usart      = usart;
    ctx->dma        = dma;
    ctx->stream     = stream;
    ctx->tx_buf     = buf;
    ctx->tx_size    = size;
    ctx->tx_head    = 0;
    ctx->tx_tail    = 0;
    ctx->tx_dma_len = 0;
    ctx->tx_busy    = 0;

    /* 确保 DMA Stream 处于已知状态 */
    DmaDisable(dma, stream);
    DmaClearTcif(dma, stream);

    /* DMA 配置模板: M→P, 8bit, MINC, TCIE, 直接模式; M0AR/NDTR 由 Kick 填入 */
    ctx->dma_cfg              = DMA_CFG_DEFAULT;
    ctx->dma_cfg.dma_stream_num  = stream;
    ctx->dma_cfg.dma_dir         = DMA_DIR_MEM_2_PER;
    ctx->dma_cfg.dma_psize       = DMA_DATA_SIZE_BIT8;
    ctx->dma_cfg.dma_msize       = DMA_DATA_SIZE_BIT8;
    ctx->dma_cfg.dma_memaddr_incr = 1;
    ctx->dma_cfg.dma_peraddr_incr = 0;
    ctx->dma_cfg.dma_per_addr    = (uint32_t)&usart->TDR;
    ctx->dma_cfg.dma_tcie        = 1;

    /* DMA1 Stream N → DMAMUX ch N, DMA2 Stream N → DMAMUX ch N+8 */
    uint32_t dmamux_ch = (dma == DMA2) ? stream + 8 : stream;
    DmaMuxRoute(DMAMUX1, dmamux_ch, req_id);
}


/*
 * 非阻塞发送: 数据拷入环形缓冲区, DMA 空闲时自动启动。
 * 返回实际入队字节数, 缓冲区满时截断。
 */
int UsartDmaSend(UsartDmaCtx_t *ctx, const uint8_t *data, uint32_t len)
{
    uint32_t head = ctx->tx_head;
    uint32_t tail = ctx->tx_tail;
    uint32_t free;

    /* 保留 1 字节不用, 使 head==tail 唯一表示"空" */
    if (head >= tail)
        free = ctx->tx_size - 1 - (head - tail);
    else
        free = tail - head - 1;

    if (len > free)
        len = free;

    for (uint32_t i = 0; i < len; i++)
    {
        ctx->tx_buf[head] = data[i];
        head++;
        if (head >= ctx->tx_size)
            head = 0;
    }
    ctx->tx_head = head;

    UsartDmaKick(ctx);
    return (int)len;
}


/*
 * DMA 传输完成中断回调, 由用户在 GIC ISR 中调用。
 * 推进 tail, 缓冲区有剩余数据则续传, 否则关闭 DMAT。
 */
void UsartDmaTxIsr(UsartDmaCtx_t *ctx)
{
    DmaClearTcif(ctx->dma, ctx->stream);

    ctx->tx_tail = (ctx->tx_tail + ctx->tx_dma_len) % ctx->tx_size;
    ctx->tx_busy = 0;

    if (ctx->tx_head != ctx->tx_tail)
        UsartDmaKick(ctx);                  /* 还有数据, 续传 */
    else
        ctx->usart->CR3 &= ~(1u << 7);     /* 缓冲区空, 关 DMAT */
}


/* ========== DMA 循环接收 ========== */


void UsartDmaRxInit(UsartDmaRxCtx_t *ctx, volatile UsartRegs_t *usart,
                    volatile DmaRegs_t *dma, uint32_t stream,
                    DmaMuxReqId_t req_id, uint8_t *buf, uint32_t size)
{
    ctx->usart   = usart;
    ctx->dma     = dma;
    ctx->stream  = stream;
    ctx->rx_buf  = buf;
    ctx->rx_size = size;
    ctx->rx_rd   = 0;

    DmaDisable(dma, stream);
    DmaClearTcif(dma, stream);

    DmaCfg_t cfg          = DMA_CFG_DEFAULT;
    cfg.dma_stream_num    = stream;
    cfg.dma_dir           = DMA_DIR_PER_2_MEM;
    cfg.dma_psize         = DMA_DATA_SIZE_BIT8;
    cfg.dma_msize         = DMA_DATA_SIZE_BIT8;
    cfg.dma_memaddr_incr  = 1;
    cfg.dma_peraddr_incr  = 0;
    cfg.dma_per_addr      = (uint32_t)&usart->RDR;
    cfg.dma_mem0_addr     = (uint32_t)buf;
    cfg.dma_ndtr          = size;
    cfg.dma_circual_buf   = 1;

    uint32_t dmamux_ch = (dma == DMA2) ? stream + 8 : stream;
    DmaMuxRoute(DMAMUX1, dmamux_ch, req_id);

    usart->ICR = (1u << 3);        /* 清 ORE */
    (void)usart->RDR;              /* 排空残留数据 */

    DmaCfg(dma, &cfg);             /* 配置并启动 DMA */
    usart->CR3 |= (1u << 6);       /* DMAR=1, 使能 USART DMA 接收 */
}


uint32_t UsartDmaRxAvail(UsartDmaRxCtx_t *ctx)
{
    uint32_t ndtr = ctx->dma->STREAM[ctx->stream].NDTR;
    uint32_t wr = ctx->rx_size - ndtr;
    if (wr >= ctx->rx_size)
        wr = 0;
    if (wr >= ctx->rx_rd)
        return wr - ctx->rx_rd;
    return ctx->rx_size - ctx->rx_rd + wr;
}


int UsartDmaRxReadOne(UsartDmaRxCtx_t *ctx, uint8_t *byte)
{
    uint32_t ndtr = ctx->dma->STREAM[ctx->stream].NDTR;
    uint32_t wr = ctx->rx_size - ndtr;
    if (wr >= ctx->rx_size)
        wr = 0;
    if (wr == ctx->rx_rd)
        return 0;
    *byte = ctx->rx_buf[ctx->rx_rd];
    ctx->rx_rd++;
    if (ctx->rx_rd >= ctx->rx_size)
        ctx->rx_rd = 0;
    return 1;
}


void UsartDmaRxStop(UsartDmaRxCtx_t *ctx)
{
    ctx->usart->CR3 &= ~(1u << 6);     /* DMAR=0 */
    DmaDisable(ctx->dma, ctx->stream);
    ctx->usart->ICR = (1u << 3);        /* 清 ORE */
    (void)ctx->usart->RDR;              /* 排空, 恢复轮询模式 */
}


