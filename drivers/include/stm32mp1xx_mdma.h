/*
 * stm32mp1xx_mdma.h
 *
 *  Created on: 2025-6-19
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_MDMA_H_
#define STM32MP1XX_MDMA_H_

#include <stdint.h>


typedef struct {
    uint32_t ISR;       // +0x00 中断状态: CRQA(16), TCIF(4), BTIF(3), BRTIF(2), CTCIF(1), TEIF(0)
    uint32_t IFCR;      // +0x04 中断标志清除 (写1清)
    uint32_t ESR;       // +0x08 错误状态: BSE(11), ASE(10), TEMD(9), TELD(8), TED(7), TEA[6:0]
    uint32_t CR;        // +0x0C 通道控制: SWRQ(16), WEX(14), HEX(13), BEX(12), SM(8), PL[7:6], xIE[5:1], EN(0)
    uint32_t TCR;       // +0x10 传输配置: BWM(31), SWRM(30), TRGM[29:28], PAM[27:26], PKE(25), TLEN[24:18], DBURST[17:15], SBURST[14:12], DINCOS[11:10], SINCOS[9:8], DSIZE[7:6], SSIZE[5:4], DINC[3:2], SINC[1:0]
    uint32_t BNDTR;     // +0x14 Block数据量: BRC[31:20], BRDUM(19), BRSUM(18), BNDT[16:0]
    uint32_t SAR;       // +0x18 源地址
    uint32_t DAR;       // +0x1C 目的地址
    uint32_t BRUR;      // +0x20 Block repeat地址更新: DUV[31:16], SUV[15:0]
    uint32_t LAR;       // +0x24 链表地址 (双字对齐, =0时通道结束)
    uint32_t TBR;       // +0x28 触发选择: TSEL[5:0]
    uint32_t RSVD0;     // +0x2C
    uint32_t MAR;       // +0x30 Mask地址 (ACK时写MDR到此地址, =0禁用)
    uint32_t MDR;       // +0x34 Mask数据
    uint32_t RSVD1;     // +0x38
    uint32_t RSVD2;     // +0x3C
} MdmaChRegs_t;


typedef struct {
    uint32_t     GISR0;     // 0x00 全局中断状态, bit[31:0]对应ch31~ch0
    uint32_t     RSVD0;     // 0x04
    uint32_t     SGISR0;    // 0x08 安全全局中断状态
    uint32_t     RSVD1[13]; // 0x0C ~ 0x3C
    MdmaChRegs_t CH[32];    // 0x40 + 0x40*x, 32个通道
} MdmaRegs_t;


#define MDMA_FLAG_TEIF      (1u << 0)
#define MDMA_FLAG_CTCIF     (1u << 1)
#define MDMA_FLAG_BRTIF     (1u << 2)
#define MDMA_FLAG_BTIF      (1u << 3)
#define MDMA_FLAG_TCIF      (1u << 4)


typedef enum {
    MDMA_PRI_LOW        = 0,
    MDMA_PRI_MED        = 1,
    MDMA_PRI_HIGH       = 2,
    MDMA_PRI_VERY_HIGH  = 3
} MdmaPrior_t;


typedef enum {
    MDMA_DATA_8BIT      = 0,
    MDMA_DATA_16BIT     = 1,
    MDMA_DATA_32BIT     = 2,
    MDMA_DATA_64BIT     = 3
} MdmaDataSize_t;


/* 注意: 1 为保留值, 不可使用 */
typedef enum {
    MDMA_INC_FIXED      = 0,
    MDMA_INC_INCR       = 2,
    MDMA_INC_DECR       = 3
} MdmaInc_t;


typedef enum {
    MDMA_TRGM_BUFFER    = 0,    // 每次触发传一个 buffer
    MDMA_TRGM_BLOCK     = 1,    // 每次触发传一个 block
    MDMA_TRGM_REP_BLOCK = 2,    // 每次触发传一个 repeated block
    MDMA_TRGM_CHANNEL   = 3     // 每次触发传完整个通道 (链表遍历完)
} MdmaTrgm_t;


typedef enum {
    MDMA_PAM_RIGHT_ZERO = 0,    // 右对齐, 高位补0
    MDMA_PAM_RIGHT_SIGN = 1,    // 右对齐, 符号扩展
    MDMA_PAM_LEFT_ZERO  = 2     // 左对齐, 低位补0
} MdmaPam_t;


/*
 * MDMA 通道配置参数
 *
 * 用法:
 *   MdmaCfg_t cfg = MDMA_CFG_DEFAULT;
 *   cfg.ch = 0;
 *   cfg.src_addr = (uint32_t)src_buf;
 *   cfg.dst_addr = (uint32_t)dst_buf;
 *   cfg.bndt = 1024;
 *   cfg.ssize = 2;  // 32bit
 *   cfg.dsize = 2;
 *   cfg.sinc = 2;   // 递增
 *   cfg.dinc = 2;
 *   cfg.tlen = 15;  // 每次 buffer 传输 16 字节
 *   cfg.trgm = 0;   // 每次请求传一个 buffer
 *   cfg.swrm = 1;   // 软件触发
 *   cfg.ctcie = 1;  // 传输完成中断
 *   MdmaCfg(MDMA, &cfg);
 *   MdmaEnable(MDMA, 0);
 *   MdmaSwTrig(MDMA, 0);
 *
 * 约束:
 *   - TLEN+1 须为 SSIZE 和 DSIZE 字节数的整数倍
 *   - BNDT 须为 SSIZE 和 DSIZE 字节数的整数倍
 *   - Burst 字节数 (2^xBURST × xSIZE字节) 须 <= TLEN+1, 且 <= 128
 *   - xINC=00(固定) 时 xBURST 最大为 100 (16拍)
 *   - xINCOS 须 >= xSIZE (否则结果未定义)
 *   - SUV 须为 SSIZE 字节数的整数倍, DUV 须为 DSIZE 字节数的整数倍
 *   - LAR 须双字对齐 (低3位=0), =0 表示链表结束
 *   - TRGM=11 时链表加载的新配置中 TRGM/SWRM 不可改变
 */
typedef struct {
    uint32_t src_addr;              // SAR 源地址
    uint32_t dst_addr;              // DAR 目的地址
    uint32_t link_addr;             // LAR 链表下一节点地址 (=0结束, 须8字节对齐)
    uint32_t mask_addr;             // MAR ACK应答写入地址 (=0禁用), 级联时填DMA的IFCR地址
    uint32_t mask_data;             // MDR ACK应答写入数据, 级联时填清TC标志的位模式
    uint16_t suv;                   // SUV block repeat后源地址更新值
    uint16_t duv;                   // DUV block repeat后目的地址更新值
    uint32_t bndt         : 17;     // BNDT block总字节数 (0~65536)
    uint32_t brc          : 12;     // BRC block重复次数, 实际传输 BRC+1 个block
    uint32_t brsum        : 1;      // BRSUM 源地址更新方向 (0=加SUV, 1=减SUV)
    uint32_t brdum        : 1;      // BRDUM 目的地址更新方向 (0=加DUV, 1=减DUV)
    uint32_t pke          : 1;      // PKE pack使能: 1=自动按目的宽度打包/拆包
    uint32_t ch           : 5;      // 通道号 0~31
    uint32_t pl           : 2;      // PL 优先级: 0=Low 1=Med 2=High 3=VeryHigh
    uint32_t trgm         : 2;      // TRGM 每次触发传输量: 0=buffer 1=block 2=repeated block 3=whole channel
    uint32_t swrm         : 1;      // SWRM 1=软件触发(忽略硬件请求), 0=硬件触发
    uint32_t tsel         : 6;      // TSEL 硬件触发源选择 (swrm=0时有效)
    uint32_t tlen         : 7;      // TLEN buffer传输长度-1, 实际 TLEN+1 字节 (1~128)
    uint32_t dburst       : 3;      // DBURST 目的burst: 0=single, N=2^N拍
    uint32_t sburst       : 3;      // SBURST 源burst: 同上
    uint32_t bwm          : 1;      // BWM 目的写bufferable: 0=non-bufferable, 1=bufferable
    uint32_t pam          : 2;      // PAM padding: 0=右对齐补0, 1=右对齐符号扩展, 2=左对齐
    uint32_t dincos       : 2;      // DINCOS 目的增量步长: 0=1B 1=2B 2=4B 3=8B
    uint32_t sincos       : 2;      // SINCOS 源增量步长: 同上
    uint32_t dsize        : 2;      // DSIZE 目的数据宽度: 0=8b 1=16b 2=32b 3=64b
    uint32_t ssize        : 2;      // SSIZE 源数据宽度: 同上
    uint32_t dinc         : 2;      // DINC 目的地址模式: 0=固定 2=递增 3=递减
    uint32_t sinc         : 2;      // SINC 源地址模式: 同上
    uint32_t tcie         : 1;      // TCIE buffer传输完成中断使能
    uint32_t btie         : 1;      // BTIE block传输完成中断使能
    uint32_t brtie        : 1;      // BRTIE block repeat完成中断使能
    uint32_t ctcie        : 1;      // CTCIE channel传输完成中断使能
    uint32_t teie         : 1;      // TEIE 传输错误中断使能
    uint32_t wex          : 1;      // WEX 双字内word序交换 (大小端转换)
    uint32_t hex          : 1;      // HEX word内half-word序交换
    uint32_t bex          : 1;      // BEX half-word内byte序交换
} MdmaCfg_t;


typedef struct {
    uint32_t TCR;
    uint32_t BNDTR;
    uint32_t SAR;
    uint32_t DAR;
    uint32_t BRUR;
    uint32_t LAR;       /* 下一节点地址, =0 结束, 须 8 字节对齐 */
    uint32_t TBR;
    uint32_t RSVD;
    uint32_t MAR;
    uint32_t MDR;
} __attribute__((aligned(8))) MdmaLinkNode_t;


extern const MdmaCfg_t MDMA_CFG_DEFAULT;
extern volatile MdmaRegs_t *const MDMA;

void MdmaCfg(volatile MdmaRegs_t *mdma, const MdmaCfg_t *cfg);
void MdmaEnable(volatile MdmaRegs_t *mdma, uint32_t ch);
void MdmaDisable(volatile MdmaRegs_t *mdma, uint32_t ch);
void MdmaSwTrig(volatile MdmaRegs_t *mdma, uint32_t ch);
uint32_t MdmaGetGisr(volatile MdmaRegs_t *mdma);
uint32_t MdmaGetChIsr(volatile MdmaRegs_t *mdma, uint32_t ch);
uint32_t MdmaGetChEsr(volatile MdmaRegs_t *mdma, uint32_t ch);
void MdmaClearChIf(volatile MdmaRegs_t *mdma, uint32_t ch, uint32_t flags);

void MdmaMemcpyInit(uint32_t ch);
void MdmaMemcpy(void *dst, const void *src, uint32_t size);

#endif /* STM32MP1XX_MDMA_H_ */
