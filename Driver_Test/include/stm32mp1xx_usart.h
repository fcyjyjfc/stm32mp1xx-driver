#ifndef __STM32MP1xx_USART_H__
#define __STM32MP1xx_USART_H__


typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

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


#endif
