
#ifndef __STM32MP1XX_SPI_H_
#define __STM32MP1XX_SPI_H_

#include <stdint.h>

typedef struct {
    uint32_t CR1;
    uint32_t CR2;
    uint32_t CFG1;
    uint32_t CFG2;
    uint32_t IER;
    uint32_t SR;
    uint32_t IFCR;
    uint32_t RSVD0;
    uint32_t TXDR;
    uint8_t  RSVD1[0X30 - 0X20 - 4];
    uint32_t RXDR;
    uint8_t  RSVD2[0X40 - 0X30 - 4];
    uint32_t CRCPOLY;
    uint32_t TXCRC;
    uint32_t RXCRC;
    uint32_t UDRDR;
    uint32_t I2SCFGR;
    uint8_t  RSVD3[0X3F0 - 0X50 -4];
    uint32_t HWCFGR;
    uint8_t  VERR;
    uint8_t  IPIDR;
    uint8_t  SIDR;
} SpiRegs_t;


typedef enum {
    SPI_CLK_IDLE0_NO_DELAY,
    SPI_CLK_IDLE0_DELAY,
    SPI_CLK_IDLE1_NO_DELAY,
    SPI_CLK_IDLE1_DELAY
} SpiClkCfg_t;


typedef enum {
    SPI_SHIFT_LSB_FIRST,
    SPI_SHIFT_MSB_FIRST
} SpiShift_t;


typedef enum {
    SPI_COMM_FULL_DUPLEX,
    SPI_COMM_SIMP_TX,
    SPI_COMM_SIMP_RX,
    SPI_COMM_HALF_DUPLEX
} SpiCommMode_t;


typedef enum {
    SPI_PROTOCOL_MOTOROLA,
    SPI_PROTOCOL_TI
} SpiProtocol_t;


typedef enum {
    SPI_MASTER,
    SPI_SLAVE
} SpiMstSlv_t;

typedef enum {
    SPI_SSM_HW,    // SS 硬件管理，内部 NSS 来自外部引脚
    SPI_SSM_SW     // SS 软件管理，内部 NSS 来自 SSI 位
} SpiSsMgmt_t;


typedef struct {
    uint32_t        spi_baud_reate_div;
    uint8_t         spi_word_len;
    SpiClkCfg_t     spi_clk_cfg;
    SpiShift_t      spi_shift;
    SpiCommMode_t   spi_comm_mode;
    SpiProtocol_t   spi_protocol;
    SpiMstSlv_t     spi_master;
    SpiSsMgmt_t     spi_ss_mgmt;
} SpiCfg_t;


extern volatile SpiRegs_t *const SPI6;
extern volatile SpiRegs_t *const SPI5;
extern volatile SpiRegs_t *const SPI4;
extern volatile SpiRegs_t *const SPI1;
extern volatile SpiRegs_t *const SPI3;
extern volatile SpiRegs_t *const SPI2;


extern void SpiCfg(volatile SpiRegs_t *const spi_reg, const SpiCfg_t *const cfg);
extern void SpiTxRx(volatile SpiRegs_t *const spi_reg, const uint8_t *wr_buf, uint8_t *rd_buf, const uint32_t len);


#endif
