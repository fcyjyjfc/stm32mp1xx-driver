#include "w25qxx.h"


/* W25QXX commands */
#define W25Q_CMD_JEDECID    0x9F
#define W25Q_CMD_RDSR1      0x05
#define W25Q_CMD_WREN       0x06
#define W25Q_CMD_SECERASE   0x20
#define W25Q_CMD_PAGEPROG   0x02
#define W25Q_CMD_RDDATA     0x03

static volatile SpiRegs_t *W25Q_SPI;


void W25Q_Init(volatile SpiRegs_t *const spi)
{
    W25Q_SPI = spi;
}


void W25Q_WriteEnable(void)
{
    uint8_t cmd = W25Q_CMD_WREN;
    SpiTxRx(W25Q_SPI, &cmd, (void *)0, 1);
}


uint8_t W25Q_ReadSR1(void)
{
    uint8_t tx[2] = { W25Q_CMD_RDSR1, 0xFF };
    uint8_t rx[2];
    SpiTxRx(W25Q_SPI, tx, rx, 2);
    return rx[1];
}


void W25Q_WaitBusy(void)
{
    while (W25Q_ReadSR1() & 0x01);
}


void W25Q_ReadJEDECID(uint8_t id[3])
{
    uint8_t buf[4] = { W25Q_CMD_JEDECID, 0xFF, 0xFF, 0xFF };
    uint8_t rd[4];
    SpiTxRx(W25Q_SPI, buf, rd, 4);
    id[0] = rd[1]; id[1] = rd[2]; id[2] = rd[3];
}


void W25Q_SectorErase(uint32_t addr)
{
    uint8_t buf[4];
    buf[0] = W25Q_CMD_SECERASE;
    buf[1] = (addr >> 16) & 0xFF;
    buf[2] = (addr >> 8) & 0xFF;
    buf[3] = addr & 0xFF;
    W25Q_WriteEnable();
    W25Q_WaitBusy();
    SpiTxRx(W25Q_SPI, buf, (void *)0, 4);
    W25Q_WaitBusy();
}


void W25Q_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint8_t tx[260];
    uint32_t i;

    tx[0] = W25Q_CMD_PAGEPROG;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;
    for (i = 0; i < len; i++)
        tx[4 + i] = data[i];

    W25Q_WriteEnable();
    W25Q_WaitBusy();
    SpiTxRx(W25Q_SPI, tx, (void *)0, 4 + len);
    W25Q_WaitBusy();
}


void W25Q_ReadData(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint8_t tx[260];
    uint8_t rx[260];
    uint32_t i;

    tx[0] = W25Q_CMD_RDDATA;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;
    for (i = 0; i < len; i++)
        tx[4 + i] = 0xFF;

    SpiTxRx(W25Q_SPI, tx, rx, 4 + len);
    for (i = 0; i < len; i++)
        data[i] = rx[4 + i];
}
