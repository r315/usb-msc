// =============================================================================
/*!
 * @file       diskio.c
 *
 * This file contains implementation of sector read/write for
 * mass storage class usb device and FatFs
 *
 * Run lsblk to discover device on /dev/
 * Install hdparm to test device speed with command hdparm -t --direct /dev/<device>
 * sudo blockdev --report /dev/sda
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#include "diskio.h"
#include "flashspi.h"
#include "cdc_msc_class.h"
#include "msc_diskio.h"

#define PRINT_DISKIO_DBG 0
#if PRINT_DISKIO_DBG && ENABLE_DBG_LOG
   #include "board.h"
   #define PRINT_DISKIO(fmt, ...) dbg_log("[DISKIO] "fmt, ##__VA_ARGS__)
#else
   #define PRINT_DISKIO(...)
#endif
uint8_t scsi_inquiry[MSC_SUPPORT_MAX_LUN][SCSI_INQUIRY_DATA_LENGTH] = {
    /* lun = 0 */
    {
#ifdef MSC_CDROM
        0x05,
        0x08,
        0x02,
        0x02,
#else   // mass strorage
        0x00, /* peripheral device type (direct-access device) */
        0x80, /* removable media bit */
        0x00, /* ansi version, ecma version, iso version */
        0x01, /* respond data format */
#endif
        SCSI_INQUIRY_DATA_LENGTH - 5, /* additional length */
        0x00, 0x00, 0x00, /* reserved */
        'B', 'I', 'T', 'H', 'I', 'U', 'M', ' ', /* vendor information "AT32" */
        'D', 'i', 's', 'k', '0', ' ', ' ', ' ',' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ', /* Product identification "Disk" */
        '2', '.', '0', '0' /* product revision level */
    }
};
/**
 * @brief  get disk basic information
 * @param  lun: logical unit number
 * @retval inquiry string
 */
uint8_t *get_inquiry (uint8_t lun)
{
   if (lun < MSC_SUPPORT_MAX_LUN)
      return (uint8_t *) scsi_inquiry[lun];
   else
      return NULL;
}
/**
 * @brief  Initialize flash
 * @retval 0: on success
 */
usb_sts_type msc_disk_init (uint8_t lun)
{
   switch (lun)
   {
      case SPI_FLASH_LUN:
         return flashspi_init ();

      case SD_CARD_LUN:
//         return sd_init ();

      case INTERNAL_FLASH_LUN:
      default:
         break;
   }

   return USB_ERROR;
}
/**
 * @brief  disk read
 * @param  lun: logical units number
 * @param  addr: logical address
 * @param  read_buf: pointer to read buffer
 * @param  len: read length
 * @retval status of usb_sts_type
 */
usb_sts_type msc_disk_read (uint8_t lun, uint32_t addr, uint8_t *read_buf,
                            uint32_t len)
{
   usb_sts_type res = USB_NOT_SUPPORT;
   //PRINT_DISKIO("msc read address 0x%x, size %u\n", addr, len);
   switch (lun)
   {
      case SPI_FLASH_LUN:
         res = (usb_sts_type) flashspi_read (read_buf, addr, len);
         break;
      default:
         break;
   }
   return res;
}
/**
 * @brief  disk write
 * @param  lun: logical units number
 * @param  addr: logical address
 * @param  buf: pointer to write buffer
 * @param  len: write length
 * @retval status of usb_sts_type
 */
usb_sts_type msc_disk_write (uint8_t lun, uint32_t addr, uint8_t *buf,
                             uint32_t len)
{
   usb_sts_type res = USB_NOT_SUPPORT;
   //PRINT_DISKIO("msc write address 0x%x, size %u\n", addr, len);
   switch (lun)
   {
      case SPI_FLASH_LUN:
         res = (usb_sts_type) flashspi_write (buf, addr, len);
         break;
      default:
         break;
   }
   return res;
}
/**
 * @brief  disk capacity
 * @param  [in] lun: logical units number
 * @param  [out] blk_nbr: number of blocks
 * @param  [out] blk_size: block size
 * @retval status of usb_sts_type
 */
usb_sts_type msc_disk_capacity (uint8_t lun, uint32_t *blk_nbr,
                                uint32_t *blk_size)
{
   switch (lun)
   {
      case SPI_FLASH_LUN:
         *blk_size = FF_MIN_SS;
         *blk_nbr  = flashspi_get_size () / *blk_size;
         return USB_OK;
      default:
         break;
   }
   return USB_NOT_SUPPORT;
}
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (BYTE pdrv)
{
   DSTATUS status = STA_NOINIT;
   return (pdrv == SPI_FLASH_LUN) ? status &= ~STA_NOINIT : status;
}
/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (BYTE pdrv)
{
   DSTATUS status = STA_NOINIT;
   if(pdrv == SPI_FLASH_LUN){
      if(flashspi_init () == FLASHSPI_OK)
      {
         status &= ~STA_NOINIT;
      }
   }
   return status;
}
/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read (BYTE pdrv,  /* Physical drive number to identify the drive */
                   BYTE *buff, /* Data buffer to store read data */
                   LBA_t sector, /* Start sector in LBA */
                   UINT count    /* Number of sectors to read */
)
{
   DRESULT status;
   //PRINT_DISKIO("read sector 0x%x, count %u\n", sector, count);
   switch (pdrv)
   {
      case SPI_FLASH_LUN:
         status = (DRESULT) flashspi_read (buff, sector * FF_MIN_SS, count * FF_MIN_SS);
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
DRESULT disk_write (BYTE pdrv, /* Physical drive number to identify the drive */
                    const BYTE *buff, /* Data to be written */
                    LBA_t sector,     /* Start sector in LBA */
                    UINT count        /* Number of sectors to write */
)
{
   DRESULT status;
   //PRINT_DISKIO("write sector 0x%x, size %u\n", sector, count);
   if (!count)
   {
      return RES_PARERR; /* Check parameter */
   }
   switch (pdrv)
   {
      case SPI_FLASH_LUN:
         status = (DRESULT) flashspi_write (buff, sector * FF_MIN_SS, count * FF_MIN_SS);
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
DRESULT disk_ioctl (BYTE pdrv, /* Physical drive number (0..) */
                    BYTE cmd,  /* Control code */
                    void *buff /* Buffer to send/receive control data */
)
{
   DRESULT status = RES_PARERR;
   if (status & STA_NOINIT)
      return RES_NOTRDY;
   switch (pdrv)
   {
      case SPI_FLASH_LUN:
         switch (cmd)
         {
            case CTRL_SYNC:
               status = RES_OK;
               break;
            case GET_SECTOR_SIZE:
               *(DWORD *) buff = FF_MIN_SS;
               status          = RES_OK;
               break;
            case GET_SECTOR_COUNT:
               *(DWORD *) buff = flashspi_get_size () / FF_MIN_SS;
               status          = RES_OK;
               break;
            case GET_BLOCK_SIZE:
               // flashspi_get_sector_size(), can cause bluescreen
               // on windows after f_mkfs
               *(DWORD *) buff = flashspi_get_size ();
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