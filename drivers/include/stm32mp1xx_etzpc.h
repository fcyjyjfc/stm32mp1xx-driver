/*
 * stm32mp1xx_etzpc.h
 *
 *  Extended TrustZone Protection Controller (RM0436 第 15 章)
 *  配置片上 RAM/ROM 安全区大小 + AHB/APB 外设访问权限
 *
 *  注意: 寄存器只能按字 (32-bit) 访问; ETZPC 本身 write-secure
 *        decprot 生效有延迟, 配置后须等 2 个 APB 周期
 */

#ifndef STM32MP1XX_ETZPC_H_
#define STM32MP1XX_ETZPC_H_

#include <stdint.h>

typedef struct {
    // 0x000
    volatile uint32_t TZMA0_SIZE;       /* ROM 安全区大小 (TZMA0) */
    // 0x004
    volatile uint32_t TZMA1_SIZE;       /* SYSRAM 安全区大小 (TZMA1) */
    // 0x008 - 0x00F
    volatile uint8_t  RSVD0[0x010 - 0x008];
    // 0x010
    volatile uint32_t DECPROT0;         /* 外设访问权限 索引 0-15 */
    // 0x014
    volatile uint32_t DECPROT1;         /* 索引 16-31 */
    // 0x018
    volatile uint32_t DECPROT2;         /* 索引 32-47 */
    // 0x01C
    volatile uint32_t DECPROT3;         /* 索引 48-63 */
    // 0x020
    volatile uint32_t DECPROT4;         /* 索引 64-79 */
    // 0x024
    volatile uint32_t DECPROT5;         /* 索引 80-95 */
    // 0x028 - 0x02F
    volatile uint8_t  RSVD1[0x030 - 0x028];
    // 0x030
    volatile uint32_t DECPROT_LOCK0;    /* decprot 锁定 索引 0-31 */
    // 0x034
    volatile uint32_t DECPROT_LOCK1;    /* 索引 32-63 */
    // 0x038
    volatile uint32_t DECPROT_LOCK2;    /* 索引 64-95 */
    // 0x03C - 0x3EF
    volatile uint8_t  RSVD2[0x3F0 - 0x03C];
    // 0x3F0
    volatile uint32_t HWCFGR;           /* 硬件配置 (只读) */
    // 0x3F4
    volatile uint32_t VERR;             /* 版本 (只读) */
    // 0x3F8
    volatile uint32_t IDR;              /* 标识 (只读) */
    // 0x3FC
    volatile uint32_t SIDR;             /* 尺寸标识 (只读) */
} EtzpcRegs_t;


extern volatile EtzpcRegs_t *const ETZPC;


/* =================== TZMA_SIZE 位定义 =================== */
#define ETZPC_TZMA_SIZE_LOCK    (1U << 31)  /* 锁定 size 字段, 锁后到下次复位 */
#define ETZPC_TZMA_SIZE_MASK    0x3FFU      /* size[9:0] */

/* =================== DECPROT 编码 (15.7.3) =================== */
#define ETZPC_DECPROT_RW_SECURE     0x0  /* 00: 读写仅安全 */
#define ETZPC_DECPROT_W_SECURE      0x1  /* 01: 读 non-secure, 写 secure */
#define ETZPC_DECPROT_M4_ISOL       0x2  /* 10: non-secure 但隔离到 M4 域 */
#define ETZPC_DECPROT_NON_SECURE    0x3  /* 11: 完全 non-secure */


/* =================== 内联简单操作 =================== */

/* 安全区大小, 值 = KB/4 (0x000 = 全 non-secure, ROM 全 secure = 0x020, SYSRAM 全 secure = 0x040) */
static inline void EtzpcTzma0SizeSet(volatile EtzpcRegs_t *e, uint32_t size) { e->TZMA0_SIZE = size; }
static inline uint32_t EtzpcTzma0SizeGet(volatile EtzpcRegs_t *e) { return e->TZMA0_SIZE & ETZPC_TZMA_SIZE_MASK; }
static inline void EtzpcTzma1SizeSet(volatile EtzpcRegs_t *e, uint32_t size) { e->TZMA1_SIZE = size; }
static inline uint32_t EtzpcTzma1SizeGet(volatile EtzpcRegs_t *e) { return e->TZMA1_SIZE & ETZPC_TZMA_SIZE_MASK; }

/* 锁定安全区大小 */
static inline void EtzpcTzma0Lock(volatile EtzpcRegs_t *e) { e->TZMA0_SIZE |= ETZPC_TZMA_SIZE_LOCK; }
static inline void EtzpcTzma1Lock(volatile EtzpcRegs_t *e) { e->TZMA1_SIZE |= ETZPC_TZMA_SIZE_LOCK; }


/* =================== 声明 =================== */

/* 外设索引 0-95, 对应 Table 94 的 DECPROT 分配 */
void EtzpcDecprotSet(volatile EtzpcRegs_t *const etzpc, uint32_t index, uint32_t val);
uint32_t EtzpcDecprotGet(volatile EtzpcRegs_t *const etzpc, uint32_t index);

/* 锁定单个外设 decprot (rs 位, 写 1 锁定, 直到下次复位) */
void EtzpcDecprotLock(volatile EtzpcRegs_t *const etzpc, uint32_t index);
uint32_t EtzpcDecprotIsLocked(volatile EtzpcRegs_t *const etzpc, uint32_t index);

#endif /* STM32MP1XX_ETZPC_H_ */
