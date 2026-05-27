#include <string.h>
#include <stdint.h>
#include "stm32mp1xx_gpio.h"
#include "stm32mp1xx_i2c.h"
#include "stm32mp1xx_usart.h"
#include "stm32mp1xx_iwdg.h"
#include "at24cxx.h"
#include "si7006.h"

#define PRINT(s)  UsartWrite(USART4, (void *)(s), strlen(s))

static const char essay[] =
    "Embedded systems development on bare-metal ARM processors requires careful "
    "attention to memory layout, peripheral initialization, and interrupt handling. "
    "The STM32MP157 is a heterogeneous SoC featuring dual Cortex-A7 cores alongside "
    "a Cortex-M4 coprocessor. When working with I2C peripherals, developers must "
    "understand the timing requirements, acknowledge mechanisms, and the nuances "
    "of repeated START conditions. A common pitfall is the interaction between "
    "AUTOEND and RESTART, which can produce an unexpected STOP before the repeated "
    "START, corrupting the bus transaction. By manually managing the STOP condition "
    "and using the RELOAD mechanism for transfers exceeding 255 bytes, reliable "
    "communication with EEPROM devices such as the AT24C64N can be achieved. "
    "The I2C bus operates at standard speeds of 100 kHz or fast mode at 400 kHz, "
    "with the timing register configured according to the peripheral clock frequency.";

static uint8_t i2c_tx_buf[921];

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
    SI7006_Init(I2C1);
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
    int32_t temperature;
    int32_t humidity;

    // read temperature
    PRINT("\r\nSensor: read temperature...\r\n");
    temperature = SI7006_ReadTemp();

    // read humidity
    PRINT("Sensor: read humidity...\r\n");
    humidity = SI7006_ReadHumi();

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

// ==================== >255-Byte Write / Read Test ====================

static void I2cEssayTest(void)
{
    const uint32_t essay_len = sizeof(essay) - 1;
    uint8_t rd_buf[sizeof(essay)];
    uint32_t offset;
    uint32_t i;
    uint32_t pass;

    IwdgKickDog(IWDG2);

    // --- 1. write essay page by page (32 bytes/page) ---
    PRINT("\r\n--- >255B Test ---\r\n");
    PRINT("Essay write page by page...\r\n");
    offset = 0;
    while (offset < essay_len)
    {
        uint32_t chunk = essay_len - offset;
        if (chunk > 32)
        {
            chunk = 32;
        }
        AT24C_Write(offset, (const uint8_t *)(essay + offset), chunk);
        for (i = 0; i < 500000; i++)
        {
            ;
        }
        offset += chunk;
    }
    IwdgKickDog(IWDG2);

    // --- 2. read back in one shot (>255 bytes, tests RELOAD) ---
    PRINT("Read back in single transaction (>255B)...\r\n");
    for (i = 0; i < essay_len; i++)
    {
        rd_buf[i] = 0;
    }
    AT24C_Read(0, rd_buf, essay_len);

    pass = 1;
    for (i = 0; i < essay_len; i++)
    {
        if (rd_buf[i] != essay[i])
        {
            pass = 0;
            break;
        }
    }
    PRINT(pass ? "  Verify: PASS\r\n" : "  Verify: FAIL\r\n");
    IwdgKickDog(IWDG2);

    // --- 3. direct I2cMstWrite >255 bytes (waveform check) ---
    PRINT("Direct I2cMstWrite >255B (waveform)...\r\n");
    for (i = 0; i < 921; i++)
    {
        i2c_tx_buf[i] = 0x10 + (i & 0x0F);
    }
    I2cMstWrite(I2C1, 0xA0, i2c_tx_buf, 921, I2C_BUS_START, I2C_BUS_STOP);
    PRINT("  Done.\r\n");
}

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
        PRINT("3. >255B R/W\r\n");
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
        else if (strcmp(buf, "3") == 0)
            I2cEssayTest();
        else
            PRINT("Invalid.\r\n");
    }
}
