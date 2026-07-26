/*
 * stm32mp1xx_sai.h
 *
 * SAI 寄存器结构体 + 驱动接口 — 基于 RM0436 §55.6
 */

#ifndef STM32MP1XX_SAI_H_
#define STM32MP1XX_SAI_H_

#include <stdint.h>


/* ================================================================== */
/*  寄存器结构体                                                        */
/* ================================================================== */

typedef struct {
    volatile uint32_t CR1;      // 0x00
    volatile uint32_t CR2;      // 0x04
    volatile uint32_t FRCR;     // 0x08
    volatile uint32_t SLOTR;    // 0x0C
    volatile uint32_t IM;       // 0x10
    volatile uint32_t SR;       // 0x14
    volatile uint32_t CLRFR;    // 0x18
    volatile uint32_t DR;       // 0x1C
} SaiBlockRegs_t;

typedef struct {
    volatile uint32_t   GCR;    // 0x00
    SaiBlockRegs_t      Block[2]; // 0x04  [0]=A, [1]=B
    volatile uint32_t   PDMCR;  // 0x44
    volatile uint32_t   PDMDLY; // 0x48
    uint8_t             RSVD[0x3F0 - 0x4C];
    volatile uint32_t   HWCFGR; // 0x3F0
    volatile uint32_t   VERR;   // 0x3F4
    volatile uint32_t   IPIDR;  // 0x3F8
    volatile uint32_t   SIDR;   // 0x3FC
} SaiRegs_t;


/* ================================================================== */
/*  枚举与配置结构体                                                    */
/* ================================================================== */

typedef enum {
    SAI_BLOCK_A,
    SAI_BLOCK_B
} SaiBlock_t;

typedef enum {
    SAI_MODE_MASTER_TX,
    SAI_MODE_MASTER_RX,
    SAI_MODE_SLAVE_TX,
    SAI_MODE_SLAVE_RX
} SaiMode_t;

typedef enum {
    SAI_PROTOCOL_FREE,      // 自由协议，模拟 I2S/PCM/TDM 等
    SAI_PROTOCOL_SPDIF,
    SAI_PROTOCOL_AC97
} SaiProtocol_t;

// SLOTSZ 编码: 00=同DS, 01=16bit, 10=32bit
typedef enum {
    SAI_SLOTSZ_EQ_DS,
    SAI_SLOTSZ_16,
    SAI_SLOTSZ_32
} SaiSlotSize_t;

/* DS[2:0] 寄存器编码值 = 枚举值 */
typedef enum {
    SAI_DS_8  = 2,
    SAI_DS_10 = 3,
    SAI_DS_16 = 4,
    SAI_DS_20 = 5,
    SAI_DS_24 = 6,
    SAI_DS_32 = 7
} SaiDataSize_t;

typedef struct {
    // 基本
    SaiMode_t     mode;
    SaiProtocol_t protocol;

    // 时钟
    uint32_t      ker_ck_hz;      // sai_ker_ck 频率 (由 RCC 配置)
    uint32_t      sample_rate;    // 采样率
    uint8_t       nodiv  : 1;     // 0=MCLK=256/512×FS, 1=SCK=MCLK
    uint8_t       osr    : 1;     // NODIV=0 时有效: 0=256×, 1=512×
    uint8_t       mcken  : 1;     // 使能 MCLK 输出
    uint8_t       mckdiv;         // MCKDIV[5:0], 0=不分频

    // 数据
    SaiDataSize_t data_size;      // 位宽: SAI_DS_8/16/24/32
    uint8_t       lsbfirst : 1;   // 0=MSB 先, 1=LSB 先
    uint8_t       ckstr : 1;      // 0=上升沿改变/下降沿采样, 1=下降沿改变/上升沿采样

    // 帧
    uint16_t      frame_len;      // 帧长 (FRL+1, SCK 周期数)
    uint8_t       fsall;          // FS 有效长度 (SCK 周期数)
    uint8_t       fspol  : 1;     // FS 极性: 0=低有效, 1=高有效
    uint8_t       fsoff  : 1;     // FS 偏移: 0=与首 bit 对齐, 1=首 bit 前
    uint8_t       fsdef  : 1;     // FS 角色: 0=仅帧起始, 1=帧起始+声道标识

    // Slot
    uint8_t       nbslot;         // slot 数
    SaiSlotSize_t slotsz;         // slot 大小
    uint8_t       fboff;          // 首 bit 偏移 (FBOFF[4:0])
    uint16_t      sloten;         // slot 使能位图 (SLOTEN[15:0])

    // 其他
    uint8_t       mono  : 1;
    uint8_t       tris  : 1;
    uint8_t       fth;            // FIFO 阈值 (FTH[2:0])
} SaiCfg_t;


/* ================================================================== */
/*  函数接口                                                           */
/* ================================================================== */

extern void SaiCfgFillI2S(SaiCfg_t *cfg, uint32_t ker_ck_hz,
                          uint32_t sample_rate, SaiDataSize_t data_size,
                          SaiMode_t mode, uint8_t mclk_en);

extern void SaiInit(SaiRegs_t *sai, SaiBlock_t block, const SaiCfg_t *cfg);

extern void SaiStart(SaiRegs_t *sai, SaiBlock_t block);
extern void SaiStop(SaiRegs_t *sai, SaiBlock_t block);

extern void SaiSendBlocking(SaiRegs_t *sai, SaiBlock_t block,
                            const uint32_t *data, uint32_t cnt);
extern void SaiRecvBlocking(SaiRegs_t *sai, SaiBlock_t block,
                            uint32_t *data, uint32_t cnt);


#endif /* STM32MP1XX_SAI_H_ */
