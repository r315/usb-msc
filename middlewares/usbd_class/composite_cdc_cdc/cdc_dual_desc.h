#ifndef __CDC_CDC_DESC_H
#define __CDC_CDC_DESC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_core.h"
#include "cdc_dual_class.h"

#if 1
#define USBD_CDC_VENDOR_ID               0x2E3C
#define USBD_CDC_PRODUCT_ID              0x5740
#else
#define USBD_CDC_VENDOR_ID               0x1915
#define USBD_CDC_PRODUCT_ID              0xC00A
#endif


#define USBD_CDC_BCD_NUM                 0x0110
#define USBD_BCD_NUM                     0x0100

#define USBD_CDC0_INT_EPT                0x82
#define USBD_CDC1_INT_EPT                0x84
#define USBD_CDC0_BULK_IN_EPT            0x81
#define USBD_CDC0_BULK_OUT_EPT           0x01
#define USBD_CDC1_BULK_IN_EPT            0x83
#define USBD_CDC1_BULK_OUT_EPT           0x03

#define USBD_CDC_SIZ_STRING_LANGID       4
#define USBD_CDC_SIZ_STRING_SERIAL       0x1A
#define USBD_CDC_SUBTYPE_HEADER_LEN      0x05
#define USBD_CDC_SUBTYPE_CFM_LEN         0x05
#define USBD_CDC_SUBTYPE_UFD_LEN         0x05
#define USBD_CDC_SUBTYPE_ACM_LEN         0x04


#define USBD_CDC1_ENABLE                 1
#define USBD_CDC2_ENABLE                 1
#define USBD_COM_ENABLE                  0

/*
#if USBD_CDC_DUAL_ENABLE
#define USBD_CONFIG_IF_NUM               4
#define USBD_CDC_CONFIG_DESC_SIZE        141
#else
#define USBD_CONFIG_IF_NUM               2
#define USBD_CDC_CONFIG_DESC_SIZE        75
#endif
*/

#define USBD_CDC_DUAL_CFG_DESC_SIZE      141
#define USBD_CONFIG_IF_NUM               4

extern usbd_desc_handler cdc_dual_desc_handler;

#ifdef __cplusplus
}
#endif

#endif
