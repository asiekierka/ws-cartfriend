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

#include <stdbool.h>
#include <stdint.h>

void xmodem_open_default(void);

void wait_for_vblank(void);

int u8_arraylist_len(uint8_t *list);
int u16_arraylist_len(uint16_t *list);

uint16_t crc16(const char *data, uint16_t len, uint16_t pad_len);

extern void crt0_restart();
