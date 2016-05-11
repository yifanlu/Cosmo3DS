/*
*   firm.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "firm.h"
#include "patches.h"
#include "memory.h"
#include "fs.h"
#include "emunand.h"
#include "crypto.h"
#include "draw.h"
#include "screeninit.h"
#include "utils.h"
#include "buttons.h"
#include "../build/patches.h"

static firmHeader *const firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;
static u8 *arm9Section;

static u32 firmSize,
           console,
           emuNAND,
           a9lhSetup,
           emuOffset,
           emuHeader;

void setupCFW(void){

    //Determine if booting with A9LH
    u32 a9lhBoot = (PDN_SPI_CNT == 0x0) ? 1 : 0;
    //Retrieve the last booted FIRM
    u8 previousFirm = CFG_BOOTENV;
    //Detect the console being used
    console = (PDN_MPCORE_CFG == 1) ? 0 : 1;
    //Get pressed buttons
    u16 pressed = HID_PAD;

    //Determine if A9LH is installed

    if(a9lhBoot){
        if(pressed == SAFE_MODE)
            error("Using Safe Mode would brick you, or remove A9LH!");

        a9lhSetup = 1;
    } else{
        a9lhSetup = 0;
    }

    //Detect EmuNAND if not booting sysNAND or returning from AGB_FIRM
    emuNAND = 0;
    if (pressed != BUTTON_L1 && previousFirm != 7){
        emuNAND = 1;
        getEmunandSect(&emuOffset, &emuHeader, &emuNAND);
    }
}

//Load FIRM into FCRAM
void loadFirm(void){
    u32 mode;
    
    // Load firm from emuNAND
    if (emuNAND){
        //Read FIRM from NAND and write to FCRAM
        nandFirm0(1, emuOffset, (u8 *)firm);
        //Check for correct decryption
        if(firm->magic != 0x4D524946)
            error("Couldn't decrypt emuNAND FIRM0");
    }
    //If not using an A9LH setup, load 9.0 FIRM from NAND
    else if(!a9lhSetup){
        //Read FIRM from NAND and write to FCRAM
        nandFirm0(0, 0, (u8 *)firm);
        //Check for correct decryption
        if(firm->magic != 0x4D524946)
            error("Couldn't decrypt NAND FIRM0 (O3DS not on 9.x?)");
    }
    //Load FIRM from SD
    else{
        const char *path = "/firmware.bin";
        firmSize = fileSize(path);
        if(!firmSize) error("firmware.bin doesn't exist");
        fileFirm0(path, (u8 *)firm);
    }

    section = firm->section;

    //Check that the loaded FIRM matches the console
    if((((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68))
        error("FIRM doesn't match this\nconsole, or it's encrypted");

    arm9Section = (u8 *)section[2].address;

    if(console)
    {
        switch(arm9Section[0x53])
        {
            case 0xFF:
                mode = 0;
                break;
            case '1':
                mode = 1;
                break;
            default:
                mode = 2;
                break;
        }
        arm9Loader(arm9Section, mode);
    }
}

//NAND redirection
static inline void loadEmu(u8 *proc9Offset){

    //Copy emuNAND code
    void *emuCodeOffset = getEmuCode(proc9Offset);
    memcpy(emuCodeOffset, emunand, emunand_size);

    //Add the data of the found emuNAND
    u32 *pos_offset = (u32 *)memsearch(emuCodeOffset, "NAND", emunand_size, 4);
    u32 *pos_header = (u32 *)memsearch(emuCodeOffset, "NCSD", emunand_size, 4);
    *pos_offset = emuOffset;
    *pos_header = emuHeader;

    //Find and add the SDMMC struct
    u32 *pos_sdmmc = (u32 *)memsearch(emuCodeOffset, "SDMC", emunand_size, 4);
    *pos_sdmmc = getSDMMC(arm9Section, section[2].size);

    //Calculate offset for the hooks
    u32 branchOffset = (u32)emuCodeOffset;

    //Add emunand hooks
    u32 emuRead,
        emuWrite;

    getEmuRW(arm9Section, section[2].size, &emuRead, &emuWrite);
    *(u16 *)emuRead = nandRedir[0];
    *((u16 *)emuRead + 1) = nandRedir[1];
    *((u32 *)emuRead + 1) = branchOffset;
    *(u16 *)emuWrite = nandRedir[0];
    *((u16 *)emuWrite + 1) = nandRedir[1];
    *((u32 *)emuWrite + 1) = branchOffset;

    //Set MPU for emu code region
    u32 *mpuOffset = getMPU(arm9Section, section[2].size);
    *mpuOffset = mpuPatch[0];
    *(mpuOffset + 6) = mpuPatch[1];
    *(mpuOffset + 9) = mpuPatch[2];
}

static inline void injectLoader(void)
{
    u8 *arm11Section0 = section[0].address;
    u32 loaderSize;
    u32 loaderOffset = getLoader(arm11Section0, &loaderSize);

    if (loaderSize < (u32)injector_size)
    {
        error("Injector too large for this FIRM! Please rebuild.");
        return;
    }

    //memcpy(section[0].address, arm11Section0, loaderOffset);
    memcpy(arm11Section0 + loaderOffset, injector, injector_size);
    //Patch content size and ExeFS size to match the repaced loader's ones
    *((u32 *)(arm11Section0 + loaderOffset) + 0x41) = loaderSize / 0x200;
    *((u32 *)(arm11Section0 + loaderOffset) + 0x69) = loaderSize / 0x200 - 5;
}

//Patches
void patchFirm(void){

    if(emuNAND){
        //Find the Process9 NCCH location
        u8 *proc9Offset = getProc9(arm9Section, section[2].size);

        //Apply emuNAND patches
        loadEmu(proc9Offset);

        //Patch FIRM reboots, not on 9.0 FIRM as it breaks firmlaunchhax
        //Calculate offset for the firmlaunch code
        void *rebootOffset = getReboot(arm9Section, section[2].size);
        //Calculate offset for the fOpen function
        u32 fOpenOffset = getfOpen(proc9Offset, rebootOffset);

        //Copy firmlaunch code
        memcpy(rebootOffset, reboot, reboot_size);

        //Put the fOpen offset in the right location
        u32 *pos_fopen = (u32 *)memsearch(rebootOffset, "OPEN", reboot_size, 4);
        *pos_fopen = fOpenOffset;
    }

    if(a9lhSetup && !emuNAND){
        //Patch FIRM partitions writes on sysNAND to protect A9LH
        u16 *writeOffset = getFirmWrite(arm9Section, section[2].size);
        *writeOffset = writeBlock[0];
        *(writeOffset + 1) = writeBlock[1];
    }

    //Replace the FIRM loader with the injector while copying section0
    injectLoader();

    //Patch ARM9 entrypoint on N3DS to skip arm9loader
    if(console)
        firm->arm9Entry = (u8 *)0x801B01C;
}

void launchFirm(void){
    //Fixes N3DS 3D
    deinitScreens();

    //Set ARM11 kernel entrypoint
    *(vu32 *)0x1FFFFFF8 = (u32)firm->arm11Entry;

    //Final jump to arm9 kernel
    ((void (*)())firm->arm9Entry)();
}