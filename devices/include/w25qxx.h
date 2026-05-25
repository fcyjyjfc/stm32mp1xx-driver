#ifndef __W25QXX_H_
#define __W25QXX_H_

#include <stdint.h>
#include "stm32mp1xx_spi.h"


/* 初始化：绑定 SPI 实例 */
void W25Q_Init(volatile SpiRegs_t *const spi);

void W25Q_ReadJEDECID(uint8_t id[3]);
uint8_t W25Q_ReadSR1(void);
void W25Q_WaitBusy(void);
void W25Q_WriteEnable(void);
void W25Q_SectorErase(uint32_t addr);
void W25Q_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len);
void W25Q_ReadData(uint32_t addr, uint8_t *data, uint16_t len);

#endif
