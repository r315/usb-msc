
// =============================================================================
/*!
 * @file       at32_spiflash.c
 *
 * This file contains MAC definitions for spi flash memories
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#ifndef __AT_SPI_FLASH_H
#define __AT_SPI_FLASH_H
#include <stdint.h>
void spiflash_cs(uint8_t state);
void spiflash_init(void);
void spiflash_sendbyte (uint8_t byte);
uint8_t spiflash_receivebyte (void);
uint8_t spiflash_xchbyte (uint8_t byte);
uint32_t spiflash_read (uint8_t *pbuffer, uint32_t len);
uint32_t spiflash_write (const uint8_t *pbuffer, uint32_t len);
#endif
