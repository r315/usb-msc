// =============================================================================
/*!
 * @file       board.h
 *
 * This file contains board specific definitions
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#ifndef _BOARD_
#define _BOARD_
#include <stdint.h>
#include "at32f415.h"
#include "at32_spiflash.h"
#include "at32_sdio.h"
#include "uart.h"
// ================================================
// LED/Debug pin
// ================================================
#if ENABLE_LED
#define LED_PIN         GPIO_PINS_11
#define LED1_OFF        GPIOB->clr = LED_PIN
#define LED1_ON         GPIOB->scr = LED_PIN
#define LED1_TOGGLE     GPIOB->odt = GPIOB->idt ^ LED_PIN
#define LED1_INIT       {GPIOB->cfghr_bit.iomc11 = 2; GPIOB->cfghr_bit.iofc11 = 0;}
#else
#define LED1_OFF
#define LED1_ON
#define LED1_TOGGLE
#define LED1_INIT
#endif
// ================================================
// Spi Configuration
// ================================================
#define SPIFLASH_PERIPHERAL         1
#define SPIFLASH                    ((SPIFLASH_PERIPHERAL == 1)? SPI1 : SPI2)
/*SCK Pin*/
#define SPIFLASH_SCK_PIN            GPIO_PINS_5
#define SPIFLASH_SCK_GPIO           GPIOA
/*MISO Pin*/
#define SPIFLASH_MISO_PIN           GPIO_PINS_6
#define SPIFLASH_MISO_GPIO          GPIOA
/*MOSI Pin*/
#define SPIFLASH_MOSI_PIN           GPIO_PINS_7
#define SPIFLASH_MOSI_GPIO          GPIOA
/*CS Pin*/
#define SPIFLASH_CS_PIN             GPIO_PINS_4
#define SPIFLASH_CS_GPIO            GPIOA
#define CS_LOW                      0
#define CS_HIGH                     1
#define FLASH_DUMMY_BYTE            0xff
// ================================================
// Push buttons Configuration
// ================================================
#define BUTTON_1_PIN                GPIO_PINS_3
#define BUTTON_1_GPIO               GPIOB
#define LAMP_1_PIN                  GPIO_PINS_2
#define LAMP_1_GPIO                 GPIOB
#define BUTTON_2_PIN                GPIO_PINS_4
#define BUTTON_2_GPIO               GPIOB
#define LAMP_2_PIN                  GPIO_PINS_1
#define LAMP_2_GPIO                 GPIOB
#define BUTTON_3_PIN                GPIO_PINS_5
#define BUTTON_3_GPIO               GPIOB
#define LAMP_3_PIN                  GPIO_PINS_0
#define LAMP_3_GPIO                 GPIOB
#define BUTTON_1_LIGHT              0
#define BUTTON_2_LIGHT              1
#define BUTTON_3_LIGHT              2
#define BUTTON_LIGHT_OFF            255
#define BUTTON_HW_INIT              button_init()
#define BUTTON_HW_READ              button_scan()
// ================================================
// UART Configuration
// ================================================
#define UART_PERIPHERAL             1
#define UART                        USART1
#define UART_TX_PIN                 GPIO_PINS_6
#define UART_TX_GPIO                GPIOB
#define UART_SPEED                  115200
#define UART_DBG_PERIPHERAL         3
#define UART_DBG                    USART3
#define UART_DBG_TX_PIN             GPIO_PINS_10
#define UART_DBG_TX_GPIO            GPIOB
#define UART_DBG_SPEED              115200
// ================================================
// Function prototypes
// ================================================
void board_init(void);
void system_clock_config(void);
void system_tick_init(void);
void delay_init(void);
void delay_ms(uint32_t ms);
uint32_t GetTick(void);
void usb_clock48m_select(usb_clk48_s clk_s);
void usb_config(void);
void usb_gpio_deinit(void);
void usb_unplug(void);
void sw_reset(void);
uint8_t usb_isConnected(void);
void button_init(void);
uint32_t button_scan(void);
void button_light(uint8_t num);
void dbg_log(const char* fmt, ...);
void dbg_log_flush(void);

extern void serial_init(void);
extern uint32_t serial_available(void);
extern uint32_t serial_write(const uint8_t *buf, uint32_t len);
extern uint32_t serial_read(uint8_t *data, uint32_t len);
#endif