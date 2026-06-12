/**
  **************************************************************************
  * @file     cdc_class.h
  * @brief    usb cdc class file
  **************************************************************************
  *
  * Copyright (c) 2025, Artery Technology, All rights reserved.
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

 /* define to prevent recursive inclusion -------------------------------------*/
#ifndef __CDC_CDC_CLASS_H
#define __CDC_CDC_CLASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usb_std.h"
#include "usbd_core.h"

/**
  * @brief usb cdc in and out max packet size define
  */
#define USBD_CDC_IN_PKT_SIZE            0x0040
#define USBD_CDC_OUT_PKT_SIZE           0x0040
#define USBD_CDC_CMD_PKT_SIZE           0x0008

typedef void (*receive_callback)(uint8_t *data, uint16_t len);

typedef struct {
    uint32_t alt_setting;
    uint8_t req;
    uint16_t len;
    uint16_t rxlen;
    __IO uint8_t tx_completed, rx_completed;
    linecoding_type linecoding;
    uint8_t rx_buff[USBD_CDC_OUT_PKT_SIZE];
    uint8_t cmd[USBD_CDC_CMD_PKT_SIZE];
    void (*cdc_set_config)(void *dev, const uint16_t value);
}cdc_dual_struct_type;

extern usbd_class_handler cdc_dual_class_handler;

#ifdef __cplusplus
}
#endif

#endif




