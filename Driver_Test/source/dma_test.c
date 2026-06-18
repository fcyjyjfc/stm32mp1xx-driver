#include <string.h>
#include "stm32mp1xx_dma.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_gic.h"
#include "stm32mp1xx_rcc.h"
#include "test_common.h"


static uint8_t tx_ring[256];
static UsartDmaCtx_t uart4_dma_ctx;


static void Dma2Str0Isr(void)
{
    UsartDmaTxIsr(&uart4_dma_ctx);
}


/* 等待 DMA 发送完成, 完成后 DMAT 已关闭, 可安全使用轮询 PRINT */
static void WaitDmaTxDone(void)
{
    while (uart4_dma_ctx.tx_busy)
    {
        IwdgKickDog(IWDG2);
    }
}


void DmaTest(void)
{
    /* DMA2 + DMAMUX 时钟使能 */
    RCC->MP_AHB2ENSETR |= (1u << 1) | (1u << 2);  /* DMA2=bit1, DMAMUX=bit2 */

    /* 绑定 UART4 + DMA2 Stream0, 路由 DMAMUX */
    UsartDmaTxInit(&uart4_dma_ctx, USART4, DMA2, 0,
                   DMAMUX_REQ_UART4_TX, tx_ring, sizeof(tx_ring));

    /* DMA2 Stream0 TC 中断注册到 GIC */
    GicdSetGroup(GIC_DMA2_STR0);
    GicdSetPriority(GIC_DMA2_STR0, 5);
    GicdSetTarget(GIC_DMA2_STR0, 1);
    GicdSetTrigMode(GIC_DMA2_STR0, 0);
    GicRegisterIrq(GIC_DMA2_STR0, Dma2Str0Isr);
    GicdEnableInt(GIC_DMA2_STR0);

    /* GIC + CPU 中断使能 */
    GicdInit();
    GiccInit(10, 2);
    __asm__ volatile(
        "mrs r0, cpsr\n\t"
        "bic r0, r0, #0x80\n\t"
        "msr cpsr, r0\n\t"
        ::: "r0"
    );

    /* 通过 DMA 发送 HELLO WORLD */
    const char *msg = "HELLO WORLD\r\n";
    UsartDmaSend(&uart4_dma_ctx, (const uint8_t *)msg, strlen(msg));
    WaitDmaTxDone();

    PRINT("DMA TX done. Press any key to exit.\r\n");

    while (1)
    {
        IwdgKickDog(IWDG2);
        uint8_t ch;
        if (UsartReadOne(USART4, &ch))
        {
            break;
        }
    }

    GicdDisableInt(GIC_DMA2_STR0);
    PRINT("DMA test exit.\r\n");
}
