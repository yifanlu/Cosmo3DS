/*
*   utils.c
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "utils.h"
#include "screeninit.h"
#include "draw.h"
#include "fs.h"
#include "i2c.h"
#include "buttons.h"

#define COLOR_TITLE    0xFF9900
#define COLOR_WHITE    0xFFFFFF
#define COLOR_RED      0x0000FF
#define COLOR_BLACK    0x000000

struct option {
    int posY;
    u32 enabled;
};

static u16 waitInput(void){
    u32 pressedKey = 0;
    u16 key;

    //Wait for no keys to be pressed
    while(HID_PAD);

    do {
        //Wait for a key to be pressed
        while(!HID_PAD);
        key = HID_PAD;

        //Make sure it's pressed
        for(u32 i = 0x13000; i; i--){
            if(key != HID_PAD) break;
            if(i == 1) pressedKey = 1;
        }
    } while(!pressedKey);

    return key;
}

void error(const char *message){
    initScreens();

    drawString("An error has occurred:", 10, 10, COLOR_RED);
    int posY = drawString(message, 10, 30, COLOR_WHITE);
    drawString("Press any button to shutdown", 10, posY + 2 * SPACING_Y, COLOR_WHITE);

    waitInput();

    //Shutdown
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1);
    while(1);
}