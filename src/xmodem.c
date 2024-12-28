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
#include <stddef.h>
#include <stdint.h>
#include <wonderful.h>
#include "input.h"
#include "ui.h"
#include "util.h"
#include "xmodem.h"

#define SOH 1
#define EOT 4
#define ACK 6
#define NAK 21
#define CAN 24

static uint8_t xmodem_idx;
static uint8_t xmodem_retry;

bool xmodem_poll_exit(void) {
	input_update();
	return (input_pressed & KEY_B);
}

void xmodem_open(uint8_t baudrate) {
	ws_serial_open(baudrate);
	ws_hwint_set_default_handler_serial_rx();
}

void xmodem_close(void) {
	ws_serial_close();
}

// call after SOH
static uint8_t xmodem_read_block(uint8_t __far* block) {
	uint8_t idx = ws_serial_getc();
	if (idx != xmodem_idx) {
		return XMODEM_CANCEL;
	}
	uint8_t idx_inv = ws_serial_getc();
	if ((idx ^ 0xFF) != idx_inv) {
		return XMODEM_CANCEL;
	}

	uint8_t checksum = 0;
	for (uint16_t i = 0; i < XMODEM_BLOCK_SIZE; i++) {
		uint8_t v = ws_serial_getc();
		checksum += v;
		if (block != NULL) { 
			block[i] = v;
		}
	}

	uint8_t checksum_actual = ws_serial_getc();
	return (checksum == checksum_actual) ? XMODEM_OK : XMODEM_ERROR;
}

static void xmodem_write_block(const uint8_t __far* block) {
	ws_serial_putc(SOH);
	ws_serial_putc(xmodem_idx);
	ws_serial_putc(xmodem_idx ^ 0xFF);

	uint8_t checksum = 0;
	for (uint16_t i = 0; i < XMODEM_BLOCK_SIZE; i++) {
		ws_serial_putc(block[i]);
		checksum += block[i];
	}

	ws_serial_putc(checksum);
}

uint8_t xmodem_recv_start(void) {
	xmodem_idx = 1;
	xmodem_retry = 1;

	return XMODEM_OK;
}

uint8_t xmodem_recv_block(uint8_t __far* block) {
recv_block_start:
	ws_serial_putc(xmodem_retry ? NAK : ACK);
	cpu_irq_disable();

	while (1) {
		if (xmodem_poll_exit()) return XMODEM_SELF_CANCEL;

		int16_t r = ws_serial_getc_nonblock();
		if (r >= 0) {
			if (r == CAN) {
				return XMODEM_CANCEL;
			} else if (r == EOT) {
				ws_serial_putc(ACK);
				return XMODEM_COMPLETE;
			} else if (r == SOH) {
				uint8_t result = xmodem_read_block(block);
				if (result == XMODEM_OK) {
					xmodem_idx++;
					xmodem_retry = 0;
					return XMODEM_OK;
				} else if (result == XMODEM_ERROR) {
					goto recv_block_error;
				} else {
					ws_serial_putc(CAN);
					return XMODEM_ERROR;
				}
			} else {
recv_block_error:
				xmodem_retry++;
				if (xmodem_retry > 10) return XMODEM_ERROR;
				goto recv_block_start;
			}
		}
	}

	cpu_irq_enable();
}

uint8_t xmodem_send_start(void) {
	xmodem_idx = 1;
	xmodem_retry = 0;

	while (!xmodem_poll_exit()) {
		int16_t r = ws_serial_getc_nonblock();
		if (r >= 0) {
			if (r == CAN) {
				return XMODEM_CANCEL;
			} else if (r == NAK) {
				return XMODEM_OK;
			}
		}

		ws_hwint_enable(HWINT_SERIAL_RX);
		cpu_halt();
	}
	return XMODEM_SELF_CANCEL;
}

uint8_t xmodem_send_block(const uint8_t __far* block) {
	uint8_t retries = 10;
send_write_again:
	if ((retries--) == 0) return XMODEM_ERROR;
	xmodem_write_block(block);

	while (!xmodem_poll_exit()) {
		int16_t r = ws_serial_getc_nonblock();
		if (r >= 0) {
			if (r == CAN) {
				return XMODEM_CANCEL;
			} else if (r == NAK) {
				goto send_write_again;
			} else if (r == ACK) {
				xmodem_idx++;
				return XMODEM_OK;
			}
		}

		ws_hwint_enable(HWINT_SERIAL_RX);
		cpu_halt();
	}
	return XMODEM_SELF_CANCEL;
}

uint8_t xmodem_send_finish(void) {
	uint8_t retries = 10;
send_write_again:
	if ((retries--) == 0) return XMODEM_ERROR;
	ws_serial_putc(EOT);

	while (!xmodem_poll_exit()) {
		int16_t r = ws_serial_getc_nonblock();
		if (r >= 0) {
			if (r == CAN) {
				return XMODEM_CANCEL;
			} else if (r == NAK) {
				goto send_write_again;
			} else if (r == ACK) {
				return XMODEM_OK;
			}
		}

		ws_hwint_enable(HWINT_SERIAL_RX);
		cpu_halt();
	}
	return XMODEM_SELF_CANCEL;
}
