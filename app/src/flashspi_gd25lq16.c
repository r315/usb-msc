#include "flashspi.h"
#include "at32_spiflash.h"
#include <stdio.h>

#define GD25LQ16_M_ID           0xC814
#define GD25LQ16_PAGE_SIZE      256
#define GD25LQ16_SECTOR_SIZE    0x1000
#define GD25LQ16_SIZE           0x00200000 /* 2MB */

//#define GD25LQ16_SR_VOLATILE

#define GD25LQ16_CMD_WRSR       0x01
#define GD25LQ16_CMD_RDSR1      0x35
#define GD25LQ16_CMD_VSRWREN    0x50    // <! Volatile status register write >

#define GD25LQ16_SR_EX_SUS1     (1<<15) // <! Set by erase suspend command >
#define GD25LQ16_SR_EX_CMP      (1<<14) // <! Change protected block >
#define GD25LQ16_SR_EX_LB3      (1<<13) // <! OTP lock bit 3 >
#define GD25LQ16_SR_EX_LB2      (1<<12) // <! OTP lock bit 2 >
#define GD25LQ16_SR_EX_LB1      (1<<11) // <! OTP lock bit 1 >
#define GD25LQ16_SR_EX_SUS2     (1<<10) // <! Set by program suspend command >
#define GD25LQ16_SR_EX_QE       (1<<9)  // <! Quad enable >
#define GD25LQ16_SR_EX_SRP1     (1<<8)  // <! SR protect bit 1 >
#define GD25LQ16_SR_EX_SRP0     (1<<7)  // <! SR protect bit 0 >
#define GD25LQ16_SR_EX_BP4      (1<<6)  // <! Block protect bit 4 >
#define GD25LQ16_SR_EX_BP3      (1<<5)  // <! Block protect bit 3 >
#define GD25LQ16_SR_EX_BP2      (1<<4)  // <! Block protect bit 2 >
#define GD25LQ16_SR_EX_BP1      (1<<3)  // <! Block protect bit 1 >
#define GD25LQ16_SR_EX_BP0      (1<<2)  // <! Block protect bit 0 >
#define GD25LQ16_SR_EX_WEL      (1<<1)  // <! Write enable latch >
#define GD25LQ16_SR_EX_WIP      (1<<0)  // <! Write in progress >

#ifdef GD25LQ16_SR_VOLATILE
static void gd25lq16_cmd_vsrwren (void)
{
   spiflash_cs (CS_LOW);

   spiflash_sendbyte (GD25LQ16_CMD_VSRWREN);
   
   spiflash_cs (CS_HIGH);
}
#endif

static void gd25lq16_cmd_wrsr(uint16_t sr_data)
{
    #ifdef GD25LQ16_SR_VOLATILE
    gd25lq16_cmd_vsrwren();
    #else
    flashspi_writeenable();
    #endif

    spiflash_cs (CS_LOW);

    spiflash_sendbyte (GD25LQ16_CMD_WRSR);

    spiflash_sendbyte (sr_data);

    spiflash_sendbyte (sr_data >> 8);

    spiflash_cs (CS_HIGH);
}

static uint8_t gd25lq16_cmd_rdsr(uint8_t sr)
{
    uint8_t sr_data;

    spiflash_cs (CS_LOW);

    spiflash_sendbyte (sr);

    sr_data = spiflash_sendbyte (FLASH_DUMMY_BYTE);

    spiflash_cs (CS_HIGH);

    return sr_data;
}

static uint16_t gd25lq16_rdsr_ex(void)
{
    uint16_t sr;

    sr = gd25lq16_cmd_rdsr(GD25LQ16_CMD_RDSR1);

    sr <<= 8;

    sr |= gd25lq16_cmd_rdsr(FLASH_SPI_CMD_RDSR);

    return sr;
}

static flashspi_res_t gd25lq16_init(void)
{
    uint16_t sr;
    
    sr = gd25lq16_rdsr_ex();

    if(sr & GD25LQ16_SR_EX_QE)
    {
        gd25lq16_cmd_wrsr(sr & ~GD25LQ16_SR_EX_QE);
    }

    return FLASHSPI_OK;
}

static flashspi_res_t gd25lq16_erase(void)
{
    return FLASHSPI_ERROR;
}

const flashspi_t gd25lq16 = 
{
    .name = "GD25LQ16",
    .mid = GD25LQ16_M_ID,
    .size = GD25LQ16_SIZE,
    .pagesize = GD25LQ16_PAGE_SIZE,
    .sectorsize = GD25LQ16_SECTOR_SIZE,
    .init = gd25lq16_init,
    .erase = gd25lq16_erase
};