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
#include "board.h"
#include "cli_simple.h"
#include "ff.h"
#include "flashspi.h"

typedef struct
{
	uint32_t totalsize;
	uint32_t freesize;
}DISK_SIZE;

static FATFS *fs;



FRESULT mount(TCHAR *path, uint8_t m);

static void dump_buffer(uint8_t *buf, uint32_t count)
{
    for(int i = 0; i < count; i ++){
        if( (i & 15) == 0)
            printf("\n%04X: ", i);
        printf("%02X ", buf[i]);
    }
    putchar('\n');
}

#ifdef ENABLE_CLI
static int sdCardCmd(int argc, char **argv)
{
    const char *card_types[] = {
        "STD_CAPACITY_SD_CARD_V1_1",
        "STD_CAPACITY_SD_CARD_V2_0",
        "HIGH_CAPACITY_SD_CARD",
        "MULTIMEDIA_CARD",
        "SECURE_DIGITAL_IO_CARD",
        "HIGH_SPEED_MULTIMEDIA_CARD",
        "SECURE_DIGITAL_IO_COMBO_CARD",
        "HIGH_CAPACITY_MMC_CARD",
        "SDIO_CARD"
    };

    const char *sd_errors[] = {
        "SD_OK" ,
        "SD_CMD_FAIL" ,
        "SD_DATA_FAIL" ,
        "SD_CMD_RSP_TIMEOUT" ,
        "SD_DATA_TIMEOUT" ,
        "SD_TX_UNDERRUN" ,
        "SD_RX_OVERRUN" ,
        "SD_START_BIT_ERR" ,
        "SD_CMD_OUT_OF_RANGE" ,
        "SD_ADDR_MISALIGNED" ,
        "SD_BLK_LEN_ERR" ,
        "SD_ERASE_SEQ_ERR" ,
        "SD_INVALID_ERASE_PARAM" ,
        "SD_WR_PROTECT_VIOLATION" ,
        "SD_LOCK_UNLOCK_ERROR" ,
        "SD_CMD_CRC_ERROR" ,
        "SD_ILLEGAL_CMD" ,
        "SD_CARD_ECC_ERROR" ,
        "SD_CARD_CONTROLLER_ERR" ,
        "SD_GENERAL_UNKNOWN_ERROR" ,
        "SD_STREAM_RD_UNDERRUN" ,
        "SD_STREAM_WR_OVERRUN" ,
        "SD_CID_CSD_OVERWRITE" ,
        "SD_WP_ERASE_SKIP" ,
        "SD_CARD_ECC_DISABLED" ,
        "SD_ERASE_RESET" ,
        "SD_AKE_SEQ_ERROR" ,
        "SD_INVALID_VOLTRANGE" ,
        "SD_ADDR_OUT_OF_RANGE" ,
        "SD_SWITCH_ERROR" ,
        "SD_SDIO_DISABLED" ,
        "SD_SDIO_FUNC_BUSY" ,
        "SD_SDIO_FUNC_ERROR" ,
        "SD_SDIO_UNKNOWN_FUNC" ,
        "SD_INTERNAL_ERROR" ,
        "SD_NOT_CONFIGURED" ,
        "SD_REQ_PENDING" ,
        "SD_REQ_NOT_APPLICABLE" ,
        "SD_INVALID_PARAMETER" ,
        "SD_UNSUPPORTED_FEATURE" ,
        "SD_UNSUPPORTED_HW" ,
        "SD_ERROR"
    };

    card_info_t *sd_card_info;

    sd_card_info = sd_card_info_get ();

    if(!strcmp(argv[1], "init")) {
        printf("SD Card Init: %s\n", sd_errors[(uint8_t)sd_init ()]);
        return CLI_OK;
    }

    if(!strcmp(argv[1], "info")) {
        printf("\tType: %s\n", card_types[sd_card_info->type]);
        printf("\tCapacity: %llu bytes\n", sd_card_info->capacity);
        printf("\tBlock size: %lu bytes\n", sd_card_info->block_size);
        printf("\tRelative card address (RCA): %u\n", sd_card_info->rca);
        /* SCR */
        printf("\tSCR\n");
        printf("\t\tSCR Structure: %u\n", sd_card_info->scr.scr_structure);
        printf("\t\tSD Spec: %u\n", sd_card_info->scr.sd_spec);
        printf("\t\tDATA After erase: %u\n", sd_card_info->scr.data_stat_after_erase);
        printf("\t\tSD Security: %u\n", sd_card_info->scr.sd_security);
        printf("\t\tBUS Widths: %u\n", sd_card_info->scr.sd_bus_widths);
        printf("\t\tV3 Spec: %u\n", sd_card_info->scr.sd_spec3);
        printf("\t\tEX Security: %u\n", sd_card_info->scr.ex_security);
        printf("\t\tV4 Spec: %u\n", sd_card_info->scr.sd_spec4);
        printf("\t\tVX Spec: %u\n", sd_card_info->scr.sd_specx);
        printf("\t\tCMD Support : %u\n", sd_card_info->scr.cmd_support);

        printf("\tCSD\n");
        printf("\t\tCSD Structure: %u\n", sd_card_info->csd.csd_struct);
        printf("\t\tWR Protection: %u\n", sd_card_info->csd.permanent_write_protect);

        return CLI_OK;
    }

    if(!strcmp(argv[1], "rb")) {
        uint8_t buffer[512]; // sd_card_info->card_blk_size];
        uint32_t address;
        if(CLI_Ha2i(argv[2], &address)){
            sdio_error_t res = sd_block_read(buffer, address << 9, sizeof(buffer));
            if(res == SD_OK){
                dump_buffer(buffer, sizeof(buffer));
            }else{
                printf("\tType: %s\n", sd_errors[res]);
            }
            return CLI_OK;
        }
    }

    if(!strcmp(argv[1], "eb")) {
        uint32_t address;
        if(CLI_Ha2i(argv[2], &address)){
            sdio_error_t res = sd_block_erase(address << 9, 1);
            if(res != SD_OK){
                printf("\tFail: %s\n", sd_errors[res]);
            }
            return CLI_OK;
        }
    }

    return CLI_BAD_PARAM;
}

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

    const flashspi_t *fls = flashspi_get_device();

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
    {"sd", sdCardCmd},
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
    board_init();

	system_clock_config();

    system_tick_init();

	NVIC_SetPriorityGrouping(NVIC_PRIORITY_GROUP_4);

    #ifdef ENABLE_CLI
    serial_init();

    CLI_Init("msd >");
    CLI_RegisterCommand(cli_cmds, sizeof(cli_cmds) / sizeof(cli_command_t));
    printf("\rType 'help' for available commands\n");

    sd_init();

    #else
    //mount(1);
	usb_config();
    #endif

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

