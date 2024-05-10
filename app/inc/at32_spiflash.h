#ifndef __AT_SPI_FLASH_H
#define __AT_SPI_FLASH_H

#include <stdint.h>

#ifndef AT32_SPI2
#define FLASH_SPI                   SPI1
#define FLASH_SPI_RESET             CRM_SPI1_PERIPH_RESET

/*SCK Pin*/
#define FLASH_SPI_SCK_PIN           GPIO_PINS_5
#define FLASH_SPI_SCK_GPIO          GPIOA

/*MISO Pin*/
#define FLASH_SPI_MISO_PIN          GPIO_PINS_6
#define FLASH_SPI_MISO_GPIO         GPIOA

/*MOSI Pin*/
#define FLASH_SPI_MOSI_PIN          GPIO_PINS_7
#define FLASH_SPI_MOSI_GPIO         GPIOA

/*CS Pin*/
#define FLASH_SPI_CS_PIN            GPIO_PINS_4
#define FLASH_SPI_CS_GPIO           GPIOA
#else
#define FLASH_SPI                   SPI2
#define FLASH_SPI_RESET             CRM_SPI2_PERIPH_RESET
#define FLASH_SPI_SCK_PIN           GPIO_PINS_13
#define FLASH_SPI_SCK_GPIO          GPIOB
#define FLASH_SPI_MISO_PIN          GPIO_PINS_14
#define FLASH_SPI_MISO_GPIO         GPIOB
#define FLASH_SPI_MOSI_PIN          GPIO_PINS_15
#define FLASH_SPI_MOSI_GPIO         GPIOB
#define FLASH_SPI_CS_PIN            GPIO_PINS_12
#define FLASH_SPI_CS_GPIO           GPIOB
#endif

#define CS_LOW                      0
#define CS_HIGH                     1
#define FLASH_DUMMY_BYTE            0xff


void spiflash_cs(uint8_t state);
void spiflash_init(void);
uint8_t spiflash_sendbyte (uint8_t byte);
uint8_t spiflash_readbyte (void);

#endif


