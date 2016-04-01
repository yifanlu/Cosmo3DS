/*
*   patches.c
*       by Reisyukaku / Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "patches.h"
#include "memory.h"

/**************************************************
*                   Patches
**************************************************/

const u32 mpuPatch[3] = {0x00360003, 0x00200603, 0x001C0603};

const u16 nandRedir[2] = {0x4C00, 0x47A0};

const u16 writeBlock[2] = {0x2000, 0x46C0};

/**************************************************
*                   Functions
**************************************************/

u8 *getProc9(u8 *pos, u32 size){
    return memsearch(pos, "ess9", size, 4);
}

void *getReboot(u8 *pos, u32 size){
    //Look for FIRM reboot code
    const u8 pattern[] = {0xDE, 0x1F, 0x8D, 0xE2};

    return memsearch(pos, pattern, size, 4) - 0x10;
}

u32 getfOpen(u8 *proc9Offset, void *rebootOffset){
    //Offset Process9 code gets loaded to in memory (defined in ExHeader)
    u32 p9MemAddr = *(u32 *)(proc9Offset + 0xC);
    //Process9 code offset (start of NCCH + ExeFS offset + ExeFS header size)
    u32 p9CodeOff = (u32)(proc9Offset - 0x204) + (*(u32 *)(proc9Offset - 0x64) * 0x200) + 0x200;

    //Firmlaunch function offset - offset in BLX opcode (A4-16 - ARM DDI 0100E) + 1
    return (u32)rebootOffset + 9 - (-((*(u32 *)rebootOffset & 0x00FFFFFF) << 2) & 0xFFFFF) - p9CodeOff + p9MemAddr;
}

u16 *getFirmWrite(u8 *pos, u32 size){
    //Look for FIRM writing code
    u8 *const off = memsearch(pos, "exe:", size, 4);
    const u8 pattern[] = {0x00, 0x28, 0x01, 0xDA};

    return (u16 *)memsearch(off - 0x100, pattern, 0x100, 4);
}

void getLoader(u8 *pos, u32 size, u32 *loaderOffset, u32 *loaderSize){
    u8 *const off = memsearch(pos, "loade", size, 5);

    *loaderOffset = (u32)(off - pos) - 0x200;
    *loaderSize = *(u32 *)(off - 0xFC) * 0x200;
}