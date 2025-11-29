#include <string.h>
#include "board.h"
#include "msc_class.h"
#include "msc_desc.h"
#include "msc_diskio.h"
#include "usbd_int.h"


static otg_core_type otg_core_struct;
static volatile uint32_t ticms;

void SysTick_Handler(void)
{
    ticms++;
}

void OTGFS1_IRQHandler(void)
{
  usbd_irq_handler(&otg_core_struct);
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

uint32_t GetTick(void)
{
    return ticms;
}

void board_init(void)
{
    SystemInit();
    system_core_clock_update();
    system_tick_init();
    LED1_INIT;
    otg_core_struct.usb_reg = NULL;
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

void usb_unplug(void)
{
    usb_disconnect(otg_core_struct.usb_reg);
    delay_ms(100);
    nvic_irq_disable(OTGFS1_IRQn);
    crm_periph_reset(CRM_OTGFS1_PERIPH_RESET, TRUE);
    usb_gpio_deinit();

    otg_core_struct.usb_reg = NULL;
}

void usb_config(void)
{
#ifdef USB_LOW_POWER_WAKUP
  usb_low_power_wakeup_config();
#endif

  /* enable otgfs clock */
  crm_periph_clock_enable(CRM_OTGFS1_PERIPH_CLOCK, TRUE);

  crm_periph_reset(CRM_OTGFS1_PERIPH_RESET, TRUE);

  /* select usb 48m clock source */
  //usb_clock48m_select(USB_CLK_HICK);
  /* Using internal oscillator */
  crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_HICK);

  crm_periph_reset(CRM_OTGFS1_PERIPH_RESET, FALSE);

  usb_gpio_config();

  /* enable otgfs irq */
  nvic_irq_enable(OTGFS1_IRQn, 0, 0);

  /* init usb */
  usbd_init(&otg_core_struct,
            USB_FULL_SPEED_CORE_ID,
            USB_ID,
            &msc_class_handler,
            &msc_desc_handler
            //&cdc_msc_class_handler,
            //&cdc_msc_desc_handler
        );
}

void sw_reset(void){
    NVIC_SystemReset();
}


