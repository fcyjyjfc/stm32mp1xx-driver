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
#define PHY_REG_PHYSR  26  /* 实时状态 (位定义待查) */
#define PHY_REG_PAGSR  31  /* 页选择 (换页窗口) */

/* =================== 页面 =================== */
#define PHY_PAGE_A43   0xA43u  /* PHY 专属页: PHYCR1/2, PHYSR, INSR */
#define PHY_PAGE_D92   0xD92u  /* 时钟配置页 (手册未公开, 定义来自 Linux maxio 驱动) */

/* =================== 页内寄存器 =================== */
#define PHY_REG_INSR      0x1Du  /* 页 A43: 中断状态, bit5 = 操作状态 */
#define PHY_REG_CLK_MODE  0x02u  /* 页 D92: 参考时钟模式, bit8 = 晶振/振荡器 */

/* =================== 位定义 =================== */
#define PHY_BMSR_AN_COMPLETE  (1u << 5)
#define PHY_INSR_OP_NORMAL    (1u << 5)  /* INSR bit5: 1 = PHY 操作正常, 0 = 异常 */
#define PHY_CLK_MODE_OSC      (1u << 8)  /* CLK_MODE bit8: 1 = 振荡器, 0 = 晶振 */

/* =================== 芯片 ID =================== */
#define MAE_PHY_ID1  0x7B74
#define MAE_PHY_ID2  0x4411

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

#endif /* PHY_MAE0621A_H_ */
