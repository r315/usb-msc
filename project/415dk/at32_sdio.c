#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "at32_sdio.h"
#include "board.h"

static sdio_error_t mmc_switch(uint8_t set, uint8_t index, uint8_t value);
static sdio_error_t sdio_switch_speed(uint32_t mode, uint32_t group, uint8_t value, uint8_t *rsp);
static void sd_dma_config(uint32_t *mbuf, uint32_t buf_size, dma_dir_type dir);

static sdio_command_struct_type sdio_command_init_struct;
static sdio_data_struct_type sdio_data_init_struct;

static uint8_t stop_flag = 0;                 /* transmit stop flag */
volatile sdio_error_t transfer_error = SD_OK; /* transmit error flag */
volatile uint8_t transfer_end = 0;            /* transmit end flag */

static card_info_t card_info; /* Information of last initialized card */
static sdio_handle_t sdio_handle;

/**
 * @brief  Get exponent of the most significant bit that is set.
 *
 * @param  number_of_bytes:
 * @retval exponent
 */
uint8_t power_of_two(uint16_t number_of_bytes)
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
 * @brief  checks for error conditions for R1 response.
 * @param  cmd: the sent command index.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_rsp1(uint8_t cmd)
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

    if (sdio_flag_get(SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_CMDTIMEOUT_FLAG);
        return SD_CMD_RSP_TIMEOUT;
    }

    if (sdio_flag_get(SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_CMDFAIL_FLAG);
        return SD_CMD_FAIL;
    }

    if (sdio_command_response_get(SDIOx) != cmd)
    {
        return SD_ILLEGAL_CMD;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    return (sdio_error_t)(sdio_response_get(SDIOx, SDIO_RSP1_INDEX) &
                          SD_OCR_ERRORBITS);
}

/**
 * @brief  checks for error conditions for R2 (cid or csd) response.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_rsp2(void)
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

    if ((timeout == 0) || sdio_flag_get(SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
    {
        status = SD_CMD_RSP_TIMEOUT;
        sdio_flag_clear(SDIOx, SDIO_CMDTIMEOUT_FLAG);

        return status;
    }

    if (sdio_flag_get(SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
    {
        status = SD_CMD_FAIL;
        sdio_flag_clear(SDIOx, SDIO_CMDFAIL_FLAG);
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    return status;
}

/**
 * @brief  checks for error conditions for R3 (ocr) response.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_rsp3(void)
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

    if (sdio_flag_get(SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_CMDTIMEOUT_FLAG);
        return SD_CMD_RSP_TIMEOUT;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    return SD_OK;
}

/**
 * @brief  checks for error conditions for r6 (rca) response.
 * @param  cmd: the sent command index.
 * @param  prca: pointer to the variable that will contain the sd card relative
 *         address rca.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_rsp6(uint8_t cmd, uint32_t *prca)
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

    if (sdio_flag_get(SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_CMDTIMEOUT_FLAG);
        return SD_CMD_RSP_TIMEOUT;
    }

    if (sdio_flag_get(SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_CMDFAIL_FLAG);
        return SD_CMD_FAIL;
    }

    rsp_cmd = sdio_command_response_get(SDIOx);
    if (rsp_cmd != cmd)
    {
        return SD_ILLEGAL_CMD;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

    if (SD_ALLZERO == (response & (SD_R6_GENERAL_UNKNOWN_ERROR |
                                   SD_R6_ILLEGAL_CMD | SD_R6_CMD_CRC_ERROR)))
    {
        *prca = response >> 16;
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

/**
 * @brief  checks for error conditions for R7 response.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_rsp7(void)
{
    sdio_error_t status = SD_OK;
    uint32_t sts_reg = 0;
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

    if ((timeout == 0) || (sts_reg & SDIO_CMDTIMEOUT_FLAG))
    {
        status = SD_CMD_RSP_TIMEOUT;
        sdio_flag_clear(SDIOx, SDIO_CMDTIMEOUT_FLAG);
        return status;
    }

    if (sts_reg & SDIO_CMDRSPCMPL_FLAG)
    {
        status = SD_OK;
        sdio_flag_clear(SDIOx, SDIO_CMDRSPCMPL_FLAG);
    }

    return status;
}

static void sd_cmdx_send(sdio_command_struct_type *sdio_cmd)
{
    /* sdio command config */
    sdio_command_config(SDIOx, sdio_cmd);
    /* enable ccsm */
    sdio_command_state_machine_enable(SDIOx, TRUE);
}

/**
 * @brief GO_IDLE_STATE
 * @param
 * @return
 */
static sdio_error_t sd_cmd0(void)
{
    uint32_t timeout = SDIO_CMD0TIMEOUT;
    sdio_command_struct_type sdio_cmd = {
        .argument = 0,
        .cmd_index = SD_CMD_GO_IDLE_STATE,
        .rsp_type = SDIO_RESPONSE_NO,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    while (timeout--)
    {
        if (sdio_flag_get(SDIOx, SDIO_CMDCMPL_FLAG) != RESET)
        {
            sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);
            return SD_OK;
        }
    }

    return SD_CMD_RSP_TIMEOUT;
}

/**
 * @brief SEND_OP_COND
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd1(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_SEND_OP_COND,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp3();
}

/**
 * @brief ALL_SEND_CID
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd2(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_ALL_SEND_CID,
        .rsp_type = SDIO_RESPONSE_LONG,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp2();
}

/**
 * @brief SEND_RELATIVE_ADDR
 * @param argument
 * @param rca
 * @return
 */
static sdio_error_t sd_cmd3(uint32_t argument, uint32_t *rca)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_SET_REL_ADDR,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return (argument != 0) ?
        sd_rsp2() :
        sd_rsp6(SD_CMD_SET_REL_ADDR, rca);
}

/**
 * @brief SWITCH_FUNC
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd6(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_HS_SWITCH,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp1(SD_CMD_HS_SWITCH);
}

/**
 * @brief SELECT/DESELECT_CARD
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd7(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_SEL_DESEL_CARD,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_INT
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp1(SD_CMD_SEL_DESEL_CARD);
}



/**
 * @brief SEND_IF_COND
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd8(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SDIO_SEND_IF_COND,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp7();
}

/**
 * @brief SEND_CSD
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd9(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_SEND_CSD,
        .rsp_type = SDIO_RESPONSE_LONG,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp2();
}

/**
 * @brief SET_BLKLEN
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd16(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_SET_BLOCKLEN,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp1(SD_CMD_SET_BLOCKLEN);
}

/**
 * @brief APP_CMD
 * @param argument
 * @return
 */
static sdio_error_t sd_cmd55(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_APP_CMD,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp1(SD_CMD_APP_CMD);
}

/**
 * @brief SET_BUS_WIDTH
 * @param argument
 * @return
 */
static sdio_error_t sd_acmd6(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_APP_SD_SET_BUSWIDTH,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp1(SD_CMD_APP_SD_SET_BUSWIDTH);
}

/**
 * @brief SD_APP_OP_COND
 * @param argument
 * @return
 */
static sdio_error_t sd_acmd41(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_SD_APP_OP_COND,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp3();
}

/**
 * @brief SEND SCR
 * @param argument
 * @return
 */
static sdio_error_t sd_acmd51(uint32_t argument)
{
    sdio_command_struct_type sdio_cmd = {
        .argument = argument,
        .cmd_index = SD_CMD_SD_APP_SEND_SCR,
        .rsp_type = SDIO_RESPONSE_SHORT,
        .wait_type = SDIO_WAIT_FOR_NO
    };

    sd_cmdx_send(&sdio_cmd);

    return sd_rsp1(SD_CMD_SD_APP_SEND_SCR);
}

/**
 * @brief  read data from or write data to sd card.
 * @param  sdio_cmd_init_struct: pointer to sdio_command_struct_type structure.
 * @param  sdio_data_init_struct: pointer to sdio_data_struct_type structure.
 * @param  buf: pointer to data buffer read or write.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sdio_command_data_send(sdio_command_struct_type *sdio_cmd,
                                           sdio_data_struct_type *sdio_data,
                                           uint32_t *buf)
{
    sdio_error_t status = SD_OK;
    uint32_t count = 0;
    uint32_t timeout = SDIO_DATATIMEOUT;
    uint32_t length = 0;

    /* reset data control register */
    SDIOx->dtcnt = 0x0;
    /* enable data path */
    sdio_data_config(SDIOx, sdio_data);
    sdio_data_state_machine_enable(SDIOx, TRUE);

    length = sdio_data->data_length;

    if (SD_TRANSFER_DMA_MODE == sdio_handle.device_mode)
    {
        if (sdio_data->transfer_direction ==
            SDIO_DATA_TRANSFER_TO_CONTROLLER)
        {
            sd_dma_config(buf, length, DMA_DIR_PERIPHERAL_TO_MEMORY);
            SDIOx->inten |= SDIO_INTR_STS_READ_MASK;
            transfer_error = SD_OK;
            transfer_end = 0;
            sdio_dma_enable(SDIOx, TRUE);
        }
    }

    /* send sd command */
    sdio_command_config(SDIOx, sdio_cmd);
    sdio_command_state_machine_enable(SDIOx, TRUE);
    status = sd_rsp1(sdio_cmd->cmd_index);
    if (status != SD_OK)
    {
        return status;
    }

    /* polling mode */
    if (SD_TRANSFER_POLLING_MODE == sdio_handle.device_mode)
    {
        if (SDIO_DATA_TRANSFER_TO_CONTROLLER == sdio_data->transfer_direction)
        {
            /* Read data */
            while (!(SDIOx->sts & (SDIO_INTR_STS_READ_MASK)))
            {
                if (sdio_flag_get(SDIOx, SDIO_RXBUFH_FLAG))
                {
                    for (count = 0; count < 8; count++, buf++)
                    {
                        *buf = sdio_data_read(SDIOx);
                    }

                    timeout = 0x7FFFFF;
                }
                else
                {
                    if (0 == timeout)
                    {
                        return SD_DATA_TIMEOUT;
                    }

                    timeout--;
                }
            }

            while (sdio_flag_get(SDIOx, SDIO_RXBUF_FLAG) != RESET)
            {
                *buf = sdio_data_read(SDIOx);
                buf++;
            }
        }
        else
        {
            /* Write data */
            while (!(SDIOx->sts & SDIO_INTR_STS_WRITE_MASK))
            {
                if (sdio_flag_get(SDIOx, SDIO_TXBUFH_FLAG) != RESET)
                {
                    for (count = 0; count < 8 && length > 0;
                         count++, buf++, length -= 4)
                    {
                        sdio_data_write(SDIOx, *buf);
                    }

                    timeout = 0x3FFFFFFF;
                }
                else
                {
                    if (timeout == 0)
                    {
                        return SD_DATA_TIMEOUT;
                    }

                    timeout--;
                }
            }
        }
        /* data timeout */
        if (sdio_flag_get(SDIOx, SDIO_DTTIMEOUT_FLAG) != RESET)
        {
            sdio_flag_clear(SDIOx, SDIO_DTTIMEOUT_FLAG);
            return SD_DATA_TIMEOUT;
        }
        /* crc error */
        else if (sdio_flag_get(SDIOx, SDIO_DTFAIL_FLAG) != RESET)
        {
            sdio_flag_clear(SDIOx, SDIO_DTFAIL_FLAG);
            return SD_DATA_FAIL;
        }
        /* over run error */
        else if (sdio_flag_get(SDIOx, SDIO_RXERRO_FLAG) != RESET)
        {
            sdio_flag_clear(SDIOx, SDIO_RXERRO_FLAG);
            return SD_RX_OVERRUN;
        }
        /* under run error */
        else if (sdio_flag_get(SDIOx, SDIO_TXERRU_FLAG) != RESET)
        {
            sdio_flag_clear(SDIOx, SDIO_TXERRU_FLAG);
            return SD_TX_UNDERRUN;
        }
        /* start bit error */
        else if (sdio_flag_get(SDIOx, SDIO_SBITERR_FLAG) != RESET)
        {
            sdio_flag_clear(SDIOx, SDIO_SBITERR_FLAG);
            return SD_START_BIT_ERR;
        }
        /* data transfer complete */
        if ((stop_flag == 1) &&
            (sdio_flag_get(SDIOx, SDIO_DTCMPL_FLAG) != RESET))
        {
            /* send cmd12, stop transmission */
            sdio_cmd->argument = 0;
            sdio_cmd->cmd_index = SD_CMD_STOP_TRANSMISSION;
            sdio_cmd->rsp_type = SDIO_RESPONSE_SHORT;
            sdio_cmd->wait_type = SDIO_WAIT_FOR_NO;

            /* sdio command config */
            sdio_command_config(SDIOx, sdio_cmd);
            /* enable ccsm */
            sdio_command_state_machine_enable(SDIOx, TRUE);

            status = sd_rsp1(SD_CMD_STOP_TRANSMISSION);

            if (status != SD_OK)
            {
                return status;
            }
        }

        sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);
    }
    else if (SD_TRANSFER_DMA_MODE == sdio_handle.device_mode)
    {
        if (sdio_data->transfer_direction == SDIO_DATA_TRANSFER_TO_CARD)
        {
            sd_dma_config(buf, length, DMA_DIR_MEMORY_TO_PERIPHERAL);
            SDIOx->inten |= SDIO_INTR_STS_WRITE_MASK;
            transfer_error = SD_OK;
            transfer_end = 0;
            sdio_dma_enable(SDIOx, TRUE);
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
static sdio_error_t sd_speed_change(uint8_t speed)
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
        status = sdio_switch_speed(0, 0, speed, switch_sts);

        if (status != SD_OK)
        {
            return status;
        }

        if ((switch_sts[13] & (1 << speed)) == 0)
        {
            return SD_SWITCH_ERROR;
        }

        /* read it back for confirmation */
        status = sdio_switch_speed(1, 0, speed, switch_sts);

        if (status != 0)
        {
            return status;
        }

        if ((switch_sts[16] & 0xF) != speed)
        {
            return SD_SWITCH_ERROR;
        }
    }
    else if (card_info.type == SDIO_HIGH_SPEED_MULTIMEDIA_CARD)
    {
        status = mmc_switch(EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH,
                            (uint8_t)speed);

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
static sdio_error_t sdio_switch_speed(uint32_t mode, uint32_t group, uint8_t value,
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

    blk_size = 64;
    power = power_of_two(blk_size);
    /* Set data length */
    status = sd_cmd16(blk_size);

    if (status != SD_OK)
    {
        return status;
    }

    sdio_data_init_struct.block_size = (sdio_block_size_type)(power);
    sdio_data_init_struct.data_length = blk_size;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    sdio_command_init_struct.argument = (mode << 31) | 0x00FFFFFF;
    sdio_command_init_struct.argument &= ~(0xF << (group * 4));
    sdio_command_init_struct.argument |= value << (group * 4);
    sdio_command_init_struct.cmd_index = SD_CMD_HS_SWITCH;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

    stop_flag = 0;

    return sdio_command_data_send(&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *)rsp);
}

/**
 * @brief  enable or disable the sdio wide bus mode.
 * @param  new_state: new state of the sdio wide bus mode. (true or false)
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_bus_wide_send(confirm_state new_state)
{
    sdio_error_t status = SD_OK;

    if (card_info.scr.sd_bus_widths)
    {
        status = sd_cmd55((uint32_t)card_info.rca << 16);

        if (status != SD_OK)
        {
            return status;
        }

        status = sd_acmd6((new_state == TRUE) ? 0x02 : 0x00);
        return status;
    }

    return SD_REQ_NOT_APPLICABLE;
}

/**
 * @brief  allows to process all the interrupts that are high.
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_irq_service(void)
{
    if (sdio_flag_get(SDIOx, SDIO_DTCMPL_FLAG) != RESET)
    {
        if (stop_flag == 1)
        {
            /* send cmd12, stop transmission */
            sdio_command_init_struct.argument = 0;
            sdio_command_init_struct.cmd_index = SD_CMD_STOP_TRANSMISSION;
            sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
            sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;
            /* sdio command config */
            sdio_command_config(SDIOx, &sdio_command_init_struct);
            /* enable ccsm */
            sdio_command_state_machine_enable(SDIOx, TRUE);
            transfer_error = sd_rsp1(SD_CMD_STOP_TRANSMISSION);
        }
        else
        {
            transfer_error = SD_OK;
        }
        /* clear flag */
        sdio_flag_clear(SDIOx, SDIO_DTCMPL_FLAG);
        transfer_end = 1;
        return transfer_error;
    }

    if (sdio_flag_get(SDIOx, SDIO_DTFAIL_FLAG) != RESET)
    {
        /* clear flag */
        sdio_flag_clear(SDIOx, SDIO_DTFAIL_FLAG);
        transfer_error = SD_DATA_FAIL;
        transfer_end = 1;
        return transfer_error;
    }

    if (sdio_flag_get(SDIOx, SDIO_DTTIMEOUT_FLAG) != RESET)
    {
        /* clear flag */
        sdio_flag_clear(SDIOx, SDIO_DTTIMEOUT_FLAG);
        transfer_error = SD_DATA_TIMEOUT;
        transfer_end = 1;
        return transfer_error;
    }

    if (sdio_flag_get(SDIOx, SDIO_RXERRO_FLAG) != RESET)
    {
        /* clear flag */
        sdio_flag_clear(SDIOx, SDIO_RXERRO_FLAG);
        transfer_error = SD_RX_OVERRUN;
        transfer_end = 1;
        return (SD_RX_OVERRUN);
    }

    if (sdio_flag_get(SDIOx, SDIO_TXERRU_FLAG) != RESET)
    {
        /* clear flag */
        sdio_flag_clear(SDIOx, SDIO_TXERRU_FLAG);
        transfer_error = SD_TX_UNDERRUN;
        transfer_end = 1;
        return (SD_TX_UNDERRUN);
    }

    if (sdio_flag_get(SDIOx, SDIO_SBITERR_FLAG) != RESET)
    {
        /* clear flag */
        sdio_flag_clear(SDIOx, SDIO_SBITERR_FLAG);
        transfer_error = SD_START_BIT_ERR;
        transfer_end = 1;
        return (SD_START_BIT_ERR);
    }

    /* disable interrupt */
    sdio_interrupt_enable(SDIOx,
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
static void sd_dma_config(uint32_t *mbuf, uint32_t buf_size, dma_dir_type dir)
{
    dma_init_type dma_init_struct;
    dma_default_para_init(&dma_init_struct);

    crm_periph_clock_enable(CRM_DMA2_PERIPH_CLOCK, TRUE);

    dma_reset(DMA2_CHANNEL4);
    dma_channel_enable(DMA2_CHANNEL4, FALSE);

    dma_init_struct.peripheral_base_addr = (uint32_t)&SDIOx->buf;
    dma_init_struct.memory_base_addr = (uint32_t)mbuf;
    dma_init_struct.direction = dir;
    dma_init_struct.buffer_size = buf_size / 4;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_WORD;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_WORD;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(DMA2_CHANNEL4, &dma_init_struct);

    // dmamux_init (DMA2MUX_CHANNEL4, DMAMUX_SDIOx);
    // dmamux_enable (DMA2, TRUE);
    dma_channel_enable(DMA2_CHANNEL4, TRUE);
}

/**
 * @brief  read current card status.
 * @param  p_card_status: card status.
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_mmc_status_read(uint32_t *p_card_status)
{
    sdio_error_t status;

    if (p_card_status == NULL) {
        return SD_INVALID_PARAMETER;
    }

    /* cmd13 SEND_STATUS */
    sdio_command_init_struct.argument = (uint32_t)(card_info.rca << 16);
    sdio_command_init_struct.cmd_index = SD_CMD_SEND_STATUS;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;
    sdio_command_config(SDIOx, &sdio_command_init_struct);
    sdio_command_state_machine_enable(SDIOx, TRUE);
    status = sd_rsp1(SD_CMD_SEND_STATUS);

    if (status == SD_OK) {
        *p_card_status = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);
    }

    return status;
}

/**
 * @brief  find the sd card scr register value.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_scr_read(uint32_t *buffer)
{
    uint32_t sts_reg = 0;
    sdio_error_t status = SD_OK;

    /* send cmd16, set block length 8 bytes*/
    status = sd_cmd16(8);

    if (status != SD_OK)
    {
        return status;
    }

    /* send cmd55 */
    status = sd_cmd55((uint32_t)card_info.rca << 16);

    if (status != SD_OK)
    {
        return status;
    }

    /* Clear data */
    sdio_data_init_struct.block_size = SDIO_DATA_BLOCK_SIZE_8B;
    sdio_data_init_struct.data_length = 8;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, TRUE);

    /* send acmd51, read scr */
    status = sd_acmd51(0);
    if (status != SD_OK)
    {
        return status;
    }

    sts_reg = SDIOx->sts;

    while (
        !(sts_reg & (SDIO_RXERRO_FLAG | SDIO_DTFAIL_FLAG | SDIO_DTTIMEOUT_FLAG |
                     SDIO_DTBLKCMPL_FLAG | SDIO_SBITERR_FLAG)))
    {
        // TODO: ADD Timeout
        if (sdio_flag_get(SDIOx, SDIO_RXBUF_FLAG) != RESET)
        {
            *buffer++ = sdio_data_read(SDIOx);
        }
        sts_reg = SDIOx->sts;
    }

    if (sdio_flag_get(SDIOx, SDIO_DTTIMEOUT_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_DTTIMEOUT_FLAG);
        return SD_DATA_TIMEOUT;
    }
    else if (sdio_flag_get(SDIOx, SDIO_DTFAIL_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_DTFAIL_FLAG);
        return SD_DATA_FAIL;
    }
    else if (sdio_flag_get(SDIOx, SDIO_RXERRO_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_RXERRO_FLAG);
        return SD_RX_OVERRUN;
    }
    else if (sdio_flag_get(SDIOx, SDIO_SBITERR_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_SBITERR_FLAG);
        return SD_START_BIT_ERR;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    return status;
}

/**
 * @brief  checks if the sd card is in programming state.
 * @param  p_status: pointer to the variable that will contain the sd card
 * state.
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_programming_status(uint8_t *p_status)
{
    volatile uint32_t response = 0, sts_reg = 0, rsp_cmd = 0;

    /* send cmd13 */
    sdio_command_init_struct.argument = (uint32_t)(card_info.rca << 16);
    sdio_command_init_struct.cmd_index = SD_CMD_SEND_STATUS;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;
    sdio_command_config(SDIOx, &sdio_command_init_struct);
    sdio_command_state_machine_enable(SDIOx, TRUE);

    sts_reg = SDIOx->sts;

    while (!(sts_reg &
             (SDIO_CMDFAIL_FLAG | SDIO_CMDTIMEOUT_FLAG | SDIO_CMDRSPCMPL_FLAG)))
    {
        sts_reg = SDIOx->sts;
    }

    if (sdio_flag_get(SDIOx, SDIO_CMDFAIL_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_CMDFAIL_FLAG);
        return SD_CMD_FAIL;
    }

    if (sdio_flag_get(SDIOx, SDIO_CMDTIMEOUT_FLAG) != RESET)
    {
        sdio_flag_clear(SDIOx, SDIO_CMDTIMEOUT_FLAG);
        return SD_CMD_RSP_TIMEOUT;
    }

    rsp_cmd = sdio_command_response_get(SDIOx);
    if (rsp_cmd != SD_CMD_SEND_STATUS)
    {
        return SD_ILLEGAL_CMD;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

    *p_status = (uint8_t)((response >> 9) & 0x0000000F);

    return SD_OK;
}

/**
 * @brief  enquires cards about their operating voltage and configures
 *         clock controls.
 *
 * Part A2, 3.6 Card initialization and identification
 *
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t sd_mmc_power_on(void)
{
    uint8_t retry = 0;
    sdio_error_t status = SD_OK;
    uint32_t response = 0, count = 0;
    uint32_t sd_type;

    /* sdio_ck is less than 400KHz in initialization stage, set to 200KHz */
    uint32_t clk_psc = (sdio_handle.clock_frequency / 200000) - 2;

    if (clk_psc > 0x3FF) {
        clk_psc = 0x3FF;
    }

    /* config sdio clock divide and edge phase */
    sdio_clock_config(SDIOx, clk_psc, SDIO_CLOCK_EDGE_FALLING);
    /* config sdio bus width to single bit */
    sdio_bus_width_config(SDIOx, SDIO_BUS_WIDTH_D1);
    /* disable flow control */
    sdio_flow_control_enable(SDIOx, FALSE);
    /* disable clock bypass */
    sdio_clock_bypass(SDIOx, FALSE);
    /* disable power saving mode */
    sdio_power_saving_mode_enable(SDIOx, FALSE);
    /* sdio power on */
    sdio_power_set(SDIOx, SDIO_POWER_ON);
    /* enable to output sdio_ck */
    sdio_clock_enable(SDIOx, TRUE);

    /**
     * Start initializarion
     * See Figure 4-2 on Physical Layer simplified specification
     * for initialization flow
     */
    for (retry = 0; retry < 5; retry++) {
        /* send cmd0, get in idle stage */
        if((status = sd_cmd0()) == SD_CMD_RSP_TIMEOUT){
            return status;
        }
    }

    /* send cmd8, check card interface feature */
    if (sd_cmd8(SD_CHECK_PATTERN) == SD_OK) {
        card_info.type = SDIO_STD_CAPACITY_SD_CARD_V2_0;
        sd_type = SD_HIGH_CAPACITY;
    } else {
        card_info.type = SDIO_STD_CAPACITY_SD_CARD_V1_1;
        sd_type = SD_STD_CAPACITY;
    }

    /* check sd card or mmc card */
    if (sd_cmd55(0) == SD_OK) {
        /* send acmd41, check voltage operation range */
        while (count < SD_MAX_VOLT_TRIAL) {
            /* send acmd41 */
            status = sd_acmd41(SD_VOLTAGE_WINDOW_SD | sd_type);

            if (status != SD_OK) {
                return status;
            }

            /* get response1 */
            response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

            /* check sd card power on stage is success or not */
            if(response & (1 << 31)){
                break;
            }

            count++;
            delay_ms(10);

            /* send cmd55 before acmd41 */
            if ((status = sd_cmd55(0)) != SD_OK) {
                return status;
            }
        }

        /* Check for Hi-Capacity card */
        if (response & SD_HIGH_CAPACITY) {
            card_info.type = SDIO_HIGH_CAPACITY_SD_CARD;
        }
    } else {
        /* CMD55 was rejected it is a mmc card or no card present */
        while (count < SD_MAX_VOLT_TRIAL) {
            delay_ms(10);
            /* send cmd1 */
            status = sd_cmd1(SD_VOLTAGE_WINDOW_MMC);

            if (status != SD_OK) {
                return status;
            }

            /* get response1 */
            response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

            if(response & (1 << 31)){
                break;
            }

            count++;
        }
        card_info.type = SDIO_MULTIMEDIA_CARD;
    }

    if (count >= SD_MAX_VOLT_TRIAL) {
        return SD_INVALID_VOLTRANGE;
    }

    return status;
}

/**
 * @brief  turns the sdio output signals off.
 * @param  none
 * @retval SD_OK
 */
sdio_error_t sd_mmc_power_off(void)
{
    /* sdio power off */
    sdio_power_set(SDIOx, SDIO_POWER_OFF);

    return SD_OK;
}

/**
 * @brief  Read necessary card information for operation
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
static sdio_error_t card_info_read(card_info_t *ci, uint32_t *table)
{
    sdio_error_t status = SD_OK;
    uint32_t *cid_table = table;
    uint32_t *csd_table = table + 4;

    /* check power status */
    if (SDIO_POWER_OFF == sdio_power_status_get(SDIOx)) {
        return SD_REQ_NOT_APPLICABLE;
    }

    /* cmd2 must be issued before cmd3 */
    if (SDIO_SECURE_DIGITAL_IO_CARD != ci->type) {
        /* Get card identification */
        if ((status = sd_cmd2(0)) != SD_OK) {
            return status;
        }
        cid_table[0] = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);
        cid_table[1] = sdio_response_get(SDIOx, SDIO_RSP2_INDEX);
        cid_table[2] = sdio_response_get(SDIOx, SDIO_RSP3_INDEX);
        cid_table[3] = sdio_response_get(SDIOx, SDIO_RSP4_INDEX);
    }

    /* Read card relative address */
    if (SDIO_MULTIMEDIA_CARD == ci->type) {
        /* mmc card */
        ci->rca = 1;
        status = sd_cmd3(1 << 16, NULL);
    }else{
        status = sd_cmd3(0, &ci->rca);
    }

    if (status != SD_OK) {
        return status;
    }

    if (SDIO_SECURE_DIGITAL_IO_CARD != ci->type) {
        /* Get card specific data, requires rca */
        if ((status = sd_cmd9(ci->rca << 16)) != SD_OK)
        {
            return status;
        }

        csd_table[0] = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);
        csd_table[1] = sdio_response_get(SDIOx, SDIO_RSP2_INDEX);
        csd_table[2] = sdio_response_get(SDIOx, SDIO_RSP3_INDEX);
        csd_table[3] = sdio_response_get(SDIOx, SDIO_RSP4_INDEX);
    }

    return SD_OK;
}


/**
 * @brief  returns information about specific card.
 * @param  card_info: pointer to a sdio_info_t structure
 *                    that contains will receive info.
 *                    CSD and CID are read on sd_card_init
 *                    and parsed here
 * @retval none
 */
static void card_info_parse(card_info_t *ci, uint32_t *table)
{
    uint8_t tmp = 0;
    uint32_t *cid_table = table;
    uint32_t *csd_table = table + 4;

    /* byte 0 */
    tmp = (uint8_t)((csd_table[0] & 0xFF000000) >> 24);
    ci->csd.csd_struct = (tmp & 0xC0) >> 6;
    ci->csd.spec_version = (tmp & 0x3C) >> 2;
    ci->csd.reserved1 = tmp & 0x03;

    /* byte 1 */
    tmp = (uint8_t)((csd_table[0] & 0x00FF0000) >> 16);
    ci->csd.taac = tmp;

    /* byte 2 */
    tmp = (uint8_t)((csd_table[0] & 0x0000FF00) >> 8);
    ci->csd.nsac = tmp;

    /* byte 3 */
    tmp = (uint8_t)(csd_table[0] & 0x000000FF);
    ci->csd.max_bus_clk_freq = tmp;

    /* byte 4 */
    tmp = (uint8_t)((csd_table[1] & 0xFF000000) >> 24);
    ci->csd.card_cmd_classes = tmp << 4;

    /* byte 5 */
    tmp = (uint8_t)((csd_table[1] & 0x00FF0000) >> 16);
    ci->csd.card_cmd_classes |= (tmp & 0xF0) >> 4;
    ci->csd.max_read_blk_length = tmp & 0x0F;

    /* byte 6 */
    tmp = (uint8_t)((csd_table[1] & 0x0000FF00) >> 8);
    ci->csd.part_blk_read = (tmp & 0x80) >> 7;
    ci->csd.write_blk_misalign = (tmp & 0x40) >> 6;
    ci->csd.read_blk_misalign = (tmp & 0x20) >> 5;
    ci->csd.dsr_implemented = (tmp & 0x10) >> 4;
    ci->csd.reserved2 = 0; /* reserved */

    if ((ci->type == SDIO_STD_CAPACITY_SD_CARD_V1_1) ||
        (ci->type == SDIO_STD_CAPACITY_SD_CARD_V2_0) ||
        (ci->type == SDIO_MULTIMEDIA_CARD))
    {
        ci->csd.device_size = (tmp & 0x03) << 10;

        /* byte 7 */
        tmp = (uint8_t)(csd_table[1] & 0x000000FF);
        ci->csd.device_size |= (tmp) << 2;

        /* byte 8 */
        tmp = (uint8_t)((csd_table[2] & 0xFF000000) >> 24);
        ci->csd.device_size |= (tmp & 0xC0) >> 6;

        ci->csd.max_read_current_vdd_min = (tmp & 0x38) >> 3;
        ci->csd.max_read_current_vdd_max = (tmp & 0x07);

        /* byte 9 */
        tmp = (uint8_t)((csd_table[2] & 0x00FF0000) >> 16);
        ci->csd.max_write_current_vdd_min = (tmp & 0xE0) >> 5;
        ci->csd.max_write_current_vdd_max = (tmp & 0x1C) >> 2;
        ci->csd.device_size_mult = (tmp & 0x03) << 1;
        /* byte 10 */
        tmp = (uint8_t)((csd_table[2] & 0x0000FF00) >> 8);
        ci->csd.device_size_mult |= (tmp & 0x80) >> 7;

        ci->capacity = (ci->csd.device_size + 1);
        ci->capacity *=
            (1 << (ci->csd.device_size_mult + 2));
        ci->block_size = 1
                                << (ci->csd.max_read_blk_length);
        ci->capacity *= ci->block_size;
    }
    else if (ci->type == SDIO_HIGH_CAPACITY_SD_CARD)
    {
        /* byte 7 */
        tmp = (uint8_t)(csd_table[1] & 0x000000FF);
        ci->csd.device_size = (tmp & 0x3F) << 16;

        /* byte 8 */
        tmp = (uint8_t)((csd_table[2] & 0xFF000000) >> 24);

        ci->csd.device_size |= (tmp << 8);

        /* byte 9 */
        tmp = (uint8_t)((csd_table[2] & 0x00FF0000) >> 16);

        ci->csd.device_size |= (tmp);

        /* byte 10 */
        tmp = (uint8_t)((csd_table[2] & 0x0000FF00) >> 8);

        ci->capacity =
            (uint64_t)(ci->csd.device_size + 1) * 512 * 1024;
        ci->block_size = 512;
    }

    ci->csd.erase_group_size = (tmp & 0x40) >> 6;
    ci->csd.erase_group_size_mult = (tmp & 0x3F) << 1;

    /* byte 11 */
    tmp = (uint8_t)(csd_table[2] & 0x000000FF);
    ci->csd.erase_group_size_mult |= (tmp & 0x80) >> 7;
    ci->csd.write_protect_group_size = (tmp & 0x7F);

    /* byte 12 */
    tmp = (uint8_t)((csd_table[3] & 0xFF000000) >> 24);
    ci->csd.write_protect_group_enable = (tmp & 0x80) >> 7;
    ci->csd.manufacturer_default_ecc = (tmp & 0x60) >> 5;
    ci->csd.write_speed_factor = (tmp & 0x1C) >> 2;
    ci->csd.max_write_blk_length = (tmp & 0x03) << 2;

    /* byte 13 */
    tmp = (uint8_t)((csd_table[3] & 0x00FF0000) >> 16);
    ci->csd.max_write_blk_length |= (tmp & 0xC0) >> 6;
    ci->csd.part_blk_write = (tmp & 0x20) >> 5;
    ci->csd.reserved3 = 0;
    ci->csd.content_protect_app = (tmp & 0x01);

    /* byte 14 */
    tmp = (uint8_t)((csd_table[3] & 0x0000FF00) >> 8);
    ci->csd.file_format_group = (tmp & 0x80) >> 7;
    ci->csd.copy_flag = (tmp & 0x40) >> 6;
    ci->csd.permanent_write_protect = (tmp & 0x20) >> 5;
    ci->csd.temp_write_protect = (tmp & 0x10) >> 4;
    ci->csd.file_formart = (tmp & 0x0C) >> 2;
    ci->csd.ecc_code = (tmp & 0x03);

    /* byte 15 */
    tmp = (uint8_t)(csd_table[3] & 0x000000FF);
    ci->csd.csd_crc = (tmp & 0xFE) >> 1;
    ci->csd.reserved4 = 1;

    /* byte 0 */
    tmp = (uint8_t)((cid_table[0] & 0xFF000000) >> 24);
    ci->cid.manufacturer_id = tmp;

    /* byte 1 */
    tmp = (uint8_t)((cid_table[0] & 0x00FF0000) >> 16);
    ci->cid.oem_app_id = tmp << 8;

    /* byte 2 */
    tmp = (uint8_t)((cid_table[0] & 0x000000FF00) >> 8);
    ci->cid.oem_app_id |= tmp;

    /* byte 3 */
    tmp = (uint8_t)(cid_table[0] & 0x000000FF);
    ci->cid.product_name1 = tmp << 24;

    /* byte 4 */
    tmp = (uint8_t)((cid_table[1] & 0xFF000000) >> 24);
    ci->cid.product_name1 |= tmp << 16;

    /* byte 5 */
    tmp = (uint8_t)((cid_table[1] & 0x00FF0000) >> 16);
    ci->cid.product_name1 |= tmp << 8;

    /* byte 6 */
    tmp = (uint8_t)((cid_table[1] & 0x0000FF00) >> 8);
    ci->cid.product_name1 |= tmp;

    /* byte 7 */
    tmp = (uint8_t)(cid_table[1] & 0x000000FF);
    ci->cid.product_name2 = tmp;

    /* byte 8 */
    tmp = (uint8_t)((cid_table[2] & 0xFF000000) >> 24);
    ci->cid.product_reversion = tmp;

    /* byte 9 */
    tmp = (uint8_t)((cid_table[2] & 0x00FF0000) >> 16);
    ci->cid.product_sn = tmp << 24;

    /* byte 10 */
    tmp = (uint8_t)((cid_table[2] & 0x0000FF00) >> 8);
    ci->cid.product_sn |= tmp << 16;

    /* byte 11 */
    tmp = (uint8_t)(cid_table[2] & 0x000000FF);
    ci->cid.product_sn |= tmp << 8;

    /* byte 12 */
    tmp = (uint8_t)((cid_table[3] & 0xFF000000) >> 24);
    ci->cid.product_sn |= tmp;

    /* byte 13 */
    tmp = (uint8_t)((cid_table[3] & 0x00FF0000) >> 16);
    ci->cid.reserved1 |= (tmp & 0xF0) >> 4;
    ci->cid.manufact_date = (tmp & 0x0F) << 8;

    /* byte 14 */
    tmp = (uint8_t)((cid_table[3] & 0x0000FF00) >> 8);
    ci->cid.manufact_date |= tmp;

    /* byte 15 */
    tmp = (uint8_t)(cid_table[3] & 0x000000FF);
    ci->cid.cid_crc = (tmp & 0xFE) >> 1;
    ci->cid.reserved2 = 1;
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
static sdio_error_t sd_bus_wide_operation_config(sdio_bus_width_type mode)
{
    sdio_error_t status = SD_OK;

    if (mode >= SDIO_BUS_WIDTH_D8)
    {
        /* not support D8 mode */
        return SD_UNSUPPORTED_FEATURE;
    }

    /* get last R1 to check if card locked or not */
    if (sdio_response_get(SDIOx, SDIO_RSP1_INDEX) & SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_ERROR;
    }

    if ((card_info.type == SDIO_MULTIMEDIA_CARD) ||
        (card_info.type == SDIO_HIGH_SPEED_MULTIMEDIA_CARD))
    {
        status = mmc_switch(EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH,
                            (uint8_t)mode);
    }
    else if ((card_info.type == SDIO_STD_CAPACITY_SD_CARD_V1_1) ||
             (card_info.type == SDIO_STD_CAPACITY_SD_CARD_V2_0) ||
             (card_info.type == SDIO_HIGH_CAPACITY_SD_CARD))
    {
        /* Configure card */
        status = sd_bus_wide_send((SDIO_BUS_WIDTH_D4 == mode) ? TRUE : FALSE);
    }

    if (status == SD_OK)
    {
        /* Configure interface */
        sdio_bus_width_config(SDIOx, mode);
    }

    return status;
}

/**
 * @brief Initializes sdio peripheral,
 *       called at startup on board init
 * @param
 * @return
 */
static sdio_error_t sdio_init(sdio_handle_t *hsdio)
{
    gpio_init_type gpio_init_struct = {0};
    crm_clocks_freq_type clocks;

    memset(hsdio, 0, sizeof(sdio_handle_t));

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SDIO1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);

    crm_clocks_freq_get(&clocks);
    hsdio->clock_frequency = clocks.ahb_freq;

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init_struct.gpio_pins =
        GPIO_PINS_2 | GPIO_PINS_3 | GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6 | GPIO_PINS_7;
    gpio_init(GPIOA, &gpio_init_struct);

    gpio_pin_remap_config(SDIO1_GMUX_0111, TRUE);

    nvic_irq_enable(SDIO1_IRQn, 0, 0);

    hsdio->sdio_x = SDIOx;

    return SD_OK;
}

/**
 * @brief Initialyzes SD/MMC driver internal
 *          data structures
 * @param
 * @return
 */
sdio_error_t sd_mmc_init(void)
{
    sdio_init(&sdio_handle);

    return SD_OK;
}

/**
 * @brief  Initializes SD/MMC card.
 *   If a card is present, it is put into standby
 *   state (ready for data transfer).
 * @param  none
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_mmc_card_init(void)
{
    uint16_t clkdiv = 0;
    sdio_error_t status = SD_OK;

    uint8_t retry = 3;

    memset(&card_info, 0, sizeof(card_info));

    while (retry--)
    {
        sdio_reset(SDIOx);
        /* power up and low level card initialization*/
        status = sd_mmc_power_on();

        if (status == SD_OK)
            break;
    }

    if (status == SD_OK)
    {
        uint32_t table[8];
        status = card_info_read(&card_info, table);
        if(status == SD_OK){
            card_info_parse(&card_info, table);
            if ((card_info.type == SDIO_MULTIMEDIA_CARD) &&
                (card_info.csd.spec_version >= 4))
            {
                card_info.type = SDIO_HIGH_SPEED_MULTIMEDIA_CARD;
            }
        }
    }

    if (status == SD_OK)
    {
        /* select card */
        status = sd_cmd7((uint32_t)(card_info.rca << 16));
    }

    if (status == SD_OK && ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == card_info.type) ||
                            (SDIO_STD_CAPACITY_SD_CARD_V2_0 == card_info.type) ||
                            (SDIO_SECURE_DIGITAL_IO_COMBO_CARD == card_info.type) ||
                            (SDIO_HIGH_CAPACITY_SD_CARD == card_info.type)))
    {
        status = sd_scr_read((uint32_t *)&card_info.scr);
    }

    /* set transfer mode */
    sdio_handle.device_mode = SDIOx_TRANSFER_MODE;
    if (status == SD_OK)
    {
        /* set normal speed */
        status = sd_speed_change(0);
    }

    if ((status == SD_OK) || (card_info.type == SDIO_MULTIMEDIA_CARD))
    {
        if (card_info.type == SDIO_STD_CAPACITY_SD_CARD_V1_1 ||
            card_info.type == SDIO_STD_CAPACITY_SD_CARD_V2_0)
        {
            /* set sdio_ck to 12mhz */
            clkdiv = sdio_handle.clock_frequency  / 12000000;

            if (clkdiv >= 2)
            {
                clkdiv -= 2;
            }
        }
        else
        {
            /* set sdio_ck to 48mhz */
            clkdiv = sdio_handle.clock_frequency  / 48000000;

            if (clkdiv >= 2)
            {
                clkdiv -= 2;
            }
        }
        /* set sdio clock divider */
        sdio_clock_config(SDIOx, clkdiv, SDIO_CLOCK_EDGE_FALLING);
    }

    if (status == SD_OK)
    {
        /* Set data width */
        status = sd_bus_wide_operation_config(SDIO_BUS_WIDTH_D4);
    }

    return status;
}

card_info_t *sd_mmc_card_info(void)
{
    return &card_info;
}

/**
 * @brief  erase continuous data block
 * @param  addr: starting address
 * @param  nblks: amount of erasing data block
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_mmc_erase(uint64_t addr, uint32_t nblks)
{
    sdio_error_t status = SD_OK;
    uint32_t sblock, eblock;
    uint8_t card_state;

    /* check card locked */
    if (sdio_response_get(SDIOx, SDIO_RSP1_INDEX) & SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_ERROR;
    }

    if (SDIO_HIGH_CAPACITY_SD_CARD == card_info.type)
    {
        sblock = addr;
        eblock = sblock + nblks - 1;
    }
    else
    {
        sblock = addr << 9;
        eblock = sblock + ((nblks - 1) << 9);
    }

    /* clear dcsm configuration */
    sdio_data_init_struct.block_size = SDIO_DATA_BLOCK_SIZE_1B;
    sdio_data_init_struct.data_length = 0;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;
    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, FALSE);

    if (SDIO_MULTIMEDIA_CARD == card_info.type ||
        SDIO_HIGH_SPEED_MULTIMEDIA_CARD == card_info.type)
    {
        /* send cmd35, set erase group start */
        sdio_command_init_struct.argument = sblock;
        sdio_command_init_struct.cmd_index = SD_CMD_ERASE_GROUP_START;
        sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
        sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        sdio_command_state_machine_enable(SDIOx, TRUE);
        status = sd_rsp1(SD_CMD_ERASE_GROUP_START);

        if (status != SD_OK)
        {
            return status;
        }

        /* send cmd36, set erase group end */
        sdio_command_init_struct.argument = eblock;
        sdio_command_init_struct.cmd_index = SD_CMD_ERASE_GROUP_END;
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        sdio_command_state_machine_enable(SDIOx, TRUE);
        status = sd_rsp1(SD_CMD_ERASE_GROUP_END);

        if (status != SD_OK)
        {
            return status;
        }
    }
    else
    {
        /* send cmd32, set erase block start */
        sdio_command_init_struct.argument = sblock;
        sdio_command_init_struct.cmd_index = SD_CMD_ERASE_WR_BLK_START;
        sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
        sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        sdio_command_state_machine_enable(SDIOx, TRUE);
        status = sd_rsp1(SD_CMD_ERASE_WR_BLK_START);

        if (status != SD_OK)
        {
            return status;
        }

        /* send cmd33, set erase block end */
        sdio_command_init_struct.argument = eblock;
        sdio_command_init_struct.cmd_index = SD_CMD_ERASE_WR_BLK_END;
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        sdio_command_state_machine_enable(SDIOx, TRUE);
        status = sd_rsp1(SD_CMD_ERASE_WR_BLK_END);

        if (status != SD_OK)
        {
            return status;
        }
    }

    /* send cmd38, start erase*/
    sdio_command_init_struct.argument = 0;
    sdio_command_init_struct.cmd_index = SD_CMD_ERASE;
    sdio_command_config(SDIOx, &sdio_command_init_struct);
    sdio_command_state_machine_enable(SDIOx, TRUE);
    status = sd_rsp1(SD_CMD_ERASE);

    if (status != SD_OK)
    {
        return status;
    }

    do{
        status = sd_programming_status(&card_state);

    }while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                  (card_state == SD_CARD_RECEIVING)));

    return status;
}

/**
 * @brief  Read one block of data from a specified address.
 *         Data transfer can be managed by dma mode or polling mode.
 *         for now block size is fixed to 512bytes
 *
 * @param  buf: pointer to the buffer that will contain the received data
 * @param  block: For V1.1 this is offset in bytes, otherwise is block number
 *
 * @param  count: Number of blocks,
 *               for now only one block is allowed
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_mmc_read(uint8_t *buf, uint64_t block, uint32_t count)
{
    sdio_error_t status = SD_OK;
    uint32_t response = 0;
    uint8_t power;
    uint16_t blk_size;

    if ((NULL == buf) || (count != 1))  // || count > card_info.last_block
    {
        return SD_INVALID_PARAMETER;
    }

    SDIOx->dtctrl = 0x0;

    /* Clip to 512 bytes */
    blk_size = (card_info.block_size  > 512) ? 512 : card_info.block_size;

    if(card_info.type != SDIO_HIGH_CAPACITY_SD_CARD){
        /* send cmd16 to set block size */
        if ((status = sd_cmd16(blk_size)) != SD_OK)
        {
            return status;
        }
        block <<= 9;
    }

    /* clear dcsm configuration */
    sdio_data_init_struct.block_size = SDIO_DATA_BLOCK_SIZE_1B;
    sdio_data_init_struct.data_length = 0;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;
    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, FALSE);
    /* check card locked */
    response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);
    if (response & SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_ERROR;
    }

    power = power_of_two(blk_size);
    sdio_data_init_struct.block_size = (sdio_block_size_type)(power);
    sdio_data_init_struct.data_length = count << power;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    /* send cmd17 for single block */
    sdio_command_init_struct.argument = block;
    sdio_command_init_struct.cmd_index = SD_CMD_READ_SINGLE_BLOCK;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

    stop_flag = 0;

    return sdio_command_data_send(&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *)buf);
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
sdio_error_t sd_block_multi_read(uint8_t *buf, long long addr,
                                 uint16_t blk_size, uint32_t nblks)
{
    sdio_error_t status = SD_OK;
    uint32_t response = 0;
    uint8_t power;

    SDIOx->dtctrl = 0x0;

    if (card_info.type == SDIO_HIGH_CAPACITY_SD_CARD)
    {
        blk_size = 512;
        addr >>= 9;
    }

    /* clear dcsm configuration */
    sdio_data_init_struct.block_size = (sdio_block_size_type)0;
    sdio_data_init_struct.data_length = 0;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, FALSE);

    response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

    /* check card locked */
    if (response & SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_ERROR;
    }

    if ((blk_size > 0) && (blk_size <= 2048) &&
        ((blk_size & (blk_size - 1)) == 0))
    {
        power = power_of_two(blk_size);

        /* send cmd16, set block size */
        sdio_command_init_struct.argument = blk_size;
        sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
        sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
        sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

        /* sdio command config */
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        /* enable ccsm */
        sdio_command_state_machine_enable(SDIOx, TRUE);

        status = sd_rsp1(SD_CMD_SET_BLOCKLEN);

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

    sdio_data_init_struct.block_size = (sdio_block_size_type)(power);
    sdio_data_init_struct.data_length = nblks * blk_size;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    /* send cmd18, read block data */
    sdio_command_init_struct.argument = addr;
    sdio_command_init_struct.cmd_index = SD_CMD_READ_MULT_BLOCK;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

    stop_flag = 1;
    return sdio_command_data_send(&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *)buf);
}

/**
 * @brief  allows to write one block starting from a specified address in a
 * card. the data transfer can be managed by dma mode or polling mode.
 * @param  buf: pointer to the buffer that contain the data to be transferred.
 * @param  addr: address from where data are to be read.
 * @param  blk_size: the sd card data block size. the block size should be 512.
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t sd_block_write(const uint8_t *buf, long long addr,
                            uint16_t blk_size)
{
    sdio_error_t status = SD_OK;
    uint8_t power = 0, card_state = 0;
    uint32_t timeout = 0, card_status = 0, response = 0;

    if (buf == NULL)
    {
        return SD_INVALID_PARAMETER;
    }

    SDIOx->dtctrl = 0x0;

    /* clear dcsm configuration */
    sdio_data_init_struct.block_size = (sdio_block_size_type)0;
    sdio_data_init_struct.data_length = 0;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, FALSE);

    response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

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
        power = power_of_two(blk_size);

        /* send cmd16, set block size */
        sdio_command_init_struct.argument = blk_size;
        sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
        sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
        sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

        /* sdio command config */
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        /* enable ccsm */
        sdio_command_state_machine_enable(SDIOx, TRUE);

        status = sd_rsp1(SD_CMD_SET_BLOCKLEN);

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
        status = sd_mmc_status_read(&card_status);
    }
    /* check ready_for_data flag */
    while (((card_status & 0x00000100) == 0) && (timeout > 0));

    if (timeout == 0)
    {
        return SD_ERROR;
    }

    /* send cmd24, write single block */
    sdio_command_init_struct.argument = addr;
    sdio_command_init_struct.cmd_index = SD_CMD_WRITE_SINGLE_BLOCK;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

    sdio_data_init_struct.block_size = (sdio_block_size_type)(power);
    sdio_data_init_struct.data_length = blk_size;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    stop_flag = 0;

    /* single block, stop command is unnecessary */
    status = sdio_command_data_send(&sdio_command_init_struct,
                                    &sdio_data_init_struct, (uint32_t *)buf);

    if (status != SD_OK)
    {
        return status;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    status = sd_programming_status(&card_state);

    while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                 (card_state == SD_CARD_RECEIVING)))
    {
        status = sd_programming_status(&card_state);
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
sdio_error_t sd_block_multi_write(const uint8_t *buf, long long addr,
                                  uint16_t blk_size, uint32_t nblks)
{
    sdio_error_t status = SD_OK;
    uint8_t power = 0, card_state = 0;
    uint32_t timeout = 0, card_status = 0, response = 0;

    if (buf == NULL)
    {
        return SD_INVALID_PARAMETER;
    }

    SDIOx->dtctrl = 0x0;

    /* clear dcsm configuration */
    sdio_data_init_struct.block_size = (sdio_block_size_type)0;
    sdio_data_init_struct.data_length = 0;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, FALSE);

    response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

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
        power = power_of_two(blk_size);

        /* send cmd16, set block size */
        sdio_command_init_struct.argument = blk_size;
        sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCKLEN;
        sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
        sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

        /* sdio command config */
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        /* enable ccsm */
        sdio_command_state_machine_enable(SDIOx, TRUE);

        status = sd_rsp1(SD_CMD_SET_BLOCKLEN);

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
        status = sd_mmc_status_read(&card_status);
    }
    /* check ready_for_data flag */
    while (((card_status & 0x00000100) == 0) && (timeout > 0));

    if (timeout == 0)
    {
        return SD_ERROR;
    }

    if ((card_info.type == SDIO_STD_CAPACITY_SD_CARD_V1_1) ||
        (card_info.type == SDIO_STD_CAPACITY_SD_CARD_V2_0) ||
        (card_info.type == SDIO_HIGH_CAPACITY_SD_CARD))
    {
        /* send cmd55 */
        status = sd_cmd55(card_info.rca << 16);

        if (status != SD_OK)
        {
            return status;
        }

        /* send cmd23, set block count */
        sdio_command_init_struct.argument = blk_size;
        sdio_command_init_struct.cmd_index = SD_CMD_SET_BLOCK_COUNT;
        sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
        sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

        /* sdio command config */
        sdio_command_config(SDIOx, &sdio_command_init_struct);
        /* enable ccsm */
        sdio_command_state_machine_enable(SDIOx, TRUE);

        status = sd_rsp1(SD_CMD_SET_BLOCK_COUNT);

        if (status != SD_OK)
        {
            return status;
        }
    }

    /* send cmd25, write mult blocks */
    sdio_command_init_struct.argument = addr;
    sdio_command_init_struct.cmd_index = SD_CMD_WRITE_MULT_BLOCK;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

    sdio_data_init_struct.block_size = (sdio_block_size_type)(power);
    sdio_data_init_struct.data_length = nblks * blk_size;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_BLOCK_TRANSFER;

    stop_flag = 1;
    /* cmd12 is needed */
    status = sdio_command_data_send(&sdio_command_init_struct,
                                    &sdio_data_init_struct, (uint32_t *)buf);

    if (status != SD_OK)
    {
        return status;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    status = sd_programming_status(&card_state);

    while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                 (card_state == SD_CARD_RECEIVING)))
    {
        status = sd_programming_status(&card_state);
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
sdio_error_t mmc_stream_read(uint8_t *buf, long long addr, uint32_t len)
{
    /* check card locked */
    if (sdio_response_get(SDIOx, SDIO_RSP1_INDEX) & SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_ERROR;
    }

    SDIOx->dtctrl = 0x0;

    /* clear dcsm configuration */
    sdio_data_init_struct.block_size = SDIO_DATA_BLOCK_SIZE_1B;
    sdio_data_init_struct.data_length = 0;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_STREAM_TRANSFER;

    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, FALSE);

    /* send cmd11, read data */
    sdio_command_init_struct.argument = addr;
    sdio_command_init_struct.cmd_index = SD_CMD_READ_DAT_UNTIL_STOP;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

    sdio_data_init_struct.block_size = (sdio_block_size_type)(5 << 4);
    sdio_data_init_struct.data_length = len;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CONTROLLER;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_STREAM_TRANSFER;

    stop_flag = 1;
    /* cmd12 is needed */
    return sdio_command_data_send(&sdio_command_init_struct,
                                  &sdio_data_init_struct, (uint32_t *)buf);
}

/**
 * @brief  write mmc card data stream.
 * @param  buf: data that writing to mmc card
 * @param  addr: start address mmc card
 * @param  len: data size
 * @retval sdio_error_t: sd card error code.
 */
sdio_error_t mmc_stream_write(uint8_t *buf, long long addr, uint32_t len)
{
    sdio_error_t status = SD_OK;
    uint32_t response = 0;
    uint8_t card_state = 0;

    if (buf == NULL)
    {
        return SD_INVALID_PARAMETER;
    }

    SDIOx->dtctrl = 0x0;

    /* clear dcsm configuration */
    sdio_data_init_struct.block_size = SDIO_DATA_BLOCK_SIZE_1B;
    sdio_data_init_struct.data_length = 0;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_STREAM_TRANSFER;

    sdio_data_config(SDIOx, &sdio_data_init_struct);
    sdio_data_state_machine_enable(SDIOx, FALSE);

    response = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);

    /* check card locked */
    if (response & SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_ERROR;
    }
    /* send cmd20, write data */
    sdio_command_init_struct.argument = addr;
    sdio_command_init_struct.cmd_index = SD_CMD_WRITE_DAT_UNTIL_STOP;
    sdio_command_init_struct.rsp_type = SDIO_RESPONSE_SHORT;
    sdio_command_init_struct.wait_type = SDIO_WAIT_FOR_NO;

    sdio_data_init_struct.block_size = (sdio_block_size_type)(15 << 4);
    sdio_data_init_struct.data_length = len;
    sdio_data_init_struct.timeout = SD_DATATIMEOUT;
    sdio_data_init_struct.transfer_direction = SDIO_DATA_TRANSFER_TO_CARD;
    sdio_data_init_struct.transfer_mode = SDIO_DATA_STREAM_TRANSFER;

    stop_flag = 1;
    /* cmd12 is needed */
    status = sdio_command_data_send(&sdio_command_init_struct,
                                    &sdio_data_init_struct, (uint32_t *)buf);

    if (status != SD_OK)
    {
        return status;
    }

    sdio_flag_clear(SDIOx, SDIO_STATIC_FLAGS);

    status = sd_programming_status(&card_state);

    while ((status == SD_OK) && ((card_state == SD_CARD_PROGRAMMING) ||
                                 (card_state == SD_CARD_RECEIVING)))
    {
        status = sd_programming_status(&card_state);
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
static sdio_error_t mmc_switch(uint8_t set, uint8_t index, uint8_t value)
{
    sdio_error_t status = SD_OK;
    uint32_t card_status = 0, timeout = 0;

    uint32_t argument =
        (EXT_CSD_Write_byte << 24) | (index << 16) | (value << 8) | set;

    status = sd_cmd6(argument);
    if (status != SD_OK)
    {
        return status;
    }

    card_status = sdio_response_get(SDIOx, SDIO_RSP1_INDEX);
    if (card_status & MMC_SWITCH_ERROR)
    {
        return SD_SWITCH_ERROR;
    }

    timeout = SD_DATATIMEOUT;
    do
    {
        timeout--;
        status = sd_mmc_status_read(&card_status);
    } /* check ready_for_data flag */
    while (((card_status & 0x00000100) == 0) && (timeout > 0));

    if (timeout == 0)
    {
        return SD_ERROR;
    }

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

sdio_error_t sd_read_disk(uint8_t *buf, uint32_t sector, uint8_t cnt)
{
    sdio_error_t sta = SD_OK;
    uint64_t lsector = sector;
    uint8_t n;
    uint32_t block_size = card_info.block_size;

    /* data address is in block units. */
    lsector *= block_size;

    if ((uint32_t)buf % 4 != 0)
    {
        uint32_t sdio_data_buffer[block_size / 4];

        for (n = 0; n < cnt; n++)
        {
            sta = sd_mmc_read((uint8_t *)sdio_data_buffer, lsector + block_size * n, block_size);
            memcpy(buf, sdio_data_buffer, block_size);
            buf += block_size;
        }
    }
    else
    {
        if (cnt == 1)
        {
            sta = sd_mmc_read(buf, lsector, block_size);
        }
        else
        {
            sta = sd_block_multi_read(buf, lsector, block_size, cnt);
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
sdio_error_t sd_write_disk(const uint8_t *buf, uint32_t sector, uint8_t cnt)
{
    sdio_error_t sta = SD_OK;
    uint8_t n;
    uint64_t lsector = sector;
    uint32_t block_size = card_info.block_size;

    /* data address is in block units. */
    lsector *= block_size;

    if ((uint32_t)buf % 4 != 0)
    {
        uint32_t sdio_data_buffer[block_size / 4];

        for (n = 0; n < cnt; n++)
        {
            memcpy(sdio_data_buffer, buf, block_size);
            sta = sd_block_write((uint8_t *)sdio_data_buffer, lsector + block_size * n, block_size);
            buf += block_size;
        }
    }
    else
    {
        if (cnt == 1)
        {
            sta = sd_block_write(buf, lsector, block_size);
        }
        else
        {
            sta = sd_block_multi_write(buf, lsector, block_size, cnt);
        }
    }

    return sta;
}

/**
 * @brief  sdio1 isr.
 * @param  none.
 * @retval none.
 */
void SDIOx_IRQHandler(void)
{
    sd_irq_service();
}
