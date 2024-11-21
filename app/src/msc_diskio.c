/**
 **************************************************************************
 * @file     msc_diskio.c
 * @version  v2.0.4
 * @date     2022-04-02
 * @brief    usb mass storage disk function
 **************************************************************************
 *                       Copyright notice & Disclaimer
 *
 * The software Board Support Package (BSP) that is made available to
 * download from Artery official website is the copyrighted work of Artery.
 * Artery authorizes customers to use, copy, and distribute the BSP
 * software and its related documentation for the purpose of design and
 * development in conjunction with Artery microcontrollers. Use of the
 * software is governed by this copyright notice and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
 * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
 * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
 * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
 * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 *
 **************************************************************************
 */
#include "msc_diskio.h"
//#include "msc_bot_scsi.h"
#include "cdc_msc_class.h"
#include "at32_sdio.h"
#include "flashspi.h"

/** @addtogroup AT32F415_periph_examples
 * @{
 */

/** @addtogroup 415_USB_device_msc
 * @{
 */
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
        0x00,
        0x00,
        0x00, /* reserved */
        'A',
        'T',
        '3',
        '2',
        ' ',
        ' ',
        ' ',
        ' ', /* vendor information "AT32" */
        'D',
        'i',
        's',
        'k',
        '0',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ', /* Product identification "Disk" */
        '2',
        '.',
        '0',
        '0' /* product revision level */
    }
};


/**
 * @brief
 * @param
 * @retval
 */
usb_sts_type msc_init (uint8_t lun)
{
   switch (lun)
   {
      case SPI_FLASH_LUN:
         return flashspi_init ();

      case SD_CARD_LUN:
         return sd_init ();

      case INTERNAL_FLASH_LUN:
      default:
         break;
   }

   return USB_ERROR;
}

/**
 * @brief  get disk inquiry
 * @param  lun: logical units number
 * @retval inquiry string
 */
uint8_t *msc_get_inquiry (uint8_t lun)
{
   if (lun < MSC_SUPPORT_MAX_LUN)
      return (uint8_t *) scsi_inquiry[lun];
   else
      return NULL;
}

/**
 * @brief  disk capacity
 * @param  lun: logical units number
 * @param  blk_nbr: pointer to number of block
 * @param  blk_size: pointer to block size
 * @retval status of usb_sts_type
 */
usb_sts_type msc_disk_capacity (uint8_t lun, uint32_t *blk_nbr,
                                uint32_t *blk_size)
{
   card_info_t *card_info;

   switch (lun)
   {
      case INTERNAL_FLASH_LUN:
         break;
      case SPI_FLASH_LUN:
         *blk_size = 512;
         *blk_nbr  = flashspi_getsize () / *blk_size;
         break;
      case SD_CARD_LUN:
         card_info = sd_card_info_get ();
         *blk_size = card_info->block_size;
         *blk_nbr  = card_info->capacity / card_info->block_size;
         break;

      default:
         return USB_ERROR;
   }
   return USB_OK;
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

   switch (lun)
   {
      case INTERNAL_FLASH_LUN:
         break;
      case SPI_FLASH_LUN:
         res = (usb_sts_type) flashspi_read (read_buf, addr, len);
         break;
      case SD_CARD_LUN:
         res = (usb_sts_type) sd_read_disk (read_buf, addr / 512, len / 512);
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

   switch (lun)
   {
      case INTERNAL_FLASH_LUN:
         break;
      case SPI_FLASH_LUN:
         res = (usb_sts_type) flashspi_write (buf, addr, len);
         break;
      case SD_CARD_LUN:
         res = (usb_sts_type) sd_write_disk (buf, addr / 512, len / 512);
         break;
      default:
         break;
   }
   return res;
}

/**
 * @}
 */

/**
 * @}
 */
