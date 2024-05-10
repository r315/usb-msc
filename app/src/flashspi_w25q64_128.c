#include "flashspi.h"

#define W25Q64_ID   0xef16
#define W25Q128_ID  0xef17

static flashspi_error_t w25q_init(void)
{
    return FLASHSPI_OK;
}

const flashspi_t w25q64 = 
{
    .id = W25Q64_ID,
    .size = 0x00800000, /*8m byte*/
    .pagesize = 256,
    .sectorsize = 0x1000,
    .init = w25q_init
};

const flashspi_t w25q128 = 
{
    .id = W25Q128_ID,
    .size = 0x01000000, /*16m byte*/
    .pagesize = 256,
    .sectorsize = 0x1000,
    .init = w25q_init
};


