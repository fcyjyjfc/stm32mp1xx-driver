/*
 * mdio_gpio.h — GPIO 模拟 MDIO 总线 (IEEE 802.3 C22 位冲击)
 *
 * 与 MAC 的 MDIO 控制器 (drivers/stm32mp1xx_gmac.c) 平级的第二条总线:
 * 直接操作 MDC/MDIO 引脚敲 C22 读帧, 用于绕过控制器做线级诊断.
 * 0xFFFF 重试等 PHY 特性不属于总线, 由调用方 (PHY 驱动/测试) 处理.
 *
 * 使用前须由调用方使能 GPIO 时钟并完成引脚初始化.
 */

#ifndef MDIO_GPIO_H_
#define MDIO_GPIO_H_

#include <stdint.h>
#include "stm32mp1xx_gpio.h"

/**
 * @brief   位冲击总线引脚配置
 * @note    mdc_af/mdio_af 是帧结束后恢复的 AF 号 (MDC/MDIO 与 MAC MDIO
 *          共用引脚, 不能留下 GPIO 占用状态给后续 MAC 访问)
 */
typedef struct {
    volatile GpioRegs_t *mdc_port;  /* MDC 引脚 */
    uint32_t mdc_pin;
    volatile GpioRegs_t *mdio_port; /* MDIO 引脚 (命令阶段推挽输出, TA/数据阶段输入) */
    uint32_t mdio_pin;
    uint32_t mdc_af;                /* AF 号, 帧结束后恢复 */
    uint32_t mdio_af;
} MdioGpio_t;

/**
 * @brief   手动敲一帧 C22 读: 32 前导 + 14 命令 + TA + 16 位数据
 * @param   bus        引脚配置 (MDC/MDIO 位置 + AF 号)
 * @param   phy_addr   PHY 地址 (5 位)
 * @param   reg_addr   寄存器地址 (5 位)
 * @return  读到的 16 位值; 0xFFFF = 总线始终悬空高 = PHY 无应答
 * @note    绕过 GMAC MDIO 控制器, 用于定位故障层级 (控制器读不出但位冲击
 *          能读 = 控制器侧问题; 位冲击也全 0xFFFF = PHY/板级硬件问题).
 *          无超时失败路径, 故不返回错误码; 0xFFFF 重试是 PHY 特性,
 *          由调用方处理
 */
uint16_t MdioGpioRead(MdioGpio_t *bus, uint32_t phy_addr, uint32_t reg_addr);

#endif /* MDIO_GPIO_H_ */
