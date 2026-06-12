#include <stdint.h>
#include "usb_std.h"
#include "usbd_sdr.h"
#include "usbd_core.h"
#include "cdc_dual_desc.h"


#define MCU_ID      (0x1FFFF7E8)

/**
  * @brief usb device standard descriptor
  */
static const uint8_t g_usbd_descriptor[USB_DEVICE_DESC_LEN]  =
{
  USB_DEVICE_DESC_LEN,                   /* bLength */
  USB_DESCIPTOR_TYPE_DEVICE,             /* bDescriptorType */
  LHBYTE(0x0200),                        /* bcdUSB */
  0x00,                                  /* bDeviceClass: Defined by interface */
  0x00,                                  /* bDeviceSubClass */
  0x00,                                  /* bDeviceProtocol */
  USB_MAX_EP0_SIZE,                      /* bMaxPacketSize */
  LHBYTE(USBD_CDC_VENDOR_ID),            /* idVendor */
  LHBYTE(USBD_CDC_PRODUCT_ID),           /* idProduct */
  LHBYTE(USBD_BCD_NUM),                  /* bcdDevice rel. 2.00 */
  USB_MFC_STRING,                        /* Index of manufacturer string */
  USB_PRODUCT_STRING,                    /* Index of product string */
  USB_SERIAL_STRING,                     /* Index of serial number string */
  0x1                                    /* bNumConfigurations */
};

static const uint8_t g_usbd_configuration[]  =
{
  //-------- Configuration descriptor -------------
  USB_DEVICE_CFG_DESC_LEN,               /* bLength: configuration descriptor size */
  USB_DESCIPTOR_TYPE_CONFIGURATION,      /* bDescriptorType: configuration */
  LHBYTE(USBD_CDC_DUAL_CFG_DESC_SIZE),   /* wTotalLength: bytes returned */
  USBD_CONFIG_IF_NUM,                    /* bNumInterfaces: */
  0x01,                                  /* bConfigurationValue: configuration value */
  USB_CONFIG_STRING,                     /* iConfiguration: index of string describing configuration */
  0x80,                                  /* bmAttributes: Bus powered */
  0x64,                                  /* MaxPower 200 mA: this current is used for detecting vbus */
#if USBD_COM_ENABLE
  //-------- Interface descriptor -------------
  USB_DEVICE_IF_DESC_LEN,                /* bLength: interface descriptor size */
  USB_DESCIPTOR_TYPE_INTERFACE,          /* bDescriptorType: interface descriptor type */
  0x00,                                  /* bInterfaceNumber: number of interface */
  0x00,                                  /* bAlternateSetting: alternate set */
  0x02,                                  /* bNumEndpoints: number of endpoints */
  0xFF,                    /* bInterfaceClass: */
  0x00,                                  /* bInterfaceSubClass: Abstract Control Model (ACM) */
  0x00,                                  /* bInterfaceProtocol: protocol code, AT Command */
  0x07,
  //-------- IN Endpoint descriptor -------------
  USB_DEVICE_EPT_LEN,                    /* bLength */
  USB_DESCIPTOR_TYPE_ENDPOINT,           /* bDescriptorType */
  USBD_CDC0_BULK_IN_EPT,                 /* bEndpointAddress */
  USB_EPT_DESC_BULK,                     /* bmAttributes */
  LHBYTE(USBD_CDC_IN_PKT_SIZE),    /* wMaxPacketSize */
  0x00,
  //-------- OUT Endpoint descriptor -------------
  USB_DEVICE_EPT_LEN,                    /* bLength */
  USB_DESCIPTOR_TYPE_ENDPOINT,           /* bDescriptorType */
  USBD_CDC0_BULK_OUT_EPT,                /* bEndpointAddress */
  USB_EPT_DESC_BULK,                     /* bmAttributes */
  LHBYTE(USBD_CDC_OUT_PKT_SIZE),   /* wMaxPacketSize */
  0x00,
#endif
#if USBD_CDC1_ENABLE
  //--------- CDC #1 Interface Association Descriptor ---------
  USB_DEVICE_IA_DESC_LEN,                /* bLength: interface association descriptor size */
  USB_DESCIPTOR_TYPE_INTERFACE_ASSOCIATION,
  0x00,                                  /* bFirstInterface */
  0x02,                                  /* bInterfaceCount */
  USB_CLASS_CODE_CDC,                    /* bFunctionClass = CDC */
  0x02,                                  /* bFunctionSubClass */
  0x00,                                  /* bFunctionProtocol */
  0x06,                                  /* iFunction */
  //--------- CDC #1 Control Interface (Interface 0) ----------
  USB_DEVICE_IF_DESC_LEN,                /* bLength: interface descriptor size */
  USB_DESCIPTOR_TYPE_INTERFACE,          /* bDescriptorType: interface descriptor type */
  0x00,                                  /* bInterfaceNumber: number of interface */
  0x00,                                  /* bAlternateSetting: alternate set */
  0x01,                                  /* bNumEndpoints: number of endpoints */
  USB_CLASS_CODE_CDC,                    /* bInterfaceClass: CDC */
  0x02,                                  /* bInterfaceSubClass: Abstract Control Model (ACM) */
  0x00,                                  /* bInterfaceProtocol: protocol code, AT Command */
  0x07,                                  /* iInterface: */
  // CDC #1 Class-Specific
  USBD_CDC_SUBTYPE_HEADER_LEN,           /* bFunctionLength: size of this descriptor in bytes */
  USBD_CDC_CS_INTERFACE,                 /* bDescriptorType: CDC interface descriptor type */
  USBD_CDC_SUBTYPE_HEADER,               /* bDescriptorSubtype: Header function Descriptor 0x00*/
  LBYTE(USBD_CDC_BCD_NUM),
  HBYTE(USBD_CDC_BCD_NUM),               /* bcdCDC: USB class definitions for communications */
  // CDC #1 Call Management
  USBD_CDC_SUBTYPE_CFM_LEN,              /* bFunctionLength: size of this descriptor in bytes */
  USBD_CDC_CS_INTERFACE,                 /* bDescriptorType: CDC interface descriptor type */
  USBD_CDC_SUBTYPE_CMF,                  /* bDescriptorSubtype: Call Management function descriptor subtype 0x01 */
  0x00,                                  /* bmCapabilities: 0x00 */
  0x01,                                  /* bDataInterface: interface number of data class interface optionally used for call management */
  // CDC #1 ACM
  USBD_CDC_SUBTYPE_ACM_LEN,              /* bFunctionLength: size of this descriptor in bytes */
  USBD_CDC_CS_INTERFACE,                 /* bDescriptorType: CDC interface descriptor type */
  USBD_CDC_SUBTYPE_ACM,                  /* bDescriptorSubtype: Abstract Control Management functional descriptor subtype 0x02 */
  0x00,                                  /* bmCapabilities: 0x00 */
  // CDC #1 Union
  USBD_CDC_SUBTYPE_UFD_LEN,              /* bFunctionLength: size of this descriptor in bytes */
  USBD_CDC_CS_INTERFACE,                 /* bDescriptorType: CDC interface descriptor type */
  USBD_CDC_SUBTYPE_UFD,                  /* bDescriptorSubtype: Union Function Descriptor subtype 0x06 */
  0x00,                                  /* bControlInterface: The interface number of the communications or data class interface 0x00 */
  0x01,                                  /* bSubordinateInterface0: interface number of first subordinate interface in the union */
  // CDC #1 Notification Endpoint (Interrupt IN)
  USB_DEVICE_EPT_LEN,                    /* bLength: size of endpoint descriptor in bytes */
  USB_DESCIPTOR_TYPE_ENDPOINT,           /* bDescriptorType: endpoint descriptor type */
  USBD_CDC0_INT_EPT,                     /* bEndpointAddress: the address of endpoint on usb device described by this descriptor */
  USB_EPT_DESC_INTERRUPT,                /* bmAttributes: endpoint attributes */
  LBYTE(USBD_CDC_CMD_PKT_SIZE),
  HBYTE(USBD_CDC_CMD_PKT_SIZE),          /* wMaxPacketSize: maximum packet size this endpoint */
  0x10,                                  /* bInterval: interval for polling endpoint for data transfers */
  //--------- CDC #1 Data Interface (Interface 1) -------
  USB_DEVICE_IF_DESC_LEN,                /* bLength: interface descriptor size */
  USB_DESCIPTOR_TYPE_INTERFACE,          /* bDescriptorType: interface descriptor type */
  0x01,                                  /* bInterfaceNumber: number of interface */
  0x00,                                  /* bAlternateSetting: alternate set */
  0x02,                                  /* bNumEndpoints: number of endpoints */
  USB_CLASS_CODE_CDCDATA,                /* bInterfaceClass: CDC-data class code */
  0x00,                                  /* bInterfaceSubClass: Data interface subclass code 0x00*/
  0x00,                                  /* bInterfaceProtocol: data class protocol code 0x00 */
  0x08,                                  /* iInterface: index of string descriptor */
  // CDC #1 Data OUT Endpoint (Bulk OUT)
  USB_DEVICE_EPT_LEN,                    /* bLength: size of endpoint descriptor in bytes */
  USB_DESCIPTOR_TYPE_ENDPOINT,           /* bDescriptorType: endpoint descriptor type */
  USBD_CDC0_BULK_OUT_EPT,                /* bEndpointAddress: the address of endpoint on usb device described by this descriptor */
  USB_EPT_DESC_BULK,                     /* bmAttributes: endpoint attributes */
  LHBYTE(USBD_CDC_IN_PKT_SIZE),          /* wMaxPacketSize: maximum packe size this endpoint */
  0x01,                                  /* bInterval: interval for polling endpoint for data transfers */
  // CDC #1 Data IN Endpoint (Bulk IN)
  USB_DEVICE_EPT_LEN,                    /* bLength: size of endpoint descriptor in bytes */
  USB_DESCIPTOR_TYPE_ENDPOINT,           /* bDescriptorType: endpoint descriptor type */
  USBD_CDC0_BULK_IN_EPT,                 /* bEndpointAddress: the address of endpoint on usb device described by this descriptor */
  USB_EPT_DESC_BULK,                     /* bmAttributes: endpoint attributes */
  LHBYTE(USBD_CDC_OUT_PKT_SIZE),         /* wMaxPacketSize: maximum packe size this endpoint */
  0x01,                                  /* bInterval: interval for polling endpoint for data transfers */
  #endif
  #if USBD_CDC2_ENABLE
  //--------- CDC #2 Interface Association Descriptor ---------
  USB_DEVICE_IA_DESC_LEN,                /* bLength: interface association descriptor size */
  USB_DESCIPTOR_TYPE_INTERFACE_ASSOCIATION,
  0x02,                                  /* bFirstInterface */
  0x02,                                  /* bInterfaceCount */
  0x02,                                  /* bFunctionClass = CDC */
  0x02,                                  /* bFunctionSubClass */
  0x01,                                  /* bFunctionProtocol */
  0x09,                                  /* iFunction */
  // ------- CDC #2 Control Interface (Interface 2) -------
  USB_DEVICE_IF_DESC_LEN,
  USB_DESCIPTOR_TYPE_INTERFACE,
  0x02,                                  /* bInterfaceNumber */
  0x00,                                  /* bAlternateSetting */
  0x01,                                  /* bNumEndpoints: 0x01 (interrupt)*/
  USB_CLASS_CODE_CDC,                    /* bInterfaceClass: CDC-data class code */
  0x02,                                  /* bInterfaceSubClass: ACM */
  0x01,                                  /* bInterfaceProtocol: AT commands defined by ITU-T v.250 */
  0x0A,                                  /* iInterface: */
  // CDC #2 Class-Specific
  USBD_CDC_SUBTYPE_HEADER_LEN,
  USBD_CDC_CS_INTERFACE,
  USBD_CDC_SUBTYPE_HEADER,
  LHBYTE(USBD_CDC_BCD_NUM),
  // CDC #2 Call Management
  USBD_CDC_SUBTYPE_CFM_LEN,
  USBD_CDC_CS_INTERFACE,
  USBD_CDC_SUBTYPE_CMF,
  0x00,                                  /* bmCapabilities: 0x00*/
  0x03,                                  /* bDataInterface: if using data interface 3 set bDataInterface=3 */
  // CDC #2 ACM
  USBD_CDC_SUBTYPE_ACM_LEN,
  USBD_CDC_CS_INTERFACE,
  USBD_CDC_SUBTYPE_ACM,
  0x00,                                  /* bmCapabilities: 0x00*/
  // CDC #2 Union
  USBD_CDC_SUBTYPE_UFD_LEN,
  USBD_CDC_CS_INTERFACE,
  USBD_CDC_SUBTYPE_UFD,
  0x02,                                  /*bControlInterface: master = 2 */
  0x03,                                  /*bSubordinateInterface0: slave = 3 */
  // CDC #2 Notification Endpoint (Interrupt IN)
  USB_DEVICE_EPT_LEN,
  USB_DESCIPTOR_TYPE_ENDPOINT,
  USBD_CDC1_INT_EPT,
  USB_EPT_DESC_INTERRUPT,
  LHBYTE(USBD_CDC_CMD_PKT_SIZE),
  0x10,
  // ------- CDC #2 Data Interface (Interface 3) -------
  USB_DEVICE_IF_DESC_LEN,
  USB_DESCIPTOR_TYPE_INTERFACE,
  0x03,                                  /* bInterfaceNumber: number of interface */
  0x00,                                  /* bAlternateSetting: alternate set */
  0x02,                                  /* bNumEndpoints: number of endpoints */
  USB_CLASS_CODE_CDCDATA,
  0x00,
  0x00,
  0x0B,
  // CDC #2 Data OUT Endpoint (Bulk OUT)
  USB_DEVICE_EPT_LEN,
  USB_DESCIPTOR_TYPE_ENDPOINT,
  USBD_CDC1_BULK_OUT_EPT,
  USB_EPT_DESC_BULK,                     /* bmAttributes: endpoint attributes */
  LHBYTE(USBD_CDC_OUT_PKT_SIZE),
  0x00,                                  /* bInterval: */
  // CDC #2 Data IN Endpoint (Bulk IN)
  USB_DEVICE_EPT_LEN,
  USB_DESCIPTOR_TYPE_ENDPOINT,
  USBD_CDC1_BULK_IN_EPT,
  USB_EPT_DESC_BULK,
  LHBYTE(USBD_CDC_OUT_PKT_SIZE),
  0x00,                                  /* bInterval: */
  #endif
};

/**
  * @brief usb string lang id
  */
static const uint8_t g_string_lang_id[USBD_CDC_SIZ_STRING_LANGID]  =
{
  USBD_CDC_SIZ_STRING_LANGID,
  USB_DESCIPTOR_TYPE_STRING,
  0x09,
  0x04,
};

static const char *g_usbd_desc_strings [] = {
    "",
    "BMP",                           /* MFC */
    "Black Magic Probe",             /* Product */
    "",                              /* Serial */
    "CDC DUAL configuration",        /* Configuration */
    "CDC DUAL interface",            /* Interface */
    "CDC #1 IAD Interface",
    "CDC #1 Control Interface",
    "CDC #1 Data Interface",
    "CDC #2 IAD Interface",
    "CDC #2 Control Interface",
    "CDC #2 Data Interface",
};

static uint8_t g_usbd_desc_buffer[256];
static usbd_desc_t g_usbd_desc;

/**
  * @brief  standard usb unicode convert
  * @param  string: source string
  * @param  unicode_buf: unicode buffer
  * @retval length
  */
static uint16_t usbd_unicode_convert(const uint8_t *string, uint8_t *unicode_buf)
{
  uint16_t str_len = 0, id_pos = 2;
  const uint8_t *tmp_str = string;

  while(*tmp_str != '\0')
  {
    str_len ++;
    unicode_buf[id_pos ++] = *tmp_str ++;
    unicode_buf[id_pos ++] = 0x00;
  }

  str_len = str_len * 2 + 2;
  unicode_buf[0] = (uint8_t)str_len;
  unicode_buf[1] = USB_DESCIPTOR_TYPE_STRING;

  return str_len;
}

/**
  * @brief  usb int convert to unicode
  * @param  value: int value
  * @param  pbus: unicode buffer
  * @param  len: length
  * @retval none
  */
static void usbd_int_to_unicode (uint32_t value , uint8_t *pbuf , uint8_t len)
{
  uint8_t idx = 0;

  for( idx = 0 ; idx < len ; idx ++)
  {
    if( ((value >> 28)) < 0xA )
    {
      pbuf[ 2 * idx] = (value >> 28) + '0';
  }
  else
  {
      pbuf[2 * idx] = (value >> 28) + 'A' - 10;
    }

    value = value << 4;

    pbuf[2 * idx + 1] = 0;
  }
}

/**
  * @brief  get device descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_descriptor(void)
{
    return &(const usbd_desc_t){
        (uint16_t)USB_DEVICE_DESC_LEN,
        g_usbd_descriptor
    };
}

/**
  * @brief  get device qualifier
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t * get_device_qualifier(void)
{
  return NULL;
}

/**
  * @brief  get config descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_configuration(void)
{
    return &(const usbd_desc_t){
        sizeof(g_usbd_configuration),
        g_usbd_configuration
    };
}

/**
  * @brief  get other speed descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_other_speed(void)
{
  return NULL;
}

/**
  * @brief  get lang id descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_lang_id(void)
{
  return &(const usbd_desc_t){
        USBD_CDC_SIZ_STRING_LANGID,
        g_string_lang_id
    };
}

/**
 * @brief Get string by index
 *
 * @param index
 * @return usbd_desc_t*
 */
static const usbd_desc_t *get_device_string(uint8_t index)
{
    if(index > (sizeof(g_usbd_desc_strings) / sizeof(*g_usbd_desc_strings))){
        return NULL;
    }

    g_usbd_desc.length = usbd_unicode_convert((const uint8_t *)g_usbd_desc_strings[index], g_usbd_desc_buffer);
    g_usbd_desc.descriptor = g_usbd_desc_buffer;
    return &g_usbd_desc;
}

/**
  * @brief  get manufacturer descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_manufacturer_string(void)
{
  return get_device_string(USB_MFC_STRING);
}

/**
  * @brief  get product descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_product_string(void)
{
  return get_device_string(USB_PRODUCT_STRING);
}

/**
  * @brief  get serial descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_serial_string(void)
{
    uint32_t *uid = (uint32_t*)MCU_ID;

    uid[0] += uid[2];

    if (uid[0] != 0){
        usbd_int_to_unicode (uid[0], &g_usbd_desc_buffer[2] , 8);
        usbd_int_to_unicode (uid[1], &g_usbd_desc_buffer[18] ,4);
    }

    g_usbd_desc_buffer[0] = USBD_CDC_SIZ_STRING_SERIAL;
    g_usbd_desc_buffer[1] = USB_DESCIPTOR_TYPE_STRING;

    return &(const usbd_desc_t){
        USBD_CDC_SIZ_STRING_SERIAL,
        g_usbd_desc_buffer
    };
}

/**
  * @brief  get interface descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_interface_string(void)
{
  return get_device_string(USB_INTERFACE_STRING);
}

/**
  * @brief  get device config descriptor
  * @param  none
  * @retval usbd_desc
  */
static const usbd_desc_t *get_device_config_string(void)
{
  return get_device_string(USB_CONFIG_STRING);
}

/**
  * @brief device descriptor handler structure
  */
usbd_desc_handler cdc_dual_desc_handler =
{
    get_device_descriptor,
    get_device_qualifier,
    get_device_configuration,
    get_device_other_speed,
    get_device_lang_id,
    get_device_manufacturer_string,
    get_device_product_string,
    get_device_serial_string,
    get_device_interface_string,
    get_device_config_string,
    get_device_string,
};
