#ifndef STM32MP1XX_RCC_H_
#define STM32MP1XX_RCC_H_

#include <stdint.h>

typedef struct {
    // 0x000 - 0x003
    volatile uint32_t TZCR;         // TrustZone control
    // 0x004 - 0x00B
    volatile uint8_t  RSVD0[0x00C - 0x004];
    // 0x00C - 0x013
    volatile uint32_t OCENSETR;     // oscillators enable set
    volatile uint32_t OCENCLRR;     // oscillators enable clear
    // 0x014
    volatile uint8_t  RSVD1[0x018 - 0x014];
    // 0x018 - 0x01F
    volatile uint32_t HSICFGR;      // HSI config
    volatile uint32_t CSICFGR;      // CSI config
    // 0x020 - 0x02B
    volatile uint32_t MPCKSELR;     // MPU clock source select
    volatile uint32_t ASSCKSELR;    // AXI sub-system clock source select
    volatile uint32_t RCK12SELR;    // PLL1/2 reference clock source select
    // 0x02C - 0x033
    volatile uint32_t MPCKDIVR;     // MPU clock divider
    volatile uint32_t AXIDIVR;      // AXI clock divider
    // 0x034 - 0x03B
    volatile uint8_t  RSVD2[0x03C - 0x034];
    // 0x03C - 0x047
    volatile uint32_t APB4DIVR;     // APB4 clock divider
    volatile uint32_t APB5DIVR;     // APB5 clock divider
    volatile uint32_t RTCDIVR;      // RTC clock divider
    // 0x048
    volatile uint32_t MSSCKSELR;    // MCU sub-system clock source select
    // 0x04C - 0x07F
    volatile uint8_t  RSVD3[0x080 - 0x04C];
    // 0x080 - 0x093
    volatile uint32_t PLL1CR;       // PLL1 control
    volatile uint32_t PLL1CFGR1;    // PLL1 config 1
    volatile uint32_t PLL1CFGR2;    // PLL1 config 2
    volatile uint32_t PLL1FRACR;    // PLL1 fractional divider
    volatile uint32_t PLL1CSGR;     // PLL1 spread spectrum control
    // 0x094 - 0x0A7
    volatile uint32_t PLL2CR;       // PLL2 control
    volatile uint32_t PLL2CFGR1;    // PLL2 config 1
    volatile uint32_t PLL2CFGR2;    // PLL2 config 2
    volatile uint32_t PLL2FRACR;    // PLL2 fractional divider
    volatile uint32_t PLL2CSGR;     // PLL2 spread spectrum control
    // 0x0A8 - 0x0BF
    volatile uint8_t  RSVD4[0x0C0 - 0x0A8];
    // 0x0C0 - 0x0D7
    volatile uint32_t I2C46CKSELR;  // I2C46 kernel clock select
    volatile uint32_t SPI6CKSELR;   // SPI6 kernel clock select
    volatile uint32_t UART1CKSELR;  // UART1 kernel clock select
    volatile uint32_t RNG1CKSELR;   // RNG1 kernel clock select
    volatile uint32_t CPERCKSELR;   // CPER kernel clock select
    volatile uint32_t STGENCKSELR;  // STGEN kernel clock select
    // 0x0D8
    volatile uint32_t DDRITFCR;     // DDR interface control
    // 0x0DC - 0x0FF
    volatile uint8_t  RSVD5[0x100 - 0x0DC];
    // 0x100 - 0x117
    volatile uint32_t MP_BOOTCR;    // MPU boot control
    volatile uint32_t MP_SREQSETR;  // MPU system request set
    volatile uint32_t MP_SREQCLRR;  // MPU system request clear
    volatile uint32_t MP_GCR;       // MPU general control
    volatile uint32_t MP_APRSTCR;   // MPU application reset control
    volatile uint32_t MP_APRSTSR;   // MPU application reset status
    // 0x118 - 0x13F
    volatile uint8_t  RSVD6[0x140 - 0x118];
    // 0x140 - 0x147
    volatile uint32_t BDCR;         // backup domain control
    volatile uint32_t RDLSICR;      // RDL SI control
    // 0x148 - 0x17F
    volatile uint8_t  RSVD7[0x180 - 0x148];
    // 0x180 - 0x1A7
    volatile uint32_t APB4RSTSETR;  // APB4 reset set
    volatile uint32_t APB4RSTCLRR;  // APB4 reset clear
    volatile uint32_t APB5RSTSETR;  // APB5 reset set
    volatile uint32_t APB5RSTCLRR;  // APB5 reset clear
    volatile uint32_t AHB5RSTSETR;  // AHB5 reset set
    volatile uint32_t AHB5RSTCLRR;  // AHB5 reset clear
    volatile uint32_t AHB6RSTSETR;  // AHB6 reset set
    volatile uint32_t AHB6RSTCLRR;  // AHB6 reset clear
    volatile uint32_t TZAHB6RSTSETR;// TZ AHB6 reset set
    volatile uint32_t TZAHB6RSTCLRR;// TZ AHB6 reset clear
    // 0x1A8 - 0x1FF
    volatile uint8_t  RSVD8[0x200 - 0x1A8];
    // 0x200 - 0x227
    volatile uint32_t MP_APB4ENSETR;   // APB4 enable set
    volatile uint32_t MP_APB4ENCLRR;   // APB4 enable clear
    volatile uint32_t MP_APB5ENSETR;   // APB5 enable set
    volatile uint32_t MP_APB5ENCLRR;   // APB5 enable clear
    volatile uint32_t MP_AHB5ENSETR;   // AHB5 enable set
    volatile uint32_t MP_AHB5ENCLRR;   // AHB5 enable clear
    volatile uint32_t MP_AHB6ENSETR;   // AHB6 enable set
    volatile uint32_t MP_AHB6ENCLRR;   // AHB6 enable clear
    volatile uint32_t MP_TZAHB6ENSETR; // TZ AHB6 enable set
    volatile uint32_t MP_TZAHB6ENCLRR; // TZ AHB6 enable clear
    // 0x228 - 0x27F
    volatile uint8_t  RSVD9[0x280 - 0x228];
    // 0x280 - 0x29F
    volatile uint32_t MC_APB4ENSETR; // MCU APB4 enable set
    volatile uint32_t MC_APB4ENCLRR; // MCU APB4 enable clear
    volatile uint32_t MC_APB5ENSETR; // MCU APB5 enable set
    volatile uint32_t MC_APB5ENCLRR; // MCU APB5 enable clear
    volatile uint32_t MC_AHB5ENSETR; // MCU AHB5 enable set
    volatile uint32_t MC_AHB5ENCLRR; // MCU AHB5 enable clear
    volatile uint32_t MC_AHB6ENSETR; // MCU AHB6 enable set
    volatile uint32_t MC_AHB6ENCLRR; // MCU AHB6 enable clear
    // 0x2A0 - 0x2FF
    volatile uint8_t  RSVD10[0x300 - 0x2A0];
    // 0x300 - 0x327
    volatile uint32_t MP_APB4LPENSETR;  // LP enable set
    volatile uint32_t MP_APB4LPENCLRR;  // LP enable clear
    volatile uint32_t MP_APB5LPENSETR;  // LP enable set
    volatile uint32_t MP_APB5LPENCLRR;  // LP enable clear
    volatile uint32_t MP_AHB5LPENSETR;  // LP enable set
    volatile uint32_t MP_AHB5LPENCLRR;  // LP enable clear
    volatile uint32_t MP_AHB6LPENSETR;  // LP enable set
    volatile uint32_t MP_AHB6LPENCLRR;  // LP enable clear
    volatile uint32_t MP_TZAHB6LPENSETR;// TZ AHB6 LP enable set
    volatile uint32_t MP_TZAHB6LPENCLRR;// TZ AHB6 LP enable clear
    // 0x328 - 0x37F
    volatile uint8_t  RSVD11[0x380 - 0x328];
    // 0x380 - 0x39F
    volatile uint32_t MC_APB4LPENSETR; // MCU APB4 LP enable set
    volatile uint32_t MC_APB4LPENCLRR; // MCU APB4 LP enable clear
    volatile uint32_t MC_APB5LPENSETR; // MCU APB5 LP enable set
    volatile uint32_t MC_APB5LPENCLRR; // MCU APB5 LP enable clear
    volatile uint32_t MC_AHB5LPENSETR; // MCU AHB5 LP enable set
    volatile uint32_t MC_AHB5LPENCLRR; // MCU AHB5 LP enable clear
    volatile uint32_t MC_AHB6LPENSETR; // MCU AHB6 LP enable set
    volatile uint32_t MC_AHB6LPENCLRR; // MCU AHB6 LP enable clear
    // 0x3A0 - 0x3FF
    volatile uint8_t  RSVD12[0x400 - 0x3A0];
    // 0x400 - 0x423
    volatile uint32_t BR_RSTSCLRR;      // BootROM reset status clear
    volatile uint32_t MP_GRSTCSETR;     // MPU global reset control set
    volatile uint32_t MP_RSTSCLRR;      // MPU reset status clear
    volatile uint32_t MP_IWDGFZSETR;    // MPU IWDG freeze set
    volatile uint32_t MP_IWDGFZCLRR;    // MPU IWDG freeze clear
    volatile uint32_t MP_CIER;          // MPU clock interrupt enable
    volatile uint32_t MP_CIFR;          // MPU clock interrupt flag
    volatile uint32_t PWRLPDLYCR;       // PWR low-power delay control
    volatile uint32_t MP_RSTSSETR;      // MPU reset status set
    // 0x424 - 0x7FF
    volatile uint8_t  RSVD13[0x800 - 0x424];
    // ===== Non-secure section (0x800-0xBFF) =====
    // 0x800 - 0x80F
    volatile uint32_t MCO1CFGR;      // MCO1 config
    volatile uint32_t MCO2CFGR;      // MCO2 config
    volatile uint32_t OCRDYR;        // oscillator clock ready
    volatile uint32_t DBGCFGR;       // debug config
    // 0x810 - 0x81F
    volatile uint8_t  RSVD14[0x820 - 0x810];
    // 0x820 - 0x83F
    volatile uint32_t RCK3SELR;      // PLL3 reference clock select
    volatile uint32_t RCK4SELR;      // PLL4 reference clock select
    volatile uint32_t TIMG1PRER;     // TIMG1 prescaler
    volatile uint32_t TIMG2PRER;     // TIMG2 prescaler
    volatile uint32_t MCUDIVR;       // MCU clock divider
    volatile uint32_t APB1DIVR;      // APB1 clock divider
    volatile uint32_t APB2DIVR;      // APB2 clock divider
    volatile uint32_t APB3DIVR;      // APB3 clock divider
    // 0x840 - 0x87F
    volatile uint8_t  RSVD15[0x880 - 0x840];
    // 0x880 - 0x8A7
    volatile uint32_t PLL3CR;        // PLL3 control
    volatile uint32_t PLL3CFGR1;     // PLL3 config 1
    volatile uint32_t PLL3CFGR2;     // PLL3 config 2
    volatile uint32_t PLL3FRACR;     // PLL3 fractional divider
    volatile uint32_t PLL3CSGR;      // PLL3 spread spectrum control
    volatile uint32_t PLL4CR;        // PLL4 control
    volatile uint32_t PLL4CFGR1;     // PLL4 config 1
    volatile uint32_t PLL4CFGR2;     // PLL4 config 2
    volatile uint32_t PLL4FRACR;     // PLL4 fractional divider
    volatile uint32_t PLL4CSGR;      // PLL4 spread spectrum control
    // 0x8A8 - 0x8BF
    volatile uint8_t  RSVD16[0x8C0 - 0x8A8];
    // 0x8C0 - 0x937
    volatile uint32_t I2C12CKSELR;   // I2C12 kernel clock select
    volatile uint32_t I2C35CKSELR;   // I2C35 kernel clock select
    volatile uint32_t SAI1CKSELR;    // SAI1 kernel clock select
    volatile uint32_t SAI2CKSELR;    // SAI2 kernel clock select
    volatile uint32_t SAI3CKSELR;    // SAI3 kernel clock select
    volatile uint32_t SAI4CKSELR;    // SAI4 kernel clock select
    volatile uint32_t SPI2S1CKSELR;  // SPI2S1 kernel clock select
    volatile uint32_t SPI2S23CKSELR; // SPI2S23 kernel clock select
    volatile uint32_t SPI45CKSELR;   // SPI45 kernel clock select
    volatile uint32_t UART6CKSELR;   // UART6 kernel clock select
    volatile uint32_t UART24CKSELR;  // UART24 kernel clock select
    volatile uint32_t UART35CKSELR;  // UART35 kernel clock select
    volatile uint32_t UART78CKSELR;  // UART78 kernel clock select
    volatile uint32_t SDMMC12CKSELR; // SDMMC12 kernel clock select
    volatile uint32_t SDMMC3CKSELR;  // SDMMC3 kernel clock select
    volatile uint32_t ETHCKSELR;     // Ethernet kernel clock select
    volatile uint32_t QSPICKSELR;    // QSPI kernel clock select
    volatile uint32_t FMCCKSELR;     // FMC kernel clock select
    // 0x908
    volatile uint8_t  RSVD17[0x90C - 0x908];
    volatile uint32_t FDCANCKSELR;   // FDCAN kernel clock select
    // 0x910
    volatile uint8_t  RSVD18[0x914 - 0x910];
    volatile uint32_t SPDIFCKSELR;   // SPDIF kernel clock select
    volatile uint32_t CECCKSELR;     // CEC kernel clock select
    volatile uint32_t USBCKSELR;     // USB kernel clock select
    volatile uint32_t RNG2CKSELR;    // RNG2 kernel clock select
    volatile uint32_t DSICKSELR;     // DSI kernel clock select
    volatile uint32_t ADCCKSELR;     // ADC kernel clock select
    volatile uint32_t LPTIM45CKSELR; // LPTIM45 kernel clock select
    volatile uint32_t LPTIM23CKSELR; // LPTIM23 kernel clock select
    volatile uint32_t LPTIM1CKSELR;  // LPTIM1 kernel clock select
    // 0x938 - 0x97F
    volatile uint8_t  RSVD19[0x980 - 0x938];
    // 0x980 - 0x9AF
    volatile uint32_t APB1RSTSETR;   // APB1 reset set
    volatile uint32_t APB1RSTCLRR;   // APB1 reset clear
    volatile uint32_t APB2RSTSETR;   // APB2 reset set
    volatile uint32_t APB2RSTCLRR;   // APB2 reset clear
    volatile uint32_t APB3RSTSETR;   // APB3 reset set
    volatile uint32_t APB3RSTCLRR;   // APB3 reset clear
    volatile uint32_t AHB2RSTSETR;   // AHB2 reset set
    volatile uint32_t AHB2RSTCLRR;   // AHB2 reset clear
    volatile uint32_t AHB3RSTSETR;   // AHB3 reset set
    volatile uint32_t AHB3RSTCLRR;   // AHB3 reset clear
    volatile uint32_t AHB4RSTSETR;   // AHB4 reset set
    volatile uint32_t AHB4RSTCLRR;   // AHB4 reset clear
    // 0x9B0 - 0x9FF
    volatile uint8_t  RSVD20[0xA00 - 0x9B0];
    // 0xA00 - 0xA3F
    volatile uint32_t MP_APB1ENSETR;    // APB1 enable set
    volatile uint32_t MP_APB1ENCLRR;    // APB1 enable clear
    volatile uint32_t MP_APB2ENSETR;    // APB2 enable set
    volatile uint32_t MP_APB2ENCLRR;    // APB2 enable clear
    volatile uint32_t MP_APB3ENSETR;    // APB3 enable set
    volatile uint32_t MP_APB3ENCLRR;    // APB3 enable clear
    volatile uint32_t MP_AHB2ENSETR;    // AHB2 enable set
    volatile uint32_t MP_AHB2ENCLRR;    // AHB2 enable clear
    volatile uint32_t MP_AHB3ENSETR;    // AHB3 enable set
    volatile uint32_t MP_AHB3ENCLRR;    // AHB3 enable clear
    volatile uint32_t MP_AHB4ENSETR;    // AHB4 enable set
    volatile uint32_t MP_AHB4ENCLRR;    // AHB4 enable clear
    // 0xA30 - 0xA37
    volatile uint8_t  RSVD21[0xA38 - 0xA30];
    volatile uint32_t MP_MLAHBENSETR;   // MLAHB enable set
    volatile uint32_t MP_MLAHBENCLRR;   // MLAHB enable clear
    // 0xA40 - 0xA7F
    volatile uint8_t  RSVD22[0xA80 - 0xA40];
    // 0xA80 - 0xABF
    volatile uint32_t MC_APB1ENSETR; // MCU APB1 enable set
    volatile uint32_t MC_APB1ENCLRR; // MCU APB1 enable clear
    volatile uint32_t MC_APB2ENSETR; // MCU APB2 enable set
    volatile uint32_t MC_APB2ENCLRR; // MCU APB2 enable clear
    volatile uint32_t MC_APB3ENSETR; // MCU APB3 enable set
    volatile uint32_t MC_APB3ENCLRR; // MCU APB3 enable clear
    volatile uint32_t MC_AHB2ENSETR; // MCU AHB2 enable set
    volatile uint32_t MC_AHB2ENCLRR; // MCU AHB2 enable clear
    volatile uint32_t MC_AHB3ENSETR; // MCU AHB3 enable set
    volatile uint32_t MC_AHB3ENCLRR; // MCU AHB3 enable clear
    volatile uint32_t MC_AHB4ENSETR; // MCU AHB4 enable set
    volatile uint32_t MC_AHB4ENCLRR; // MCU AHB4 enable clear
    volatile uint32_t MC_AXIMENSETR; // MCU AXI enable set
    volatile uint32_t MC_AXIMENCLRR; // MCU AXI enable clear
    volatile uint32_t MC_MLAHBENSETR;// MCU MLAHB enable set
    volatile uint32_t MC_MLAHBENCLRR;// MCU MLAHB enable clear
    // 0xAC0 - 0xAFF
    volatile uint8_t  RSVD23[0xB00 - 0xAC0];
    // 0xB00 - 0xB3F
    volatile uint32_t MP_APB1LPENSETR;  // APB1 LP enable set
    volatile uint32_t MP_APB1LPENCLRR;  // APB1 LP enable clear
    volatile uint32_t MP_APB2LPENSETR;  // APB2 LP enable set
    volatile uint32_t MP_APB2LPENCLRR;  // APB2 LP enable clear
    volatile uint32_t MP_APB3LPENSETR;  // APB3 LP enable set
    volatile uint32_t MP_APB3LPENCLRR;  // APB3 LP enable clear
    volatile uint32_t MP_AHB2LPENSETR;  // AHB2 LP enable set
    volatile uint32_t MP_AHB2LPENCLRR;  // AHB2 LP enable clear
    volatile uint32_t MP_AHB3LPENSETR;  // AHB3 LP enable set
    volatile uint32_t MP_AHB3LPENCLRR;  // AHB3 LP enable clear
    volatile uint32_t MP_AHB4LPENSETR;  // AHB4 LP enable set
    volatile uint32_t MP_AHB4LPENCLRR;  // AHB4 LP enable clear
    volatile uint32_t MP_AXIMLPENSETR;  // AXIM LP enable set
    volatile uint32_t MP_AXIMLPENCLRR;  // AXIM LP enable clear
    volatile uint32_t MP_MLAHBLPENSETR; // MLAHB LP enable set
    volatile uint32_t MP_MLAHBLPENCLRR; // MLAHB LP enable clear
    // 0xB40 - 0xB7F
    volatile uint8_t  RSVD24[0xB80 - 0xB40];
    // 0xB80 - 0xBBF
    volatile uint32_t MC_APB1LPENSETR;  // MCU APB1 LP enable set
    volatile uint32_t MC_APB1LPENCLRR;  // MCU APB1 LP enable clear
    volatile uint32_t MC_APB2LPENSETR;  // MCU APB2 LP enable set
    volatile uint32_t MC_APB2LPENCLRR;  // MCU APB2 LP enable clear
    volatile uint32_t MC_APB3LPENSETR;  // MCU APB3 LP enable set
    volatile uint32_t MC_APB3LPENCLRR;  // MCU APB3 LP enable clear
    volatile uint32_t MC_AHB2LPENSETR;  // MCU AHB2 LP enable set
    volatile uint32_t MC_AHB2LPENCLRR;  // MCU AHB2 LP enable clear
    volatile uint32_t MC_AHB3LPENSETR;  // MCU AHB3 LP enable set
    volatile uint32_t MC_AHB3LPENCLRR;  // MCU AHB3 LP enable clear
    volatile uint32_t MC_AHB4LPENSETR;  // MCU AHB4 LP enable set
    volatile uint32_t MC_AHB4LPENCLRR;  // MCU AHB4 LP enable clear
    volatile uint32_t MC_AXIMLPENSETR;  // MCU AXIM LP enable set
    volatile uint32_t MC_AXIMLPENCLRR;  // MCU AXIM LP enable clear
    volatile uint32_t MC_MLAHBLPENSETR; // MCU MLAHB LP enable set
    volatile uint32_t MC_MLAHBLPENCLRR; // MCU MLAHB LP enable clear
    // 0xBC0 - 0xBFF
    volatile uint8_t  RSVD25[0xC00 - 0xBC0];
    // 0xC00 - 0xC1B
    volatile uint32_t MC_RSTSCLRR;   // MCU reset status clear
    volatile uint8_t  RSVD26[0xC14 - 0xC04];
    volatile uint32_t MC_CIER;       // MCU clock interrupt enable
    volatile uint32_t MC_CIFR;       // MCU clock interrupt flag
    // 0xC1C - 0xFF3
    volatile uint8_t  RSVD27[0xFF4 - 0xC1C];
    // 0xFF4 - 0xFFF
    volatile uint32_t VERR;          // version register
    volatile uint32_t IDR;           // ID register
    volatile uint32_t SIDR;          // size ID register
} RccRegs_t;

extern volatile RccRegs_t *const RCC;

#endif /* STM32MP1XX_RCC_H_ */
