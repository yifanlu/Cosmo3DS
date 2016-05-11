/*
*   fs.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "fs.h"
#include "fatfs/ff.h"
#include "firm.h"

static FATFS fs;

u32 mountSD(void){
    if(f_mount(&fs, "0:", 1) != FR_OK) return 0;
    return 1;
}

u32 fileRead(void *dest, const char *path, u32 size){
    FRESULT fr;
    FIL fp;
    unsigned int br = 0;

    fr = f_open(&fp, path, FA_READ);
    if(fr == FR_OK){
        if(!size) size = f_size(&fp);
        fr = f_read(&fp, dest, size, &br);
    }

    f_close(&fp);
    return fr ? 0 : 1;
}

u32 fileWrite(const void *buffer, const char *path, u32 size){
    FRESULT fr;
    FIL fp;
    unsigned int br = 0;

    fr = f_open(&fp, path, FA_WRITE | FA_OPEN_ALWAYS);
    if(fr == FR_OK) fr = f_write(&fp, buffer, size, &br);

    f_close(&fp);
    return fr ? 0 : 1;
}

u32 fileSize(const char *path){
    FIL fp;
    u32 size = 0;

    if(f_open(&fp, path, FA_READ) == FR_OK)
        size = f_size(&fp);

    f_close(&fp);
    return size;
}

u32 fileExists(const char *path){
    FIL fp;
    u32 exists = 0;

    if(f_open(&fp, path, FA_READ) == FR_OK) exists = 1;

    f_close(&fp);
    return exists;
}

void fileDelete(const char *path){
    f_unlink(path);
}

void fileFirm0(const char *path, u8 *outbuf) {
    FRESULT fr;
    FIL fp;
    unsigned int br = 0;
    firmHeader *hdr = (firmHeader *)outbuf;

    if (f_open(&fp, path, FA_READ) != FR_OK){
        return;
    }
    fr = f_read(&fp, outbuf, 0x200, &br);
    if (fr != FR_OK || hdr->magic != 0x4D524946) { // FIRM
        goto err;
    }

    for (int i = 0; i < 4; i++) {
        if (hdr->section[i].size) {
            if (f_lseek(&fp, hdr->section[i].offset) != FR_OK) {
                goto err;
            }
            if (f_read(&fp, hdr->section[i].address, hdr->section[i].size, &br) != FR_OK) {
                goto err;
            }
        }
    }

err:
    f_close(&fp);
}