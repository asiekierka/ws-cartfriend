#pragma once
/**
 * CartFriend - XMODEM transfer code
 *
 * Copyright (c) 2022 Adrian "asie" Siekierka
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <stdbool.h>
#include <stdint.h>

#define XMODEM_BLOCK_SIZE 128

#define XMODEM_OK          0 /* OK */
#define XMODEM_CANCEL      1 /* user cancellation */
#define XMODEM_SELF_CANCEL 2 /* local cancellation */
#define XMODEM_ERROR       3 /* transfer error */
#define XMODEM_COMPLETE    4 /* no more blocks to receive */

bool xmodem_poll_exit(void);

void xmodem_open(uint8_t baudrate);
void xmodem_close(void);

uint8_t xmodem_send_start(void);
uint8_t xmodem_send_block(const uint8_t __far* block);
uint8_t xmodem_send_finish(void);

uint8_t xmodem_recv_start(void);
uint8_t xmodem_recv_block(uint8_t __far* block);
