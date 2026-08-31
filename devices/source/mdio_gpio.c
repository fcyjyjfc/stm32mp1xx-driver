/*
 * mdio_gpio.c — GPIO 模拟 MDIO 总线 (IEEE 802.3 C22 位冲击)
 *
 *  Created on: 2026-8-31
 *      Author: gjsbr
 */

#include "mdio_gpio.h"
#include "stm32mp1xx_gpio.h"

#define MDIO_GPIO_BIT_DELAY  2


/**
 * @brief   一拍位时序延时
 * @note    目标 MDC ~0.5-2MHz. 实测此 PHY 对超慢 MDC 会帧超时不答
 *          (延时放大 50 倍 ≈ 50kHz 时全 0xFFFF, 而 666kHz 的 GMAC
 *          控制器读成功过), 故延时参数不能随意调大.
 */
static void MdioGpioDelay(void)
{
    volatile uint32_t d;
    for (d = 0; d < MDIO_GPIO_BIT_DELAY; d++)   /* 空转一拍, 决定 MDC 频率 */
        ;
}


/**
 * @brief   MDC 敲一拍, 同时把 MDIO 驱到指定电平 (命令阶段用)
 * @param   bus    引脚配置
 * @param   level  要驱动的电平 (0/1)
 * @note    命令阶段 MDIO 是推挽输出, 高电平由 GPIO 强驱动, 不依赖
 *          板上外部上拉; 输入阶段写 ODR 无效, 不影响总线.
 */
static void MdioGpioClk(const MdioGpio_t *bus, uint32_t level)
{
    if (level)                              /* 先把 MDIO 驱到本拍要发的电平 */
        GpioOutHi(bus->mdio_port, bus->mdio_pin);
    else
        GpioOutLow(bus->mdio_port, bus->mdio_pin);

    GpioOutHi(bus->mdc_port, bus->mdc_pin); /* MDC 上升沿: PHY 在边沿采样 */
    MdioGpioDelay();
    GpioOutLow(bus->mdc_port, bus->mdc_pin); /* 下降沿, 完成一拍 */
    MdioGpioDelay();
}


/**
 * @brief   MDC 敲一拍, 低电平期间采样 MDIO (读数据阶段用)
 * @param   bus  引脚配置
 * @return  采到的位电平 (0/1)
 * @note    采样点必须放在低电平阶段: 此 PHY 数据更新贴近 MDC 上升沿,
 *          高电平期间采样会采到"下一位" → 序列整体左移一位
 *          (0x4411 读出 0x8823, 实测回归). 低电平期间数据已稳定,
 *          采样才正确.
 */
static uint32_t MdioGpioReadBit(const MdioGpio_t *bus)
{
    uint32_t b;

    GpioOutLow(bus->mdc_port, bus->mdc_pin);
    MdioGpioDelay(); /* 等待低电平稳定，低电平期间数据稳定, 在此采样 */
    b = GpioInData(bus->mdio_port, bus->mdio_pin); /* 采样 MDIO */
    GpioOutHi(bus->mdc_port, bus->mdc_pin);  /* 拉高进入下一拍 (PHY 在上升沿附近更新) */
    MdioGpioDelay();
    return b;
}


/**
 * @brief   手动敲一帧 C22 读: 32 前导 + 14 命令 + TA + 16 位数据
 * @param   bus        引脚配置 (MDC/MDIO 位置 + AF 号)
 * @param   phy_addr   PHY 地址 (5 位)
 * @param   reg_addr   寄存器地址 (5 位)
 * @return  读到的 16 位值; 0xFFFF = 总线始终悬空高 = PHY 无应答
 * @note    用途: 绕过 GMAC MDIO 控制器定位故障层级 —— 控制器读不出但
 *          位冲击能读 = 问题在控制器侧 (MDIOAR/采样路径); 位冲击也
 *          全 0xFFFF = PHY/电源/走线硬件问题.
 *          命令阶段必须推挽强驱动: 开漏+内部 40k 上拉的 RC 上升沿
 *          1.2µs, 而 MDC 666kHz 位宽仅 1.5µs, 弱驱动下 PHY 采不到
 *          合法命令 (实测回归).
 *          TA/数据阶段切输入释放总线 (内部上拉保空闲电平), 只采样
 *          PHY 驱动的数据.
 *          帧结束恢复 AF: MDC/MDIO 与 MAC MDIO 控制器共用引脚, 不留
 *          GPIO 占用状态给后续 MAC 访问. OTYPER/PUPDR 不在此恢复,
 *          由调用方的引脚初始化重置.
 *          0xFFFF 重试 (PHY 首次读 REG2 bug) 是 PHY 特性, 不属于
 *          总线, 由调用方处理.
 */
uint16_t MdioGpioRead(MdioGpio_t *bus, uint32_t phy_addr, uint32_t reg_addr)
{
    uint32_t cmd = (1u << 12) | (1u << 11)
                 | ((phy_addr & 0x1Fu) << 5)
                 |  (reg_addr & 0x1Fu);
    uint16_t val = 0;
    int i;

    /* 1. MDC 推挽输出; MDIO 推挽输出 (命令阶段强驱动) */
    GpioMode(bus->mdc_port, bus->mdc_pin, GPIO_MODER_OUTPUT);
    GpioOtype(bus->mdc_port, bus->mdc_pin, GPIO_OTYPE_PUSH_PULL);
    GpioOutLow(bus->mdc_port, bus->mdc_pin);

    GpioMode(bus->mdio_port, bus->mdio_pin, GPIO_MODER_OUTPUT);
    GpioOtype(bus->mdio_port, bus->mdio_pin, GPIO_OTYPE_PUSH_PULL);
    GpioOutHi(bus->mdio_port, bus->mdio_pin);

    /* 2. 32 拍前导: MDIO 恒 1 (PHY 据此识别帧起始并对齐位流) */
    for (i = 0; i < 32; i++)
        MdioGpioClk(bus, 1);

    /* 3. 14 位命令, MSB 先出 (ST=01 + OP=10 读 + PA5 + RDA5) */
    for (i = 13; i >= 0; i--)
        MdioGpioClk(bus, (cmd >> i) & 1u);

    /* 4. TA/数据阶段: MDIO 切输入 (释放总线, PHY 驱动数据, 上拉保 Z) */
    GpioMode(bus->mdio_port, bus->mdio_pin, GPIO_MODER_INPUT);
    GpioPullUpDown(bus->mdio_port, bus->mdio_pin, GPIO_PUPDR_PULL_UP);

    /* TA: 2 拍释放 (读操作 PHY 在第二拍应拉低) */
    MdioGpioClk(bus, 1);
    MdioGpioClk(bus, 1);

    /* 5. 16 位数据: MDC 低电平期间采样 (此 PHY 上升沿附近更新数据) */
    for (i = 0; i < 16; i++)
        val = (val << 1) | MdioGpioReadBit(bus);

    /* 6. 恢复 AF 还给 MAC MDIO (OTYPER/PUPDR 由调用方初始化重置) */
    GpioMode(bus->mdc_port, bus->mdc_pin, GPIO_MODER_AF);
    GpioAf(bus->mdc_port, bus->mdc_pin, bus->mdc_af);
    GpioMode(bus->mdio_port, bus->mdio_pin, GPIO_MODER_AF);
    GpioAf(bus->mdio_port, bus->mdio_pin, bus->mdio_af);

    return val;
}
