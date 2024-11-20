# Mass Storage Device implementation example.

This example emulates an usb mass storage class device that 
is seen  as a removable device by Linux or Windows.
The supported devices are sdcard <4GB and spi flash.

# Pinouts

## MMC/SD Card
<pre>
    sdio1                     at32f415            sd/mmc card  _________
                                                     ______/            |
  - sdio1_d2                    PA6          <-->   |dat2/x             |
  - sdio1_d3                    PA7          <-->   |dat3/cs            |
  - sdio1_cmd                   PA3          --->   |cmd/di             |
                                                    |vdd                |
  - sdio1_ck                    PA2          --->   |clk                |
                                                    |gnd                | 
  - sdio1_d0                    PA4          <-->   |dat0/do            |
  - sdio1_d1                    PA5          <-->   |dat1/x_____________|
</pre>

## SPI Flash
<pre>
    spi1                     at32f415            spi flash
  - cs                          PA4          --->   nCS
  - sck                         PA5          --->   SCLK
  - miso                        PA6          <---   DO
  - mosi                        PA7          --->   DI
</pre>

# Build

Build project for spi flash

>$ make spiflash  

Build project for sd card

>$ make sdcard

Program Artery chip

>$ make program
