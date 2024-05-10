#ifndef _415DK_BOARD_
#define _415DK_BOARD_

#include <stdint.h>
#include "at32f415.h"

#define LED1_ON         GPIOA->scr = (1 << 8)
#define LED1_OFF        GPIOA->clr = (1 << 8)
#define LED1_TOGGLE     GPIOA->odt = GPIOA->idt ^ (1 << 8)
#define LED1_INIT \
        CRM->apb2en_bit.gpioaen = 1; \
        GPIOA->cfghr |= (6 << 0)

void at32_board_init(void);
void system_clock_config(void);
void delay_init(void);
void delay_ms(uint32_t ms);
void usb_clock48m_select(usb_clk48_s clk_s);
void usb_gpio_config(void);
void usb_gpio_deinit(void);
void sw_reset(void);
#endif