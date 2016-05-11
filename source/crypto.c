/*
*   crypto.c
*       by Reisyukaku / Aurora Wright
*   Crypto libs from http://github.com/b1l1s/ctr
*
*   Copyright (c) 2016 All Rights Reserved
*/

#include "crypto.h"
#include "firm.h"
#include "memory.h"
#include "fatfs/sdmmc/sdmmc.h"

/****************************************************************
*                   Crypto Libs
****************************************************************/

/* original version by megazig */

#ifndef __thumb__
#define BSWAP32(x) {\
	__asm__\
	(\
		"eor r1, %1, %1, ror #16\n\t"\
		"bic r1, r1, #0xFF0000\n\t"\
		"mov %0, %1, ror #8\n\t"\
		"eor %0, %0, r1, lsr #8\n\t"\
		:"=r"(x)\
		:"0"(x)\
		:"r1"\
	);\
};

#define ADD_u128_u32(u128_0, u128_1, u128_2, u128_3, u32_0) {\
__asm__\
	(\
		"adds %0, %4\n\t"\
		"addcss %1, %1, #1\n\t"\
		"addcss %2, %2, #1\n\t"\
		"addcs %3, %3, #1\n\t"\
		: "+r"(u128_0), "+r"(u128_1), "+r"(u128_2), "+r"(u128_3)\
		: "r"(u32_0)\
		: "cc"\
	);\
}
#else
#define BSWAP32(x) {x = __builtin_bswap32(x);}

#define ADD_u128_u32(u128_0, u128_1, u128_2, u128_3, u32_0) {\
__asm__\
	(\
		"mov r4, #0\n\t"\
		"add %0, %0, %4\n\t"\
		"adc %1, %1, r4\n\t"\
		"adc %2, %2, r4\n\t"\
		"adc %3, %3, r4\n\t"\
		: "+r"(u128_0), "+r"(u128_1), "+r"(u128_2), "+r"(u128_3)\
		: "r"(u32_0)\
		: "cc", "r4"\
	);\
}
#endif /*__thumb__*/

static void aes_setkey(u8 keyslot, const void *key, u32 keyType, u32 mode)
{
	if(keyslot <= 0x03) return; // Ignore TWL keys for now
	u32 *key32 = (u32 *)key;
	*REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)) | mode;
	*REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | AES_KEYCNT_WRITE;

	REG_AESKEYFIFO[keyType] = key32[0];
	REG_AESKEYFIFO[keyType] = key32[1];
	REG_AESKEYFIFO[keyType] = key32[2];
	REG_AESKEYFIFO[keyType] = key32[3];
}

static void aes_use_keyslot(u8 keyslot)
{
	if(keyslot > 0x3F)
		return;

	*REG_AESKEYSEL = keyslot;
	*REG_AESCNT = *REG_AESCNT | 0x04000000; /* mystery bit */
}

static void aes_setiv(const void *iv, u32 mode)
{
	const u32 *iv32 = (const u32 *)iv;
	*REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)) | mode;

	// Word order for IV can't be changed in REG_AESCNT and always default to reversed
	if(mode & AES_INPUT_NORMAL)
	{
		REG_AESCTR[0] = iv32[3];
		REG_AESCTR[1] = iv32[2];
		REG_AESCTR[2] = iv32[1];
		REG_AESCTR[3] = iv32[0];
	}
	else
	{
		REG_AESCTR[0] = iv32[0];
		REG_AESCTR[1] = iv32[1];
		REG_AESCTR[2] = iv32[2];
		REG_AESCTR[3] = iv32[3];
	}
}

static void aes_advctr(void *ctr, u32 val, u32 mode)
{
	u32 *ctr32 = (u32 *)ctr;
	
	int i;
	if(mode & AES_INPUT_BE)
	{
		for(i = 0; i < 4; ++i) // Endian swap
			BSWAP32(ctr32[i]);
	}
	
	if(mode & AES_INPUT_NORMAL)
	{
		ADD_u128_u32(ctr32[3], ctr32[2], ctr32[1], ctr32[0], val);
	}
	else
	{
		ADD_u128_u32(ctr32[0], ctr32[1], ctr32[2], ctr32[3], val);
	}
	
	if(mode & AES_INPUT_BE)
	{
		for(i = 0; i < 4; ++i) // Endian swap
			BSWAP32(ctr32[i]);
	}
}

static void aes_change_ctrmode(void *ctr, u32 fromMode, u32 toMode)
{
	u32 *ctr32 = (u32 *)ctr;
	int i;
	if((fromMode ^ toMode) & AES_CNT_INPUT_ENDIAN)
	{
		for(i = 0; i < 4; ++i)
			BSWAP32(ctr32[i]);
	}

	if((fromMode ^ toMode) & AES_CNT_INPUT_ORDER)
	{
		u32 temp = ctr32[0];
		ctr32[0] = ctr32[3];
		ctr32[3] = temp;

		temp = ctr32[1];
		ctr32[1] = ctr32[2];
		ctr32[2] = temp;
	}
}

static void aes_batch(void *dst, const void *src, u32 blockCount)
{
	*REG_AESBLKCNT = blockCount << 16;
	*REG_AESCNT |=	AES_CNT_START;
	
	const u32 *src32	= (const u32 *)src;
	u32 *dst32			= (u32 *)dst;
	
	u32 wbc = blockCount;
	u32 rbc = blockCount;
	
	while(rbc)
	{
		if(wbc && ((*REG_AESCNT & 0x1F) <= 0xC)) // There's space for at least 4 ints
		{
			*REG_AESWRFIFO = *src32++;
			*REG_AESWRFIFO = *src32++;
			*REG_AESWRFIFO = *src32++;
			*REG_AESWRFIFO = *src32++;
			wbc--;
		}
		
		if(rbc && ((*REG_AESCNT & (0x1F << 0x5)) >= (0x4 << 0x5))) // At least 4 ints available for read
		{
			*dst32++ = *REG_AESRDFIFO;
			*dst32++ = *REG_AESRDFIFO;
			*dst32++ = *REG_AESRDFIFO;
			*dst32++ = *REG_AESRDFIFO;
			rbc--;
		}
	}
}

static void aes(void *dst, const void *src, u32 blockCount, void *iv, u32 mode, u32 ivMode)
{
	*REG_AESCNT =	mode |
					AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER |
					AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN |
					AES_CNT_FLUSH_READ | AES_CNT_FLUSH_WRITE;

	u32 blocks;
	while(blockCount != 0)
	{
		if((mode & AES_ALL_MODES) != AES_ECB_ENCRYPT_MODE
		&& (mode & AES_ALL_MODES) != AES_ECB_DECRYPT_MODE)
			aes_setiv(iv, ivMode);

		blocks = (blockCount >= 0xFFFF) ? 0xFFFF : blockCount;

		// Save the last block for the next decryption CBC batch's iv
		if((mode & AES_ALL_MODES) == AES_CBC_DECRYPT_MODE)
		{
			memcpy(iv, src + (blocks - 1) * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
			aes_change_ctrmode(iv, AES_INPUT_BE | AES_INPUT_NORMAL, ivMode);
		}

		// Process the current batch
		aes_batch(dst, src, blocks);

		// Save the last block for the next encryption CBC batch's iv
		if((mode & AES_ALL_MODES) == AES_CBC_ENCRYPT_MODE)
		{
			memcpy(iv, dst + (blocks - 1) * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
			aes_change_ctrmode(iv, AES_INPUT_BE | AES_INPUT_NORMAL, ivMode);
		}
		
		// Advance counter for CTR mode
		else if((mode & AES_ALL_MODES) == AES_CTR_MODE)
			aes_advctr(iv, blocks, ivMode);

		src += blocks * AES_BLOCK_SIZE;
		dst += blocks * AES_BLOCK_SIZE;
		blockCount -= blocks;
	}
}

static void sha_init(u32 mode)
{
    while(*REG_SHACNT & 1);
    *REG_SHACNT = mode | SHA_CNT_OUTPUT_ENDIAN | SHA_NORMAL_ROUND;
}

static void sha_update(const void* src, u32 size)
{    
    const u32* src32 = (const u32*)src;
    
    while(size >= 0x40) {
        while(*REG_SHACNT & 1);
        for(u32 i = 0; i < 4; i++) {
            *REG_SHAINFIFO = *src32++;
            *REG_SHAINFIFO = *src32++;
            *REG_SHAINFIFO = *src32++;
            *REG_SHAINFIFO = *src32++;
        }
        size -= 0x40;
    }
    while(*REG_SHACNT & 1);
    memcpy((void*)REG_SHAINFIFO, src32, size);
}

static void sha_get(void* res) {
    *REG_SHACNT = (*REG_SHACNT & ~SHA_NORMAL_ROUND) | SHA_FINAL_ROUND;
    while(*REG_SHACNT & SHA_FINAL_ROUND);
    while(*REG_SHACNT & 1);
    memcpy(res, (void*)REG_SHAHASH, (256 / 8));
}

static void sha_quick(void* res, const void* src, u32 size, u32 mode) {
    sha_init(mode);
    sha_update(src, size);
    sha_get(res);
}

/****************************************************************
*                   Nand/FIRM Crypto stuff
****************************************************************/

//Nand key#2 (0x12C10)
static const u8 key2[0x10] = {
    0x42, 0x3F, 0x81, 0x7A, 0x23, 0x52, 0x58, 0x31, 0x6E, 0x75, 0x8E, 0x3A, 0x39, 0x43, 0x2E, 0xD0
};

//Get Nand CTR key
static void getNandCTR(u8 *buf){
    // calculate CTRNAND/TWL ctr from NAND CID
    // Taken from Decrypt9
    u8 NandCid[16];
    u8 shasum[32];
    
    sdmmc_get_cid(1, (uint32_t *)NandCid);
    sha_quick(shasum, NandCid, 16, SHA256_MODE);
    memcpy(buf, shasum, 16);
}

//Read firm0 from NAND and write to buffer
void nandFirm0(u32 usesd, u32 sdoff, u8 *outbuf){
    u8 CTR[0x10];
    u32 last;
    firmHeader *hdr = (firmHeader *)outbuf;
    getNandCTR(CTR);

    if (usesd)
        sdmmc_sdcard_readsectors(sdoff + (0x0B130000 / 0x200), 1, outbuf);
    else
        sdmmc_nand_readsectors(0x0B130000 / 0x200, 1, outbuf);

    aes_use_keyslot(0x06);
    aes_advctr(CTR, 0x0B130000/0x10, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes(outbuf, outbuf, 0x200 / AES_BLOCK_SIZE, CTR, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);
    if (hdr->magic != 0x4D524946) // FIRM
        return;

    last = 0x200;
    for (int i = 0; i < 4; i++) {
        if (!hdr->section[i].size)
            continue;

        if (usesd)
            sdmmc_sdcard_readsectors(sdoff + (0x0B130000 + hdr->section[i].offset) / 0x200, hdr->section[i].size / 0x200, hdr->section[i].address);
        else
            sdmmc_nand_readsectors((0x0B130000 + hdr->section[i].offset) / 0x200, hdr->section[i].size / 0x200, hdr->section[i].address);

        aes_advctr(CTR, hdr->section[i].offset - last, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes(hdr->section[i].address, hdr->section[i].address, hdr->section[i].size / AES_BLOCK_SIZE, CTR, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);
        last = hdr->section[i].offset + hdr->section[i].size;
    }
}

//ARM9Loader replacement
void arm9Loader(u8 *arm9Section, u32 mode)
{
    //Firm keys
    u8 keyY[0x10],
       arm9BinCTR[0x10],
       arm9BinSlot = mode ? 0x16 : 0x15;

    //Setup keys needed for arm9bin decryption
    memcpy(keyY, arm9Section + 0x10, 0x10);
    memcpy(arm9BinCTR, arm9Section + 0x20, 0x10);

    //Calculate the size of the ARM9 binary
    u32 arm9BinSize = 0;
    //http://stackoverflow.com/questions/12791077/atoi-implementation-in-c
    for(u8 *tmp = arm9Section + 0x30; *tmp; tmp++)
        arm9BinSize = (arm9BinSize << 3) + (arm9BinSize << 1) + *tmp - '0';

    if(mode)
    {
        const u8 key1[0x10] = {0x07, 0x29, 0x44, 0x38, 0xF8, 0xC9, 0x75, 0x93, 0xAA, 0x0E, 0x4A, 0xB4, 0xAE, 0x84, 0xC1, 0xD8},
                 key2[0x10] = {0x42, 0x3F, 0x81, 0x7A, 0x23, 0x52, 0x58, 0x31, 0x6E, 0x75, 0x8E, 0x3A, 0x39, 0x43, 0x2E, 0xD0};
        u8 keyX[0x10];

        aes_setkey(0x11, mode == 1 ? key1 : key2, AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_use_keyslot(0x11);
        aes(keyX, arm9Section + 0x60, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
        aes_setkey(arm9BinSlot, keyX, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    }

    aes_setkey(arm9BinSlot, keyY, AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setiv(arm9BinCTR, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(arm9BinSlot);

    //Decrypt arm9bin
    aes(arm9Section + 0x800, arm9Section + 0x800, arm9BinSize / AES_BLOCK_SIZE, arm9BinCTR, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);

    //Set >=9.6 KeyXs
    if(mode == 2)
    {
        u8 keyData[0x10] = {0xDD, 0xDA, 0xA4, 0xC6, 0x2C, 0xC4, 0x50, 0xE9, 0xDA, 0xB6, 0x9B, 0x0D, 0x9D, 0x2A, 0x21, 0x98},
           decKey[0x10];

        //Set keys 0x19..0x1F keyXs
        aes_use_keyslot(0x11);
        for(u8 slot = 0x19; slot < 0x20; slot++)
        {
            aes(decKey, keyData, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
            aes_setkey(slot, decKey, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
            keyData[0xF] += 1;
        }
    }
}