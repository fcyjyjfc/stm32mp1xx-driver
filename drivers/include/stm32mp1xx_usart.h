#ifndef __STM32MP1xx_USART_H__
#define __STM32MP1xx_USART_H__


#include <stdint.h>

typedef struct {
    uint32_t CR1;
    uint32_t CR2;
    uint32_t CR3;
    uint32_t BRR;
    uint32_t GTPR;
    uint32_t RTOR;
    uint32_t RQR;
    uint32_t ISR;
    uint32_t ICR;
    uint32_t RDR;
    uint32_t TDR;
    uint32_t PRESC;
    uint8_t  RSVD0[0x3EC - 0x2C - 4];
    uint32_t HWCFGR[2];
    uint32_t VERR;
    uint32_t IPIDR;
    uint32_t SIDR;
} UsartRegs_t;


typedef enum {
    USART_WORD_LEN_7,
    USART_WORD_LEN_8,
    USART_WORD_LEN_9
} UsartWordLen_t;


typedef enum {
    USART_PARITY_NONE,
    USART_PARITY_ODD,
    USART_PARITY_EVEN
} UsartParity_t;

typedef enum {
    USART_STOP_BIT_1,
    USART_STOP_BIT_2,
    USART_STOP_BIT_0_5,
    USART_STOP_BIT_1_5
} UsartStopBit_t;


typedef enum {
    USART_SAMPLE_ONE,
    USART_SAMPLE_THREE
} UsartSampleOnt_t;


typedef struct {
    uint32_t usart_baud_rate;
    uint32_t usart_word_len;
    uint32_t usart_stop_bit;
    uint32_t usart_parity;
    uint32_t usart_sample_mode;
    uint32_t usart_fifo_en;
    uint32_t usart_rxff_tl;
    uint32_t usart_txff_tl;
    uint32_t usart_one_sample;
} UsartCfg_t;


extern volatile UsartRegs_t *const USART1;
extern volatile UsartRegs_t *const USART6;
extern volatile UsartRegs_t *const USART8;
extern volatile UsartRegs_t *const USART7;
extern volatile UsartRegs_t *const USART5;
extern volatile UsartRegs_t *const USART4;
extern volatile UsartRegs_t *const USART3;
extern volatile UsartRegs_t *const USART2;


extern void UsartCfg(volatile UsartRegs_t *const usart_reg, const UsartCfg_t *const cfg);
extern void UsartWrite(volatile UsartRegs_t *const usart_reg, const uint8_t *dat, const uint32_t len);
extern void UsartRead(volatile UsartRegs_t *const usart_reg, uint8_t *dat, const uint32_t len);
extern uint32_t UsartReadOne(volatile UsartRegs_t *const usart_reg, uint8_t *dat);
extern uint8_t UsartReadAll(volatile UsartRegs_t *const usart_reg);


#include "stm32mp1xx_dma.h"

/*
 * USART DMA 发送上下文
 * 内部维护环形缓冲区，DMA 从中取连续段搬运到 TDR。
 * 缓冲区最多存 tx_size-1 字节（留 1 字节区分满/空）。
 */
typedef struct {
    volatile UsartRegs_t *usart;       // 绑定的 USART 实例
    volatile DmaRegs_t   *dma;         // 绑定的 DMA 控制器
    uint32_t              stream;      // DMA Stream 编号 0~7
    DmaCfg_t              dma_cfg;     // DMA 配置模板, M0AR/NDTR 每次 Kick 时更新
    uint8_t              *tx_buf;      // 环形缓冲区首地址
    uint32_t              tx_size;     // 环形缓冲区总大小
    volatile uint32_t     tx_head;     // 写指针, 由 UsartDmaSend 推进
    volatile uint32_t     tx_tail;     // 读指针, 由 DMA TC 中断推进
    volatile uint32_t     tx_dma_len;  // 当前 DMA 传输长度, 用于 TC 中断推进 tail
    volatile int          tx_busy;     // DMA 正在传输标志
} UsartDmaCtx_t;

/* 一次性初始化: 绑定 USART/DMA/缓冲区, 路由 DMAMUX, 不启动传输 */
void UsartDmaTxInit(UsartDmaCtx_t *ctx, volatile UsartRegs_t *usart,
                    volatile DmaRegs_t *dma, uint32_t stream,
                    DmaMuxReqId_t req_id, uint8_t *buf, uint32_t size);

/* 非阻塞发送: 数据拷入环形缓冲, DMA 空闲时自动启动, 返回实际入队字节数 */
int  UsartDmaSend(UsartDmaCtx_t *ctx, const uint8_t *data, uint32_t len);

/* DMA TC 中断回调: 注册到 GIC, 在 ISR 中调用 */
void UsartDmaTxIsr(UsartDmaCtx_t *ctx);


/*
 * USART DMA 接收上下文
 * DMA 以循环模式持续写入环形缓冲区, TC 中断计圈, 应用层轮询读取。
 * 当 DMA 越过读指针（溢出）时, 置 rx_ovf 标志, 丢弃被覆盖的旧数据。
 */
typedef struct {
    volatile UsartRegs_t *usart;
    volatile DmaRegs_t   *dma;
    uint32_t              stream;
    uint8_t              *rx_buf;
    uint32_t              rx_size;
    uint32_t              rx_rd;
    volatile uint32_t     rx_wr_wrap;   // TC 中断递增, DMA 完成一圈
    uint32_t              rx_rd_wrap;   // 读指针越过末尾时递增
    volatile uint8_t      rx_ovf;      // 溢出标志, 应用层检查并清除
} UsartDmaRxCtx_t;

/* 初始化: 配置 DMA 循环接收 + TC 中断, 路由 DMAMUX, 立即启动 */
void UsartDmaRxInit(UsartDmaRxCtx_t *ctx, volatile UsartRegs_t *usart,
                    volatile DmaRegs_t *dma, uint32_t stream,
                    DmaMuxReqId_t req_id, uint8_t *buf, uint32_t size);

/* DMA TC 中断回调: 注册到 GIC, 在 ISR 中调用 */
void UsartDmaRxIsr(UsartDmaRxCtx_t *ctx);

/* 查询可读字节数 */
uint32_t UsartDmaRxAvail(UsartDmaRxCtx_t *ctx);

/* 读取一个字节, 返回 1=成功/溢出(同时置 rx_ovf), 0=无数据 */
int  UsartDmaRxReadOne(UsartDmaRxCtx_t *ctx, uint8_t *byte);

/* 停止 DMA 接收, 恢复轮询模式 */
void UsartDmaRxStop(UsartDmaRxCtx_t *ctx);


#endif
