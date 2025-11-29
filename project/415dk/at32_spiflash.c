// =============================================================================
/*!
 * @file       at32_spiflash.c
 *
 * This file contains MAC implementation for spi flash memories
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#include "at32f415.h"
#include "at32_spiflash.h"
#include "board.h"
#define SPI_DMA_RX_FLAG  (SPIFLASH_PERIPHERAL == 1) ? DMA1_FDT2_FLAG : DMA1_FDT4_FLAG
#define SPI_DMA_TX_FLAG  (SPIFLASH_PERIPHERAL == 1) ? DMA1_FDT3_FLAG : DMA1_FDT5_FLAG
static dma_channel_type *spi_dma_rx;
static dma_channel_type *spi_dma_tx;
/**
 * @brief  Enables disables spi flash
 * @param  state: 0 - selected, deselected otherwise
 * @retval None
 */
void spiflash_cs (uint8_t state)
{
   if (state)
      SPIFLASH_CS_GPIO->scr = SPIFLASH_CS_PIN;
   else
      SPIFLASH_CS_GPIO->clr = SPIFLASH_CS_PIN;
}
/**
 * @brief  Initializes the peripherals used by the SPI FLASH driver.
 * @param  None
 * @retval None
 */
void spiflash_init (void)
{
   gpio_init_type GPIO_InitStructure;
   spi_init_type SPI_InitStruct;
   dma_init_type dma_init_struct;
   /*RCC Config*/
   #if SPIFLASH_PERIPHERAL == 1
   crm_periph_clock_enable (CRM_SPI1_PERIPH_CLOCK, TRUE);
   crm_periph_clock_enable (CRM_GPIOA_PERIPH_CLOCK, TRUE);
   spi_dma_rx = DMA1_CHANNEL2;
   spi_dma_tx = DMA1_CHANNEL3;
   #else
   crm_periph_clock_enable (CRM_SPI2_PERIPH_CLOCK, TRUE);
   crm_periph_clock_enable (CRM_GPIOB_PERIPH_CLOCK, TRUE);
   spi_dma_rx = DMA1_CHANNEL4;
   spi_dma_tx = DMA1_CHANNEL5;
   #endif
   crm_periph_clock_enable (CRM_DMA1_PERIPH_CLOCK, TRUE);
   spi_i2s_reset(SPIFLASH);
   /*Deselect FLASH*/
   spiflash_cs (CS_HIGH);
   /*Configure SPI SCK pin*/
   GPIO_InitStructure.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
   GPIO_InitStructure.gpio_mode           = GPIO_MODE_MUX;
   GPIO_InitStructure.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
   GPIO_InitStructure.gpio_pins           = SPIFLASH_SCK_PIN;
   gpio_init (SPIFLASH_SCK_GPIO, &GPIO_InitStructure);
   /*Configure SPI MISO pin*/
   GPIO_InitStructure.gpio_pins = SPIFLASH_MISO_PIN;
   gpio_init (SPIFLASH_MISO_GPIO, &GPIO_InitStructure);
   /*Configure SPI MOSI pin*/
   GPIO_InitStructure.gpio_pins = SPIFLASH_MOSI_PIN;
   gpio_init (SPIFLASH_MOSI_GPIO, &GPIO_InitStructure);
   /*Configure SPI CS pin*/
   GPIO_InitStructure.gpio_pins = SPIFLASH_CS_PIN;
   GPIO_InitStructure.gpio_mode = GPIO_MODE_OUTPUT;
   gpio_init (SPIFLASH_CS_GPIO, &GPIO_InitStructure);
   /*SPI Configuration*/
   SPI_InitStruct.transmission_mode      = SPI_TRANSMIT_FULL_DUPLEX;
   SPI_InitStruct.master_slave_mode      = SPI_MODE_MASTER;
   SPI_InitStruct.mclk_freq_division     = SPI_MCLK_DIV_4;
   SPI_InitStruct.frame_bit_num          = SPI_FRAME_8BIT;
   SPI_InitStruct.first_bit_transmission = SPI_FIRST_BIT_MSB;
   SPI_InitStruct.cs_mode_selection      = SPI_CS_SOFTWARE_MODE;
   SPI_InitStruct.clock_polarity         = SPI_CLOCK_POLARITY_LOW;
   SPI_InitStruct.clock_phase            = SPI_CLOCK_PHASE_1EDGE;
   spi_init (SPIFLASH, &SPI_InitStruct);
   spi_crc_polynomial_set (SPIFLASH, 7);
   /* Configure DMA */
   dma_reset(spi_dma_tx);
   dma_reset(spi_dma_rx);
   dma_default_para_init(&dma_init_struct);
   dma_init_struct.buffer_size = 0;
   dma_init_struct.memory_base_addr = (uint32_t)0;
   dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
   dma_init_struct.memory_inc_enable = TRUE;
   dma_init_struct.peripheral_base_addr = (uint32_t)&SPIFLASH->dt;
   dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
   dma_init_struct.peripheral_inc_enable = FALSE;
   dma_init_struct.priority = DMA_PRIORITY_MEDIUM;
   dma_init_struct.loop_mode_enable = FALSE;
   dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
   dma_init(spi_dma_tx, &dma_init_struct);
   dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
   dma_init(spi_dma_rx, &dma_init_struct);
   /*Enable SPI*/
   spi_enable (SPIFLASH, TRUE);
}
/**
 * @brief
 *
 * @param
 * @retval
 */
static void spiflash_check_errors (void)
{
   volatile uint8_t status = SPIFLASH->sts;
   // overrun error, read spi_dt then spi_sts
   if(status & (SPI_I2S_ROERR_FLAG)){
      status = spi_i2s_data_receive (SPIFLASH);
      status = SPIFLASH->sts;
   }
   // underload error, read spi_sts
   if(status & (SPI_I2S_ROERR_FLAG)){
      status = SPIFLASH->sts;
   }
}
/**
 * @brief
 *
 * @param
 * @retval
 */
uint8_t spiflash_xchbyte (uint8_t byte)
{
   spiflash_check_errors ();
   while (spi_i2s_flag_get (SPIFLASH, SPI_I2S_TDBE_FLAG) == RESET);
   spi_i2s_data_transmit (SPIFLASH, byte);
   while (spi_i2s_flag_get (SPIFLASH, SPI_I2S_RDBF_FLAG) == RESET);
   return spi_i2s_data_receive (SPIFLASH);
}
/**
 * @brief  Sends a byte through the SPI interface and return the byte received
 *         from the SPI bus.
 * @param  byte: byte to send.
 * @retval The value of the received byte.
 */
void spiflash_sendbyte (uint8_t byte)
{
   spiflash_xchbyte (byte);
}
/**
 * @brief  Reads a byte from the SPI Flash.
 * @note   This function must be used only if the Start_Read_Sequence function
 *         has been previously called.
 * @param  None
 * @retval Byte Read from the SPI Flash.
 */
uint8_t spiflash_receivebyte (void)
{
   return spiflash_xchbyte (FLASH_DUMMY_BYTE);
}
/**
 * @brief  Reads a block of data using DMA
 * @param  pbuffer  Destination of read data
 * @param  len  Length of data to be read
 * @retval Number of bytes read
 */
uint32_t spiflash_read (uint8_t *pbuffer, uint32_t len)
{
   uint8_t dummy_data = FLASH_DUMMY_BYTE;
   /* Don't support transfers larger than 64k
    * Because DMA is limited to 64k and read byte by byte
    * is too slow
    */
   if(len >= 0x10000){
      return 0;
   }
   /* for small transfers just read one by one */
   if(len < 10)
   {
      uint32_t loop = len;
      while (loop--)
      {
         *pbuffer = spiflash_receivebyte ();
         pbuffer++;
      }
      return len;
   }
   /* Larger transfers, use DMA */
   dma_data_number_set(spi_dma_rx, len);
   spi_dma_rx->maddr = (uint32_t)pbuffer;
   /* RX DMA only works if a TX DMA is also set */
   dma_data_number_set(spi_dma_tx, len);
   spi_dma_tx->maddr = (uint32_t)&dummy_data;

   /* Copy same data byte */
   spi_dma_tx->ctrl_bit.mincm = FALSE;

   spi_i2s_dma_receiver_enable(SPIFLASH, TRUE);
   spi_i2s_dma_transmitter_enable(SPIFLASH, TRUE);
   dma_flag_clear(SPI_DMA_TX_FLAG);
   dma_flag_clear(SPI_DMA_RX_FLAG);
   dma_channel_enable(spi_dma_rx, TRUE);
   dma_channel_enable(spi_dma_tx, TRUE);
   while(dma_flag_get(SPI_DMA_RX_FLAG) == RESET);
   dma_channel_enable(spi_dma_rx, FALSE);
   dma_channel_enable(spi_dma_tx, FALSE);
   spi_dma_tx->ctrl_bit.mincm = TRUE;
   spi_i2s_dma_receiver_enable(SPIFLASH, FALSE);
   spi_i2s_dma_transmitter_enable(SPIFLASH, FALSE);
   spiflash_check_errors ();
   return len - dma_data_number_get(spi_dma_rx);
}
/**
 * @brief  Reads a block of data using DMA
 * @param  pbuffer  Destination of read data
 * @param  len  Length of data to be read
 * @retval Number of bytes read
 */
uint32_t spiflash_write (const uint8_t *pbuffer, uint32_t len)
{
   /* Don't support transfers larger than 64k
    * Because DMA is limited to 64k and read byte by byte
    * is too slow
    */
   if(len >= 0x10000){
      return 0;
   }
   /* for small transfers just write one by one */
   if(len < 10)
   {
      uint32_t loop = len;
      while (loop--)
      {
         spiflash_sendbyte (*(pbuffer++));
      }
      return len;
   }
   /* Larger transfers, use DMA */
   dma_data_number_set(spi_dma_tx, len);
   spi_dma_tx->maddr = (uint32_t)pbuffer;
   spi_i2s_dma_transmitter_enable(SPIFLASH, TRUE);
   dma_flag_clear(SPI_DMA_TX_FLAG);
   dma_channel_enable(spi_dma_tx, TRUE);
   while(dma_flag_get(SPI_DMA_TX_FLAG) == RESET);
   spi_i2s_dma_transmitter_enable(SPIFLASH, FALSE);
   dma_channel_enable(spi_dma_tx, FALSE);
   // An overrun error (ROERR flag) will occur because we are only
   // transmitting data without reading from spi_dt,
   // leading to the receive buffer overflow.
   spiflash_check_errors ();
   return len - dma_data_number_get(spi_dma_tx);
}