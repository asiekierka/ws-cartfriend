/**
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

	.arch	i186
	.code16
	.intel_syntax noprefix

	.section .start
	.global _start
_start:
	cli

	// store correct state of AX and DS in a screen LUT
	// we need to initialize DS to access RAM, but we want to preserve it
	out	0x20, ax
	mov	ax, ds
	out	0x22, ax

	// initialize DS to 0x0000
	mov	ax, 0x0000
	mov	ds, ax
	
	// preserve registers in RAM
	in	ax, 0x20
	mov	[0x40], ax
	mov	[0x42], bx
	mov	[0x44], cx
	mov	[0x46], dx
	mov	[0x48], sp
	mov	[0x4A], bp
	mov	[0x4C], si
	mov	[0x4E], di
	// CS is known
	in	ax, 0x22
	mov	[0x50], ax
	mov	[0x52], es
	mov	[0x54], ss

	// configure SP, ES, SS, flags
	mov	sp, offset "__wf_heap_top"
	mov	ax, 0x0000
	mov	es, ax
	mov	ss, ax

	pushf
	pop ax
	mov	[0x56], ax

	// CartFriend end

_start_continue:
	// set DS:SI to the location of the data block
	.reloc	.+1, R_386_SEG16, "__wf_data_block!"
	mov	ax, 0
	mov	ds, ax
	mov	si, offset "__wf_data_block"

	cld

_start_parse_data_block:
	lodsw
	mov	cx, ax
	test	cx, cx
	jz	_start_finish_data_block
	lodsw
	mov	di, ax
	lodsw
#ifndef SRAM
	cmp	di, 0x4000
	jb	_start_parse_data_block_non_wsc

	// data block requests WSC mode?
	in	al, 0xA0
	test	al, 0x02
	// if the console is not color, skip the block entirely
	jz	_start_parse_data_block
	// initialize WSC mode
	in	al, 0x60
	or	al, 0x80
	out	0x60, al
	
_start_parse_data_block_non_wsc:
#endif
	test	ah, 0x80
	jnz	_start_data_block_clear

_start_data_block_move:
	shr	cx, 1
	rep	movsw
	jnc	_start_parse_data_block
	movsb
	jmp	_start_parse_data_block

_start_data_block_clear:
	xor	ax, ax
	shr	cx, 1
	rep	stosw
	jnc	_start_parse_data_block
	stosb
	jmp	_start_parse_data_block

_start_finish_data_block:

	// initialize DS
	push	es
	pop	ds

	// clear int enable
	out	0xB2, al

	// configure default interrupt base
	mov	al, 0x08
	out	0xB0, al

	// reset console mode to mono
	in	al, 0x60
	and	al, 0x1F
	out	0x60, al

#ifdef __IA16_CMODEL_IS_FAR_TEXT
	.reloc	.+3, R_386_SEG16, "main!"
	jmp 0:main
#else
	jmp main
#endif

	.global crt0_restart
crt0_restart:
	cli
	// configure SP, ES, SS, flags
	mov	sp, offset "__wf_heap_top"
	xor	ax, ax
	mov	ds, ax
	mov	es, ax
	mov	ss, ax
	jmp _start_continue
