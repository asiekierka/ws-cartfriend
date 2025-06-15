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

#include <wonderful.h>
#include "config.h"

	.arch	i186
	.code16
	.intel_syntax noprefix

#ifdef USE_SLOT_SYSTEM
	.global sram_copy_to_buffer_check_flash
	.align 2
sram_copy_to_buffer_check_flash:
	push	si
	push	di
	push	ds
	push	es

	// configure ds:si = 0x1000:dx, es:di = 0x0000:ax, dx = 0xFFFF, cx = 0x80
	mov di, ax
	mov si, dx
	xor ax, ax
	mov es, ax
	mov dx, 0xFFFF
	mov ah, 0x10
	mov ds, ax
	mov cx, 0x80 // 128 words = 256 bytes
	cld
	.align 2, 0x90
sram_copy_buffer_checkff_loop:
.rept 4
	lodsw // 3 cycles
	stosw // 3 cycles
	cmp ax, dx // 1 cycle
	jnz sram_copy_buffer_checkff_loop_found_data // 1 cycle
	dec cx // 1 cycle
.endr
	jnz sram_copy_buffer_checkff_loop // 4 cycles

	// all 0xFFFF
	mov al, 0
	pop	es
	pop	ds
	pop	di
	pop	si
	IA16_RET

	.align 2
sram_copy_buffer_checkff_loop_found_data:
	dec cx
	mov al, 1
	rep movsw

sram_copy_buffer_checkff_loop_done:
	pop	es
	pop	ds
	pop	di
	pop	si
	IA16_RET

	// 0x3000:offset => 0x1000:offset
	.global sram_copy_from_bank1
	.align 2
sram_copy_from_bank1:
	push	si
	push	di
	push	ds
	push	es

	mov si, ax
	mov di, ax
	mov ax, 0x1000
	mov es, ax // es:di = 0x1000:offset
	mov ah, 0x30
	mov ds, ax // ds:si = 0x3000:offset
	mov cx, dx // cx = words
	cld
	rep movsw

	pop	es
	pop	ds
	pop	di
	pop	si
	IA16_RET


#endif
