#include "at32_spiflash.h"
#include "at32f415.h"

/**
 * @brief  Enables disables spi flash
 * @param  state: 0 - selected, deselected otherwise
 * @retval None
 */
void spiflash_cs (uint8_t state)
{
   if (state)
      FLASH_SPI_CS_GPIO->scr = FLASH_SPI_CS_PIN;
   else
      FLASH_SPI_CS_GPIO->clr = FLASH_SPI_CS_PIN;
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

   /*RCC Config*/
   #ifndef AT32_SPI2
   crm_periph_clock_enable (CRM_SPI1_PERIPH_CLOCK, TRUE);
   crm_periph_clock_enable (CRM_GPIOA_PERIPH_CLOCK, TRUE);
   #else
   crm_periph_clock_enable (CRM_SPI2_PERIPH_CLOCK, TRUE);
   crm_periph_clock_enable (CRM_GPIOB_PERIPH_CLOCK, TRUE);
   #endif

   crm_periph_reset(FLASH_SPI_RESET, TRUE);

   /*Deselect FLASH*/
   spiflash_cs (CS_HIGH);

   /*Configure SPI SCK pin*/
   GPIO_InitStructure.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
   GPIO_InitStructure.gpio_mode           = GPIO_MODE_MUX;
   GPIO_InitStructure.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
   GPIO_InitStructure.gpio_pins           = FLASH_SPI_SCK_PIN;
   gpio_init (FLASH_SPI_SCK_GPIO, &GPIO_InitStructure);

   /*Configure SPI MISO pin*/
   GPIO_InitStructure.gpio_pins = FLASH_SPI_MISO_PIN;
   gpio_init (FLASH_SPI_MISO_GPIO, &GPIO_InitStructure);

   /*Configure SPI MOSI pin*/
   GPIO_InitStructure.gpio_pins = FLASH_SPI_MOSI_PIN;
   gpio_init (FLASH_SPI_MOSI_GPIO, &GPIO_InitStructure);

   /*Configure SPI CS pin*/
   GPIO_InitStructure.gpio_pins = FLASH_SPI_CS_PIN;
   GPIO_InitStructure.gpio_mode = GPIO_MODE_OUTPUT;
   gpio_init (FLASH_SPI_CS_GPIO, &GPIO_InitStructure);

   crm_periph_reset(FLASH_SPI_RESET, FALSE);

   /*SPI Configuration*/
   SPI_InitStruct.transmission_mode      = SPI_TRANSMIT_FULL_DUPLEX;
   SPI_InitStruct.master_slave_mode      = SPI_MODE_MASTER;
   SPI_InitStruct.mclk_freq_division     = SPI_MCLK_DIV_4;
   SPI_InitStruct.frame_bit_num          = SPI_FRAME_8BIT;
   SPI_InitStruct.first_bit_transmission = SPI_FIRST_BIT_MSB;
   SPI_InitStruct.cs_mode_selection      = SPI_CS_SOFTWARE_MODE;
   SPI_InitStruct.clock_polarity         = SPI_CLOCK_POLARITY_HIGH;
   SPI_InitStruct.clock_phase            = SPI_CLOCK_PHASE_2EDGE;
   spi_init (FLASH_SPI, &SPI_InitStruct);

   spi_crc_polynomial_set (FLASH_SPI, 7);

   /*Enable SPI*/
   spi_enable (FLASH_SPI, TRUE);
}

/**
 * @brief  Sends a byte through the SPI interface and return the byte received
 *         from the SPI bus.
 * @param  byte: byte to send.
 * @retval The value of the received byte.
 */
uint8_t spiflash_sendbyte (uint8_t byte)
{
   /*!< Loop while DR register in not emplty */
   while (spi_i2s_flag_get (FLASH_SPI, SPI_I2S_TDBE_FLAG) == RESET)
      ;

   /*!< Send byte through the SPI1 peripheral */
   spi_i2s_data_transmit (FLASH_SPI, byte);

   /*!< Wait to receive a byte */
   while (spi_i2s_flag_get (FLASH_SPI, SPI_I2S_RDBF_FLAG) == RESET)
      ;

   /*!< Return the byte read from the SPI bus */
   return spi_i2s_data_receive (FLASH_SPI);
}

/**
 * @brief  Reads a byte from the SPI Flash.
 * @note   This function must be used only if the Start_Read_Sequence function
 *         has been previously called.
 * @param  None
 * @retval Byte Read from the SPI Flash.
 */
uint8_t spiflash_readbyte (void)
{
   return (spiflash_sendbyte (FLASH_DUMMY_BYTE));
}



