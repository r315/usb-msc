#include <stdio.h>
#include "flashspi.h"
#include "at32_spiflash.h"
#include "board.h"

#define W25Q64_M_ID             0xEF16
#define W25Q64_DEV_ID           0xEF4017  // Single

#define W25Q128_M_ID            0xEF17

#define W25X32_M_ID             0xEF15
#define W25X32_DEV_ID           0xEF3016   // Manufacturer and device id

#define W25X32_PAGE_SIZE        256
#define W25X32_SECTOR_SIZE      0x1000     /* 4kB */
#define W25X32_SIZE             0x00400000 /* 4MB */

#define WINBOND_tCE             100000UL    // 100s

#define W25X32_CMD_WRSR         0x01
#define W25X32_CMD_WRITE_DIS    0x04
#define W25X32_CMD_READ_FAST    0x0B
#define W25X32_CMD_READ_FAST_DUAL  0x3B
#define W25X32_CMD_BLOCK_ERASE  0xD8
#define W25X32_CMD_CHIP_ERASE   0xC7
#define W25X32_CMD_POWER_DOWN   0xB9
#define W25X32_CMD_POWER_UP_ID  0xAB

#define WINDOND_SR_BUSY         (1<<0)

static uint32_t winbond_read_id()
{
    uint32_t device_id = 0;
    spiflash_cs (CS_LOW);
    spiflash_sendbyte (FLASH_SPI_CMD_RDID);
    device_id |= spiflash_readbyte () << 16; // M7-M0
    device_id |= spiflash_readbyte () << 8;  // ID15-ID8
    device_id |= spiflash_readbyte () << 0;  // ID7-ID0
    spiflash_cs (CS_HIGH);
    return device_id;
}

static uint8_t winbond_rdsr(uint8_t sr)
{
    uint8_t sr_data;

    spiflash_cs (CS_LOW);
    spiflash_sendbyte (sr);
    sr_data = spiflash_sendbyte (FLASH_DUMMY_BYTE);
    spiflash_cs (CS_HIGH);

    return sr_data;
}

static flashspi_res_t w25q_init(void)
{
    return FLASHSPI_OK;
}

static flashspi_res_t w25x32_init(void)
{
    return (winbond_read_id() == W25X32_DEV_ID) ? FLASHSPI_OK: FLASHSPI_ERROR_ID;
}

static flashspi_res_t winbond_erase(void)
{
    uint32_t time;
    flashspi_writeenable();

    spiflash_cs (CS_LOW);
    spiflash_sendbyte (W25X32_CMD_CHIP_ERASE);
    spiflash_cs (CS_HIGH);
    time = get_tick();

    do{
        delay_ms(1);
        uint8_t sr = winbond_rdsr(FLASH_SPI_CMD_RDSR);
        if(!(sr & WINDOND_SR_BUSY)){
            return FLASHSPI_OK;
        }
    }while(get_tick() - time < WINBOND_tCE);

    return FLASHSPI_ERROR_TIMEOUT;
}

const flashspi_t w25x32 =
{
    .name = "W25X32",
    .mid = W25X32_M_ID,
    .size = W25X32_SIZE,
    .pagesize = W25X32_PAGE_SIZE,
    .sectorsize = W25X32_SECTOR_SIZE,
    .init = w25x32_init,
    .erase = winbond_erase
};

const flashspi_t w25q64 =
{
    .name = "W25Q64",
    .mid = W25Q64_M_ID,
    .size = 0x00800000, /*8m byte*/
    .pagesize = 256,
    .sectorsize = 0x1000,
    .init = w25q_init,
    .erase = winbond_erase
};

const flashspi_t w25q128 =
{
    .name = "W25Q128",
    .mid = W25Q128_M_ID,
    .size = 0x01000000, /*16m byte*/
    .pagesize = 256,
    .sectorsize = 0x1000,
    .init = w25q_init,
    .erase = winbond_erase
};


