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
    FLASHSPI_OK = 0,            /* (0) Success */
    FLASHSPI_ERROR,             /* (1) Generic error */
    FLASHSPI_ERROR_ID,          /* (2) ID mismatch */
    FLASHSPI_ERROR_TIMEOUT,     /* (3) Operation timedout */
}flashspi_res_t;

typedef struct {
    const char *name;
    const uint32_t size;
    const uint32_t sectorsize;
    const uint32_t pagesize;
    const uint32_t mid;             // Manufacturer/id
    flashspi_res_t (*init)(void);
    flashspi_res_t (*erase)(void);
}flashspi_t;

flashspi_res_t flashspi_init(void);
flashspi_res_t flashspi_write(const uint8_t* pbuffer, uint32_t writeaddr, uint16_t numbytetowrite);
flashspi_res_t flashspi_read(uint8_t* pbuffer, uint32_t readaddr, uint16_t numbytetoread);
void flashspi_writeenable(void);
uint32_t flashspi_getsize(void);
uint32_t flashspi_getsectorsize(void);
uint32_t flashspi_getpagesize(void);
const char* flashspi_getname (void);
const flashspi_t *flashspi_get(void);
flashspi_res_t flashspi_erase(void);
#endif