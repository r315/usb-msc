// =============================================================================
/*!
 * @file       flashspi_winbond.c
 *
 * This file contains flash spi device specific commands implementation
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#include <stdio.h>
#include "flashspi.h"
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
static flashspi_res_t winbond_wait_ready(void)
{
   return flashspi_wait_ready (WINBOND_tCE);
}
static flashspi_res_t w25q_init(void)
{
    return FLASHSPI_OK;
}
static flashspi_res_t w25x32_init(void)
{
    return (flashspi_read_id_jedec() == W25X32_DEV_ID) ? FLASHSPI_OK: FLASHSPI_ERROR_ID;
}
const flashspi_t w25x32 =
{
    .name = "W25X32",
    .mid = W25X32_M_ID,
    .size = W25X32_SIZE,
    .pagesize = W25X32_PAGE_SIZE,
    .sectorsize = W25X32_SECTOR_SIZE,
    .init = w25x32_init,
    .wait_ready = winbond_wait_ready
};
const flashspi_t w25q64 =
{
    .name = "W25Q64",
    .mid = W25Q64_M_ID,
    .size = 0x00800000, /*8m byte*/
    .pagesize = 256,
    .sectorsize = 0x1000,
    .init = w25q_init,
    .wait_ready = winbond_wait_ready
};
const flashspi_t w25q128 =
{
    .name = "W25Q128",
    .mid = W25Q128_M_ID,
    .size = 0x01000000, /*16m byte*/
    .pagesize = 256,
    .sectorsize = 0x1000,
    .init = w25q_init,
    .wait_ready = winbond_wait_ready
};
