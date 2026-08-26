/*
 * stm32mp1xx_gmac.h
 *
 *  Created on: 2026-8-25
 *      Author: gjsbr
 */

#ifndef STM32MP1XX_GMAC_H_
#define STM32MP1XX_GMAC_H_

#include <stdint.h>


/* 通道寄存器块, 每通道间距 0x80, 基偏移 0x1100 + 0x80*x (x = 0~1)
 * Rx 相关字段 (RXCR/RXDLAR/RXDTPR/RXRLR/RXIWTR/CARXDR/CARXBR/MFCR) 仅通道 0 存在
 * 通道 1 对应位置为保留 */
typedef struct {
    uint32_t CR; // 0x00 通道控制: DSL/PBLX8/MSS
    uint32_t TXCR; // 0x04 发送控制: ST/OSF/TSE/TXPBL/IPBL/TCW/TQOS
    uint32_t RXCR; // 0x08 接收控制 (仅通道0): SR/RBSZ/RXPBL/RPF/RQOS
    uint8_t  RSVD0[8]; // 0x0C
    uint32_t TXDLAR; // 0x14 Tx 描述符列表基址, 双字对齐, 仅停止时可写
    uint8_t  RSVD1[4]; // 0x18
    uint32_t RXDLAR; // 0x1C Rx 描述符列表基址 (仅通道0), 仅停止时可写
    uint32_t TXDTPR; // 0x20 Tx 描述符尾指针, 写 = 向硬件提交新描述符
    uint8_t  RSVD2[4]; // 0x24
    uint32_t RXDTPR; // 0x28 Rx 描述符尾指针 (仅通道0)
    uint32_t TXRLR; // 0x2C Tx 环长度, 10 位最多 1024, 建议 ≥ 4
    uint32_t RXRLR; // 0x30 Rx 环长度 + ARBS (仅通道0)
    uint32_t IER; // 0x34 中断使能: NIE/AIE 汇总 + 各事件位
    uint32_t RXIWTR; // 0x38 Rx 中断看门狗 (仅通道0): RWT/RWTU, 中断合并
    uint32_t SFCSR; // 0x3C AVB 时槽功能控制状态: ESC/ASC/SIV/RSN
    uint8_t  RSVD3[4]; // 0x40
    uint32_t CATXDR; // 0x44 当前应用 Tx 描述符指针 (只读)
    uint8_t  RSVD4[4]; // 0x48
    uint32_t CARXDR; // 0x4C 当前应用 Rx 描述符指针 (只读, 仅通道0)
    uint8_t  RSVD5[4]; // 0x50
    uint32_t CATXBR; // 0x54 当前应用 Tx 缓冲指针 (只读)
    uint8_t  RSVD6[4]; // 0x58
    uint32_t CARXBR; // 0x5C 当前应用 Rx 缓冲指针 (只读, 仅通道0)
    uint32_t SR; // 0x60 状态: TI/RI/TBU/RBU/NIS/AIS/FBE/REB/TEB 等, rc_w1
    uint8_t  RSVD7[8]; // 0x64
    uint32_t MFCR; // 0x6C 丢帧计数: MFC/MFCO, 读即清 (rc_r)
    uint8_t  RSVD8[0x10]; // 0x70
} EthDmaChanRegs_t;


/* DMA 寄存器块, 映射于 ETH 基址 + 0x1000 */
typedef struct {
    uint32_t         DMAMR; // 0x1000 DMA 模式: SWR 软复位/TAA/INTM/TXPR/DSPW
    uint32_t         DMASBMR; // 0x1004 系统总线模式: FB/BLENx/ONEKBBE/OSR_LMT/AAL
    uint32_t         DMAISR; // 0x1008 中断聚合状态: MACIS/MTLIS/DC0IS/DC1IS (只读)
    uint32_t         DMADSR; // 0x100C 调试状态: TPS/RPS 状态机/AXI 通道活跃 (只读)
    uint8_t          RSVD0[0x1020 - 0x1010];
    uint32_t         DMAA4TXACR; // 0x1020 AXI4 Tx cache 信号: THC/TEC/TDRC
    uint32_t         DMAA4RXACR; // 0x1024 AXI4 Rx cache 信号: RDC/RHC/RPC/RDWC
    uint32_t         DMAA4DACR; // 0x1028 AXI4 描述符 cache 信号: RDRC/TDWD/TDWC
    uint8_t          RSVD1[0x1040 - 0x102C];
    uint32_t         DMALPIEI; // 0x1040 AXI LPI 进入间隔: LPIEI (×64 周期)
    uint8_t          RSVD2[0x1100 - 0x1044];
    EthDmaChanRegs_t CH[2]; // 0x1100 通道 0, 0x1180 通道 1
} EthDmaRegs_t;


/* 队列寄存器块, 每队列间距 0x40, 基偏移 0x0D00 + 0x40*x (x = 0~1)
 * AVB 字段 (TXQECR/TXQSSCR/TXQHCR/TXQLCR) 仅队列 1 存在, 队列 0 对应位置为保留 */
typedef struct {
    uint32_t TXQOMR; // 0x00 Tx 队列操作模式: TXQEN/TSF/TTC/TQS/FTQ
    uint32_t TXQUR; // 0x04 Tx 队列下溢计数: UFFRMCNT/UFCNTOVF, 读即清
    uint32_t TXQDR; // 0x08 Tx 队列调试: TXQSTS/TWCSTS/TRCSTS/STXSTSF/PTXQ (只读)
    uint8_t  RSVD0[4]; // 0x0C
    uint32_t TXQECR; // 0x10 ETS 控制 (仅队列1, AVB): SLC/CC/AVALG
    uint32_t TXQESR; // 0x14 每时槽平均比特 ABS (只读)
    uint32_t TXQQWR; // 0x18 WRR 权重 (≤0x64) 或 idleSlopeCredit (×1024)
    uint32_t TXQSSCR; // 0x1C sendSlopeCredit (仅队列1, AVB, ×1024)
    uint32_t TXQHCR; // 0x20 hiCredit (仅队列1, AVB, ×1024)
    uint32_t TXQLCR; // 0x24 loCredit (仅队列1, AVB, ×1024 补码)
    uint8_t  RSVD1[4]; // 0x28
    uint32_t QICSR; // 0x2C 队列中断控制状态: RXOIE/TXUIE 使能, RXOVFIS/TXUNFIS/ABPSIS 状态 (rc_w1)
    uint32_t RXQOMR; // 0x30 Rx 队列操作模式: RQS/RSF/FEP/FUP/RTC/RFA/RFD/EHFC
    uint32_t RXQMPOCR; // 0x34 Rx 队列丢包/溢出计数: MISPKTCNT/OVFPKTCNT, 读即清
    uint32_t RXQDR; // 0x38 Rx 队列调试: PRXQ/RXQSTS/RRCSTS/RWCSTS (只读)
    uint32_t RXQCR; // 0x3C Rx 队列仲裁控制: RXQ_FRM_ARBIT/RXQ_WEGT (权重-1 编码)
} EthMtlQueueRegs_t;


/* MTL 寄存器块, 映射于 ETH 基址 + 0x0C00 */
typedef struct {
    uint32_t          MTLOMR; // 0x0C00 操作模式: SCHALG/RAA/CNTCLR/CNTPRST/DTXSTS
    uint8_t           RSVD0[0x0C20 - 0x0C04];
    uint32_t          MTLISR; // 0x0C20 中断聚合: Q0IS/Q1IS (只读, 清源在 QICSR)
    uint8_t           RSVD1[0x0D00 - 0x0C24];
    EthMtlQueueRegs_t QUEUE[2]; // 0x0D00 队列 0, 0x0D40 队列 1
} EthMtlRegs_t;


/* 单个 MAC 地址对: 0x0300 + 0x8*x, x = 0~3 (地址 0 ~ 地址 3)
 * 地址 0 的 HR 只含 AE (恒 1), 无 SA/MBC */
typedef struct {
    uint32_t HR; // +0x00 高: AE/SA/MBC[5:0]/ADDRHI (MBC 屏蔽字节, 组地址过滤)
    uint32_t LR; // +0x04 低: ADDRLO
} EthMacAddrRegs_t;


/* L3/L4 过滤块, 每块间距 0x30, 基偏移 0x0900 + 0x30*x (x = 0~1)
 * 过滤器 0: L3L4C0R/L4A0R/L3A00R~A30R; 过滤器 1: L3L4C1R/L4A1R/L3A01R~A31R
 * L3A0R~A3R: IPv4 只用 A0R(SA)/A1R(DA), A2R/A3R 仅 IPv6 有效 */
typedef struct {
    uint32_t L3L4CR; // +0x00 控制: L4DPIM/L4DPM/L4SPIM/L4SPM/L4PEN/L3HDBM/L3HSBM/L3DAIM/L3DAM/L3SAIM/L3SAM/L3PEN
    uint32_t L4AR; // +0x04 L4 端口: L4DP/L4SP (L4PEN 选 TCP/UDP)
    uint8_t  RSVD0[8]; // +0x08
    uint32_t L3A0R; // +0x10 IPv4 SA / IPv6 [31:0]
    uint32_t L3A1R; // +0x14 IPv4 DA / IPv6 [63:32]
    uint32_t L3A2R; // +0x18 IPv6 [95:64] (IPv4 保留)
    uint32_t L3A3R; // +0x1C IPv6 [127:96] (IPv4 保留)
    uint8_t  RSVD1[0x10]; // +0x20 补齐到 0x30 间距
} EthMacL34FilterRegs_t;


/* MMC 寄存器块, 映射于 ETH 基址 + 0x0700 */
typedef struct {
    uint32_t CTRL; // 0x0700 MMC 控制: UCDBC/CNTPRSTLVL/CNTPRST/CNTFREEZ/RSTONRD/CNTSTOPRO/CNTRST
    uint32_t RXISR; // 0x0704 Rx 中断状态: RXLPITRCIS/RXLPIUSCIS/RXUCGPIS/RXALGNERPIS/RXCRCERPIS, rc_r (读对应计数器清)
    uint32_t TXISR; // 0x0708 Tx 中断状态: TXLPITRCIS/TXLPIUSCIS/TXGPKTIS/TXMCOLGPIS/TXSCOLGPIS, rc_r
    uint32_t RXIMR; // 0x070C Rx 中断掩码: 与 RXISR 对应 (置 1 屏蔽)
    uint32_t TXIMR; // 0x0710 Tx 中断掩码: 与 TXISR 对应 (置 1 屏蔽)
    uint8_t  RSVD0[0x074C - 0x0714];
    uint32_t TXSNGLCOLG; // 0x074C Tx 单次冲突好包数 (半双工)
    uint32_t TXMULTCOLG; // 0x0750 Tx 多次冲突好包数 (半双工)
    uint8_t  RSVD1[0x0768 - 0x0754];
    uint32_t TXPKTG; // 0x0768 Tx 好包总数
    uint8_t  RSVD2[0x0794 - 0x076C];
    uint32_t RXCRCERR; // 0x0794 Rx CRC 错误包数
    uint32_t RXALGNERR; // 0x0798 Rx 对齐错误包数 (仅 10/100 有效)
    uint8_t  RSVD3[0x07C4 - 0x079C];
    uint32_t RXUCASTG; // 0x07C4 Rx 单播好包数
    uint8_t  RSVD4[0x07EC - 0x07C8];
    uint32_t TXLPIUSC; // 0x07EC Tx LPI 微秒数 (±1 µs 误差)
    uint32_t TXLPITRC; // 0x07F0 Tx LPI 进入次数 (Automate 模式也计数)
    uint32_t RXLPIUSC; // 0x07F4 Rx LPI 微秒数 (±1 µs 误差)
    uint32_t RXLPITRC; // 0x07F8 Rx LPI 进入次数
    uint8_t  RSVD5[4]; // 0x07FC
} EthMmcRegs_t;


/* MAC 寄存器块, 映射于 ETH 基址 + 0x0000 */
typedef struct {
    uint32_t              CR; // 0x0000 操作模式: ARPEN/SARC/IPC/IPG/GPSLCE/S2KP/CST/ACS/WD/BE/JD/JE/PS/FES/DM/LM/ECRSFD/DO/DCRS/DR/BL/DC/PRELEN/TE/RE
    uint32_t              ECR; // 0x0004 扩展控制: EIPG/EIPGEN/USP/SPEN/DCRCC/GPSL (巨型包大小限制)
    uint32_t              PFR; // 0x0008 包过滤: RA/DNTU/IPFE/VTFE/HPF/SAF/SAIF/PCF/DBF/PM/DAIF/HMC/HUC/PR
    uint32_t              WTR; // 0x000C 看门狗超时: PWE/WTO (PWE=1 且 CR.WD=0 时生效)
    uint32_t              HT0R; // 0x0010 哈希表低 32 位: HT0
    uint32_t              HT1R; // 0x0014 哈希表高 32 位: HT63T32 (CRC32 反转取高 7 位索引)
    uint8_t               RSVD0[0x0050 - 0x0018];
    uint32_t              VTR; // 0x0050 VLAN 标签: EIVLRXS/EIVLS/ERIVLT/EDVLP/VTHM/EVLRXS/EVLS/DOVLTC/ERSVLM/ESVL/VTIM/ETV/VL
    uint8_t               RSVD1[4]; // 0x0054
    uint32_t              VHTR; // 0x0058 VLAN 哈希表: VLHT (16 位, CRC 高 4 位索引)
    uint8_t               RSVD2[4]; // 0x005C
    uint32_t              VIR; // 0x0060 外层 VLAN 包含(发送): VLTI/CSVL/VLP/VLC/VLT
    uint32_t              IVIR; // 0x0064 内层 VLAN 包含(发送): 结构同 VIR
    uint8_t               RSVD3[8]; // 0x0068
    uint32_t              Q0TXFCR; // 0x0070 Tx 流控: PT/DZPQ/PLT/TFE/FCB_BPA (FCB 写前须读 0)
    uint8_t               RSVD4[0x0090 - 0x0074];
    uint32_t              RXFCR; // 0x0090 Rx 流控: UP/RFE (RFE 解码收到的 Pause 包)
    uint32_t              RXQCR; // 0x0094 Rx 队列控制: UFFQ/MFFQ/VFFQ (过滤失败包路由, 仅 PFR.RA=1)
    uint8_t               RSVD5[8]; // 0x0098
    uint32_t              RXQC0R; // 0x00A0 队列使能: RXQ0EN/RXQ1EN (**默认全禁, 必须显式使能**)
    uint32_t              RXQC1R; // 0x00A4 队列路由: MCBCQ/UPQ/PTPQ/TPQC
    uint32_t              RXQC2R; // 0x00A8 优先级路由: PSRQ0/PSRQ1 (各队列位图须互斥)
    uint8_t               RSVD6[4]; // 0x00AC
    uint32_t              ISR; // 0x00B0 中断状态: RXSTSIS/TXSTSIS/TSIS/MMCTXIS/MMCRXIS/MMCIS/LPIIS/PMTIS/PHYIS/RGSMIIIS (各源清除路径不同)
    uint32_t              IER; // 0x00B4 中断使能: 与 ISR 各位对应 (RXSTSIE/TXSTSIE/TSIE/LPIIE/PMTIE/PHYIE/RGSMIIIE)
    uint32_t              RXTXSR; // 0x00B8 Rx/Tx 错误状态: RWT/EXCOL/LCOL/EXDEF/LCARR/NCARR/TJT, rc_r (RCWE=1 时写 1 清)
    uint8_t               RSVD7[4]; // 0x00BC
    uint32_t              PCSR; // 0x00C0 PMT 控制状态: RWKFILTRST/RWKPTR/RWKPFE/GLBLUCAST/RWKPRCVD/MGKPRCVD/RWKPKTEN/MGKPKTEN/PWRDWN
    uint32_t              RWKPFR; // 0x00C4 远程唤醒包过滤器: 必须整体写, 顺序写 8/16/32 次 (指针见 PCSR[26:24])
    uint8_t               RSVD8[8]; // 0x00C8
    uint32_t              LCSR; // 0x00D0 LPI 控制状态: LPITCSE/LPITE/LPITXA/PLSEN/PLS/LPIEN/RLPIST/TLPIST/RLPIEX/RLPIEN/TLPIEX/TLPIEN
    uint32_t              LTCR; // 0x00D4 LPI 定时器: LST(ms)/TWT(µs)
    uint32_t              LETR; // 0x00D8 LPI 进入定时器: LPIET (µs, 粒度 8 µs, [2:0] 只读)
    uint32_t              USTCR; // 0x00DC 1 µs tick 计数器: TIC_1US_CNTR (= CSR 周期数-1)
    uint8_t               RSVD9[0x00F8 - 0x00E0];
    uint32_t              PHYCSR; // 0x00F8 PHY 接口状态: LNKSTS/LNKSPEED/LNKMOD (RGMII 读回) + LUD/TC
    uint8_t               RSVD10[0x0110 - 0x00FC];
    uint32_t              VR; // 0x0110 版本: USERVER/SNPSVER (只读)
    uint32_t              DR; // 0x0114 调试: TFCSTS/TPESTS/RFCFCSTS/RPESTS (只读)
    uint8_t               RSVD11[4]; // 0x0118
    uint32_t              HWF0R; // 0x011C 硬件特性 0: ACTPHYSEL/校验卸载/EEE/1588/MMC/PMT/MDIO/VLAN 等 (只读)
    uint32_t              HWF1R; // 0x0120 硬件特性 1: L3L4FNUM/HASHTBLSZ/TSO/AVB/TXFIFOSIZE/RXFIFOSIZE (只读)
    uint32_t              HWF2R; // 0x0124 硬件特性 2: AUXSNAPNUM/PPSOUTNUM/TDCSZ/TXCHCNT/RDCSZ/RXCHCNT/TXQCNT/RXQCNT (只读)
    uint32_t              HWF3R; // 0x0128 硬件特性 3: DVLAN/CBTISEL/NRVF (只读)
    uint8_t               RSVD12[0x0200 - 0x012C];
    uint32_t              MDIOAR; // 0x0200 MDIO 地址: PSE/BTB/PA/RDA/NTC/CR/SKAP/GOC/C45E/GB (GB=1 期间不可写)
    uint32_t              MDIODR; // 0x0204 MDIO 数据: RA/GD (C45E=1 时 RA 有效)
    uint8_t               RSVD13[8]; // 0x0208
    uint32_t              ARPAR; // 0x0210 ARP 地址: ARPPA (IPv4 目的地址, ARP 卸载)
    uint8_t               RSVD14[0x0230 - 0x0214];
    uint32_t              CSRSWCR; // 0x0230 CSR 软件控制: SEEN/RCWE (RCWE 切换全局清除语义 rc_r→rc_w1)
    uint8_t               RSVD15[0x0300 - 0x0234];
    EthMacAddrRegs_t      ADDR[4]; // 0x0300 地址 0, 0x0308 地址 1, 0x0310 地址 2, 0x0318 地址 3
    uint8_t               RSVD16[0x0700 - 0x0320];
    EthMmcRegs_t          MMCR; // 0X0700 - 0X07FF
    uint8_t               RSVD17[0x0900 - 0x0800];
    EthMacL34FilterRegs_t L34F[2]; // 0x0900 过滤器 0, 0x0930 过滤器 1
    uint8_t               RSVD18[0x0B00 - 0x0960];
    uint32_t              TSCR; // 0x0B00 时间戳控制: AV8021ASMEN/TXTSSTSM/TSENMACADDR/SNAPTYPSEL/TSMSTRENA/TSEVNTENA/TSIPV4ENA/TSIPV6ENA/TSIPENA/TSVER2ENA/TSCTRLSSR/TSENALL/TSADDREG/TSUPDT/TSINIT/TSCFUPDT/TSENA
    uint32_t              SSIR; // 0x0B04 亚秒增量: SSINC (按 PTP 时钟和 TSCTRLSSR 计算)
    uint32_t              STSR; // 0x0B08 系统时间秒: TSS (只读)
    uint32_t              STNR; // 0x0B0C 系统时间亚秒: TSSS (只读, 1ns/0.46ns 精度)
    uint32_t              STSUR; // 0x0B10 系统时间秒更新: TSS (ADDSUB=1 时写补码)
    uint32_t              STNUR; // 0x0B14 系统时间亚秒更新: ADDSUB/TSSS (ADDSUB=1 时 TSSS 不可为 0)
    uint32_t              TSAR; // 0x0B18 时间戳加数: TSAR (Fine 模式, 累加器溢出更新)
    uint8_t               RSVD19[4]; // 0x0B1C
    uint32_t              TSSR; // 0x0B20 时间戳状态: ATSNS/ATSSTM/ATSSTN/TXTSSIS/TSTRGTERR0/AUXTSTRIG/TSTARGT0/TSSOVF (读清, [27:25] 除外)
    uint8_t               RSVD20[0x0B30 - 0x0B24];
    uint32_t              TXTSSNR; // 0x0B30 Tx 时间戳状态纳秒: TXTSSMIS/TXTSSLO
    uint32_t              TXTSSSR; // 0x0B34 Tx 时间戳状态秒: TXTSSHI (**读本寄存器清 TSSR.TXTSSIS**)
    uint8_t               RSVD21[8]; // 0x0B38
    uint32_t              ACR; // 0x0B40 辅助控制: ATSEN3~0/ATSFC (外部触发打时间戳)
    uint8_t               RSVD22[4]; // 0x0B44
    uint32_t              ATSNR; // 0x0B48 辅助时间戳纳秒: AUXTSLO (FIFO 深度 4)
    uint32_t              ATSSR; // 0x0B4C 辅助时间戳秒: AUXTSHI (**读本寄存器弹出 FIFO 顶部**)
    uint32_t              TSIACR; // 0x0B50 入口不对称校正: OSTIAC (ns×2^16, 补码)
    uint32_t              TSEACR; // 0x0B54 出口不对称校正: OSTEAC (须编程为负值)
    uint32_t              TSICNR; // 0x0B58 入口校正纳秒: TSIC
    uint32_t              TSECNR; // 0x0B5C 出口校正纳秒: TSEC
    uint8_t               RSVD23[0x0B70 - 0x0B60];
    uint32_t              PPSCR; // 0x0B70 PPS 控制 (双视图): TRGTMODSEL0/PPSEN0 + PPSCTRL(固定) 或 PPSCMD(灵活)
    uint8_t               RSVD24[0x0B80 - 0x0B74];
    uint32_t              PPSTTSR; // 0x0B80 PPS 目标时间秒: TSTRH0
    uint32_t              PPSTTNR; // 0x0B84 PPS 目标时间纳秒: TRGTBUSY0/TTSL0 (BUSY=1 时不可更新)
    uint32_t              PPSIR; // 0x0B88 PPS 间隔: PPSINT0 (编程值 = 间隔-1)
    uint32_t              PPSWR; // 0x0B8C PPS 宽度: PPSWIDTH0 (编程值 = 宽度-1, 须 < PPSINT0)
    uint8_t               RSVD25[0x0BC0 - 0x0B90];
    uint32_t              POCR; // 0x0BC0 PTP 卸载控制: DN/DRRDIS/APDREQTRIG/ASYNCTRIG/APDREQEN/ASYNCEN/PTOEN
    uint32_t              SPI0R; // 0x0BC4 源端口标识 [31:0]: SPI0
    uint32_t              SPI1R; // 0x0BC8 源端口标识 [63:32]: SPI1
    uint32_t              SPI2R; // 0x0BCC 源端口标识 [79:64]: SPI2 (仅低 16 位)
    uint32_t              LMIR; // 0x0BD0 日志消息间隔: LMPDRI/DRSYNCR/LSI (log 格式, 负数补码)
    uint8_t               RSVD26[0x0C00 - 0x0BD4];
} EthMacRegs_t;


/* ========== GMAC 驱动 API ========== */

/* 顶层结构体: 覆盖 ETH 基址 0x0000 ~ 0x11FF 全部寄存器
 * 手册按功能分三部分描写, 此处按内存偏移嵌套对应 */
typedef struct {
    EthMacRegs_t MAC;                    /* 0x0000 MAC 核心 (含 MMC/L3L4/PTP) */
    EthMtlRegs_t MTL;                    /* 0x0C00 MTL */
    uint8_t      RSVD0[0x1000 - 0x0D80]; /* 0x0D80 ~ 0x0FFF 保留区 */
    EthDmaRegs_t DMA;                    /* 0x1000 DMA */
} GmacRegs_t;

extern volatile GmacRegs_t *const ETH;   /* ETH: 0x5800A000 */


typedef struct {
    uint8_t  gmac_mac_addr[6]; /* MAC 地址 (写 MACA0), 全 0 = 不设置 */
    uint32_t gmac_speed;       /* 10 / 100 / 1000 Mbps, 对应 MACCR PS/FES */
    uint32_t gmac_duplex;      /* 1 = 全双工 (DM=1), 0 = 半双工; 1000M 必须全双工 */
    uint32_t gmac_jumbo;       /* 1 = 巨型帧使能 (JE=1), 0 = 标准 1518 限制 */
    uint32_t gmac_strip_crc;   /* 1 = 自动剥离 <1536 字节包的 Pad/CRC (ACS=1) */
} GmacMacCfg_t;

typedef struct {
    uint32_t gmac_mdc_div;    /* MDC = HCLK / 该值, 取 16/26/42/62/102/124 */
} GmacMdioCfg_t;


extern int  GmacSoftReset(volatile GmacRegs_t *gmac);
extern void GmacMacInit(volatile GmacRegs_t *gmac, const GmacMacCfg_t *cfg);
extern void GmacMacStart(volatile GmacRegs_t *gmac);
extern void GmacMacDeinit(volatile GmacRegs_t *gmac);
extern void GmacMdioInit(volatile GmacRegs_t *gmac, const GmacMdioCfg_t *cfg);
extern int  GmacMdioRead(volatile GmacRegs_t *gmac, uint32_t phy_addr, uint32_t reg_addr, uint16_t *val);
extern int  GmacMdioWrite(volatile GmacRegs_t *gmac, uint32_t phy_addr, uint32_t reg_addr, uint16_t val);


#endif /* STM32MP1XX_GMAC_H_ */
