/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various existing       */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "at32_sdio.h"
#include "diskio.h" /* Declarations of disk functions */
#include "flashspi.h"

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (BYTE pdrv /* Physical drive number to identify the drive */
)
{
   DSTATUS status = STA_NOINIT;

   switch (pdrv)
   {
      case SPI_FLASH:
      case SD_CARD: /* SD CARD */
         status &= ~STA_NOINIT;
         break;

      default:
         break;
   }

   return status;
}


/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
    BYTE pdrv /* Physical drive number to identify the drive */
)
{
   DSTATUS status = STA_NOINIT;

   switch (pdrv)
   {
      case SD_CARD: /* SD CARD */
         if ((DRESULT) sd_init () == RES_OK)
         {
            status &= ~STA_NOINIT;
         }         
         break;

      case SPI_FLASH:
         if ((DRESULT) flash_spi_init () == RES_OK)
         {
            status &= ~STA_NOINIT;
         }         
         break;

      default:
         break;
   }
   return status;
}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (BYTE pdrv,  /* Physical drive nmuber to identify the drive */
                   BYTE *buff, /* Data buffer to store read data */
                   LBA_t sector, /* Start sector in LBA */
                   UINT count    /* Number of sectors to read */
)
{
   DRESULT status;

   switch (pdrv)
   {
      case SD_CARD:
         status = (DRESULT) sd_read_disk (buff, sector, count);
         break;
      
      case SPI_FLASH:
         status = (DRESULT) flash_spi_read (buff, sector * 512, count * 512);
         break;

      default:
         status = RES_PARERR;
   }

   return status;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (BYTE pdrv, /* Physical drive nmuber to identify the drive */
                    const BYTE *buff, /* Data to be written */
                    LBA_t sector,     /* Start sector in LBA */
                    UINT count        /* Number of sectors to write */
)
{
   DRESULT status;

   if (!count)
   {
      return RES_PARERR; /* Check parameter */
   }

   switch (pdrv)
   {
      case SD_CARD:
         status = (DRESULT) sd_write_disk (buff, sector, count);
         break;

      case SPI_FLASH:
         status = (DRESULT) flash_spi_write (buff, sector * 512, count * 512);
         break;

      default:
         status = RES_PARERR;
   }

   return status;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (BYTE pdrv, /* Physical drive nmuber (0..) */
                    BYTE cmd,  /* Control code */
                    void *buff /* Buffer to send/receive control data */
)
{
   DRESULT status = RES_PARERR;

   if (status & STA_NOINIT)
      return RES_NOTRDY;

   switch (pdrv)
   {
      case SD_CARD:         
         switch (cmd)
         {
            case CTRL_SYNC:
               status = RES_OK;
               break;
            case GET_SECTOR_SIZE:
               *(DWORD *) buff = 512;
               status          = RES_OK;
               break;
            case GET_SECTOR_COUNT:
               *(DWORD *) buff = sd_card_info.card_capacity / 512;
               status          = RES_OK;
               break;
            case GET_BLOCK_SIZE:
               *(DWORD *) buff = sd_card_info.card_blk_size;
               status          = RES_OK;
               break;
            default:
               status = RES_PARERR;
               break;
         }
         break;
      
      case SPI_FLASH:
         switch (cmd)
         {
            case CTRL_SYNC:
               status = RES_OK;
               break;
            case GET_SECTOR_SIZE:
               *(DWORD *) buff = 512;
               status          = RES_OK;
               break;
            case GET_SECTOR_COUNT:
               *(DWORD *) buff =  flash_spi_getsize () / 512;
               status          = RES_OK;
               break;
            case GET_BLOCK_SIZE:
               *(DWORD *) buff =  flash_spi_getsize ();
               status          = RES_OK;
               break;
            default:
               status = RES_PARERR;
               break;
         }
         break;

      default:
         break;
   }
   return status;
}
