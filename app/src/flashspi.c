#include <stddef.h>
#include "flashspi.h"
#include "at32_spiflash.h"

#define FLASH_DEVICES_COUNT sizeof (flash_spi_devices) / sizeof (flashspi_t *)

static uint32_t flash_spi_getid (void);

static void flash_spi_erasesector (uint32_t sectoraddr);
static void flash_spi_waitforwriteend (void);
static void flash_spi_writepage (const uint8_t *pbuffer, uint32_t writeaddr, uint16_t numbytetowrite);
static void flash_spi_writebuffer (const uint8_t *pbuffer, uint32_t writeaddr, uint16_t numbytetowrite);

extern const flashspi_t gd25lq16;
extern const flashspi_t w25q64;
extern const flashspi_t w25q128;

static const flashspi_t *spiflash;
static const flashspi_t *flash_spi_devices[] = {&gd25lq16, &w25q64, &w25q128};

/**
 * @brief Calls SOC low level spi initialization and select flash
 *        according read id.
 * @param  None
 * @retval None
 */
flashspi_error_t flash_spi_init (void)
{
   spiflash_init ();

   uint32_t device_id = flash_spi_getid ();

   spiflash = NULL;

   for (uint8_t i = 0; i < FLASH_DEVICES_COUNT; i++)
   {
      if (flash_spi_devices[i]->id == device_id)
      {
         spiflash = flash_spi_devices[i];
         break;
      }
   }

   if(spiflash){
      return spiflash->init();
   }

   return FLASHSPI_ERROR;
}

/**
 * @brief  Get FLASH SIZE.
 * @param  None
 * @retval FLASH identification
 */
uint32_t flash_spi_getsize (void)
{
   return (spiflash) ? spiflash->size : 0;
}

/**
 * @brief  Get FLASH Sector SIZE.
 * @param  None
 * @retval FLASH identification
 */
uint32_t flash_spi_getsectorsize (void)
{
   return (spiflash) ? spiflash->sectorsize : 0;
}

/**
 * @brief  Get FLASH Page SIZE.
 * @param  None
 * @retval FLASH identification
 */
uint32_t flash_spi_getpagesize (void)
{
   return (spiflash) ? spiflash->pagesize : 0;
}

/**
 * @brief  Reads a block of data from the FLASH.
 * @param  pBuffer: pointer to the buffer that receives the data read from the
 * FLASH.
 * @param  ReadAddr: FLASH's internal address to read from.
 * @param  NumByteToRead: number of bytes to read from the FLASH.
 * @retval None
 */
flashspi_error_t flash_spi_read (uint8_t *pbuffer, uint32_t readaddr,
                     uint16_t numbytetoread)
{
   /*!< select the flash: chip select low */
   spiflash_cs (CS_LOW);

   /*!< send "read from memory " instruction */
   spiflash_sendbyte (FLASH_SPI_CMD_READ);
   /*!< send readaddr high nibble address byte to read from */
   spiflash_sendbyte ((readaddr & 0xff0000) >> 16);
   /*!< send readaddr medium nibble address byte to read from */
   spiflash_sendbyte ((readaddr & 0xff00) >> 8);
   /*!< send readaddr low nibble address byte to read from */
   spiflash_sendbyte (readaddr & 0xff);

   while (numbytetoread--) /*!< while there is data to be read */
   {
      /*!< read a byte from the flash */
      *pbuffer = spiflash_sendbyte (FLASH_DUMMY_BYTE);
      /*!< point to the next location where the byte read will be saved */
      pbuffer++;
   }

   /*!< deselect the flash: chip select high */
   spiflash_cs (CS_HIGH);

   return FLASHSPI_OK;
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
flashspi_error_t flash_spi_write (const uint8_t *pbuffer, uint32_t writeaddr,
                      uint16_t numbytetowrite)
{
   uint32_t cnt = 0;
   uint32_t sectornum, sectoroffset, sectorremain;
   uint8_t gtmpbuff[spiflash->sectorsize];

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
      flash_spi_read (gtmpbuff, sectornum * spiflash->sectorsize,
                      spiflash->sectorsize);

      /*check data is 0xff ?*/
      for (cnt = 0; cnt < sectorremain; cnt++)
      {
         if (gtmpbuff[sectoroffset + cnt] != 0xff)
            break;
      }

      if (cnt < sectorremain) /*need sector erase*/
      {
         flash_spi_erasesector (sectornum * spiflash->sectorsize);
         for (cnt = 0; cnt < sectorremain; cnt++)
            gtmpbuff[cnt + sectoroffset] = pbuffer[cnt];
         flash_spi_writebuffer (gtmpbuff, sectornum * spiflash->sectorsize,
                                spiflash->sectorsize);
      }
      else
      {
         flash_spi_writebuffer (pbuffer, writeaddr, sectorremain);
      }

      if (sectorremain == numbytetowrite)
         break;

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
 * @brief  Reads FLASH identification.
 * @param  None
 * @retval FLASH identification
 */
static uint32_t flash_spi_getid (void)
{
   uint16_t flash_id = 0;
   spiflash_cs (CS_LOW);
   spiflash_sendbyte (FLASH_SPI_CMD_REMS);
   spiflash_sendbyte (0x00);
   spiflash_sendbyte (0x00);
   spiflash_sendbyte (0x00);
   flash_id |= spiflash_readbyte () << 8;
   flash_id |= spiflash_readbyte ();
   spiflash_cs (CS_HIGH);
   return flash_id;
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
static void flash_spi_writebuffer (const uint8_t *pbuffer, uint32_t writeaddr,
                            uint16_t numbytetowrite)
{
   uint16_t pager;
   pager = spiflash->pagesize - (writeaddr % spiflash->pagesize);
   if (numbytetowrite <= pager)
   {
      pager = numbytetowrite;
   }

   while (1)
   {
      flash_spi_writepage (pbuffer, writeaddr, pager);
      if (pager == numbytetowrite)
         break;
      else
      {
         pbuffer += pager;
         writeaddr += pager;
         numbytetowrite -= pager;
         if (numbytetowrite > spiflash->pagesize)
            pager = spiflash->pagesize;
         else
            pager = numbytetowrite;
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
static void flash_spi_writepage (const uint8_t *pbuffer, uint32_t writeaddr,
                                 uint16_t numbytetowrite)
{
   /*!< enable the write access to the flash */
   flash_spi_writeenable ();

   /*!< select the flash: chip select low */
   spiflash_cs (CS_LOW);
   /*!< send "write to memory " instruction */
   spiflash_sendbyte (FLASH_SPI_CMD_PP);
   /*!< send writeaddr high nibble address byte to write to */
   spiflash_sendbyte ((writeaddr & 0xff0000) >> 16);
   /*!< send writeaddr medium nibble address byte to write to */
   spiflash_sendbyte ((writeaddr & 0xff00) >> 8);
   /*!< send writeaddr low nibble address byte to write to */
   spiflash_sendbyte (writeaddr & 0xff);

   /*!< while there is data to be written on the flash */
   while (numbytetowrite--)
   {
      /*!< send the current byte */
      spiflash_sendbyte (*pbuffer);
      /*!< point on the next byte to be written */
      pbuffer++;
   }

   /*!< deselect the flash: chip select high */
   spiflash_cs (CS_HIGH);

   /*!< wait the end of flash writing */
   flash_spi_waitforwriteend ();
}

/**
 * @brief  Enables the write access to the FLASH.
 * @param  None
 * @retval None
 */
void flash_spi_writeenable (void)
{
   /*!< Select the FLASH: Chip Select low */
   spiflash_cs (CS_LOW);

   /*!< Send "Write Enable" instruction */
   spiflash_sendbyte (FLASH_SPI_CMD_WREN);

   /*!< Deselect the FLASH: Chip Select high */
   spiflash_cs (CS_HIGH);
}

/**
 * @brief  Polls the status of the Write In Progress (WIP) flag in the FLASH's
 *         status register and loop until write opertaion has completed.
 * @param  None
 * @retval None
 */
static void flash_spi_waitforwriteend (void)
{
   uint8_t flashstatus = 0;

   /*!< Select the FLASH: Chip Select low */
   spiflash_cs (CS_LOW);

   /*!< Send "Read Status Register" instruction */
   spiflash_sendbyte (FLASH_SPI_CMD_RDSR);

   /*!< Loop as long as the memory is busy with a write cycle */
   do
   {
      /*!< Send a dummy byte to generate the clock needed by the FLASH
       and put the value of the status register in FLASH_Status variable */
      flashstatus = spiflash_sendbyte (FLASH_DUMMY_BYTE);

   } while ((flashstatus & 0x1) != 0); /* Write in progress */

   /*!< Deselect the FLASH: Chip Select high */
   spiflash_cs (CS_HIGH);
}

/**
 * @brief  Erases the specified FLASH sector.
 * @param  SectorAddr: address of the sector to erase.
 * @retval None
 */
static void flash_spi_erasesector (uint32_t sectoraddr)
{
   /*!< send write enable instruction */
   flash_spi_writeenable ();

   /*!< sector erase */
   /*!< select the flash: chip select low */
   spiflash_cs (CS_LOW);
   /*!< send sector erase instruction */
   spiflash_sendbyte (FLASH_SPI_CMD_SE);
   /*!< send sectoraddr high nibble address byte */
   spiflash_sendbyte ((sectoraddr & 0xff0000) >> 16);
   /*!< send sectoraddr medium nibble address byte */
   spiflash_sendbyte ((sectoraddr & 0xff00) >> 8);
   /*!< send sectoraddr low nibble address byte */
   spiflash_sendbyte (sectoraddr & 0xff);
   /*!< deselect the flash: chip select high */
   spiflash_cs (CS_HIGH);

   /*!< wait the end of flash writing */
   flash_spi_waitforwriteend ();
}
