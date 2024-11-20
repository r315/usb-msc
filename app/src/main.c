/**
  **************************************************************************
  * @file     main.c
  * @version  v2.0.8
  * @date     2022-04-25
  * @brief    main program
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "415dk_board.h"
#include "cli_simple.h"
#include "ff.h"
#include "msc_class.h"
#include "msc_desc.h"
#include "msc_diskio.h"
#include "usbd_int.h"
#include "flashspi.h"

typedef struct
{
	uint32_t totalsize;
	uint32_t freesize;
}DISK_SIZE;

static FATFS *fs;
static otg_core_type otg_core_struct;


FRESULT mount(TCHAR *path, uint8_t m);

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

#ifdef ENABLE_CLI
static int listCmd(int argc, char **argv)
{
    FRESULT res;
    FILINFO fileInfo;
    uint32_t totalFiles = 0;
    uint32_t totalDirs = 0;
    DIR dir;

    res = f_opendir(&dir, "/");

    if(res != FR_OK) {
        printf("f_opendir() failed, res = %d\r\n", res);
        return CLI_OK;
    }

    printf("--------\r\nRoot directory:\r\n");

    for(;;) {
        res = f_readdir(&dir, &fileInfo);
        if((res != FR_OK) || (fileInfo.fname[0] == '\0')) {
            break;
        }

        if(fileInfo.fattrib & AM_DIR) {
            printf("  DIR  %s\r\n", fileInfo.fname);
            totalDirs++;
        } else {
            printf("  FILE %s\r\n", fileInfo.fname);
            totalFiles++;
        }
    }

    printf("(total: %lu dirs, %lu files)\r\n--------\r\n", totalDirs, totalFiles);

    res = f_closedir(&dir);

    return CLI_OK;
}

static int catCmd(int argc, char **argv)
{
    FRESULT res;
    FIL file;
    UINT br;
    char c;

    res = f_open(&file, argv[1], FA_READ);

    if(res != FR_OK) {
        printf("f_open() failed, res = %d\r\n", res);
        return CLI_OK;
    }

    do{
        res = f_read(&file, &c, 1, &br);
        printf("%c", c);
    }while(res == FR_OK && br == 1);

    f_close(&file);

    return CLI_OK_LF;
}

static int mountCmd(int argc, char **argv)
{
    if(argc < 2){
        printf("usage: mount <0|1>\n");
        return CLI_BAD_PARAM;
    }

	mount("0:", argv[1][0] == '1');
    return CLI_OK;
}

static int resetCmd(int argc, char **argv)
{
    sw_reset();
    return CLI_OK;
}

static int flashCmd(int argc, char **argv)
{
    flashspi_res_t res;

    if(argc < 2){
        printf("usage: flash <info|erase|fs>\n");
        return CLI_OK_LF;
    }

    const flashspi_t *fls = flashspi_get();

    if(!fls){
        printf("No spi flash detected\n");
        return CLI_OK;
    }

    if(!strcmp(argv[1], "info")){
        printf("Name: %s\n", fls->name);
        printf("Total size: %u (0x%08X) bytes\n", (unsigned int)fls->size, (unsigned int)fls->size);
        printf("Sector size: %u (0x%04X) bytes\n",  (unsigned int)fls->sectorsize,  (unsigned int)fls->sectorsize);
        printf("Page size: %u (0x%02X) bytes\n",  (unsigned int)fls->pagesize,  (unsigned int)fls->pagesize);
        return CLI_OK;
    }

    if(!strcmp(argv[1], "erase")) {
        res = flashspi_erase();
        if(res != FLASHSPI_OK)
            printf("Error %d\n", res);
        return CLI_OK;
    }

    if(!strcmp(argv[1], "fs")) {
        #if FF_USE_MKFS
        f_mkfs("0:", FM_FAT, NULL, fls->sectorsize);
        #else
        printf("f_mkfs not enabled\n");
        #endif
        return CLI_OK;
    }

    return CLI_BAD_PARAM;
}

static int unplugCmd(int argc, char **argv)
{
    usb_unplug();
    return CLI_OK;
}

static int plugCmd(int argc, char **argv)
{
    usb_config();
    return CLI_OK;
}

static int echoCmd(int argc, char **argv)
{
    while(--argc){
        argv++;
        printf("%s ", *argv);
    }

    return CLI_OK_LF;
}

cli_command_t cli_cmds [] = {
    {"help", ((int (*)(int, char**))CLI_Commands)},
    {"echo", echoCmd},
    {"reset", resetCmd},
    {"plug", plugCmd},
    {"unplug", unplugCmd},
    {"mount", mountCmd},
    {"list", listCmd},
    {"cat", catCmd},
    {"flash", flashCmd},
};
#endif

FRESULT getDiskSize(TCHAR *path, DISK_SIZE *disk_size)
{
	FATFS *pfs;
    FRESULT res;
	DWORD fre_clust;

	res = f_getfree(path, &fre_clust, &pfs);

	if(res == FR_OK){
		disk_size->totalsize = (pfs->n_fatent - 2) * pfs->csize / 2;
		disk_size->freesize = fre_clust * pfs->csize / 2;
	}

	return res;
}

FRESULT mount(TCHAR *path, uint8_t m)
{
    FRESULT r;
    DISK_SIZE disk_size = {0};

    if(m)
    {
        if(fs){
            printf("FS already mounted\n");
            return FR_OK;
        }

        fs = (FATFS*)malloc(sizeof(FATFS));

        if(!fs){
            printf("Fail to allocate FS\n");
        }

        r = f_mount(fs, path, 0);  // 0 = mount later option

        printf("Mount ");
        if(r != FR_OK){
            printf("fail: %d\n", r);
        }else{
            r = getDiskSize(path, &disk_size);
            if(r == FR_OK){
                printf("ok\n"
                       "\tDisk size: %luk\n", disk_size.totalsize);
                printf("\t     Free: %luk\n", disk_size.freesize);
            }else{
                printf("FRESULT Error: %d\n", r);
            }
        }
    }else{
        r = f_mount(NULL, "0:" ,0);

        if(fs){
            free(fs);
        }

        fs = NULL;

        printf("Unmounted\n");
    }

    return r;
}

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
    at32_board_init();

	system_clock_config();

    system_tick_init();

	NVIC_SetPriorityGrouping(NVIC_PRIORITY_GROUP_4);

    #ifdef ENABLE_CLI
    serial_init();

    CLI_Init("msd >");
    CLI_RegisterCommand(cli_cmds, sizeof(cli_cmds) / sizeof(cli_command_t));
    printf("\rType 'help' for available commands\n");

    msc_init(0);

    #else
    //mount(1);
	usb_config();
    #endif

    otg_core_struct.usb_reg = NULL;

	while(1)
	{
        #if ENABLE_CLI
        if(CLI_ReadLine()){
            CLI_HandleLine();
        }
        #endif
        delay_ms(10);
	}
}

/**
  * @brief  this function handles otgfs interrupt.
  * @param  none
  * @retval none
  */
void OTGFS1_IRQHandler(void)
{
  usbd_irq_handler(&otg_core_struct);
}

