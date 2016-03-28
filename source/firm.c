/*
*   firm.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include "firm.h"
#include "patches.h"
#include "memory.h"
#include "fs.h"
#include "emunand.h"
#include "crypto.h"
#include "draw.h"
#include "../build/emunand.h"

const void *firmLocation = (void*)0x24000000;
firmHeader *firm = NULL;
firmSectionHeader *section = NULL;
u32 emuOffset = 0,
    emuHeader = 0,
    emuRead = 0,
    emuWrite = 0,
    sdmmcOffset = 0, 
    firmSize = 0,
    mpuOffset = 0,
    emuCodeOffset = 0;

//Load firm into FCRAM
void loadFirm(void){
    //Read FIRM from SD card and write to FCRAM
	const char firmPath[] = "/rei/firmware.bin";
	firmSize = fileSize(firmPath);
    fileRead(firmLocation, firmPath, firmSize);
    
    //Decrypt firmware blob
    u8 firmIV[0x10] = {0};
    aes_setkey(0x16, memeKey, AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(0x16);
    aes(firmLocation, firmLocation, firmSize / AES_BLOCK_SIZE, firmIV, AES_CBC_DECRYPT_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);
    
    //Parse firmware
    firm = firmLocation;
    section = firm->section;
    arm9loader(firmLocation + section[2].offset);
    
    //Set MPU for emu/thread code region
    getMPU(firmLocation, firmSize, &mpuOffset);
    memcpy((u8*)mpuOffset, mpu, sizeof(mpu));
}

//Nand redirection
void loadEmu(void){
    
    //Check for force sysnand
    if(((~*(unsigned *)0x10146000) & 0xFFF) == (1 << 3)) return;
    
    //Read emunand code from SD
    u32 size = build_emunand_bin_len;
    getEmuCode(firmLocation, &emuCodeOffset, firmSize);
    memcpy(emuCodeOffset, build_emunand_bin, size);
    
    //Find and patch emunand related offsets
    u32 *pos_sdmmc = memsearch(emuCodeOffset, "SDMC", size, 4);
    u32 *pos_offset = memsearch(emuCodeOffset, "NAND", size, 4);
    u32 *pos_header = memsearch(emuCodeOffset, "NCSD", size, 4);
	getSDMMC(firmLocation, &sdmmcOffset, firmSize);
    getEmunandSect(&emuOffset, &emuHeader);
    getEmuRW(firmLocation, firmSize, &emuRead, &emuWrite);
    getMPU(firmLocation, firmSize, &mpuOffset);
	*pos_sdmmc = sdmmcOffset;
	*pos_offset = emuOffset;
	*pos_header = emuHeader;
	
    //Add emunand hooks
    memcpy((u8*)emuRead, nandRedir, sizeof(nandRedir));
    memcpy((u8*)emuWrite, nandRedir, sizeof(nandRedir));
}

void launchFirm(void){
    
    //Copy firm partitions to respective memory locations
    memcpy(section[0].address, firmLocation + section[0].offset, section[0].size);
    memcpy(section[1].address, firmLocation + section[1].offset, section[1].size);
    memcpy(section[2].address, firmLocation + section[2].offset, section[2].size);
    
    //Run ARM11 screen stuff
    vu32 *arm11 = (vu32*)0x1FFFFFF8;
    *arm11 = (u32)shutdownLCD;
    while (*arm11);
    
    //Set ARM11 kernel
    *arm11 = (u32)firm->arm11Entry;
    
    //Final jump to arm9 binary
    ((void (*)())0x801B01C)();
}