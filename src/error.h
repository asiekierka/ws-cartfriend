#pragma once
/**
 * Copyright (c) 2022 Adrian Siekierka
 *
 * CartFriend is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * CartFriend is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with CartFriend. If not, see <https://www.gnu.org/licenses/>. 
 */

#include <stdint.h>
#include <wonderful.h>

#define ERROR_CODE_SRAM_SLOT_OVERFLOW_UNKNOWN 0x0001
#define ERROR_CODE_SRAM_SLOT_OVERFLOW_SWITCH 0x0002
#define ERROR_CODE_UNLOCK_OVERFLOW 0x0003
#define ERROR_CODE_LOCK_UNDERFLOW 0x0004
#define ERROR_CODE_SRAM_ODD_SIZE_UNHANDLED 0x0005

#define ERROR_CODE_WW_UNPACK_FAILED 0x0101
#define ERROR_CODE_WW_FLASH_FAILED 0x0102

void error_critical(uint16_t code, uint16_t extra) __far;
