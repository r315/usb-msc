// =============================================================================
/*!
 * @file       flashspi_gigadevice.c
 *
 * This file contains flash spi generic commands implementation
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#include <stddef.h>
#include "flashspi.h"
#include "board.h"

#define FLASH_DEVICES_COUNT sizeof (flashspi_devices) / sizeof (flashspi_t *)
#define MAX_SECTOR_SIZE     0x2000 /* 8kB */

#define PRINT_FLASHSPI_DBG 0

#if PRINT_FLASHSPI_DBG && ENABLE_DBG_LOG
   #define PRINT_FLASHSPI(fmt, ...) dbg_log("[FLASHSPI] "fmt, ##__VA_ARGS__)
#else
   #define PRINT_FLASHSPI(...)
#endif

static uint32_t flashspi_read_id (void);
static void flashspi_sector_erase (uint32_t sectoraddr);
static void flashspi_write_page (const uint8_t *pbuffer, uint32_t writeaddr, uint16_t numbytetowrite);
static void flashspi_write_sector (const uint8_t *pbuffer, uint32_t writeaddr, uint16_t numbytetowrite);

extern const flashspi_t gd25lq16;
extern const flashspi_t w25q64;
extern const flashspi_t w25q128;
extern const flashspi_t w25x32;
extern const flashspi_t at25sf321b;

static const flashspi_t *spiflash;
static const flashspi_t *flashspi_devices[] = {
    &gd25lq16,
    &w25q64,
    &w25q128,
    &w25x32,
    &at25sf321b
};

static uint8_t gtmpbuff[MAX_SECTOR_SIZE];

/**
 * @brief Calls SOC low level spi initialization and select flash
 *        according read id.
 * @param  None
 * @retval None
 */
flashspi_res_t flashspi_init (void)
{
   spiflash_init ();

   uint32_t device_id = flashspi_read_id ();

   spiflash = NULL;

   for (uint8_t i = 0; i < FLASH_DEVICES_COUNT; i++)
   {
      if (flashspi_devices[i]->mid == device_id)
      {
         spiflash = flashspi_devices[i];
         break;
      }
   }

   if(spiflash){
      return spiflash->init();
   }

   return FLASHSPI_ERROR;
}

/**
 * @brief Waits for busy bit reset on status register
 *        with timeout.
 *
 * @param timeout [in] maximum time waiting for busy bit
 *
 * @retval FLASHSPI_OK on success, FLASHSPI_ERROR_TIMEOUT otherwise
 */
flashspi_res_t flashspi_wait_ready(uint32_t timeout)
{
   uint32_t time;
   flashspi_res_t res = FLASHSPI_ERROR_TIMEOUT;

   time = GetTick();

   spiflash_cs (CS_LOW);

   spiflash_sendbyte (FLASHSPI_CMD_RDSR);

   do{
      uint8_t sr_data = spiflash_receivebyte ();
      if(!(sr_data & FLASHSPI_SR_BSY)){
         res = FLASHSPI_OK;
         break;
      }
   }while(GetTick() - time < timeout);

   spiflash_cs (CS_HIGH);

   return res;
}

/**
 * @brief  Reads a block of data from the FLASH.
 * @param  pBuffer: pointer to the buffer that receives
 * the data read from the FLASH.
 * @param  ReadAddr: FLASH's internal address to read from.
 * @param  NumByteToRead: number of bytes to read from the FLASH.
 * @retval None
 */
flashspi_res_t flashspi_read (uint8_t *pbuffer, uint32_t readaddr,
                     uint16_t numbytetoread)
{
   flashspi_res_t res = FLASHSPI_OK;
   //PRINT_FLASHSPI("read %u bytes from addr 0x%x\n", numbytetoread, readaddr);
   /*!< select the flash: chip select low */
   spiflash_cs (CS_LOW);

   /*!< send "read from memory " instruction */
   spiflash_sendbyte (FLASHSPI_CMD_READ);
   /*!< send readaddr high nibble address byte to read from */
   spiflash_sendbyte ((readaddr & 0xff0000) >> 16);
   /*!< send readaddr medium nibble address byte to read from */
   spiflash_sendbyte ((readaddr & 0xff00) >> 8);
   /*!< send readaddr low nibble address byte to read from */
   spiflash_sendbyte (readaddr & 0xff);

   res = spiflash_read(pbuffer, numbytetoread) == numbytetoread ?
                        FLASHSPI_OK : FLASHSPI_ERROR;

   /*!< deselect the flash: chip select high */
   spiflash_cs (CS_HIGH);

   return res;
}

/**
 * @brief  Writes block of data to the FLASH. In this function, the number of
 *         WRITE cycles are reduced, using Page WRITE sequence.
 * @param  pBuffer: pointer to the buffer  containing the data to be written
 *         to the FLASH.
 * @param  WriteAddr: FLASH's internal address to write to.
 * @param  NumByteToWrite: number of bytes to write to the FLASH.
 * @retval None
 */
flashspi_res_t flashspi_write (const uint8_t *pbuffer, uint32_t writeaddr,
                      uint16_t numbytetowrite)
{
   uint32_t cnt = 0;
   uint32_t sectornum, sectoroffset, sectorremain;
   //PRINT_FLASHSPI("write %d bytes to addr 0x%x\n", numbytetowrite, writeaddr);

   if(spiflash->sectorsize > sizeof(gtmpbuff)){
      return FLASHSPI_ERROR_NOMEM;
   }

   sectornum    = writeaddr / spiflash->sectorsize;
   sectoroffset = writeaddr % spiflash->sectorsize;
   sectorremain = spiflash->sectorsize - sectoroffset;

   if (numbytetowrite <= sectorremain)
   {
      sectorremain = numbytetowrite;
   }

   while (1)
   {
      /*read all sector data*/
      flashspi_read (gtmpbuff, sectornum * spiflash->sectorsize,
                      spiflash->sectorsize);

      /*check data is 0xff ?*/
      for (cnt = 0; cnt < sectorremain; cnt++)
      {
         if (gtmpbuff[sectoroffset + cnt] != 0xff)
            break;
      }

      if (cnt < sectorremain) /*need sector erase*/
      {
         flashspi_sector_erase (sectornum * spiflash->sectorsize);

         /* Replace sector data with new data at the desired adddress */
         for (cnt = 0; cnt < sectorremain; cnt++)
            gtmpbuff[cnt + sectoroffset] = pbuffer[cnt];

         /* rewrite sector */
         flashspi_write_sector (gtmpbuff, sectornum * spiflash->sectorsize,
                                spiflash->sectorsize);
      }
      else
      {
         /* Sector is empty, simply write new data */
         flashspi_write_sector (pbuffer, writeaddr, sectorremain);
      }

      if (sectorremain == numbytetowrite){
         break;
      }
      else
      {
         sectornum++;
         sectoroffset = 0;
         pbuffer += sectorremain;
         writeaddr += sectorremain;
         numbytetowrite -= sectorremain;
         if (numbytetowrite > spiflash->sectorsize)
         {
            sectorremain = spiflash->sectorsize;
         }
         else
         {
            sectorremain = numbytetowrite;
         }
      }
   }

   return FLASHSPI_OK;
}

/**
 * @brief Chip erase, erases all data on
 *        device
 *
 * @return command result
 */
flashspi_res_t flashspi_erase(void)
{
    if(!spiflash){
        return FLASHSPI_ERROR;
    }

    flashspi_write_enable();

    spiflash_cs (CS_LOW);
    spiflash_sendbyte (FLASHSPI_CMD_CE);
    spiflash_cs (CS_HIGH);

    return spiflash->wait_ready();
}

/**
 * @brief  Reads generic FLASH identification.
 * @param  None
 * @retval FLASH identification
 */
static uint32_t flashspi_read_id (void)
{
   uint16_t flash_id = 0;

   spiflash_cs (CS_LOW);
   spiflash_sendbyte (FLASHSPI_CMD_REMS);
   spiflash_sendbyte (0x00);
   spiflash_sendbyte (0x00);
   spiflash_sendbyte (0x00);
   flash_id |= spiflash_receivebyte () << 8;
   flash_id |= spiflash_receivebyte ();
   spiflash_cs (CS_HIGH);

   return flash_id;
}

/**
 * @brief  Reads JEDEC manufacturer and device
 *         information
 * @param  None
 * @retval FLASH identification
 */
uint32_t flashspi_read_id_jedec()
{
    uint32_t device_id = 0;

    spiflash_cs (CS_LOW);
    spiflash_sendbyte (FLASHSPI_CMD_RDID);
    device_id |= spiflash_receivebyte () << 16; // M7-M0
    device_id |= spiflash_receivebyte () << 8;  // ID15-ID8
    device_id |= spiflash_receivebyte () << 0;  // ID7-ID0
    spiflash_cs (CS_HIGH);

    return device_id;
}

/**
 * @brief  Generic status register read
 * @param  None
 * @retval status register value
 */
uint8_t flashspi_read_status(void)
{
    uint8_t sr_data;

    spiflash_cs (CS_LOW);
    spiflash_sendbyte (FLASHSPI_CMD_RDSR);
    sr_data = spiflash_receivebyte ();
    spiflash_cs (CS_HIGH);

    return sr_data;
}

/**
 * @brief  Get current active spi flash device.
 * @param  None
 * @retval
 */
const flashspi_t *flashspi_get_device (void)
{
   return spiflash;
}

/**
 * @brief  Get FLASH SIZE.
 * @param  None
 * @retval FLASH identification
 */
uint32_t flashspi_get_size (void)
{
   return (spiflash) ? spiflash->size : 0;
}

/**
 * @brief  Get FLASH Sector SIZE.
 * @param  None
 * @retval FLASH identification
 */
uint32_t flashspi_get_sector_size (void)
{
   return (spiflash) ? spiflash->sectorsize : 0;
}

/**
 * @brief  Get FLASH Page SIZE.
 * @param  None
 * @retval FLASH identification
 */
uint32_t flashspi_get_page_size (void)
{
   return (spiflash) ? spiflash->pagesize : 0;
}

/**
 * @brief  Get FLASH Page SIZE.
 * @param  None
 * @retval FLASH identification
 */
const char* flashspi_get_name (void)
{
   return (spiflash) ? spiflash->name : "";
}

/**
 * @brief  Writes block of data to the FLASH. In this function, the number of
 *         WRITE cycles are reduced, using Page WRITE sequence.
 * @param  pBuffer: Pointer to sector data to be written
 * @param  WriteAddr: FLASH's internal address to write to.
 * @param  NumByteToWrite: number of bytes to write to the FLASH.
 * @retval None
 */
static void flashspi_write_sector (const uint8_t *pbuffer, uint32_t writeaddr,
                            uint16_t numbytetowrite)
{
   uint16_t Size;
   //PRINT_FLASHSPI("sector write 0x%x, %u bytes\n", writeaddr, numbytetowrite);
   Size = spiflash->pagesize - (writeaddr % spiflash->pagesize);

   if (numbytetowrite <= Size)
   {
      Size = numbytetowrite;
   }

   while (1)
   {
      flashspi_write_page (pbuffer, writeaddr, Size);

      if (Size == numbytetowrite)
      {
         return;
      }

      pbuffer += Size;
      writeaddr += Size;
      numbytetowrite -= Size;

      if (numbytetowrite > spiflash->pagesize)
      {
         Size = spiflash->pagesize;
      }
      else
      {
         Size = numbytetowrite;
      }
   }
}

/**
 * @brief  Writes more than one byte to the FLASH with a single WRITE cycle
 *         (Page WRITE sequence).
 * @note   The number of byte can't exceed the FLASH page size.
 * @param  pBuffer: pointer to the buffer  containing the data to be written
 *         to the FLASH.
 * @param  WriteAddr: FLASH's internal address to write to.
 * @param  NumByteToWrite: number of bytes to write to the FLASH, must be equal
 *         or less than "sFLASH_PAGESIZE" value.
 * @retval None
 */
static void flashspi_write_page (const uint8_t *pbuffer, uint32_t writeaddr,
                                 uint16_t numbytetowrite)
{
   //PRINT_FLASHSPI("page write 0x%x %u bytes\n", writeaddr, numbytetowrite);
   /*!< enable the write access to the flash */
   flashspi_write_enable ();

   /*!< select the flash: chip select low */
   spiflash_cs (CS_LOW);
   /*!< send "write to memory " instruction */
   spiflash_sendbyte (FLASHSPI_CMD_PP);
   /*!< send writeaddr high nibble address byte to write to */
   spiflash_sendbyte ((writeaddr & 0xff0000) >> 16);
   /*!< send writeaddr medium nibble address byte to write to */
   spiflash_sendbyte ((writeaddr & 0xff00) >> 8);
   /*!< send writeaddr low nibble address byte to write to */
   spiflash_sendbyte (writeaddr & 0xff);

   spiflash_write(pbuffer, numbytetowrite);

   /*!< deselect the flash: chip select high */
   spiflash_cs (CS_HIGH);

   /*!< wait the end of flash writing */
   spiflash->wait_ready ();
}

/**
 * @brief  Enables the write access to the FLASH.
 * @param  None
 * @retval None
 */
void flashspi_write_enable (void)
{
   /*!< Select the FLASH: Chip Select low */
   spiflash_cs (CS_LOW);

   /*!< Send "Write Enable" instruction */
   spiflash_sendbyte (FLASHSPI_CMD_WREN);

   /*!< Deselect the FLASH: Chip Select high */
   spiflash_cs (CS_HIGH);
}

/**
 * @brief  Erases the specified FLASH sector.
 * @param  SectorAddr: address of the sector to erase.
 * @retval None
 */
static void flashspi_sector_erase (uint32_t addr)
{
   //PRINT_FLASHSPI("erase sector 0x%x\n", addr);
   /*!< send write enable instruction */
   flashspi_write_enable ();

   /*!< sector erase */
   /*!< select the flash: chip select low */
   spiflash_cs (CS_LOW);
   /*!< send sector erase instruction */
   spiflash_sendbyte (FLASHSPI_CMD_SE);
   /*!< send sectoraddr high nibble address byte */
   spiflash_sendbyte ((addr & 0xff0000) >> 16);
   /*!< send sectoraddr medium nibble address byte */
   spiflash_sendbyte ((addr & 0xff00) >> 8);
   /*!< send sectoraddr low nibble address byte */
   spiflash_sendbyte (addr & 0xff);
   /*!< deselect the flash: chip select high */
   spiflash_cs (CS_HIGH);

   /*!< wait the end of flash writing */
   spiflash->wait_ready();
}
