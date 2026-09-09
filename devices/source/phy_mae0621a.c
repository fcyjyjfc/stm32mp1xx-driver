/*
 * phy_mae0621a.c — PHY 设备驱动 (实测 ID 0x7B74/0x4411, MAE0621A)
 *
 *  Created on: 2026-8-31
 *      Author: gjsbr
 */

#include "phy_mae0621a.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_btimer.h"


/**
 * @brief   经 MAC MDIO 控制器读一个寄存器, 对 0xFFFF 结果自动重试
 * @param   dev   目标 PHY (gmac 基址 + phy_addr 共同定位总线事务)
 * @param   reg   C22 寄存器地址 (0~31)
 * @param   val   输出读到的值
 * @return  0 = 成功 (val 为最终值, 可能仍是 0xFFFF = PHY 无应答);
 *          -1 = GB 超时 (MAC/MDC 侧问题)
 * @note    此 PHY 存在已知 bug: 首次读 REG2 (ID1) 可能返回 0xFFFF,
 *          故读到 0xFFFF 时重试至多 3 次. GB 超时说明总线事务根本没
 *          完成 (时钟/控制器侧问题), 重试无意义, 直接失败返回.
 *          上层所有读都走这里, 重试对上层黑盒.
 */
static int MaePhyReadRetry(MaePhy_t *dev, uint32_t reg, uint16_t *val)
{
    int t;

    for (t = 0; t < 3; t++)
    {
        if (!GmacMdioRead(dev->gmac, dev->phy_addr, reg, val))
            return -1;                 /* GB 超时: 事务未完成, 重试无意义 */
        if (*val != 0xFFFF)
            return 0;                  /* 读到非全 1 即有效 */
    }
    return 0;                          /* 3 次都 0xFFFF: 无应答, 返回最终值 */
}


/**
 * @brief   PHY 复位: 复位脚拉低 20ms 再释放, 之后等 200ms 才能访问寄存器
 * @param   dev   复位脚由 rst_port/rst_pin 指定, 引脚配置在此完成
 * @note    时序依据 (PHY 手册复位时序): 拉低 ≥10ms 让内部稳压器完全
 *          起效, 释放后 ≥30ms 等晶振起振 + PLL 锁定. 取 20/200ms
 *          是为留慢起振余量 (晶振批次/低温差异).
 *          引脚配置放这里而非上层, 让驱动自包含: 上层只声明"复位脚
 *          是哪个", 怎么配、按什么时序拉是驱动内部的事 (黑盒).
 *          延时走 BasicTimerDelayMs: 它每次自包含重配, 幂等, 不依赖
 *          任何静态标志 —— 启动代码不清 .bss, 静态标志复位后是 DDR
 *          残留 (可能非 0), 靠标志跳过配置会让 TIM7 时钟未使能而在
 *          延时里死循环 (历史回归根因).
 */
void MaePhyReset(MaePhy_t *dev)
{
    GpioMode(dev->rst_port, dev->rst_pin, GPIO_MODER_OUTPUT);
    GpioOtype(dev->rst_port, dev->rst_pin, GPIO_OTYPE_PUSH_PULL);   /* AF11 是 CRS, 复位必须 GPIO 直驱 */
    GpioOspeed(dev->rst_port, dev->rst_pin, GPIO_OSPEED_HI);

    GpioOutLow(dev->rst_port, dev->rst_pin);
    BasicTimerDelayMs(TIM7, 20);         /* 低脉冲 ≥10ms, 20ms 留余量 */

    GpioOutHi(dev->rst_port, dev->rst_pin);
    BasicTimerDelayMs(TIM7, 200);        /* 释放后 ≥30ms 才能访问, 200ms 留起振余量 */
}


/**
 * @brief   参考时钟模式初始化 (复刻 Linux maxio 驱动 clk_init)
 * @param   dev  目标 PHY
 * @return  0 = 时钟模式已校正; -1 = 软复位超时 (总线侧问题)
 * @note    根因背景: 此 PHY 复位后默认振荡器模式 (bit8=1), 本板工作
 *          在晶振模式 (bit8=0; Linux 修复 printk 0x200a 与本机读回
 *          0x200A 一致), 内部 PLL 锁不住 → PMA/AN 引擎无时钟 → 线上零
 *          活动 (ANLPAR 恒 0, 对端看不到本机), AN 永不完成. MDIO 属
 *          MDC 时钟域不受影响, 故 ID/寄存器读写全部正常 —— 这是裸机
 *          与 Linux 行为差异的真因 (Linux maxio 驱动在 probe/config_init
 *          各跑一次本流程, 裸机此前从未执行).
 *          时序与 Linux 驱动一致: 先 SWR 再判 INSR. SWR 会把时钟模式
 *          重置回错误默认, 修复必须跟在 SWR 之后 (Linux phylib 在
 *          link-down 后只重跑 config_aneg 不复跑 clk_init, 故 A7 侧
 *          软复位后 AN 同样挂死). 翻转后留 100ms 等 PLL 重锁.
 *          INSR bit5 = PHY 操作状态 (1 正常 / 0 异常, 手册 8.4.17);
 *          页 D92/reg2 手册未公开, 位定义来自 Linux 驱动源码.
 */
int MaePhyClkInit(MaePhy_t *dev)
{
    uint16_t bmcr_v, pag, insr, clk;
    int i;

    /* 1. SWR (Linux: BMCR_RESET | 读改写; SWR 复位全部寄存器, 直接全 1 等价) */
    MaePhyRegWrite(dev, PHY_REG_BMCR, PHY_BMCR_SWR);
    for (i = 0; i < 100; i++)
    {
        if (MaePhyRegRead(dev, PHY_REG_BMCR, &bmcr_v) != 0)
            return -1;
        if (!(bmcr_v & PHY_BMCR_SWR))
            break;
        BasicTimerDelayMs(TIM7, 5);
    }
    if (i >= 100)
        return -1;                       /* SWR 未完成, 后续判定不可信 */

    /* 2. 换页读工作状态 INSR; 换页前保存 PAGSR 用后恢复 */
    MaePhyRegRead(dev, PHY_REG_PAGSR, &pag);
    MaePhyRegWrite(dev, PHY_REG_PAGSR, PHY_PAGE_A43);
    MaePhyRegRead(dev, PHY_REG_INSR, &insr);

    /* 3. bit5=0 (异常) → 翻转 CLK_MODE bit8 (晶振 ↔ 振荡器) */
    if (!(insr & PHY_INSR_OP_NORMAL))
    {
        MaePhyRegWrite(dev, PHY_REG_PAGSR, PHY_PAGE_D92);
        MaePhyRegRead(dev, PHY_REG_CLK_MODE, &clk);
        MaePhyRegWrite(dev, PHY_REG_CLK_MODE, clk ^ PHY_CLK_MODE_OSC);
    }

    /* 4. 恢复原页, 留 PLL 重锁时间 */
    MaePhyRegWrite(dev, PHY_REG_PAGSR, pag);
    BasicTimerDelayMs(TIM7, 100);
    return 0;
}


/**
 * @brief   读 PHY 身份: ID1 (reg2) + ID2 (reg3)
 * @param   dev   目标 PHY
 * @param   id1   输出: OUI 厂商代码 + 部分型号
 * @param   id2   输出: 型号 + 版本号
 * @return  0 = 成功; -1 = GB 超时
 * @note    两个寄存器都走 MaePhyReadRetry (首次读 REG2 的 0xFFFF bug
 *          已由重试吸收). 上层拿到的就是重试后的稳定值, 无需关心
 *          重试逻辑. 本板实测 0x7B74/0x4411 (MAE0621A).
 */
int MaePhyGetId(MaePhy_t *dev, uint16_t *id1, uint16_t *id2)
{
    if (MaePhyReadRetry(dev, PHY_REG_ID1, id1) != 0)   /* 先读 ID1, 首次读 bug 在重试里吸收 */
        return -1;
    if (MaePhyReadRetry(dev, PHY_REG_ID2, id2) != 0)   /* 再读 ID2, 两个寄存器独立重试 */
        return -1;
    return 0;
}


/**
 * @brief   在总线上找应答的 PHY 地址
 * @param   dev   期望地址在 phy_addr; 找到后回填实际应答地址
 * @return  >=0 = 应答的 PHY 地址; -1 = 总线上没有应答者
 *          (失败时 phy_addr 恢复为期望值)
 * @note    先试期望地址 (strap 配置, 正常情况就是它, 零扫描开销);
 *          失败才全地址段扫描 (0~31), 兼容 strap 配置与板子不一致的
 *          调试场景. "应答"判定 = ID1/ID2 均非 0xFFFF —— 全 1 是
 *          MDIO 总线无驱动的空闲电平, 不代表有任何 PHY 存在.
 */
int MaePhyFind(MaePhy_t *dev)
{
    uint16_t id1, id2;
    uint32_t want = dev->phy_addr;
    uint32_t addr;

    if (MaePhyGetId(dev, &id1, &id2) == 0
        && id1 != 0xFFFF && id2 != 0xFFFF)
        return (int)want;                   /* 期望地址直接应答, 零扫描开销 */

    for (addr = 0; addr < 32; addr++)
    {
        if (addr == want)
            continue;                       /* 期望地址已试过 */
        dev->phy_addr = addr;               /* 扫描: 逐个地址尝试 */
        if (MaePhyGetId(dev, &id1, &id2) == 0
            && id1 != 0xFFFF && id2 != 0xFFFF)
            return (int)addr;               /* 命中即回填实际地址 */
    }

    dev->phy_addr = want;                   /* 无应答, 恢复期望地址 */
    return -1;
}


/**
 * @brief   读 PHY 寄存器 (自动重试 0xFFFF)
 * @param   dev   目标 PHY
 * @param   reg   C22 寄存器地址 (0~31)
 * @param   val   输出读到的值
 * @return  0 = 成功; -1 = GB 超时
 * @note    与 MaePhyGetId 的区别是寄存器地址任意, 供诊断用
 *          (寄存器栅格图、写读回环等). 与 MaePhyGetId 一样,
 *          首次读 bug 的重试已封装, 上层无需感知.
 */
int MaePhyRegRead(MaePhy_t *dev, uint32_t reg, uint16_t *val)
{
    return MaePhyReadRetry(dev, reg, val);
}


/**
 * @brief   写 PHY 寄存器
 * @param   dev   目标 PHY
 * @param   reg   C22 寄存器地址 (0~31)
 * @param   val   要写入的 16 位值
 * @return  0 = 成功; -1 = GB 超时 (总线事务未完成, 写入未生效)
 */
int MaePhyRegWrite(MaePhy_t *dev, uint32_t reg, uint16_t val)
{
    if (!GmacMdioWrite(dev->gmac, dev->phy_addr, reg, val))
        return -1;
    return 0;
}


/**
 * @brief   等自协商完成: 轮询 BMSR.AN_COMPLETE, 每 20ms 一次
 * @param   dev         目标 PHY
 * @param   timeout_ms  超时上限 (毫秒)
 * @return  0 = 自协商完成; -1 = 超时或读失败
 * @note    只有"协商完成"属于 PHY 侧 (MDIO 可及). link up 与协商
 *          速率/双工以 MAC 侧 ETH_MACPHYCSR 为准 (RGMII 引脚上的
 *          实际信号), 由上层自行读取 —— PHY 驱动不碰 MAC 寄存器
 *          (外设隔离). 轮询间隔 20ms: 协商通常 <2s, 间隔对总耗时
 *          影响可忽略, 却省去每轮一次的寄存器操作.
 */
int MaePhyWaitAnComplete(MaePhy_t *dev, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    uint16_t bmsr;

    while (elapsed < timeout_ms)
    {
        if (MaePhyRegRead(dev, PHY_REG_BMSR, &bmsr) != 0)
            return -1;                      /* 读失败: 总线异常, 直接退出 */
        if (bmsr & PHY_BMSR_AN_COMPLETE)
            return 0;                       /* 协商完成 */
        BasicTimerDelayMs(TIM7, 20);        /* 未完成, 等 20ms 再查 */
        elapsed += 20;
    }
    return -1;                              /* 超时: 检查网线/对端 */
}


/**
 * @brief   LED 显示配置: 通用千兆双灯方案 (页 D04 LCR)
 * @param   dev   目标 PHY
 * @return  0 = 成功; -1 = MDIO 故障
 * @note    接线假设 LED0=绿灯/LED1=橙灯 (FS-MP1A 板上仅这两路到网口),
 *          颜色反转则互换字段赋值. 重映射: 绿灯(即 LED0) = 任意速率
 *          链路(10M/100M/1000M 三链路位同置) + 活动; 橙灯(LED1) =
 *          1000M 指示 (通用千兆方案: 橙灯代表高速). LED2 保持复位
 *          默认 0x6000 (未接线). 保留的链路/活动位与默认一致.
 */
int MaePhyLedConfig(MaePhy_t *dev)
{
    uint16_t pag, lcr;

    if (MaePhyRegRead(dev, PHY_REG_PAGSR, &pag) != 0)
        return -1;

    /* 换页窗口: 进 D04 读改写 LCR, 退出恢复原页 (MaePhyClkInit 同款) */
    if (MaePhyRegWrite(dev, PHY_REG_PAGSR, PHY_PAGE_D04) != 0)
        return -1;

    if (MaePhyRegRead(dev, PHY_REG_LCR, &lcr) != 0)
        return -1;

    lcr &= ~(PHY_LCR_LED0_MASK | PHY_LCR_LED1_MASK);
    lcr |= PHY_LCR_LED0_LINK_10 | PHY_LCR_LED0_LINK_100
         | PHY_LCR_LED0_LINK_1000 | PHY_LCR_LED0_ACT   /* LED0: 任意速率链路+活动 */
         | PHY_LCR_LED1_LINK_1000;                      /* LED1: 1000M 指示 */
    if (MaePhyRegWrite(dev, PHY_REG_LCR, lcr) != 0)
        return -1;

    return MaePhyRegWrite(dev, PHY_REG_PAGSR, pag);
}
