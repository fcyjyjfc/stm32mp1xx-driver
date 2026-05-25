#include "m74hc595.h"


static volatile SpiRegs_t *Led_SPI;

/* 共阴数码管段码表 0~F (bit: dp-g-f-e-d-c-b-a) */
static const uint8_t hex_seg[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
};


void Led_Init(volatile SpiRegs_t *const spi)
{
    Led_SPI = spi;
}


void Led_DisplayDigit(uint8_t digit, uint8_t num)
{
    uint8_t cmd[2];

    if (digit < 1 || digit > 4)
        return;
    if (num > 15)
        num = 15;

    cmd[0] = 1 << (digit - 1);   /* bit0~3 对应 D1~D4 */
    cmd[1] = hex_seg[num];
    SpiTx(Led_SPI, cmd, 2);
}


void Led_Clear(void)
{
    uint8_t cmd[2] = { 0xf, 0 };
    SpiTx(Led_SPI, cmd, 2);
}


void Led_DisplayAll(uint8_t num)
{
    uint8_t cmd[2];

    if (num > 15)
        num = 15;

    cmd[0] = 0x0F;              /* 4 个位全选 */
    cmd[1] = hex_seg[num];
    SpiTx(Led_SPI, cmd, 2);
}
