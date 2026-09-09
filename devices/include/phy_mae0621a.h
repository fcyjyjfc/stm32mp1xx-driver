/*
 * phy_mae0621a.h — PHY 设备驱动 (实测 ID 0x7B74/0x4411, MAE0621A)
 *
 * 通过 MAC 的 MDIO 控制器 (drivers/stm32mp1xx_gmac.c GmacMdioRead/Write)
 * 与 PHY 通信. 对上层提供黑盒 API: 复位/读 ID/找地址/寄存器访问/等协商,
 * 内部实现 (首次读重试、复位时序) 对上层不可见.
 */

#ifndef PHY_MAE0621A_H_
#define PHY_MAE0621A_H_

#include <stdint.h>
#include "stm32mp1xx_gmac.h"
#include "stm32mp1xx_gpio.h"

/* =================== C22 寄存器 =================== */
#define PHY_REG_BMCR   0   /* 基本控制 */
#define PHY_REG_BMSR   1   /* 基本状态 */
#define PHY_REG_ID1    2   /* PHY ID 高 16 位 */
#define PHY_REG_ID2    3   /* PHY ID 低 16 位 + 版本 */
#define PHY_REG_ANAR   4   /* 自协商广告 */
#define PHY_REG_ANLPAR 5   /* 自协商对端广告 */
#define PHY_REG_ANER   6   /* 自协商扩展状态 */
#define PHY_REG_GBCR   9   /* 1000BASE-T 控制 (下一页) */
#define PHY_REG_GBSR   10  /* 1000BASE-T 状态 */
#define PHY_REG_INER   18  /* 中断使能 (可写位写读回环探针) */
#define PHY_REG_PHYCR1 24  /* 页 A43: PHY 控制 1 */
#define PHY_REG_PHYCR2 25  /* 页 A43: PHY 控制 2 */
#define PHY_REG_PHYSR  26  /* 页 A43: 实时状态 (link/speed/duplex) */
#define PHY_REG_PAGSR  31  /* 页选择 (换页窗口) */

/* =================== 页面 =================== */
#define PHY_PAGE_DEFAULT 0xA42u  /* 默认页 (IEEE 标准寄存器) */
#define PHY_PAGE_A43     0xA43u  /* PHY 专属页: PHYCR1/2, PHYSR, INSR */
#define PHY_PAGE_D92     0xD92u  /* 时钟配置页 (手册未公开, 定义来自 Linux maxio 驱动) */
#define PHY_PAGE_D04     0xD04u  /* LED 配置页 (LCR) */

/* =================== 页内寄存器 =================== */
#define PHY_REG_INSR      0x1Du  /* 页 A43: 中断状态, bit5 = 操作状态 */
#define PHY_REG_CLK_MODE  0x02u  /* 页 D92: 参考时钟模式, bit8 = 晶振/振荡器 */
#define PHY_REG_LCR       0x10u  /* 页 D04: LED 控制 (表 7-4) */

/* =================== 位定义 =================== */
#define PHY_BMCR_SWR          (1u << 15)  /* 软复位 (硬件自清零) */
#define PHY_BMCR_ANE          (1u << 12)  /* 自协商使能 */
#define PHY_BMCR_RESTART_AN   (1u << 9)   /* 重启自协商 (硬件自清零) */
#define PHY_BMSR_AN_COMPLETE  (1u << 5)   /* BMSR: 自协商完成 */
#define PHY_ANER_LP_AN_ABLE   (1u << 0)   /* ANER: 对端支持 AN (读清锁存) */
#define PHY_GBCR_1000T_FDX    (1u << 9)   /* GBCR: 1000BASE-T 全双工广告 */
#define PHY_PHYSR_LINK        (1u << 2)   /* PHYSR: 实时 link 状态 */
#define PHY_PHYSR_FDX         (1u << 3)   /* PHYSR: 1 = 全双工 */
#define PHY_PHYSR_SPEED       (3u << 4)   /* PHYSR: 速率索引 [5:4] (0=10M/1=100M/2=1G) */
#define PHY_INSR_OP_NORMAL    (1u << 5)   /* INSR bit5: 1 = PHY 操作正常, 0 = 异常 */
#define PHY_CLK_MODE_OSC      (1u << 8)   /* CLK_MODE bit8: 1 = 振荡器, 0 = 晶振 */
/* LCR 各 LED 功能位 (表 7-4): 每 LED = 10M/100M/1000M 链路位 + 活动位,
 * 位 2/7/12 是空位; 链路与活动可自由组合 (组合表见手册表 7-5/7-6) */
#define PHY_LCR_LED0_LINK_10   (1u << 0)
#define PHY_LCR_LED0_LINK_100  (1u << 1)
#define PHY_LCR_LED0_LINK_1000 (1u << 3)
#define PHY_LCR_LED0_ACT       (1u << 4)
#define PHY_LCR_LED0_MASK      (PHY_LCR_LED0_LINK_10 | PHY_LCR_LED0_LINK_100 \
                              | PHY_LCR_LED0_LINK_1000 | PHY_LCR_LED0_ACT)
#define PHY_LCR_LED1_LINK_10   (1u << 5)
#define PHY_LCR_LED1_LINK_100  (1u << 6)
#define PHY_LCR_LED1_LINK_1000 (1u << 8)
#define PHY_LCR_LED1_ACT       (1u << 9)
#define PHY_LCR_LED1_MASK      (PHY_LCR_LED1_LINK_10 | PHY_LCR_LED1_LINK_100 \
                              | PHY_LCR_LED1_LINK_1000 | PHY_LCR_LED1_ACT)

/* =================== 芯片 ID =================== */
#define MAE_PHY_ID1  0x7B74
#define MAE_PHY_ID2  0x4411

/* =================== 属性常量 (实测/手册复位默认) =================== */
#define MAE_PHY_BMCR_RESET_VAL  0x1140u  /* BMCR 复位默认: bit12 ANE=1 */
#define MAE_PHY_ANAR_ADV        0x01E1u  /* 广告: 10/100 HDX+FDX; 1G 走 GBCR bit9 */
#define MAE_PHY_INER_RESET_VAL  0x0020u  /* INER 复位默认: bit5=1 */
#define MAE_PHY_INER_RW_MASK    0x06BDu  /* INER 实测可写位 0,2,3,4,5,7,9,10 */
#define MAE_PHY_LCR_RESET_VAL   0x6251u  /* LCR 复位默认: LED0=0x11(10M+ACT), LED1=0x240(100M+ACT), LED2=0x6000(1000M+ACT) */

/**
 * @brief   驱动上下文
 * @note    phy_addr 初始为期望地址 (strap); MaePhyFind 找到应答者后回填.
 *          rst_port/rst_pin 是板级复位脚, MaePhyReset 内自行完成 GPIO 配置.
 */
typedef struct {
    volatile GmacRegs_t *gmac;         /* MDIO 走 MAC 控制器 */
    uint32_t   phy_addr;               /* 目标地址; MaePhyFind 后回填实际值 */
    volatile GpioRegs_t *rst_port;     /* 复位脚 (板级 PH2) */
    uint32_t   rst_pin;
} MaePhy_t;

/* =================== API =================== */

/**
 * @brief   PHY 复位: 拉低 20ms + 释放 200ms
 * @param   dev  驱动上下文 (复位脚由 rst_port/rst_pin 指定)
 * @note    复位完成前不可访问寄存器; 内部自行配置引脚与延时
 */
void MaePhyReset(MaePhy_t *dev);

/**
 * @brief   读 ID1/ID2 (reg2/3)
 * @param   dev   驱动上下文
 * @param   id1   输出: ID 高 16 位
 * @param   id2   输出: ID 低 16 位
 * @return  0 = 成功 (val 为最终值, 可能仍是 0xFFFF 表示无应答); -1 = GB 超时
 * @note    首次读 REG2 的 0xFFFF bug (PHY 已知) 已内部重试×3, 对上层黑盒
 */
int  MaePhyGetId(MaePhy_t *dev, uint16_t *id1, uint16_t *id2);

/**
 * @brief   找应答的 PHY: 期望地址优先, 失败则扫 0~31
 * @param   dev   驱动上下文 (phy_addr 为期望地址)
 * @return  >=0 = PHY 地址 (回填 dev->phy_addr); -1 = 总线无应答者
 */
int  MaePhyFind(MaePhy_t *dev);

/**
 * @brief   读 PHY 寄存器 (含 0xFFFF 重试×3)
 * @param   dev   驱动上下文
 * @param   reg   C22 寄存器地址
 * @param   val   输出值
 * @return  0 = 成功; -1 = GB 超时 (MAC/MDC 侧问题)
 */
int  MaePhyRegRead (MaePhy_t *dev, uint32_t reg, uint16_t *val);

/**
 * @brief   写 PHY 寄存器
 * @param   dev   驱动上下文
 * @param   reg   C22 寄存器地址
 * @param   val   要写入的 16 位值
 * @return  0 = 成功; -1 = GB 超时 (写入未生效)
 */
int  MaePhyRegWrite(MaePhy_t *dev, uint32_t reg, uint16_t val);

/**
 * @brief   参考时钟模式初始化 (复刻 Linux maxio 驱动 clk_init, 软复位含在内)
 * @param   dev  驱动上下文
 * @return  0 = 完成 (时钟模式已校正); -1 = 软复位超时 (总线异常)
 * @note    必须跟在复位之后: 软复位会把时钟模式重置回默认值.
 *          时序: SWR → 等 bit15 清零 → 读 INSR(页 A43) bit5 → 异常则
 *          翻转 CLK_MODE(页 D92 reg2) bit8 (晶振 ↔ 振荡器) → 恢复原页
 *          → 延时 100ms 等 PLL 重锁. 翻转条件与方向均来自 Linux maxio
 *          驱动 (drivers/net/phy/maxio.c), 黑盒对上层.
 */
int  MaePhyClkInit(MaePhy_t *dev);

/**
 * @brief   等自协商完成 (轮询 BMSR.AN_COMPLETE)
 * @param   dev         驱动上下文
 * @param   timeout_ms  超时上限 (毫秒)
 * @return  0 = 完成; -1 = 超时或读失败
 * @note    link up 与协商速率需另从 MAC PHYCSR 读 (PHY 驱动不碰 MAC 寄存器)
 */
int  MaePhyWaitAnComplete(MaePhy_t *dev, uint32_t timeout_ms);

/**
 * @brief   LED 显示配置: 通用千兆双灯方案 (页 D04 LCR)
 * @param   dev  驱动上下文
 * @return  0 = 成功; -1 = MDIO 读写失败 (寄存器未生效)
 * @note    接线假设 (FS-MP1A 板上仅 LED0/LED1 到网口): LED0=绿灯,
 *          LED1=橙灯. 若实测颜色相反, 互换下方字段即可 (纯软件映射).
 *          极性由复位 strap 采样固定 (CFG_EXT→LED0/CFG_LDO[0]→LED1),
 *          软件不可改. PHY 复位会清 LCR 回默认, 必须在复位流程之后调用
 *          (GmacEthLinkUp 尾部). 默认即每速率单灯 (10M/100M/1000M 分灯),
 *          本配置重映射为: 绿灯 = 任意速率链路 + 活动, 橙灯 = 1000M 指示.
 */
int  MaePhyLedConfig(MaePhy_t *dev);

#endif /* PHY_MAE0621A_H_ */
