
#include "usbd_core.h"
#include "cdc_dual_class.h"
#include "cdc_dual_desc.h"

static cdc_dual_struct_type cdc_struct[2];

/**
  * @brief  usb device function
  * @param  udev: to the structure of usbd_core_type
  * @param  cmd: request number
  * @param  buff: request buffer
  * @param  len: buffer length
  * @retval none
  */
static void class_function(cdc_dual_struct_type *pcdc, uint8_t cmd, uint8_t *buff, uint16_t len)
{
    (void)len;

    switch(cmd)
    {
        case SET_LINE_CODING:
        pcdc->linecoding.bitrate = (uint32_t)(buff[0] | (buff[1] << 8) | (buff[2] << 16) | (buff[3] <<24));
        pcdc->linecoding.format = buff[4];
        pcdc->linecoding.parity = buff[5];
        pcdc->linecoding.data = buff[6];
        break;

        case GET_LINE_CODING:
        buff[0] = (uint8_t)pcdc->linecoding.bitrate;
        buff[1] = (uint8_t)(pcdc->linecoding.bitrate >> 8);
        buff[2] = (uint8_t)(pcdc->linecoding.bitrate >> 16);
        buff[3] = (uint8_t)(pcdc->linecoding.bitrate >> 24);
        buff[4] = (uint8_t)(pcdc->linecoding.format);
        buff[5] = (uint8_t)(pcdc->linecoding.parity);
        buff[6] = (uint8_t)(pcdc->linecoding.data);
        break;

        default:
        break;
    }
}

/**
  * @brief  initialize usb custom hid endpoint
  * @param  udev: to the structure of usbd_core_type
  * @retval status of usb_sts_type
  */
static usb_sts_type class_init_handler(void *udev)
{
    usbd_core_type *pudev = (usbd_core_type *)udev;
    cdc_dual_struct_type *pcdc = (cdc_dual_struct_type *)pudev->class_handler->pdata;

    for(uint8_t i = 0; i < 2; i++){
        pcdc[i].tx_completed = 1;
        pcdc[i].rx_completed = 0;
        pcdc[i].alt_setting = 0;
        pcdc[i].linecoding.bitrate = 115200;
        pcdc[i].linecoding.data = 0;
        pcdc[i].linecoding.format = 0;
        pcdc[i].linecoding.parity = 8;
    }

    usbd_ept_open(pudev, USBD_CDC0_INT_EPT, EPT_INT_TYPE, USBD_CDC_CMD_PKT_SIZE);
    usbd_ept_open(pudev, USBD_CDC0_BULK_IN_EPT, EPT_BULK_TYPE, USBD_CDC_IN_PKT_SIZE);
    usbd_ept_open(pudev, USBD_CDC0_BULK_OUT_EPT, EPT_BULK_TYPE, USBD_CDC_OUT_PKT_SIZE);
    //usbd_ept_open(pudev, USBD_CDC1_INT_EPT, EPT_INT_TYPE, USBD_CDC_CMD_PKT_SIZE);
    usbd_ept_open(pudev, USBD_CDC1_BULK_IN_EPT, EPT_BULK_TYPE, USBD_CDC_IN_PKT_SIZE);
    usbd_ept_open(pudev, USBD_CDC1_BULK_OUT_EPT, EPT_BULK_TYPE, USBD_CDC_OUT_PKT_SIZE);

    // Start receiving
    usbd_ept_recv(pudev, USBD_CDC0_BULK_OUT_EPT, pcdc[0].rx_buff, USBD_CDC_OUT_PKT_SIZE);
    usbd_ept_recv(pudev, USBD_CDC1_BULK_OUT_EPT, pcdc[1].rx_buff, USBD_CDC_OUT_PKT_SIZE);

    return USB_OK;
}

/**
  * @brief  clear endpoint or other state
  * @param  udev: to the structure of usbd_core_type
  * @retval status of usb_sts_type
  */
static usb_sts_type class_clear_handler(void *udev)
{
    usbd_core_type *pudev = (usbd_core_type *)udev;

    usbd_ept_close(pudev, USBD_CDC0_INT_EPT);
    usbd_ept_close(pudev, USBD_CDC0_BULK_IN_EPT);
    usbd_ept_close(pudev, USBD_CDC0_BULK_OUT_EPT);
    usbd_ept_close(pudev, USBD_CDC1_BULK_IN_EPT);
    usbd_ept_close(pudev, USBD_CDC1_BULK_OUT_EPT);

    return USB_OK;
}

static void class_setup_class_handler(usbd_core_type *pudev, cdc_dual_struct_type *pcdc, usb_setup_type *setup)
{
    switch(setup->bRequest){
        case SET_LINE_CODING:
            pcdc->req = setup->bRequest;
            pcdc->len = setup->wLength;
            usbd_ctrl_recv(pudev, pcdc->cmd, pcdc->len);
            break;

        case GET_LINE_CODING:
            class_function(pcdc, setup->bRequest, pcdc->cmd, setup->wLength);
            usbd_ctrl_send(pudev, pcdc->cmd, setup->wLength);
            break;

        case SET_CONTROL_LINE_STATE:
            if(pcdc->cdc_set_config){
                pcdc->cdc_set_config(pudev, setup->wValue);
            }
            break;

        default:
            break;
    }
}

static void class_setup_standard_handler(usbd_core_type *pudev, cdc_dual_struct_type *pcdc, usb_setup_type *setup)
{
    switch(setup->bRequest)
    {
        case USB_STD_REQ_GET_DESCRIPTOR:
            usbd_ctrl_unsupport(pudev);
            break;
        case USB_STD_REQ_GET_INTERFACE:
            usbd_ctrl_send(pudev, (uint8_t *)&pcdc->alt_setting, 1);
            break;
        case USB_STD_REQ_SET_INTERFACE:
            pcdc->alt_setting = setup->wValue;
            break;
        case USB_STD_REQ_CLEAR_FEATURE:
            break;
        case USB_STD_REQ_SET_FEATURE:
            break;
        case USB_STD_REQ_SET_CONFIGURATION:
            break;
        default:
            usbd_ctrl_unsupport(pudev);
            break;
    }
}

/**
  * @brief  usb device class setup request handler
  * @param  udev: to the structure of usbd_core_type
  * @param  setup: setup packet
  * @retval status of usb_sts_type
  */
static usb_sts_type class_setup_handler(void *udev, usb_setup_type *setup)
{
    usb_sts_type status = USB_OK;
    usbd_core_type *pudev = (usbd_core_type *)udev;
    cdc_dual_struct_type *pcdc = (cdc_dual_struct_type *)pudev->class_handler->pdata;

    switch(setup->bmRequestType & USB_REQ_TYPE_RESERVED)
    {
        /* class request */
        case USB_REQ_TYPE_CLASS:
            class_setup_class_handler(pudev, pcdc, setup);
            break;
        /* standard request */
        case USB_REQ_TYPE_STANDARD:
            class_setup_standard_handler(pudev, pcdc, setup);
            break;

        case USB_REQ_TYPE_VENDOR:
        case USB_REQ_TYPE_RESERVED:

        default:
            usbd_ctrl_unsupport(pudev);
        break;
    }
    return status;
}

/**
  * @brief  usb device endpoint 0 in status stage complete
  * @param  udev: to the structure of usbd_core_type
  * @retval status of usb_sts_type
  */
static usb_sts_type class_ept0_tx_handler(void *udev)
{
    (void)udev;
    return USB_OK;
}

/**
  * @brief  usb device endpoint 0 out status stage complete
  * @param  udev: usb device core handler type
  * @retval status of usb_sts_type
  */
static usb_sts_type class_ept0_rx_handler(void *udev)
{
    usbd_core_type *pudev = (usbd_core_type *)udev;
    cdc_dual_struct_type *pcdc = (cdc_dual_struct_type *)pudev->class_handler->pdata;
    uint32_t recv_len = usbd_get_recv_len(pudev, 0);

    if(pcdc->req == SET_LINE_CODING){
        /* class process */
        class_function(pcdc, pcdc->req, pcdc->cmd, recv_len);
    }

    return USB_OK;
}

/**
  * @brief  usb device transmission complete handler
  * @param  udev: to the structure of usbd_core_type
  * @param  ept_num: endpoint number
  * @retval status of usb_sts_type
  */
static usb_sts_type class_in_handler(void *udev, uint8_t ept_num)
{
    usbd_core_type *pudev = (usbd_core_type *)udev;
    uint8_t port = (USBD_CDC0_BULK_IN_EPT & 0x7F) == ept_num ? 0 : 1;
    cdc_dual_struct_type *pcdc = &((cdc_dual_struct_type *)(pudev->class_handler->pdata))[port];

    usbd_flush_tx_fifo(pudev, ept_num);
    pcdc->tx_completed = 1;

    return USB_OK;
}

/**
  * @brief  usb device endpoint receive data
  * @param  udev: to the structure of usbd_core_type
  * @param  ept_num: endpoint number
  * @retval status of usb_sts_type
  */
static usb_sts_type class_out_handler(void *udev, uint8_t ept_num)
{
    usbd_core_type *pudev = (usbd_core_type *)udev;
    uint8_t port = USBD_CDC0_BULK_OUT_EPT == ept_num ? 0 : 1;
    cdc_dual_struct_type *pcdc = &((cdc_dual_struct_type *)(pudev->class_handler->pdata))[port];

    pcdc->rxlen = usbd_get_recv_len(pudev, ept_num);
    pcdc->rx_completed = 1;

    return USB_OK;
}

/**
  * @brief  usb device sof handler
  * @param  udev: to the structure of usbd_core_type
  * @retval status of usb_sts_type
  */
static usb_sts_type class_sof_handler(void *udev)
{
    (void)udev;
    return USB_OK;
}

/**
  * @brief  usb device event handler
  * @param  udev: to the structure of usbd_core_type
  * @param  event: usb device event
  * @retval status of usb_sts_type
  */
static usb_sts_type class_event_handler(void *udev, usbd_event_type event)
{
    usb_sts_type status = USB_OK;
    (void)udev;

    switch(event)
    {
        case USBD_RESET_EVENT:

        /* ...user code... */

        break;
        case USBD_SUSPEND_EVENT:

        /* ...user code... */

        break;
        case USBD_WAKEUP_EVENT:
        /* ...user code... */

        break;
        case USBD_INISOINCOM_EVENT:
        break;
        case USBD_OUTISOINCOM_EVENT:
        break;

        default:
        break;
    }
    return status;
}

/* usb device class handler */

usbd_class_handler cdc_dual_class_handler = {
    class_init_handler,
    class_clear_handler,
    class_setup_handler,
    class_ept0_tx_handler,
    class_ept0_rx_handler,
    class_in_handler,
    class_out_handler,
    class_sof_handler,
    class_event_handler,
    &cdc_struct
};