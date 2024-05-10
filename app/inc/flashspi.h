#ifndef FLASHSPI_H
#define FLASHSPI_H

#include <stdint.h>

#define FLASH_SPI_CMD_PP        0x02  /*!< Page Program */
#define FLASH_SPI_CMD_READ      0x03  /*!< Read Page Register instruction */
#define FLASH_SPI_CMD_RDSR	    0x05  /*!< Read Status Register instruction */
#define FLASH_SPI_CMD_WREN      0x06  /*!< Write enable instruction */
#define FLASH_SPI_CMD_SE        0x20  /*!< Sector Erase instruction */
#define FLASH_SPI_CMD_REMS      0x90  /*!< Read Electronic Manufacturer Signature */
#define FLASH_SPI_CMD_RDID      0x9F  /*!< Read ID (JEDEC Manufacturer ID and JEDEC CFI) */

typedef enum {
    FLASHSPI_OK = 0,
    FLASHSPI_ERROR
}flashspi_error_t;

typedef struct {
    const uint32_t size;
    const uint32_t sectorsize;
    const uint32_t pagesize;
    const uint32_t id;
    flashspi_error_t (*init)(void);
}flashspi_t;

flashspi_error_t flash_spi_init(void);
flashspi_error_t flash_spi_write(const uint8_t* pbuffer, uint32_t writeaddr, uint16_t numbytetowrite);
flashspi_error_t flash_spi_read(uint8_t* pbuffer, uint32_t readaddr, uint16_t numbytetoread);
uint32_t flash_spi_getsize(void);
uint32_t flash_spi_getsectorsize(void);
uint32_t flash_spi_getpagesize(void);
void flash_spi_writeenable(void);
#endif