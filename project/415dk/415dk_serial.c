#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "at32f415.h"
#include "at32f415_crm.h"
#include "at32f415_gpio.h"

#define UART_BUFFER_SIZE  128

#define UART_ENABLE_TX_FIFO 0
#define UART_ENABLE_RX_DMA  1

#define USART_CTRL1_VAL     0x200C

#if UART_ENABLE_TX_FIFO
#define TX_INT              USART_TDBE_FLAG
static uint8_t tx_buf[UART_BUFFER_SIZE];
#else
#define TX_INT              0
#endif

#if UART_ENABLE_RX_DMA
#define RX_INT              USART_RDBF_FLAG
#else
#define RX_INT              0
#endif

static uint8_t rx_buf[UART_BUFFER_SIZE];
static volatile uint16_t tx_rd, tx_wr, rx_rd, rx_wr;

/**
 * API
 * */
void serial_init(void){
    crm_clocks_freq_type clocks;
    crm_clocks_freq_get(&clocks);
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    crm_periph_reset(CRM_USART1_PERIPH_CLOCK, FALSE);
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_reset(CRM_USART1_PERIPH_CLOCK, TRUE);


    USART1->ctrl1 = USART_CTRL1_VAL | RX_INT | TX_INT;

    USART1->baudr = clocks.apb2_freq / 115200;

    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_pins           = GPIO_PINS_9 | GPIO_PINS_10;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    rx_rd = rx_wr = tx_rd = tx_wr = 0;

#if UART_ENABLE_RX_DMA
    dma_init_type dma_init_struct;

    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);
    dma_reset(DMA1_CHANNEL5);
    dma_default_para_init(&dma_init_struct);

    dma_init_struct.buffer_size = UART_BUFFER_SIZE;
    dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.memory_base_addr = (uint32_t)rx_buf;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_base_addr = (uint32_t)&USART1->dt;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_LOW;
    dma_init_struct.loop_mode_enable = TRUE;
    dma_init(DMA1_CHANNEL5, &dma_init_struct);

    USART1->ctrl3_bit.dmaren = TRUE;
    dma_channel_enable(DMA1_CHANNEL5, TRUE);
#else
    NVIC_SetPriority(USART1_IRQn, 10);
    NVIC_EnableIRQ(USART1_IRQn);
#endif
}

uint32_t serial_available(void){
#if UART_ENABLE_RX_DMA
    uint16_t idx = UART_BUFFER_SIZE - DMA1_CHANNEL5->dtcnt;
    return (idx > rx_rd) ? idx - rx_rd : rx_rd - idx;
#else
	return (rx_wr > rx_rd) ? rx_wr - rx_rd : rx_rd - rx_wr;
#endif
}

uint32_t serial_write(const uint8_t *buf, uint32_t len){
	usart_type *uart = USART1;
    const uint8_t *end = buf + len;

	while(buf < end){
        #if UART_ENABLE_TX_FIFO
        uint16_t size = (tx_wr > tx_rd) ? tx_wr - tx_rd : tx_rd - tx_wr;
		if(UART_BUFFER_SIZE - size > 0){
			tx_buf[tx_wr++] = *buf++;
            if(tx_wr == UART_BUFFER_SIZE){
                tx_wr = 0;
            }
		}else{
			uart->ctrl1_bit.tdbeien = 1;
			while(tx_wr == tx_rd);
		}
        #else
        while(!(uart->sts & USART_TDC_FLAG)){
        }
        uart->dt = *buf++;
        #endif
	}

    #if UART_ENABLE_TX_FIFO
	uart->ctrl1_bit.tdbeien = 1;
    #endif
    return len;
}

uint32_t serial_read(uint8_t *data, uint32_t len){
    uint32_t count = len;

	while(count--){
        while(serial_available() == 0);
        *data++ = rx_buf[rx_rd++];
        if(rx_rd == UART_BUFFER_SIZE){
            rx_rd = 0;
        }
    }

    return len;
}

/**
  * @brief  this function handles usart1 interrupt.
  * @param  none
  * @retval none
  */
void USART1_IRQHandler(void){
    uint32_t isrflags = USART1->sts;
    uint32_t errorflags = isrflags & 0x000F;

    if (errorflags){
        // read DT after STS read clears error flags
        errorflags = USART1->dt;
        return;
    }

    #if !UART_ENABLE_RX_DMA
    if (isrflags & USART_RDBF_FLAG){
        if(serial_available() < UART_BUFFER_SIZE){
            rx_buf[rx_wr++] = USART1->dt;
            if(rx_wr == UART_BUFFER_SIZE){
                rx_wr = 0;
            }else{
                errorflags = USART1->dt;
            }
        }
    }
    #endif

    #if UART_ENABLE_TX_FIFO
    if (isrflags & USART_TDBE_FLAG){
        /* TX empty, send more data or finish transmission */
        if(tx_wr == tx_rd){
            USART1->sts_bit.tdc = 0;            // Clear Transmit Data Complete bit since no write to DT ocurred
            USART1->ctrl1_bit.tdbeien = 0;      // Disable Transmit Buffer Empty interrupt
        }else{
            USART1->dt = tx_buf[tx_rd++];
            if(tx_rd == UART_BUFFER_SIZE){
                tx_rd = 0;
            }
        }
    }
    #endif
}

