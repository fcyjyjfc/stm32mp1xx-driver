#include <string.h>
#include "stm32mp1xx_spi.h"
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_rcc.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

/* W25QXX commands */
#define W25Q_CMD_JEDECID    0x9F
#define W25Q_CMD_RDSR1      0x05
#define W25Q_CMD_WREN       0x06
#define W25Q_CMD_SECERASE   0x20
#define W25Q_CMD_PAGEPROG   0x02
#define W25Q_CMD_RDDATA     0x03

static SpiCfg_t Spi4Cfg;

static void SpiInit(void)
{
    RCC->MP_APB2ENSETR |= 1 << 9;   // SPI4 clock
    RCC->MP_AHB4ENSETR |= 0x7f;      // GPIOA-G enable

    GpioMode(GPIO_E, 11, GPIO_MODER_AF);
    GpioMode(GPIO_E, 12, GPIO_MODER_AF);
    GpioMode(GPIO_E, 13, GPIO_MODER_AF);
    GpioMode(GPIO_E, 14, GPIO_MODER_AF);
    GpioAf(GPIO_E, 11, 5);
    GpioAf(GPIO_E, 12, 5);
    GpioAf(GPIO_E, 13, 5);
    GpioAf(GPIO_E, 14, 5);
    GpioOtype(GPIO_E, 11, GPIO_OTYPE_PUSH_PULL);
    GpioOtype(GPIO_E, 12, GPIO_OTYPE_PUSH_PULL);
    GpioOtype(GPIO_E, 13, GPIO_OTYPE_PUSH_PULL);
    GpioOtype(GPIO_E, 14, GPIO_OTYPE_PUSH_PULL);
    GpioOspeed(GPIO_E, 11, GPIO_OSPEED_MEDIUM);
    GpioOspeed(GPIO_E, 12, GPIO_OSPEED_MEDIUM);
    GpioOspeed(GPIO_E, 13, GPIO_OSPEED_MEDIUM);
    GpioOspeed(GPIO_E, 14, GPIO_OSPEED_MEDIUM);
    GpioPullUpDown(GPIO_E, 11, GPIO_PUPDR_NO);
    GpioPullUpDown(GPIO_E, 12, GPIO_PUPDR_NO);
    GpioPullUpDown(GPIO_E, 13, GPIO_PUPDR_NO);
    GpioPullUpDown(GPIO_E, 14, GPIO_PUPDR_NO);

    Spi4Cfg.spi_baud_reate_div = 6;
    Spi4Cfg.spi_clk_cfg        = SPI_CLK_IDLE0_DELAY;
    Spi4Cfg.spi_comm_mode      = SPI_COMM_FULL_DUPLEX;
    Spi4Cfg.spi_master         = SPI_MASTER;
    Spi4Cfg.spi_protocol       = SPI_PROTOCOL_MOTOROLA;
    Spi4Cfg.spi_shift          = SPI_SHIFT_MSB_FIRST;
    Spi4Cfg.spi_word_len       = 7;
    Spi4Cfg.spi_ss_mgmt        = SPI_SSM_SW;

    SpiCfg(SPI4, &Spi4Cfg);
}

static void PrintHex8(uint8_t val)
{
    char buf[3];
    char hex[] = "0123456789ABCDEF";
    buf[0] = hex[val >> 4];
    buf[1] = hex[val & 0xF];
    buf[2] = '\0';
    PRINT(buf);
}

static void PrintDec(char *buf, int32_t val)
{
    if (val < 0)
    {
        *buf++ = '-';
        val = -val;
    }
    char *p = buf;
    do {
        *p++ = '0' + val % 10;
        val /= 10;
    } while (val > 0);
    *p = '\0';
    p--;
    while (buf < p)
    {
        char t = *buf;
        *buf++ = *p;
        *p = t;
        p--;
    }
}

/* --- W25QXX helpers --- */

static void W25Q_WriteEnable(void)
{
    uint8_t cmd = W25Q_CMD_WREN;
    SpiTxRx(SPI4, &cmd, (void *)0, 1);
}

static uint8_t W25Q_ReadSR1(void)
{
    uint8_t tx[2] = { W25Q_CMD_RDSR1, 0xFF };
    uint8_t rx[2];
    SpiTxRx(SPI4, tx, rx, 2);
    return rx[1];
}

static void W25Q_WaitBusy(void)
{
    while (W25Q_ReadSR1() & 0x01);
}

static void W25Q_ReadJEDECID(uint8_t id[3])
{
    uint8_t buf[4];
    buf[0] = W25Q_CMD_JEDECID;
    buf[1] = 0xFF;
    buf[2] = 0xFF;
    buf[3] = 0xFF;
    uint8_t rd[4];
    SpiTxRx(SPI4, buf, rd, 4);
    id[0] = rd[1];
    id[1] = rd[2];
    id[2] = rd[3];
}

static void W25Q_SectorErase(uint32_t addr)
{
    uint8_t buf[4];
    buf[0] = W25Q_CMD_SECERASE;
    buf[1] = (addr >> 16) & 0xFF;
    buf[2] = (addr >> 8) & 0xFF;
    buf[3] = addr & 0xFF;
    W25Q_WriteEnable();
    W25Q_WaitBusy();
    SpiTxRx(SPI4, buf, (void *)0, 4);
    W25Q_WaitBusy();
}

static void W25Q_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint8_t tx[260];  // 4-byte header + max 256 data
    tx[0] = W25Q_CMD_PAGEPROG;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;
    int i;
    for (i = 0; i < len; i++)
        tx[4 + i] = data[i];

    W25Q_WriteEnable();
    W25Q_WaitBusy();
    SpiTxRx(SPI4, tx, (void *)0, 4 + len);
    W25Q_WaitBusy();
}

static void W25Q_ReadData(uint32_t addr, uint8_t *data, uint16_t len)
{
    uint8_t tx[260];  // 4-byte header + dummy
    uint8_t rx[260];
    tx[0] = W25Q_CMD_RDDATA;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;
    int i;
    for (i = 0; i < len; i++)
        tx[4 + i] = 0xFF;

    SpiTxRx(SPI4, tx, rx, 4 + len);

    for (i = 0; i < len; i++)
        data[i] = rx[4 + i];
}

/* ===== Flash Test ===== */

static void FlashTest(void)
{
    uint8_t id[3];
    uint8_t buf[256];
    uint8_t verify[256];
    char dec[16];
    int i;

    PRINT("\r\n===== W25QXX Flash Test =====\r\n");

    SpiInit();
    IwdgKickDog(IWDG2);

    // 1. Read JEDEC ID
    W25Q_ReadJEDECID(id);
    PRINT("JEDEC ID: ");
    PrintHex8(id[0]);
    PRINT(" ");
    PrintHex8(id[1]);
    PRINT(" ");
    PrintHex8(id[2]);
    PRINT("\r\n");

    if (id[0] == 0xFF && id[1] == 0xFF && id[2] == 0xFF)
    {
        PRINT("Flash not responding (all 0xFF). Check hardware.\r\n");
        return;
    }
    IwdgKickDog(IWDG2);

    // 2. Read SR1
    uint8_t sr = W25Q_ReadSR1();
    PRINT("SR1: ");
    PrintHex8(sr);
    PRINT("\r\n");
    IwdgKickDog(IWDG2);

    // 3. Sector erase at address 0
//    PRINT("Erasing sector 0...\r\n");
//    W25Q_SectorErase(0);
//    PRINT("Erase done.\r\n");
//    IwdgKickDog(IWDG2);

    // 4. Read back to verify erased (all 0xFF)
//    W25Q_ReadData(0, verify, 256);
//    PRINT("Erased data[0..15]:");
//    for (i = 0; i < 16; i++)
//    {
//        PRINT(" ");
//        PrintHex8(verify[i]);
//    }
//    PRINT("\r\n");
//    IwdgKickDog(IWDG2);

    // 5. Prepare test pattern and program page
    for (i = 0; i < 256; i++)
        buf[i] = i;

    PRINT("Programming page 0 (256 bytes)...\r\n");
    W25Q_PageProgram(0, buf, 256);
    PRINT("Program done.\r\n");
    IwdgKickDog(IWDG2);

    // 6. Read back and verify
    W25Q_ReadData(0, verify, 256);

    int errors = 0;
    for (i = 0; i < 256; i++)
    {
        if (verify[i] != (uint8_t)i)
        {
            if (errors < 8)
            {
                PRINT("Mismatch @");
                PrintDec(dec, i);
                PRINT(dec);
                PRINT(": W=");
                PrintHex8(i);
                PRINT(" R=");
                PrintHex8(verify[i]);
                PRINT("\r\n");
            }
            errors++;
        }
    }
    IwdgKickDog(IWDG2);

    if (errors == 0)
    {
        PRINT("Verify PASSED (0..255).\r\n");

        PRINT("Data[0..31]:");
        for (i = 0; i < 32; i++)
        {
            if ((i & 0xF) == 0) PRINT("\r\n  ");
            PRINT(" ");
            PrintHex8(verify[i]);
        }
        PRINT("\r\n");
    }
    else
    {
        PRINT("Verify FAILED: ");
        PrintDec(dec, errors);
        PRINT(dec);
        PRINT(" errors\r\n");
    }
}

/* ===== LED Display Test ===== */

static void LedTest(void)
{
    PRINT("\r\n===== LED Display Test =====\r\n");

    SpiInit();

    static const uint8_t hex_map[16] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
        0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
    };

    uint8_t cmd[2];

    // off
    cmd[0] = 0; cmd[1] = 0;
    SpiTxRx(SPI4, cmd, (void *)0, 2);

    PRINT("Scrolling hex digits. Press any key to stop.\r\n");

    int count = 0;
    while (1)
    {
        IwdgKickDog(IWDG2);

        int j;
        for (j = 0; j < 4; j++)
        {
            cmd[0] = 1 << j;
            cmd[1] = hex_map[(count + j) & 0xF];
            SpiTxRx(SPI4, cmd, (void *)0, 2);
        }

        volatile int d;
        for (d = 0; d < 500000; d++);

        count++;

        char ch;
        if (UsartReadOne(USART4, (uint8_t *)&ch) == 1)
        {
            cmd[0] = 0; cmd[1] = 0;
            SpiTxRx(SPI4, cmd, (void *)0, 2);
            PRINT("stopped.\r\n");
            break;
        }
    }
}

/* ===== Submenu ===== */

static void PrintSubMenu(void)
{
    PRINT("\r\n===== SPI Test =====\r\n");
    PRINT("1. W25QXX Flash\r\n");
    PRINT("2. LED Display\r\n");
    PRINT("0. Back\r\n");
    PRINT("Select: ");
}

void SpiTest(void)
{
    char buf[8];

    while (1)
    {
        IwdgKickDog(IWDG2);
        PrintSubMenu();

        int pos = 0;
        char ch;
        while (pos < (int)sizeof(buf) - 1)
        {
            IwdgKickDog(IWDG2);
            if (UsartReadOne(USART4, (uint8_t *)&ch) == 0)
                continue;
            if (ch == 'S' || ch == 's')
            {
                UsartWrite(USART4, (void *)"\r\n", 2);
                break;
            }
            UsartWrite(USART4, &ch, 1);
            buf[pos++] = ch;
        }
        buf[pos] = '\0';

        if (strcmp(buf, "0") == 0)
            break;
        else if (strcmp(buf, "1") == 0)
            FlashTest();
        else if (strcmp(buf, "2") == 0)
            LedTest();
        else
            PRINT("Invalid selection.\r\n");
    }
}
