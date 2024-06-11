#include "415dk_board.h"
#include "at32_sdio.h"
#include <string.h>

static volatile uint32_t ticms;

void SysTick_Handler(void)
{ 
    ticms++;
}

void delay_ms(uint32_t ms)
{
    volatile uint32_t end = ticms + ms;
    while (ticms < end){ }
}

void delay_us(uint32_t us)
{
    volatile uint32_t systick = SysTick->VAL;
    while(SysTick->VAL == systick);
}

uint32_t get_tick(void)
{
    return ticms;
}

void at32_board_init(void)
{
    SystemInit();
    system_core_clock_update();
    system_tick_init();
    LED1_INIT;
}

/**
  * @brief  this function config gpio.
  * @param  none
  * @retval none
  */
void usb_gpio_config(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);

  // idle state for FS usb
  gpio_bits_write(GPIOA, GPIO_PINS_11, TRUE);  // D- Low
  gpio_bits_write(GPIOA, GPIO_PINS_12, TRUE);  // D+ High

  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = GPIO_PINS_11 | GPIO_PINS_12;
  gpio_init(GPIOA, &gpio_init_struct);
}

void usb_gpio_deinit(void)
{
    gpio_init_type gpio_init_struct;

    gpio_default_para_init(&gpio_init_struct);

    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_11 | GPIO_PINS_12;
    gpio_init(GPIOA, &gpio_init_struct);
}

void sw_reset(void){
    NVIC_SystemReset();
}


