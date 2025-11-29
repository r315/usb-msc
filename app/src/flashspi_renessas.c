// =============================================================================
/*!
 * @file       flashspi_renessas.c
 *
 * This file contains flash spi device specific commands implementation
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#include <stdio.h>
#include "flashspi.h"
#include "board.h"

#define AT25SF321B_M_ID         0x1F15
#define AT25SF321B_DEV_ID       0x1f8701

#define AT25SF321B_CMD_RDSR1    0x05
#define AT25SF321B_CMD_RDSR2    0x35
#define AT25SF321B_CMD_RDSR3    0x15
#define AT25SF321B_CMD_WRSR1    0x01
#define AT25SF321B_CMD_WRSR2    0x31
#define AT25SF321B_CMD_WRSR3    0x11
#define AT25SF321B_CMD_RDUID    0x4B

#define RENESSAS_SR1_SRP0       0x80
#define RENESSAS_SR1_BP4        0x40
#define RENESSAS_SR1_BP3        0x20
#define RENESSAS_SR1_BP2        0x10
#define RENESSAS_SR1_BP1        0x08
#define RENESSAS_SR1_BP0        0x04
#define RENESSAS_SR1_WEL        0x02
#define RENESSAS_SR1_RDY        0x01

#define RENESSAS_SR2_E_SU       0x80
#define RENESSAS_SR2_CMP        0x40
#define RENESSAS_SR2_LB3        0x20
#define RENESSAS_SR2_LB2        0x10
#define RENESSAS_SR2_LB1        0x08
#define RENESSAS_SR2_P_SUS      0x04
#define RENESSAS_SR2_QE         0x02
#define RENESSAS_SR2_SRP1       0x01


#define RENESSAS_tCE            30000UL  //Maximum chip erase time (30s)

static uint8_t renessas_rdsr_ex(uint8_t status)
{
    uint8_t sr_data;

    spiflash_cs (CS_LOW);
    spiflash_sendbyte (status);
    sr_data = spiflash_receivebyte ();
    spiflash_cs (CS_HIGH);

    return sr_data;
}

static void renessas_wrsr_ex(uint8_t sr, uint16_t data)
{
    flashspi_write_enable();

    spiflash_cs (CS_LOW);
    spiflash_sendbyte (sr);
    spiflash_sendbyte (data);
    spiflash_cs (CS_HIGH);
}

static flashspi_res_t renessas_wait_ready(void)
{
   return flashspi_wait_ready (RENESSAS_tCE);
}


static flashspi_res_t at25sf321b_init(void)
{
    uint32_t id_jedec = flashspi_read_id_jedec();
    uint8_t status = renessas_rdsr_ex(AT25SF321B_CMD_RDSR2);

    if(status & RENESSAS_SR2_QE){
        renessas_wrsr_ex(AT25SF321B_CMD_RDSR2, status & ~RENESSAS_SR2_QE);
    }

    return (id_jedec == AT25SF321B_DEV_ID) ? FLASHSPI_OK: FLASHSPI_ERROR_ID;
}

const flashspi_t at25sf321b =
{
    .name = "AT25SF321B",
    .mid = AT25SF321B_M_ID,
    .size = 0x00400000, /*4m byte*/
    .pagesize = 256,
    .sectorsize = 0x1000,
    .init = at25sf321b_init,
    .wait_ready = renessas_wait_ready
};
