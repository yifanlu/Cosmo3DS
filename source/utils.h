/*
*   utils.h
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define CFG_BOOTENV (*(vu8 *)0x10010000)

void error(const char *message);