// =============================================================================
/*!
 * @file       flashspi.h
 *
 * This file contains definitions for spi flash support
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#ifndef FLASHSPI_H
#define FLASHSPI_H
#include <stdint.h>
#define FLASHSPI_CMD_PP         0x02  /*!< Page Program */
#define FLASHSPI_CMD_READ       0x03  /*!< Read Page Register instruction */
#define FLASHSPI_CMD_RDSR       0x05  /*!< Read Status Register instruction */
#define FLASHSPI_CMD_WREN       0x06  /*!< Write enable instruction */
#define FLASHSPI_CMD_SE         0x20  /*!< Sector Erase instruction */
#define FLASHSPI_CMD_REMS       0x90  /*!< Read Electronic Manufacturer Signature */
#define FLASHSPI_CMD_RDID       0x9F  /*!< Read ID (JEDEC Manufacturer ID and JEDEC CFI) */
#define FLASHSPI_CMD_CE         0xC7  /*!< Chip Erase */
#define FLASHSPI_SR_BSY         0x01
typedef enum {
    FLASHSPI_OK = 0,            /* (0) Success */
    FLASHSPI_ERROR,             /* (1) Generic error */
    FLASHSPI_ERROR_ID,          /* (2) ID mismatch */
    FLASHSPI_ERROR_TIMEOUT,     /* (3) Operation timedout */
    FLASHSPI_ERROR_NOMEM,       /* (4) No memory available */
    FLASHSPI_ERROR_BP,          /* (5) Block protected */
    FLASHSPI_ERROR_PP           /* (6) Page program */
}flashspi_res_t;
typedef struct {
    const char *name;
    const uint32_t size;
    const uint32_t sectorsize;
    const uint32_t pagesize;
    const uint32_t mid;             // Manufacturer/id
    flashspi_res_t (*init)(void);
    flashspi_res_t (*wait_ready)(void);
}flashspi_t;
flashspi_res_t flashspi_init(void);
flashspi_res_t flashspi_write(const uint8_t* pbuffer, uint32_t writeaddr, uint16_t numbytetowrite);
flashspi_res_t flashspi_read(uint8_t* pbuffer, uint32_t readaddr, uint16_t numbytetoread);
void flashspi_write_enable(void);
const flashspi_t *flashspi_get_device(void);
uint32_t flashspi_get_size(void);
uint32_t flashspi_get_sector_size(void);
uint32_t flashspi_get_page_size(void);
const char* flashspi_get_name (void);
flashspi_res_t flashspi_erase(void);
uint32_t flashspi_read_id_jedec(void);
uint8_t flashspi_read_status(void);
flashspi_res_t flashspi_wait_ready(uint32_t timeout);
#endif