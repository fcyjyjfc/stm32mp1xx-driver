#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_i2c.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "at24cxx.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

void I2cInit(void)
{
    *(uint32_t *)(0X50000000 + 0XA28) |= 1 << 5;
    *(uint32_t *)(0X50000000 + 0XA00) |= 1 << 21;

    GpioMode(GPIO_F, 15, GPIO_MODER_AF);
    GpioMode(GPIO_F, 14, GPIO_MODER_AF);
    GpioAf(GPIO_F, 15, 5);
    GpioAf(GPIO_F, 14, 5);
    GpioOtype(GPIO_F, 15, GPIO_OTYPE_OD);
    GpioOtype(GPIO_F, 14, GPIO_OTYPE_OD);
    GpioOspeed(GPIO_F, 15, GPIO_OSPEED_LOW);
    GpioOspeed(GPIO_F, 14, GPIO_OSPEED_LOW);

    I2cCfg(I2C1);
    AT24C_Init(I2C1, 0xA0);
}

// ==================== EEPROM Test (0xA0) ====================

static void I2cEepromTest(void)
{
    uint8_t wr_buf[32];
    uint8_t rd_buf[32];
    int pass;
    int i;

    // --- test 1: write 0..31 to addr 0, read back ---
    PRINT("\r\nEEPROM: write 0..31 to addr 0\r\n");
    for (i = 0; i < 32; i++)
        wr_buf[i] = i;
    AT24C_Write(0, wr_buf, 32);

    for (i = 0; i < 1000000; i++);

    PRINT("EEPROM: readback addr 0..31\r\n");
    for (i = 0; i < 32; i++)
        rd_buf[i] = 0;
    AT24C_Read(0, rd_buf, 32);

    pass = 1;
    for (i = 0; i < 32; i++)
    {
        if (rd_buf[i] != i)
        {
            pass = 0;
            break;
        }
    }
    PRINT(pass ? "  addr 0..31 : PASS\r\n" : "  addr 0..31 : FAIL\r\n");

    // --- test 2: write 0xAA to addr 32..63, read back ---
    PRINT("EEPROM: write 0xAA to addr 32..63\r\n");
    for (i = 0; i < 32; i++)
        wr_buf[i] = 0xAA;
    AT24C_Write(32, wr_buf, 32);

    for (i = 0; i < 1000000; i++);

    PRINT("EEPROM: readback addr 32..63\r\n");
    for (i = 0; i < 32; i++)
        rd_buf[i] = 0;
    AT24C_Read(32, rd_buf, 32);

    pass = 1;
    for (i = 0; i < 32; i++)
    {
        if (rd_buf[i] != 0xAA)
        {
            pass = 0;
            break;
        }
    }
    PRINT(pass ? "  addr 32..63: PASS\r\n" : "  addr 32..63: FAIL\r\n");
}

// ==================== Sensor Test (0x80) ====================

static void FormatVal(char *buf, int32_t val)
{
    if (val < 0)
    {
        *buf++ = '-';
        val = -val;
    }
    int32_t i = val / 100;
    int32_t f = val % 100;
    if (i >= 100) *buf++ = '0' + i / 100;
    if (i >= 10)  *buf++ = '0' + (i / 10) % 10;
    *buf++ = '0' + i % 10;
    *buf++ = '.';
    *buf++ = '0' + f / 10;
    *buf++ = '0' + f % 10;
    *buf = '\0';
}

static void I2cSensorTest(void)
{
    uint8_t rd_buf[2];
    uint16_t raw;
    int32_t temperature;
    int32_t humidity;

    // read temperature
    PRINT("\r\nSensor: read temperature...\r\n");
    uint8_t temp_cmd = 0xE3;
    I2cMstWrite(I2C1, 0x80, &temp_cmd, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
    I2cMstRead(I2C1, 0x80, rd_buf, 2, I2C_BUS_RESTART, I2C_BUS_STOP);

    raw = (rd_buf[0] << 8) | rd_buf[1];
    temperature = 17572 * raw / 65536 - 4685;

    // read humidity
    PRINT("Sensor: read humidity...\r\n");
    uint8_t hum_cmd = 0xE5;
    I2cMstWrite(I2C1, 0x80, &hum_cmd, 1, I2C_BUS_START, I2C_BUS_NO_STOP);
    I2cMstRead(I2C1, 0x80, rd_buf, 2, I2C_BUS_RESTART, I2C_BUS_STOP);

    raw = (rd_buf[0] << 8) | rd_buf[1];
    humidity = 12500 * raw / 65536 - 600;

    // format and print
    char buf[64];
    char tmp[16];

    FormatVal(tmp, temperature);
    int pos = 0;
    const char *s = "  Temp: ";
    while (*s) buf[pos++] = *s++;
    s = tmp;
    while (*s) buf[pos++] = *s++;
    s = " C\r\n";
    while (*s) buf[pos++] = *s++;

    FormatVal(tmp, humidity);
    s = "  Humi: ";
    while (*s) buf[pos++] = *s++;
    s = tmp;
    while (*s) buf[pos++] = *s++;
    s = " %\r\n";
    while (*s) buf[pos++] = *s++;

    UsartWrite(USART4, (void *)buf, pos);
}

// ==================== I2C Test Sub-Menu ====================

static int ReadLine(char *buf, int max_len)
{
    int pos = 0;
    char ch;
    while (pos < max_len - 1)
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
    return pos;
}

void I2cTest(void)
{
    char buf[8];

    while (1)
    {
    	IwdgKickDog(IWDG2);

        PRINT("\r\n----- I2C Test Menu -----\r\n");
        PRINT("1. EEPROM (0xA0)\r\n");
        PRINT("2. Sensor (0x80)\r\n");
        PRINT("0. Back\r\n");
        PRINT("--------------------------\r\n");
        PRINT("Select: ");

        ReadLine(buf, sizeof(buf));

        if (strcmp(buf, "0") == 0)
            break;
        if (strcmp(buf, "1") == 0)
            I2cEepromTest();
        else if (strcmp(buf, "2") == 0)
            I2cSensorTest();
        else
            PRINT("Invalid.\r\n");
    }
}
