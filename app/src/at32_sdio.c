#include <stddef.h>
#include <string.h>
#include "at32_sdio.h"
#include "board.h"

static sdio_error_t command_error (void);
static sdio_error_t command_rsp1_error (uint8_t cmd);
static sdio_error_t command_rsp2_error (void);
static sdio_error_t command_rsp3_error (void);
//static sdio_error_t command_rsp4_error (uint8_t cmd);
//static sdio_error_t command_rsp5_error (uint8_t cmd);
static sdio_error_t command_rsp6_error (uint8_t cmd, uint16_t *p_rca);
static sdio_error_t command_rsp7_error (void);
static sdio_error_t mmc_switch (uint8_t set, uint8_t index, uint8_t value);
static sdio_error_t sdio_switch_speed (uint32_t mode, uint32_t group, uint8_t value, uint8_t *rsp);
static void sd_dma_config (uint32_t *mbuf, uint32_t buf_size, dma_dir_type dir);
static void sd_card_info_parse(card_info_t *info);

static sdio_command_struct_type sdio_command_init_struct;
static sdio_data_struct_type sdio_data_init_struct;

static sd_memory_card_type card_type = SDIO_STD_CAPACITY_SD_CARD_V1_1;

static uint32_t csd_table[4], cid_table[4], rca = 0; /* csd, sid, rca */
static sd_data_transfer_mode_type device_mode =
    SD_TRANSFER_POLLING_MODE;                 /* working mode */

static uint8_t stop_flag             = 0;     /* transmit stop flag */
volatile sdio_error_t transfer_error = SD_OK; /* transmit error flag */
volatile uint8_t transfer_end        = 0;     /* transmit end flag */

static card_info_t card_info;                 /* card information */

/**
 * @brief  converts the number of bytes in power of two and returns the power.
 * @param  number_of_bytes: number of bytes.
 * @retval none
 */
static uint8_t convert_from_bytes_to_power_of_two (uint16_t number_of_bytes)
{
   uint8_t count = 0;

   while (number_of_bytes != 1)
   {
      number_of_bytes >>= 1;
      count++;
   }

   return count;
}

/**
 * @brief  set sdio clock devision factor.
 * @param  clkdiv: sdio_ck = ahbclk / (clkdiv+2)
 * @retval none
 */
static void sdio_clock_set (uint32_t clk_div)
{
   /* config clock divide [7:0] */
   SDIOx->clkctrl_bit.clkdiv_l = (clk_div & 0xFF);

   /* config clock divide [9:8] */
   SDIOx->clkctrl_bit.clkdiv_h = ((clk_div & 0x300) >> 8);
}

/**
 * @brief  read data from or write data to sd card.
 * @param  sdio_cmd_init_struct: pointer to sdio_command_struct_type structure.
 * @param  sdio_data_init_struct: pointer to sdio_data_struct_type structure.
 * @param  buf: pointer to data buffer read or write.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sdio_command_data_send (sdio_command_struct_type *sdio_cmd_init_t,
                                     sdio_data_struct_type *sdio_data_init_t,
                                     uint32_t *buf)
{
   sdio_error_t status = SD_OK;
   uint32_t count      = 0;
   uint32_t timeout    = SDIO_DATATIMEOUT;
   uint32_t length     = 0;

   if (buf == NULL)
   {
      return SD_INVALID_PARAMETER;
   }
   /* clear dtctrl register */
   SDIOx->dtcnt = 0x0;

   /* sdio command config */
   sdio_data_config (SDIOx, sdio_data_init_t);
   /* enable dcsm */
   sdio_data_state_machine_enable (SDIOx, TRUE);

   length = sdio_data_init_t->data_length;

   if (device_mode == SD_TRANSFER_DMA_MODE)
   {
      if (sdio_data_init_t->transfer_direction ==
          SDIO_DATA_TRANSFER_TO_CONTROLLER)
      {
         sd_dma_config (buf, length, DMA_DIR_PERIPHERAL_TO_MEMORY);
         SDIOx->inten |= SDIO_INTR_STS_READ_MASK;
         transfer_error = SD_OK;
         transfer_end   = 0;
         sdio_dma_enable (SDIOx, TRUE);
      }
   }

   /* sdio command config */
   sdio_command_config (SDIOx, sdio_cmd_init_t);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (sdio_cmd_init_t->cmd_index);
   if (status != SD_OK)
   {
      return status;
   }

   /* polling mode */
   if (device_mode == SD_TRANSFER_POLLING_MODE)
   {
      if (SDIO_DATA_TRANSFER_TO_CONTROLLER ==
          sdio_data_init_t->transfer_direction)
      {
         while (!(SDIOx->sts & (SDIO_INTR_STS_READ_MASK)))
         {
            if (sdio_flag_get (SDIOx, SDIO_RXBUFH_FLAG) != RESET)
            {
               for (count = 0; count < 8; count++, buf++)
               {
                  *buf = sdio_data_read (SDIOx);
               }

               timeout = 0x7FFFFF;
            }
            else
            {
               if (0 == timeout)
               {
                  sd_init ();
                  return SD_DATA_TIMEOUT;
               }

               timeout--;
            }
         }

         while (sdio_flag_get (SDIOx, SDIO_RXBUF_FLAG) != RESET)
         {
            *buf = sdio_data_read (SDIOx);
            buf++;
         }
      }
      else
      {
         while (!(SDIOx->sts & SDIO_INTR_STS_WRITE_MASK))
         {
            if (sdio_flag_get (SDIOx, SDIO_TXBUFH_FLAG) != RESET)
            {
               for (count = 0; count < 8 && length > 0;
                    count++, buf++, length -= 4)
               {
                  sdio_data_write (SDIOx, *buf);
               }

               timeout = 0x3FFFFFFF;
            }
            else
            {
               if (timeout == 0)
               {
                  sd_init ();
                  return SD_DATA_TIMEOUT;
               }

               timeout--;
            }
         }
      }
      /* data timeout */
      if (sdio_flag_get (SDIOx, SDIO_DTTIMEOUT_FLAG) != RESET)
      {
         sdio_flag_clear (SDIOx, SDIO_DTTIMEOUT_FLAG);
         return SD_DATA_TIMEOUT;
      }
      /* crc error */
      else if (sdio_flag_get (SDIOx, SDIO_DTFAIL_FLAG) != RESET)
      {
         sdio_flag_clear (SDIOx, SDIO_DTFAIL_FLAG);
         return SD_DATA_FAIL;
      }
      /* over run error */
      else if (sdio_flag_get (SDIOx, SDIO_RXERRO_FLAG) != RESET)
      {
         sdio_flag_clear (SDIOx, SDIO_RXERRO_FLAG);
         return SD_RX_OVERRUN;
      }
      /* under run error */
      else if (sdio_flag_get (SDIOx, SDIO_TXERRU_FLAG) != RESET)
      {
         sdio_flag_clear (SDIOx, SDIO_TXERRU_FLAG);
         return SD_TX_UNDERRUN;
      }
      /* start bit error */
      else if (sdio_flag_get (SDIOx, SDIO_SBITERR_FLAG) != RESET)
      {
         sdio_flag_clear (SDIOx, SDIO_SBITERR_FLAG);
         return SD_START_BIT_ERR;
      }
      /* data transfer complete */
      if ((stop_flag == 1) &&
          (sdio_flag_get (SDIOx, SDIO_DTCMPL_FLAG) != RESET))
      {
         /* send cmd12, stop transmission */
         sdio_cmd_init_t->argument  = 0;
         sdio_cmd_init_t->cmd_index = SD_CMD_STOP_TRANSMISSION;
         sdio_cmd_init_t->rsp_type  = SDIO_RESPONSE_SHORT;
         sdio_cmd_init_t->wait_type = SDIO_WAIT_FOR_NO;

         /* sdio command config */
         sdio_command_config (SDIOx, sdio_cmd_init_t);
         /* enable ccsm */
         sdio_command_state_machine_enable (SDIOx, TRUE);

         status = command_rsp1_error (SD_CMD_STOP_TRANSMISSION);

         if (status != SD_OK)
         {
            return status;
         }
      }

      sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);
   }
   else if (device_mode == SD_TRANSFER_DMA_MODE)
   {
      if (sdio_data_init_t->transfer_direction == SDIO_DATA_TRANSFER_TO_CARD)
      {
         sd_dma_config (buf, length, DMA_DIR_MEMORY_TO_PERIPHERAL);
         SDIOx->inten |= SDIO_INTR_STS_WRITE_MASK;
         transfer_error = SD_OK;
         transfer_end   = 0;
         sdio_dma_enable (SDIOx, TRUE);
      }

      while (!(SDIOx->sts & SDIOx->inten) && timeout)
      {
         timeout--;

         if (transfer_end)
         {
            break;
         }
      }

      if (timeout == 0)
      {
         sd_init ();
         return SD_DATA_TIMEOUT;
      }

      if (transfer_error != SD_OK)
      {
         status = transfer_error;
      }
   }

   return status;
}

/**
 * @brief  set bus speed
 * @param  speed: 0,normal speed; 1,high speed.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_speed_change (uint8_t speed)
{
   sdio_error_t status = SD_OK;
   uint8_t switch_sts[64];

   if (speed > 1)
   {
      return SD_INVALID_PARAMETER;
   }

   /* check card type */
   if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == card_info.type) ||
       (SDIO_STD_CAPACITY_SD_CARD_V2_0 == card_info.type) ||
       (SDIO_HIGH_CAPACITY_SD_CARD == card_info.type))
    {
      /* version 1.01 card does not support cmd6 */
      if (card_info.scr.sd_spec == 0)
      {
         return speed == 0 ? SD_OK : SD_UNSUPPORTED_FEATURE;
      }

      /* check group 1 function support speed */
      status = sdio_switch_speed (0, 0, speed, switch_sts);

      if (status != SD_OK)
      {
         return status;
      }

      if ((switch_sts[13] & (1 << speed)) == 0)
      {
         return SD_SWITCH_ERROR;
      }

      /* read it back for confirmation */
      status = sdio_switch_speed (1, 0, speed, switch_sts);

      if (status != 0)
      {
         return status;
      }

      if ((switch_sts[16] & 0xF) != speed)
      {
         return SD_SWITCH_ERROR;
      }
   }
   else if (card_type == SDIO_HIGH_SPEED_MULTIMEDIA_CARD)
   {
      status = mmc_switch (EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH,
                           (uint8_t) speed);

      if (status != 0)
      {
         return status;
      }
   }

   return status;
}

/**
 * @brief  switch sd card speed to high speed
 * @param  mode: polling mode or dma mode
 * @param  group: selected group
 * @param  value: wanted speed
 * @param  rsp: switch status
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sdio_switch_speed (uint32_t mode, uint32_t group, uint8_t value,
                        uint8_t *rsp)
{
   sdio_error_t status = SD_OK;
   uint8_t power;
   uint16_t blk_size;

   if (rsp == NULL)
   {
      return SD_INVALID_PARAMETER;
   }

   SDIOx->dtctrl = 0x0;

   blk_size      = 64;
   power         = convert_from_bytes_to_power_of_two (blk_size);

   sdio_command_init_struct.argument  = blk_size;
   sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (SD_CMD_SET_BLOCKLEN);

   if (status != SD_OK)
   {
      return status;
   }

   sdio_data_init_struct.block_size         = (sdio_block_size_type) (power);
   sdio_data_init_struct.data_length        = blk_size;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_command_init_struct.argument        = (mode << 31) | 0x00FFFFFF;
   sdio_command_init_struct.argument &= ~(0xF << (group * 4));
   sdio_command_init_struct.argument |= value << (group * 4);
   sdio_command_init_struct.cmd_index = SD_CMD_HS_SWITCH;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   stop_flag                          = 0;

   return sdio_command_data_send (&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *) rsp);
}

/**
 * @brief  enable or disable the sdio wide bus mode.
 * @param  new_state: new state of the sdio wide bus mode. (true or false)
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sdio_bus_wide_enable (confirm_state new_state)
{
   sdio_error_t status = SD_OK;
   uint32_t response   = 0;
   uint8_t arg         = 0x00;

   if (new_state == TRUE)
   {
      arg = 0x02;
   }
   else
   {
      arg = 0x00;
   }

   /* get response1 */
   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked or not */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }

   if (card_info.scr.sd_bus_widths)
   {
      sdio_command_init_struct.argument  = (uint32_t) (card_info.rca << 16);
      sdio_command_init_struct.cmd_index = SD_CMD_APP_CMD;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_APP_CMD);

      if (status != SD_OK)
      {
         return status;
      }

      sdio_command_init_struct.argument  = arg;
      sdio_command_init_struct.cmd_index = SD_CMD_APP_SD_SET_BUSWIDTH;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_APP_SD_SET_BUSWIDTH);
      return status;
   }
   else
   {
      return SD_REQ_NOT_APPLICABLE;
   }
}

/**
 * @brief  allows to process all the interrupts that are high.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_irq_service (void)
{
   if (sdio_flag_get (SDIOx, SDIO_DTCMPL_FLAG) != RESET)
   {
      if (stop_flag == 1)
      {
         /* send cmd12, stop transmission */
         sdio_command_init_struct.argument  = 0;
         sdio_command_init_struct.cmd_index = SD_CMD_STOP_TRANSMISSION;
         sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
         sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;
         /* sdio command config */
         sdio_command_config (SDIOx, &sdio_command_init_struct);
         /* enable ccsm */
         sdio_command_state_machine_enable (SDIOx, TRUE);
         transfer_error = command_rsp1_error (SD_CMD_STOP_TRANSMISSION);
      }
      else
      {
         transfer_error = SD_OK;
      }
      /* clear flag */
      sdio_flag_clear (SDIOx, SDIO_DTCMPL_FLAG);
      transfer_end = 1;
      return transfer_error;
   }

   if (sdio_flag_get (SDIOx, SDIO_DTFAIL_FLAG) != RESET)
   {
      /* clear flag */
      sdio_flag_clear (SDIOx, SDIO_DTFAIL_FLAG);
      transfer_error = SD_DATA_FAIL;
      transfer_end   = 1;
      return transfer_error;
   }

   if (sdio_flag_get (SDIOx, SDIO_DTTIMEOUT_FLAG) != RESET)
   {
      /* clear flag */
      sdio_flag_clear (SDIOx, SDIO_DTTIMEOUT_FLAG);
      transfer_error = SD_DATA_TIMEOUT;
      transfer_end   = 1;
      return transfer_error;
   }

   if (sdio_flag_get (SDIOx, SDIO_RXERRO_FLAG) != RESET)
   {
      /* clear flag */
      sdio_flag_clear (SDIOx, SDIO_RXERRO_FLAG);
      transfer_error = SD_RX_OVERRUN;
      transfer_end   = 1;
      return (SD_RX_OVERRUN);
   }

   if (sdio_flag_get (SDIOx, SDIO_TXERRU_FLAG) != RESET)
   {
      /* clear flag */
      sdio_flag_clear (SDIOx, SDIO_TXERRU_FLAG);
      transfer_error = SD_TX_UNDERRUN;
      transfer_end   = 1;
      return (SD_TX_UNDERRUN);
   }

   if (sdio_flag_get (SDIOx, SDIO_SBITERR_FLAG) != RESET)
   {
      /* clear flag */
      sdio_flag_clear (SDIOx, SDIO_SBITERR_FLAG);
      transfer_error = SD_START_BIT_ERR;
      transfer_end   = 1;
      return (SD_START_BIT_ERR);
   }

   /* disable interrupt */
   sdio_interrupt_enable (SDIOx,
                          (SDIO_DTFAIL_INT | SDIO_DTTIMEOUT_INT |
                           SDIO_DTCMP_INT | SDIO_TXBUFH_INT | SDIO_RXBUFH_INT |
                           SDIO_TXERRU_INT | SDIO_RXERRO_INT |
                           SDIO_SBITERR_INT),
                          FALSE);
   return (SD_OK);
}

/**
 * @brief  set dma configuation for sdio
 * @param  mbuf: buffer address
 * @param  buf_size: transmission data size
 * @param  dir: dma direction, it is DMA_DIR_MEMORY_TO_PERIPHERAL(writing data)
 *              or DMA_DIR_PERIPHERAL_TO_MEMORY(read data)
 * @retval none
 */
static void sd_dma_config (uint32_t *mbuf, uint32_t buf_size, dma_dir_type dir)
{
   dma_init_type dma_init_struct;
   dma_default_para_init (&dma_init_struct);

   crm_periph_clock_enable (CRM_DMA2_PERIPH_CLOCK, TRUE);

   dma_reset (DMA2_CHANNEL4);
   dma_channel_enable (DMA2_CHANNEL4, FALSE);

   dma_init_struct.peripheral_base_addr  = (uint32_t) &SDIOx->buf;
   dma_init_struct.memory_base_addr      = (uint32_t) mbuf;
   dma_init_struct.direction             = dir;
   dma_init_struct.buffer_size           = buf_size / 4;
   dma_init_struct.peripheral_inc_enable = FALSE;
   dma_init_struct.memory_inc_enable     = TRUE;
   dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_WORD;
   dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_WORD;
   dma_init_struct.loop_mode_enable      = FALSE;
   dma_init_struct.priority              = DMA_PRIORITY_HIGH;
   dma_init (DMA2_CHANNEL4, &dma_init_struct);

   //dmamux_init (DMA2MUX_CHANNEL4, DMAMUX_SDIOx);
   //dmamux_enable (DMA2, TRUE);
   dma_channel_enable (DMA2_CHANNEL4, TRUE);
}


/**
 * @brief  read current card status.
 * @param  p_card_status: card status.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_status_send (uint32_t *p_card_status)
{
   sdio_error_t status = SD_OK;

   if (p_card_status == NULL)
   {
      status = SD_INVALID_PARAMETER;
      return status;
   }

   sdio_command_init_struct.argument  = (uint32_t) (rca << 16);
   sdio_command_init_struct.cmd_index = SD_CMD_SEND_STATUS;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (SD_CMD_SEND_STATUS);

   if (status != SD_OK)
   {
      return status;
   }

   *p_card_status = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   return status;
}

/**
 * @brief  returns information about sd card.
 * @param  none.
 * @retval sd_card_state_type: sd card state code.
 */
sd_card_state_type sd_state_get (void)
{
   uint32_t response = 0;

   if (sd_status_send (&response) != SD_OK)
   {
      return SD_CARD_ERROR;
   }
   else
   {
      return (sd_card_state_type) ((response >> 9) & 0x0F);
   }
}

/**
 * @brief  find the sd card scr register value.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_scr_read (uint32_t *buffer)
{
   uint32_t index = 0, sts_reg = 0;
   sdio_error_t status = SD_OK;

   /* send cmd16, set block length */
   sdio_command_init_struct.argument  = (uint32_t) 8;
   sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (SD_CMD_SET_BLOCKLEN);

   if (status != SD_OK)
   {
      return status;
   }

   /* send cmd55 */
   sdio_command_init_struct.argument  = (uint32_t) (card_info.rca << 16);
   sdio_command_init_struct.cmd_index = SD_CMD_APP_CMD;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (SD_CMD_APP_CMD);

   if (status != SD_OK)
   {
      return status;
   }

   sdio_data_init_struct.block_size         = SDIO_DATA_BLOCK_SIZE_8B;
   sdio_data_init_struct.data_length        = 8;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, TRUE);

   /* send cmd51 */
   sdio_command_init_struct.argument  = 0x0;
   sdio_command_init_struct.cmd_index = SD_CMD_SD_APP_SEND_SCR;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (SD_CMD_SD_APP_SEND_SCR);

   if (status != SD_OK)
   {
      return status;
   }

   sts_reg = SDIOx->sts;

   while (
       !(sts_reg & (SDIO_RXERRO_FLAG | SDIO_DTFAIL_FLAG | SDIO_DTTIMEOUT_FLAG |
                    SDIO_DTBLKCMPL_FLAG | SDIO_SBITERR_FLAG)))
   {
    //TODO: ADD Timeout
      if (sdio_flag_get (SDIOx, SDIO_RXBUF_FLAG) != RESET)
      {
         *(buffer + index) = sdio_data_read (SDIOx);
         index++;
      }
      sts_reg = SDIOx->sts;
   }

   if (sdio_flag_get (SDIOx, SDIO_DTTIMEOUT_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_DTTIMEOUT_FLAG);
      return SD_DATA_TIMEOUT;
   }
   else if (sdio_flag_get (SDIOx, SDIO_DTFAIL_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_DTFAIL_FLAG);
      return SD_DATA_FAIL;
   }
   else if (sdio_flag_get (SDIOx, SDIO_RXERRO_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_RXERRO_FLAG);
      return SD_RX_OVERRUN;
   }
   else if (sdio_flag_get (SDIOx, SDIO_SBITERR_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_SBITERR_FLAG);
      return SD_START_BIT_ERR;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   return status;
}

/**
 * @brief  checks if the sd card is in programming state.
 * @param  p_status: pointer to the variable that will contain the sd card
 * state.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_programming_status (uint8_t *p_status)
{
   volatile uint32_t response = 0, sts_reg = 0, rsp_cmd = 0;

   /* send cmd13 */
   sdio_command_init_struct.argument  = (uint32_t) (rca << 16);
   sdio_command_init_struct.cmd_index = SD_CMD_SEND_STATUS;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   sts_reg = SDIOx->sts;

   while (!(sts_reg &
            (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG)))
   {
      sts_reg = SDIOx->sts;
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDFAIL_FLAG);
      return SD_CMD_FAIL;
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);
      return SD_CMD_RSP_TIMEOUT;
   }

   rsp_cmd = sdio_command_response_get (SDIOx);
   if (rsp_cmd != SD_CMD_SEND_STATUS)
   {
      return SD_ILLEGAL_CMD;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   response  = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   *p_status = (uint8_t) ((response >> 9) & 0x0000000F);

   return SD_OK;
}

/**
 * @brief  enquires cards about their operating voltage and configures
 *         clock controls.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_mmc_power_on (void)
{
   uint8_t retry       = 0;
   sdio_error_t status = SD_OK;
   uint32_t response   = 0, count = 0, valid_voltage = 0;
   uint32_t sd_type;

   crm_clocks_freq_type clocks;

   crm_clocks_freq_get(&clocks);

   /* sdio_ck is less than 400KHz in initialization stage, set to 200KHz */
   uint32_t clk_psc = (clocks.ahb_freq / 200000) - 2;

   if (clk_psc > 0x3FF) {
      clk_psc = 0x3FF;
   }

   /* config sdio clock divide and edge phase */
   sdio_clock_config (SDIOx, clk_psc, SDIO_CLOCK_EDGE_FALLING);
   /* config sdio bus width */
   sdio_bus_width_config (SDIOx, SDIO_BUS_WIDTH_D1);
   /* disable flow control */
   sdio_flow_control_enable (SDIOx, FALSE);
   /* disable clock bypass */
   sdio_clock_bypass (SDIOx, FALSE);
   /* disable power saving mode */
   sdio_power_saving_mode_enable (SDIOx, FALSE);

   /* sdio power on */
   sdio_power_set (SDIOx, SDIO_POWER_ON);
   /* enable to output sdio_ck */
   sdio_clock_enable (SDIOx, TRUE);

   /**
    * Start initializarion
    * See Figure 4-2 on Physical Layer simplified specification
    * for initialization flow
    */

   for (retry = 0; retry < 5; retry++)
   {
      /* send cmd0, get in idle stage */
      sdio_command_init_struct.argument  = 0x0;
      sdio_command_init_struct.cmd_index = SD_CMD_GO_IDLE_STATE;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_NO;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      /* get command status */
      status = command_error ();

      if(status == SD_CMD_RSP_TIMEOUT){
        return status;
      }
   }

   /* send cmd8, check card interface feature */
   sdio_command_init_struct.argument  = SD_CHECK_PATTERN;
   sdio_command_init_struct.cmd_index = SDIO_SEND_IF_COND;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   /* waiting R7 */
   status = command_rsp7_error ();

   if (status == SD_OK){
      card_type = SDIO_STD_CAPACITY_SD_CARD_V2_0;
      sd_type   = SD_HIGH_CAPACITY;
   }else{
      card_type = SDIO_STD_CAPACITY_SD_CARD_V1_1;
      sd_type   = SD_STD_CAPACITY;
   }

   /* send cmd55, check sd version */
   sdio_command_init_struct.argument  = 0x00;
   sdio_command_init_struct.cmd_index = SD_CMD_APP_CMD;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   /* waiting R1 */
   status = command_rsp1_error (SD_CMD_APP_CMD);

   /* check sd card or mmc card */
   if (SD_OK == status)
   {
      /* send acmd41, check voltage operation range */
      while ((!valid_voltage) && (count < SD_MAX_VOLT_TRIAL))
      {
         delay_ms (10);

         /* send cmd55 before acmd41 */
         sdio_command_init_struct.argument  = 0x00;
         sdio_command_init_struct.cmd_index = SD_CMD_APP_CMD;
         sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
         sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

         /* sdio command config */
         sdio_command_config (SDIOx, &sdio_command_init_struct);
         /* enable ccsm */
         sdio_command_state_machine_enable (SDIOx, TRUE);

         /* waiting R1 */
         status = command_rsp1_error (SD_CMD_APP_CMD);

         /* if any errors occured, return status */
         if (status != SD_OK)
         {
            return status;
         }

         /* send acmd41 */
         sdio_command_init_struct.argument  = SD_VOLTAGE_WINDOW_SD | sd_type;
         sdio_command_init_struct.cmd_index = SD_CMD_SD_APP_OP_COND;
         sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
         sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

         /* sdio command config */
         sdio_command_config (SDIOx, &sdio_command_init_struct);
         /* enable ccsm */
         sdio_command_state_machine_enable (SDIOx, TRUE);

         /* waiting R3 */
         status = command_rsp3_error ();

         /* if any errors occured, return status */
         if (status != SD_OK)
         {
            return status;
         }

         /* get response1 */
         response      = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

         /* check sd card power on stage is success or not */
         valid_voltage = (((response >> 31) == 1) ? 1 : 0);
         count++;
      }

      if (count >= SD_MAX_VOLT_TRIAL)
      {
         status = SD_INVALID_VOLTRANGE;
         return status;
      }

      if (response &= SD_HIGH_CAPACITY)
      {
         card_type = SDIO_HIGH_CAPACITY_SD_CARD;
      }
   }
   /* CMD55 was rejected it is a mmc card or no card present */
   else
   {
      /* send cmd1 */
      while ((!valid_voltage) && (count < SD_MAX_VOLT_TRIAL))
      {
         delay_ms (10);

         sdio_command_init_struct.argument  = SD_VOLTAGE_WINDOW_MMC;
         sdio_command_init_struct.cmd_index = SD_CMD_SEND_OP_COND;
         sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
         sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

         /* sdio command config */
         sdio_command_config (SDIOx, &sdio_command_init_struct);
         /* enable ccsm */
         sdio_command_state_machine_enable (SDIOx, TRUE);

         /* waiting R3 */
         status = command_rsp3_error ();

         if (status != SD_OK)
         {
            return status;
         }

         /* get response1 */
         response      = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

         valid_voltage = (((response >> 31) == 1) ? 1 : 0);
         count++;
      }

      if (count >= SD_MAX_VOLT_TRIAL)
      {
         return SD_INVALID_VOLTRANGE;
      }

      card_type = SDIO_MULTIMEDIA_CARD;
   }

   return status;
}

/**
 * @brief  turns the sdio output signals off.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_power_off (void)
{
   /* sdio power off */
   sdio_power_set (SDIOx, SDIO_POWER_OFF);

   return SD_OK;
}

/**
 * @brief  intialises all cards or single card as the case may be card(s) come
 *         into standby state.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_card_init (void)
{
   sdio_error_t status = SD_OK;
   uint16_t rca_temp   = 0x01;

   /* check power status */
   if (SDIO_POWER_OFF == sdio_power_status_get (SDIOx))
   {
      return SD_REQ_NOT_APPLICABLE;
   }

   /* check is secure_digital_io_card or not */
   if (SDIO_SECURE_DIGITAL_IO_CARD != card_type)
   {
      /* send cmd2, get cid register */
      sdio_command_init_struct.argument  = 0x0;
      sdio_command_init_struct.cmd_index = SD_CMD_ALL_SEND_CID;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_LONG;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp2_error ();
      if (status != SD_OK)
      {
         return status;
      }

      cid_table[0] = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);
      cid_table[1] = sdio_response_get (SDIOx, SDIO_RSP2_INDEX);
      cid_table[2] = sdio_response_get (SDIOx, SDIO_RSP3_INDEX);
      cid_table[3] = sdio_response_get (SDIOx, SDIO_RSP4_INDEX);
   }

   /* check card type */
   if ((card_type == SDIO_STD_CAPACITY_SD_CARD_V1_1) ||
       (card_type == SDIO_STD_CAPACITY_SD_CARD_V2_0) ||
       (card_type == SDIO_SECURE_DIGITAL_IO_COMBO_CARD) ||
       (card_type == SDIO_HIGH_CAPACITY_SD_CARD))
   {
      /* send cmd3, get rca */
      sdio_command_init_struct.argument  = 0x00;
      sdio_command_init_struct.cmd_index = SD_CMD_SET_REL_ADDR;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp6_error (SD_CMD_SET_REL_ADDR, &rca_temp);
      if (status != SD_OK)
      {
         return status;
      }
   }

   /* mmc card */
   if (card_type == SDIO_MULTIMEDIA_CARD)
   {
      /* send cmd3 */
      sdio_command_init_struct.argument  = (uint32_t) (rca_temp << 16);
      sdio_command_init_struct.cmd_index = SD_CMD_SET_REL_ADDR;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp2_error ();
      if (status != SD_OK)
      {
         return status;
      }
   }

   /* check is secure_digital_io_card or not */
   if (card_type != SDIO_SECURE_DIGITAL_IO_CARD)
   {
      rca                                = rca_temp;
      sdio_command_init_struct.argument  = (uint32_t) (rca << 16);
      sdio_command_init_struct.cmd_index = SD_CMD_SEND_CSD;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_LONG;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp2_error ();
      if (status != SD_OK)
      {
         return status;
      }

      csd_table[0] = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);
      csd_table[1] = sdio_response_get (SDIOx, SDIO_RSP2_INDEX);
      csd_table[2] = sdio_response_get (SDIOx, SDIO_RSP3_INDEX);
      csd_table[3] = sdio_response_get (SDIOx, SDIO_RSP4_INDEX);
   }

   return SD_OK;
}

/**
 * @brief  enable wide bus opeartion for the requeseted card if supported by
 *         card.
 * @param  mode: specifies the sd card wide bus mode.
 *   this parameter can be one of the following values:
 *     @arg SDIO_BUS_WIDTH_D8: 8-bit data transfer (only for mmc)
 *     @arg SDIO_BUS_WIDTH_D4: 4-bit data transfer
 *     @arg SDIO_BUS_WIDTH_D1: 1-bit data transfer
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_wide_bus_operation_config (sdio_bus_width_type mode)
{
   sdio_error_t status = SD_OK;

   if ((card_type == SDIO_MULTIMEDIA_CARD) ||
       (card_type == SDIO_HIGH_SPEED_MULTIMEDIA_CARD))
   {
      status = mmc_switch (EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH,
                           (uint8_t) mode);
   }
   else if ((card_type == SDIO_STD_CAPACITY_SD_CARD_V1_1) ||
            (card_type == SDIO_STD_CAPACITY_SD_CARD_V2_0) ||
            (card_type == SDIO_HIGH_CAPACITY_SD_CARD))
   {
      if (mode >= 2)
      {
         /* not support D8 mode */
         return SD_UNSUPPORTED_FEATURE;
      }

      if (SDIO_BUS_WIDTH_D4 == mode)
      {
         status = sdio_bus_wide_enable (TRUE);
      }
      else
      {
         status = sdio_bus_wide_enable (FALSE);
      }
   }

   if (status == SD_OK)
   {
      sdio_bus_width_config (SDIOx, mode);
   }

   return status;
}

/**
 * @brief  set sdio work mode.
 * @param  mode
 *         parameter as following values:
 *         - SD_TRANSFER_POLLING_MODE: dma mode.
 *         - SD_TRANSFER_POLLING_MODE: polling mode.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_device_mode_set (uint32_t mode)
{
   sdio_error_t status = SD_OK;

   if ((mode == SD_TRANSFER_DMA_MODE) || (mode == SD_TRANSFER_POLLING_MODE))
   {
      device_mode = (sd_data_transfer_mode_type) mode;
   }
   else
   {
      status = SD_INVALID_PARAMETER;
   }

   return status;
}

/**
 * @brief  selects or deselects the corresponding card.
 * @param  addr: address of the card to be selected.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_deselect_select (uint32_t addr)
{
   /* send cmd7, select card */
   sdio_command_init_struct.argument  = addr;
   sdio_command_init_struct.cmd_index = SD_CMD_SEL_DESEL_CARD;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_INT;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   return command_rsp1_error (SD_CMD_SEL_DESEL_CARD);
}

/**
 * @brief  Initializes sdio peripheral and card.
 *         Card if present is put into standby
 *          state (ready for data transfer).
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_init (void)
{
    uint16_t clkdiv                 = 0;
    sdio_error_t status             = SD_OK;
    gpio_init_type gpio_init_struct = {0};
    uint8_t retry                   = 3;
    crm_clocks_freq_type clocks;

    memset(&card_info, 0, sizeof(card_info));

    crm_periph_clock_enable (CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable (CRM_SDIO1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable (CRM_IOMUX_PERIPH_CLOCK, TRUE);

    crm_clocks_freq_get(&clocks);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init_struct.gpio_pins =
        GPIO_PINS_2 | GPIO_PINS_3 | GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6 | GPIO_PINS_7;
    gpio_init (GPIOA, &gpio_init_struct);

    gpio_pin_remap_config(SDIO1_GMUX_0111, TRUE);

    nvic_irq_enable (SDIO1_IRQn, 0, 0);

    while (retry--)
    {
        sdio_reset (SDIOx);
        /* power up and low level card initialization*/
        status = sd_mmc_power_on ();

        if (status == SD_OK)
            break;
    }

    if (status == SD_OK)
    {
        status = sd_card_init ();
    }

    if (status == SD_OK)
    {
        sd_card_info_parse (&card_info);
    }

    if ((card_info.type == SDIO_MULTIMEDIA_CARD) &&
       (card_info.csd.spec_version >= 4))
    {
        card_info.type = SDIO_HIGH_SPEED_MULTIMEDIA_CARD;
    }

    if (status == SD_OK)
    {
        /* select card */
        status = sd_deselect_select ((uint32_t) (card_info.rca << 16));
    }

    if (status == SD_OK && ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == card_info.type) ||
                           (SDIO_STD_CAPACITY_SD_CARD_V2_0 == card_info.type) ||
                           (SDIO_SECURE_DIGITAL_IO_COMBO_CARD == card_info.type) ||
                           (SDIO_HIGH_CAPACITY_SD_CARD == card_info.type)))
    {
        status = sd_scr_read ((uint32_t*)&card_info.scr);
    }

    if (status == SD_OK)
    {
        /* set normal speed */
        status = sd_speed_change (0);
    }

    if ((status == SD_OK) || (card_type == SDIO_MULTIMEDIA_CARD))
    {
        if (card_info.type == SDIO_STD_CAPACITY_SD_CARD_V1_1 ||
            card_info.type == SDIO_STD_CAPACITY_SD_CARD_V2_0)
        {
            /* set sdio_ck to 12mhz */
            clkdiv = clocks.ahb_freq / 12000000;

            if (clkdiv >= 2)
            {
                clkdiv -= 2;
            }
        }
        else
        {
            /* set sdio_ck to 48mhz */
            clkdiv = clocks.ahb_freq / 48000000;

            if (clkdiv >= 2)
            {
                clkdiv -= 2;
            }
        }
        /* set sdio clock divider */
        sdio_clock_set (clkdiv);

        /* set transfer mode */
        status = sd_device_mode_set (SDIOx_TRANSFER_MODE);
    }

    if (status == SD_OK)
    {
        /* Set data width */
        //status = sd_wide_bus_operation_config (SDIO_BUS_WIDTH_D4);
    }

   return status;
}

/**
 * @brief  returns information about specific card.
 * @param  card_info: pointer to a sdio_info_t structure
 *                    that contains will receive info.
 *                    CSD and CID are read on sd_card_init
 *                    and parsed here
 * @retval none
 */
static void sd_card_info_parse (card_info_t *card_info)
{
   uint8_t tmp          = 0;

   card_info->type      = (uint8_t) card_type;
   card_info->rca       = (uint16_t) rca;

   /* byte 0 */
   tmp                  = (uint8_t) ((csd_table[0] & 0xFF000000) >> 24);
   card_info->csd.csd_struct   = (tmp & 0xC0) >> 6;
   card_info->csd.spec_version = (tmp & 0x3C) >> 2;
   card_info->csd.reserved1    = tmp & 0x03;

   /* byte 1 */
   tmp                        = (uint8_t) ((csd_table[0] & 0x00FF0000) >> 16);
   card_info->csd.taac = tmp;

   /* byte 2 */
   tmp                        = (uint8_t) ((csd_table[0] & 0x0000FF00) >> 8);
   card_info->csd.nsac = tmp;

   /* byte 3 */
   tmp                        = (uint8_t) (csd_table[0] & 0x000000FF);
   card_info->csd.max_bus_clk_freq = tmp;

   /* byte 4 */
   tmp = (uint8_t) ((csd_table[1] & 0xFF000000) >> 24);
   card_info->csd.card_cmd_classes = tmp << 4;

   /* byte 5 */
   tmp = (uint8_t) ((csd_table[1] & 0x00FF0000) >> 16);
   card_info->csd.card_cmd_classes |= (tmp & 0xF0) >> 4;
   card_info->csd.max_read_blk_length = tmp & 0x0F;

   /* byte 6 */
   tmp = (uint8_t) ((csd_table[1] & 0x0000FF00) >> 8);
   card_info->csd.part_blk_read      = (tmp & 0x80) >> 7;
   card_info->csd.write_blk_misalign = (tmp & 0x40) >> 6;
   card_info->csd.read_blk_misalign  = (tmp & 0x20) >> 5;
   card_info->csd.dsr_implemented    = (tmp & 0x10) >> 4;
   card_info->csd.reserved2          = 0; /* reserved */

   if ((card_type == SDIO_STD_CAPACITY_SD_CARD_V1_1) ||
       (card_type == SDIO_STD_CAPACITY_SD_CARD_V2_0) ||
       (card_type == SDIO_MULTIMEDIA_CARD))
   {
      card_info->csd.device_size = (tmp & 0x03) << 10;

      /* byte 7 */
      tmp                               = (uint8_t) (csd_table[1] & 0x000000FF);
      card_info->csd.device_size |= (tmp) << 2;

      /* byte 8 */
      tmp = (uint8_t) ((csd_table[2] & 0xFF000000) >> 24);
      card_info->csd.device_size |= (tmp & 0xC0) >> 6;

      card_info->csd.max_read_current_vdd_min = (tmp & 0x38) >> 3;
      card_info->csd.max_read_current_vdd_max = (tmp & 0x07);

      /* byte 9 */
      tmp = (uint8_t) ((csd_table[2] & 0x00FF0000) >> 16);
      card_info->csd.max_write_current_vdd_min = (tmp & 0xE0) >> 5;
      card_info->csd.max_write_current_vdd_max = (tmp & 0x1C) >> 2;
      card_info->csd.device_size_mult          = (tmp & 0x03) << 1;
      /* byte 10 */
      tmp = (uint8_t) ((csd_table[2] & 0x0000FF00) >> 8);
      card_info->csd.device_size_mult |= (tmp & 0x80) >> 7;

      card_info->capacity = (card_info->csd.device_size + 1);
      card_info->capacity *=
          (1 << (card_info->csd.device_size_mult + 2));
      card_info->block_size = 1
                                 << (card_info->csd.max_read_blk_length);
      card_info->capacity *= card_info->block_size;
   }
   else if (card_type == SDIO_HIGH_CAPACITY_SD_CARD)
   {
      /* byte 7 */
      tmp                               = (uint8_t) (csd_table[1] & 0x000000FF);
      card_info->csd.device_size = (tmp & 0x3F) << 16;

      /* byte 8 */
      tmp = (uint8_t) ((csd_table[2] & 0xFF000000) >> 24);

      card_info->csd.device_size |= (tmp << 8);

      /* byte 9 */
      tmp = (uint8_t) ((csd_table[2] & 0x00FF0000) >> 16);

      card_info->csd.device_size |= (tmp);

      /* byte 10 */
      tmp = (uint8_t) ((csd_table[2] & 0x0000FF00) >> 8);

      card_info->capacity =
          (uint64_t) (card_info->csd.device_size + 1) * 512 * 1024;
      card_info->block_size = 512;
   }


   card_info->csd.erase_group_size      = (tmp & 0x40) >> 6;
   card_info->csd.erase_group_size_mult = (tmp & 0x3F) << 1;

   /* byte 11 */
   tmp = (uint8_t) (csd_table[2] & 0x000000FF);
   card_info->csd.erase_group_size_mult |= (tmp & 0x80) >> 7;
   card_info->csd.write_protect_group_size = (tmp & 0x7F);

   /* byte 12 */
   tmp = (uint8_t) ((csd_table[3] & 0xFF000000) >> 24);
   card_info->csd.write_protect_group_enable = (tmp & 0x80) >> 7;
   card_info->csd.manufacturer_default_ecc   = (tmp & 0x60) >> 5;
   card_info->csd.write_speed_factor         = (tmp & 0x1C) >> 2;
   card_info->csd.max_write_blk_length       = (tmp & 0x03) << 2;

   /* byte 13 */
   tmp = (uint8_t) ((csd_table[3] & 0x00FF0000) >> 16);
   card_info->csd.max_write_blk_length |= (tmp & 0xC0) >> 6;
   card_info->csd.part_blk_write      = (tmp & 0x20) >> 5;
   card_info->csd.reserved3           = 0;
   card_info->csd.content_protect_app = (tmp & 0x01);

   /* byte 14 */
   tmp = (uint8_t) ((csd_table[3] & 0x0000FF00) >> 8);
   card_info->csd.file_format_group       = (tmp & 0x80) >> 7;
   card_info->csd.copy_flag               = (tmp & 0x40) >> 6;
   card_info->csd.permanent_write_protect = (tmp & 0x20) >> 5;
   card_info->csd.temp_write_protect      = (tmp & 0x10) >> 4;
   card_info->csd.file_formart            = (tmp & 0x0C) >> 2;
   card_info->csd.ecc_code                = (tmp & 0x03);

   /* byte 15 */
   tmp                             = (uint8_t) (csd_table[3] & 0x000000FF);
   card_info->csd.csd_crc   = (tmp & 0xFE) >> 1;
   card_info->csd.reserved4 = 1;

   /* byte 0 */
   tmp = (uint8_t) ((cid_table[0] & 0xFF000000) >> 24);
   card_info->cid.manufacturer_id = tmp;

   /* byte 1 */
   tmp = (uint8_t) ((cid_table[0] & 0x00FF0000) >> 16);
   card_info->cid.oem_app_id = tmp << 8;

   /* byte 2 */
   tmp = (uint8_t) ((cid_table[0] & 0x000000FF00) >> 8);
   card_info->cid.oem_app_id |= tmp;

   /* byte 3 */
   tmp                                 = (uint8_t) (cid_table[0] & 0x000000FF);
   card_info->cid.product_name1 = tmp << 24;

   /* byte 4 */
   tmp = (uint8_t) ((cid_table[1] & 0xFF000000) >> 24);
   card_info->cid.product_name1 |= tmp << 16;

   /* byte 5 */
   tmp = (uint8_t) ((cid_table[1] & 0x00FF0000) >> 16);
   card_info->cid.product_name1 |= tmp << 8;

   /* byte 6 */
   tmp = (uint8_t) ((cid_table[1] & 0x0000FF00) >> 8);
   card_info->cid.product_name1 |= tmp;

   /* byte 7 */
   tmp                                 = (uint8_t) (cid_table[1] & 0x000000FF);
   card_info->cid.product_name2 = tmp;

   /* byte 8 */
   tmp = (uint8_t) ((cid_table[2] & 0xFF000000) >> 24);
   card_info->cid.product_reversion = tmp;

   /* byte 9 */
   tmp = (uint8_t) ((cid_table[2] & 0x00FF0000) >> 16);
   card_info->cid.product_sn = tmp << 24;

   /* byte 10 */
   tmp = (uint8_t) ((cid_table[2] & 0x0000FF00) >> 8);
   card_info->cid.product_sn |= tmp << 16;

   /* byte 11 */
   tmp = (uint8_t) (cid_table[2] & 0x000000FF);
   card_info->cid.product_sn |= tmp << 8;

   /* byte 12 */
   tmp = (uint8_t) ((cid_table[3] & 0xFF000000) >> 24);
   card_info->cid.product_sn |= tmp;

   /* byte 13 */
   tmp = (uint8_t) ((cid_table[3] & 0x00FF0000) >> 16);
   card_info->cid.reserved1 |= (tmp & 0xF0) >> 4;
   card_info->cid.manufact_date = (tmp & 0x0F) << 8;

   /* byte 14 */
   tmp = (uint8_t) ((cid_table[3] & 0x0000FF00) >> 8);
   card_info->cid.manufact_date |= tmp;

   /* byte 15 */
   tmp                             = (uint8_t) (cid_table[3] & 0x000000FF);
   card_info->cid.cid_crc   = (tmp & 0xFE) >> 1;
   card_info->cid.reserved2 = 1;
}

card_info_t *sd_card_info_get (void)
{
    return &card_info;
}

/**
 * @brief  erase continuous data block
 * @param  addr: starting address
 * @param  nblks: amount of erasing data block
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_block_erase (uint64_t addr, uint32_t nblks)
{
   sdio_error_t status = SD_OK;
   uint32_t start_addr = 0, end_addr = 0, response = 0;
   uint8_t card_state;

   if (card_info.type == SDIO_HIGH_CAPACITY_SD_CARD)
   {
      start_addr = addr >> 9;
      end_addr   = start_addr + nblks - 1;
   }
   else
   {
      start_addr = addr;
      end_addr   = start_addr + (nblks - 1) * 512;
   }

   /* clear dcsm configuration */
   sdio_data_init_struct.block_size         = SDIO_DATA_BLOCK_SIZE_1B;
   sdio_data_init_struct.data_length        = 0;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, FALSE);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }

   if (card_info.type == SDIO_MULTIMEDIA_CARD ||
       card_info.type == SDIO_HIGH_SPEED_MULTIMEDIA_CARD)
   {
      /* send cmd35, set erase group start */
      sdio_command_init_struct.argument  = start_addr;
      sdio_command_init_struct.cmd_index = SD_CMD_ERASE_GRP_START;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_ERASE_GRP_START);

      if (status != SD_OK)
      {
         return status;
      }

      /* send cmd36, set erase group end */
      sdio_command_init_struct.argument  = end_addr;
      sdio_command_init_struct.cmd_index = SD_CMD_ERASE_GRP_END;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_ERASE_GRP_END);

      if (status != SD_OK)
      {
         return status;
      }

      /* send cmd38, start erase gourp */
      sdio_command_init_struct.argument  = 0;
      sdio_command_init_struct.cmd_index = SD_CMD_ERASE;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_ERASE);

      if (status != SD_OK)
      {
         return status;
      }
   }
   else
   {
      /* send cmd32, set erase block start */
      sdio_command_init_struct.argument  = start_addr;
      sdio_command_init_struct.cmd_index = SD_CMD_SD_ERASE_GRP_START;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_SD_ERASE_GRP_START);

      if (status != SD_OK)
      {
         return status;
      }

      /* send cmd33, set erase block end */
      sdio_command_init_struct.argument  = end_addr;
      sdio_command_init_struct.cmd_index = SD_CMD_SD_ERASE_GRP_END;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_SD_ERASE_GRP_END);

      if (status != SD_OK)
      {
         return status;
      }

      /* send cmd38, start erase block */
      sdio_command_init_struct.argument  = 0;
      sdio_command_init_struct.cmd_index = SD_CMD_ERASE;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_ERASE);

      if (status != SD_OK)
      {
         return status;
      }
   }

   status = sd_programming_status (&card_state);

   while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                (card_state == SD_CARD_RECEIVING)))
   {
      status = sd_programming_status (&card_state);
   }

   return status;
}

/**
 * @brief  allows to read one block from a specified address in a card. the data
 *         transfer can be managed by dma mode or polling mode.
 * @param  buf: pointer to the buffer that will contain the received data
 * @param  addr: address from where data are to be read.
 * @param  blk_size: the sd card data block size.
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_block_read (uint8_t *buf, uint64_t addr, uint16_t blk_size)
{
   sdio_error_t status = SD_OK;
   uint32_t response   = 0;
   uint8_t power;

   if ((NULL == buf) || (blk_size == 0) || (blk_size > 2048)
       || ((blk_size & (blk_size - 1))))
   {
      return SD_INVALID_PARAMETER;
   }

   SDIOx->dtctrl = 0x0;

   if (card_info.type == SDIO_HIGH_CAPACITY_SD_CARD)
   {
      blk_size = 512;
      addr >>= 9;
   }

   /* clear dcsm configuration */
   sdio_data_init_struct.block_size         = SDIO_DATA_BLOCK_SIZE_1B;
   sdio_data_init_struct.data_length        = 0;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, FALSE);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }

   power = convert_from_bytes_to_power_of_two (blk_size);

   /* send cmd16, set block size */
   sdio_command_init_struct.argument  = blk_size;
   sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (SD_CMD_SET_BLOCKLEN);

   if (status != SD_OK)
   {
         return status;
   }

   sdio_data_init_struct.block_size         = (sdio_block_size_type) (power);
   sdio_data_init_struct.data_length        = blk_size;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_command_init_struct.argument        = addr;
   sdio_command_init_struct.cmd_index       = SD_CMD_READ_SINGLE_BLOCK;
   sdio_command_init_struct.rsp_type        = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type       = SDIO_WAIT_FOR_NO;

   stop_flag                                = 0;

   return sdio_command_data_send (&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *) buf);
}

/**
 * @brief  allows to read blocks from a specified address  in a card. the data
 *         transfer can be managed by dma mode or polling mode.
 * @param  buf: pointer to the buffer that will contain the received data.
 * @param  addr: address from where data are to be read.
 * @param  blk_size: the sd card data block size. the block size should be 512.
 * @param  nblks: number of blocks to be read.
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_block_multi_read (uint8_t *buf, long long addr,
                                  uint16_t blk_size, uint32_t nblks)
{
   sdio_error_t status = SD_OK;
   uint32_t response   = 0;
   uint8_t power;

   SDIOx->dtctrl = 0x0;

   if (card_type == SDIO_HIGH_CAPACITY_SD_CARD)
   {
      blk_size = 512;
      addr >>= 9;
   }

   /* clear dcsm configuration */
   sdio_data_init_struct.block_size         = (sdio_block_size_type) 0;
   sdio_data_init_struct.data_length        = 0;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, FALSE);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }

   if ((blk_size > 0) && (blk_size <= 2048) &&
       ((blk_size & (blk_size - 1)) == 0))
   {
      power = convert_from_bytes_to_power_of_two (blk_size);

      /* send cmd16, set block size */
      sdio_command_init_struct.argument  = blk_size;
      sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_SET_BLOCKLEN);

      if (status != SD_OK)
      {
         return status;
      }
   }
   else
   {
      return SD_INVALID_PARAMETER;
   }

   /* check max receive length */
   if (nblks * blk_size > SD_MAX_DATA_LENGTH)
   {
      return SD_INVALID_PARAMETER;
   }

   sdio_data_init_struct.block_size         = (sdio_block_size_type) (power);
   sdio_data_init_struct.data_length        = nblks * blk_size;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   /* send cmd18, read block data */
   sdio_command_init_struct.argument        = addr;
   sdio_command_init_struct.cmd_index       = SD_CMD_READ_MULT_BLOCK;
   sdio_command_init_struct.rsp_type        = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type       = SDIO_WAIT_FOR_NO;

   stop_flag                                = 1;
   return sdio_command_data_send (&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *) buf);
}

/**
 * @brief  allows to write one block starting from a specified address in a
 * card. the data transfer can be managed by dma mode or polling mode.
 * @param  buf: pointer to the buffer that contain the data to be transferred.
 * @param  addr: address from where data are to be read.
 * @param  blk_size: the sd card data block size. the block size should be 512.
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_block_write (const uint8_t *buf, long long addr,
                             uint16_t blk_size)
{
   sdio_error_t status = SD_OK;
   uint8_t power = 0, card_state = 0;
   uint32_t timeout = 0, card_status = 0, response = 0;

   if (buf == NULL)
   {
      return SD_INVALID_PARAMETER;
   }

   SDIOx->dtctrl                            = 0x0;

   /* clear dcsm configuration */
   sdio_data_init_struct.block_size         = (sdio_block_size_type) 0;
   sdio_data_init_struct.data_length        = 0;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, FALSE);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }

   if (card_info.type == SDIO_HIGH_CAPACITY_SD_CARD)
   {
      blk_size = 512;
      addr >>= 9;
   }

   if ((blk_size > 0) && (blk_size <= 2048) &&
       ((blk_size & (blk_size - 1)) == 0))
   {
      power = convert_from_bytes_to_power_of_two (blk_size);

      /* send cmd16, set block size */
      sdio_command_init_struct.argument  = blk_size;
      sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_SET_BLOCKLEN);

      if (status != SD_OK)
      {
         return status;
      }
   }
   else
   {
      return SD_INVALID_PARAMETER;
   }

   timeout = SD_DATATIMEOUT;

   do
   {
      timeout--;
      status = sd_status_send (&card_status);
   }
   /* check ready_for_data flag */
   while (((card_status & 0x00000100) == 0) && (timeout > 0));

   if (timeout == 0)
   {
      return SD_ERROR;
   }

   /* send cmd24, write single block */
   sdio_command_init_struct.argument        = addr;
   sdio_command_init_struct.cmd_index       = SD_CMD_WRITE_SINGLE_BLOCK;
   sdio_command_init_struct.rsp_type        = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type       = SDIO_WAIT_FOR_NO;

   sdio_data_init_struct.block_size         = (sdio_block_size_type) (power);
   sdio_data_init_struct.data_length        = blk_size;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   stop_flag                                = 0;

   /* single block, stop command is unnecessary */
   status = sdio_command_data_send (&sdio_command_init_struct,
                                    &sdio_data_init_struct, (uint32_t *) buf);

   if (status != SD_OK)
   {
      return status;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   status = sd_programming_status (&card_state);

   while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                (card_state == SD_CARD_RECEIVING)))
   {
      status = sd_programming_status (&card_state);
   }

   return status;
}

/**
 * @brief  allows to write blocks starting from a specified address in a card.
 *         the data transfer can be managed by dma mode only.
 * @param  buf: pointer to the buffer that contain the data to be transferred.
 * @param  addr: address from where data are to be read.
 * @param  blk_size: the sd card data block size. the block size should be 512.
 * @param  nblks: number of blocks to be written.
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_block_multi_write (const uint8_t *buf, long long addr,
                                   uint16_t blk_size, uint32_t nblks)
{
   sdio_error_t status = SD_OK;
   uint8_t power = 0, card_state = 0;
   uint32_t timeout = 0, card_status = 0, response = 0;

   if (buf == NULL)
   {
      return SD_INVALID_PARAMETER;
   }

   SDIOx->dtctrl                            = 0x0;

   /* clear dcsm configuration */
   sdio_data_init_struct.block_size         = (sdio_block_size_type) 0;
   sdio_data_init_struct.data_length        = 0;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, FALSE);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }

   if (card_type == SDIO_HIGH_CAPACITY_SD_CARD)
   {
      blk_size = 512;
      addr >>= 9;
   }

   if ((blk_size > 0) && (blk_size <= 2048) &&
       ((blk_size & (blk_size - 1)) == 0))
   {
      power = convert_from_bytes_to_power_of_two (blk_size);

      /* send cmd16, set block size */
      sdio_command_init_struct.argument  = blk_size;
      sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_SET_BLOCKLEN);

      if (status != SD_OK)
      {
         return status;
      }
   }
   else
   {
      return SD_INVALID_PARAMETER;
   }

   if ((nblks * blk_size) > SD_MAX_DATA_LENGTH)
   {
      return SD_INVALID_PARAMETER;
   }

   timeout = SD_DATATIMEOUT;

   do
   {
      timeout--;
      status = sd_status_send (&card_status);
   }
   /* check ready_for_data flag */
   while (((card_status & 0x00000100) == 0) && (timeout > 0));

   if (timeout == 0)
   {
      return SD_ERROR;
   }

   if ((card_type == SDIO_STD_CAPACITY_SD_CARD_V1_1) ||
       (card_type == SDIO_STD_CAPACITY_SD_CARD_V2_0) ||
       (card_type == SDIO_HIGH_CAPACITY_SD_CARD))
   {
      /* send cmd55 */
      sdio_command_init_struct.argument  = (uint32_t) rca << 16;
      sdio_command_init_struct.cmd_index = SD_CMD_APP_CMD;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_APP_CMD);

      if (status != SD_OK)
      {
         return status;
      }

      /* send cmd23, set block count */
      sdio_command_init_struct.argument  = blk_size;
      sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCK_COUNT;
      sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
      sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

      /* sdio command config */
      sdio_command_config (SDIOx, &sdio_command_init_struct);
      /* enable ccsm */
      sdio_command_state_machine_enable (SDIOx, TRUE);

      status = command_rsp1_error (SD_CMD_SET_BLOCK_COUNT);

      if (status != SD_OK)
      {
         return status;
      }
   }

   /* send cmd25, write mult blocks */
   sdio_command_init_struct.argument        = addr;
   sdio_command_init_struct.cmd_index       = SD_CMD_WRITE_MULT_BLOCK;
   sdio_command_init_struct.rsp_type        = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type       = SDIO_WAIT_FOR_NO;

   sdio_data_init_struct.block_size         = (sdio_block_size_type) (power);
   sdio_data_init_struct.data_length        = nblks * blk_size;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_BLOCK_TRANSFER;

   stop_flag                                = 1;
   /* cmd12 is needed */
   status = sdio_command_data_send (&sdio_command_init_struct,
                                    &sdio_data_init_struct, (uint32_t *) buf);

   if (status != SD_OK)
   {
      return status;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   status = sd_programming_status (&card_state);

   while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                (card_state == SD_CARD_RECEIVING)))
   {
      status = sd_programming_status (&card_state);
   }

   return status;
}

/**
 * @brief  read mmc card data stream.
 * @param  buf: buffer of saving data from mmc card
 * @param  addr: start address of data from mmc card
 * @param  len: data size
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t mmc_stream_read (uint8_t *buf, long long addr, uint32_t len)
{
   uint32_t response                        = 0;

   SDIOx->dtctrl                            = 0x0;

   /* clear dcsm configuration */
   sdio_data_init_struct.block_size         = SDIO_DATA_BLOCK_SIZE_1B;
   sdio_data_init_struct.data_length        = 0;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_STREAM_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, FALSE);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }
   /* send cmd11, read data */
   sdio_command_init_struct.argument        = addr;
   sdio_command_init_struct.cmd_index       = SD_CMD_READ_DAT_UNTIL_STOP;
   sdio_command_init_struct.rsp_type        = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type       = SDIO_WAIT_FOR_NO;

   sdio_data_init_struct.block_size         = (sdio_block_size_type) (5 << 4);
   sdio_data_init_struct.data_length        = len;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_STREAM_TRANSFER;

   stop_flag                                = 1;
   /* cmd12 is needed */
   return sdio_command_data_send (&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *) buf);
}

/**
 * @brief  write mmc card data stream.
 * @param  buf: data that writing to mmc card
 * @param  addr: start address mmc card
 * @param  len: data size
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t mmc_stream_write (uint8_t *buf, long long addr, uint32_t len)
{
   sdio_error_t status = SD_OK;
   uint32_t response   = 0;
   uint8_t card_state  = 0;

   if (buf == NULL)
   {
      return SD_INVALID_PARAMETER;
   }

   SDIOx->dtctrl                            = 0x0;

   /* clear dcsm configuration */
   sdio_data_init_struct.block_size         = SDIO_DATA_BLOCK_SIZE_1B;
   sdio_data_init_struct.data_length        = 0;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_STREAM_TRANSFER;

   sdio_data_config (SDIOx, &sdio_data_init_struct);
   sdio_data_state_machine_enable (SDIOx, FALSE);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   /* check card locked */
   if (response & SD_CARD_LOCKED)
   {
      return SD_LOCK_UNLOCK_ERROR;
   }
   /* send cmd20, write data */
   sdio_command_init_struct.argument        = addr;
   sdio_command_init_struct.cmd_index       = SD_CMD_WRITE_DAT_UNTIL_STOP;
   sdio_command_init_struct.rsp_type        = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type       = SDIO_WAIT_FOR_NO;


   sdio_data_init_struct.block_size         = (sdio_block_size_type) (15 << 4);
   sdio_data_init_struct.data_length        = len;
   sdio_data_init_struct.timeout            = SD_DATATIMEOUT;
   sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
   sdio_data_init_struct.transfer_mode      = SDIO_DATA_STREAM_TRANSFER;

   stop_flag                                = 1;
   /* cmd12 is needed */
   status = sdio_command_data_send (&sdio_command_init_struct,
                                    &sdio_data_init_struct, (uint32_t *) buf);

   if (status != SD_OK)
   {
      return status;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   status = sd_programming_status (&card_state);

   while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                (card_state == SD_CARD_RECEIVING)))
   {
      status = sd_programming_status (&card_state);
   }

   return status;
}

/**
 * @brief  switch mmc card speed to high speed
 * @param  set: new state of the sdio wide bus mode.
 * @param  index: offset.
 * @param  value: expect speed.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t mmc_switch (uint8_t set, uint8_t index, uint8_t value)
{
   sdio_error_t status  = SD_OK;
   uint32_t card_status = 0, timeout = 0;

   sdio_command_init_struct.argument =
       (EXT_CSD_Write_byte << 24) | (index << 16) | (value << 8) | set;
   sdio_command_init_struct.cmd_index = SD_CMD_HS_SWITCH;
   sdio_command_init_struct.rsp_type  = SDIO_RESPONSE_SHORT;
   sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

   /* sdio command config */
   sdio_command_config (SDIOx, &sdio_command_init_struct);
   /* enable ccsm */
   sdio_command_state_machine_enable (SDIOx, TRUE);

   status = command_rsp1_error (SD_CMD_HS_SWITCH);
   if (status != SD_OK)
   {
      return status;
   }

   card_status = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);
   if (card_status & MMC_SWITCH_ERROR)
   {
      return SD_SWITCH_ERROR;
   }

   timeout = SD_DATATIMEOUT;
   do
   {
      timeout--;
      status = sd_status_send (&card_status);
   } /* check ready_for_data flag */
   while (((card_status & 0x00000100) == 0) && (timeout > 0));

   if (timeout == 0)
   {
      return SD_ERROR;
   }

   return status;
}

/**
 * @brief  checks for error conditions for cmd0.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_error (void)
{
   uint32_t timeout    = SDIO_CMD0TIMEOUT;

   while (timeout--)
   {
      if (sdio_flag_get (SDIOx, SDIO_CMDCMPL_FLAG) != RESET)
      {
        sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);
        return SD_OK;
      }
   }

   return SD_CMD_RSP_TIMEOUT;
}

/**
 * @brief  checks for error conditions for R7 response.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_rsp7_error (void)
{
   sdio_error_t status = SD_OK;
   uint32_t sts_reg    = 0;
   uint32_t timeout    = SDIO_CMD0TIMEOUT;

   while (timeout--)
   {
      sts_reg = SDIOx->sts;

      if (sts_reg &
          (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG))
      {
         break;
      }
   }

   if ((timeout == 0) || (sts_reg & SDIO_CMDTIMEOUT_FLAG))
   {
      status = SD_CMD_RSP_TIMEOUT;
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);
      return status;
   }

   if (sts_reg & SDIO_CMDRSPCMPL_FLAG)
   {
      status = SD_OK;
      sdio_flag_clear (SDIOx, SDIO_CMDRSPCMPL_FLAG);
   }

   return status;
}

/**
 * @brief  checks for error conditions for R1 response.
 * @param  cmd: the sent command index.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_rsp1_error (uint8_t cmd)
{
   uint32_t sts_reg = 0;

   while (1)
   {
      sts_reg = SDIOx->sts;

      if (sts_reg &
          (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG))
      {
         break;
      }
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);
      return SD_CMD_RSP_TIMEOUT;
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDFAIL_FLAG);
      return SD_CMD_FAIL;
   }

   if (sdio_command_response_get (SDIOx) != cmd)
   {
      return SD_ILLEGAL_CMD;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   return (sdio_error_t) (sdio_response_get (SDIOx, SDIO_RSP1_INDEX) &
                          SD_OCR_ERRORBITS);
}

/**
 * @brief  checks for error conditions for R3 (ocr) response.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_rsp3_error (void)
{
   uint32_t sts_reg = 0;
   ;

   while (1)
   {
      sts_reg = SDIOx->sts;

      if (sts_reg &
          (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG))
      {
         break;
      }
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);
      return SD_CMD_RSP_TIMEOUT;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   return SD_OK;
}

/**
 * @brief  checks for error conditions for R2 (cid or csd) response.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_rsp2_error (void)
{
   sdio_error_t status = SD_OK;
   uint32_t sts_reg;
   uint32_t timeout = SDIO_CMD0TIMEOUT;

   while (timeout--)
   {
      sts_reg = SDIOx->sts;

      if (sts_reg &
          (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG))
      {
         break;
      }
   }

   if ((timeout == 0) || sdio_flag_get (SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
   {
      status = SD_CMD_RSP_TIMEOUT;
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);

      return status;
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
   {
      status = SD_CMD_FAIL;
      sdio_flag_clear (SDIOx, SDIO_CMDFAIL_FLAG);
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   return status;
}

#if 0 /* Not used */
/**
 * @brief  checks for error conditions for r4 response.
 * @param  cmd: the sent command index.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_rsp4_error (uint8_t cmd)
{
   uint32_t sts_reg = 0, rsp_cmd = 0;

   while (1)
   {
      sts_reg = SDIOx->sts;

      if (sts_reg &
          (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG))
      {
         break;
      }
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);
      return SD_CMD_RSP_TIMEOUT;
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDFAIL_FLAG);

      return SD_CMD_FAIL;
   }

   rsp_cmd = sdio_command_response_get (SDIOx);
   if (rsp_cmd != cmd)
   {
      return SD_ILLEGAL_CMD;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   return SD_OK;
}

/**
 * @brief  checks for error conditions for r5 response.
 * @param  cmd: the sent command index.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_rsp5_error (uint8_t cmd)
{
   uint32_t sts_reg = 0, rsp_cmd = 0, response = 0;

   while (1)
   {
      sts_reg = SDIOx->sts;

      if (sts_reg &
          (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG))
      {
         break;
      }
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);

      return SD_CMD_RSP_TIMEOUT;
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDFAIL_FLAG);

      return SD_CMD_FAIL;
   }

   rsp_cmd = sdio_command_response_get (SDIOx);
   if (rsp_cmd != cmd)
   {
      return SD_ILLEGAL_CMD;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   if (response & SD_R5_OUT_OF_RANGE)
   {
      return SD_CMD_OUT_OF_RANGE;
   }

   if (response & SD_R5_FUNCTION_NUMBER)
   {
      return SD_SDIO_UNKNOWN_FUNC;
   }

   if (response & SD_R5_ERROR)
   {
      return SD_GENERAL_UNKNOWN_ERROR;
   }

   return SD_OK;
}
#endif
/**
 * @brief  checks for error conditions for r6 (rca) response.
 * @param  cmd: the sent command index.
 * @param  prca: pointer to the variable that will contain the sd card relative
 *         address rca.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t command_rsp6_error (uint8_t cmd, uint16_t *prca)
{
   sdio_error_t status = SD_OK;
   uint32_t sts_reg, rsp_cmd = 0, response = 0;

   while (1)
   {
      sts_reg = SDIOx->sts;

      if (sts_reg &
          (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG))
      {
         break;
      }
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDTIMEOUT_FLAG);
      return SD_CMD_RSP_TIMEOUT;
   }

   if (sdio_flag_get (SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
   {
      sdio_flag_clear (SDIOx, SDIO_CMDFAIL_FLAG);
      return SD_CMD_FAIL;
   }

   rsp_cmd = sdio_command_response_get (SDIOx);
   if (rsp_cmd != cmd)
   {
      return SD_ILLEGAL_CMD;
   }

   sdio_flag_clear (SDIOx, SDIO_STATIC_FLAGS);

   response = sdio_response_get (SDIOx, SDIO_RSP1_INDEX);

   if (SD_ALLZERO == (response & (SD_R6_GENERAL_UNKNOWN_ERROR |
                                  SD_R6_ILLEGAL_CMD | SD_R6_CMD_CRC_ERROR)))
   {
      *prca = (uint16_t) (response >> 16);
      return status;
   }

   if (response & SD_R6_GENERAL_UNKNOWN_ERROR)
   {
      return SD_GENERAL_UNKNOWN_ERROR;
   }

   if (response & SD_R6_ILLEGAL_CMD)
   {
      return SD_ILLEGAL_CMD;
   }

   if (response & SD_R6_CMD_CRC_ERROR)
   {
      return SD_CMD_CRC_ERROR;
   }

   return status;
}

sdio_error_t sd_read_disk (uint8_t *buf, uint32_t sector, uint8_t cnt)
{
   sdio_error_t sta  = SD_OK;
   uint64_t lsector = sector;
   uint8_t n;
   uint32_t block_size = card_info.block_size;

   /* data address is in block units. */
   lsector *= block_size;

   if ((uint32_t) buf % 4 != 0)
   {
      uint32_t sdio_data_buffer[block_size / 4];

      for (n = 0; n < cnt; n++)
      {
         sta = sd_block_read ((uint8_t*)sdio_data_buffer, lsector + block_size * n, block_size);
         memcpy (buf, sdio_data_buffer, block_size);
         buf += block_size;
      }
   }
   else
   {
      if (cnt == 1)
      {
         sta = sd_block_read (buf, lsector, block_size);
      }
      else
      {
         sta = sd_block_multi_read (buf, lsector, block_size, cnt);
      }
   }

   return sta;
}

/**
 * @brief  write sd card sector
 * @param  buf: write data buf
 * @param  sector: sector address
 * @param  cnt: sector count
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_write_disk (const uint8_t *buf, uint32_t sector, uint8_t cnt)
{
   sdio_error_t sta = SD_OK;
   uint8_t n;
   uint64_t lsector = sector;
   uint32_t block_size = card_info.block_size;

   /* data address is in block units. */
   lsector *= block_size;

   if ((uint32_t) buf % 4 != 0)
   {
      uint32_t sdio_data_buffer[block_size / 4];

      for (n = 0; n < cnt; n++)
      {
         memcpy (sdio_data_buffer, buf, block_size);
         sta = sd_block_write ((uint8_t*)sdio_data_buffer, lsector + block_size * n, block_size);
         buf += block_size;
      }
   }
   else
   {
      if (cnt == 1)
      {
         sta = sd_block_write (buf, lsector, block_size);
      }
      else
      {
         sta = sd_block_multi_write (buf, lsector, block_size, cnt);
      }
   }

   return sta;
}

/**
 * @brief  sdio1 isr.
 * @param  none.
 * @retval none.
 */
void SDIOx_IRQHandler (void)
{
   sd_irq_service ();
}
