/**
 * CartFriend platform drivers + headers
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

#include <wonderful.h>

	.arch	i186
	.code16
	.intel_syntax noprefix
	.global launch_ram_asm

	.align 2
launch_ram_asm:
	cli
	cld

	mov si, 0x2004
	mov byte ptr [si], 0xEA
	mov [si+1], ax
	mov [si+3], dx

	mov ax, [0x40]
	mov [0x2000], ax
	mov ax, [0x50]
	mov [0x2002], ax

	xor ax, ax
	mov ds, ax
	mov es, ax

	// restore a few registers (ones not used later) here already
	mov	bx, [0x42]
	mov	si, [0x4C]

	// clear (almost) all memory we can (rep stosw in relocated part)
	cmp dx, 0x1000
	jb launch_ram_asm_skip_memory_clear

	mov di, 0
	mov cx, (0x40 >> 1)
	rep stosw

	mov di, 0x58
	mov cx, ((0x2000 - 0x58) >> 1)
	rep stosw

	mov di, 0x200A
	mov cx, ((0x4000 - 0x200A) >> 1)
	rep stosw

	// restore register state
	mov cx, [0x56]
	push cx
	popf
	push ax // clear stack area we just used
	mov	cx, [0x44]
	mov	dx, [0x46]
	mov	sp, [0x48]
	mov	bp, [0x4A]
	mov	di, [0x4E]
	mov	es, [0x52]
	mov	ss, [0x54]

	// take clearing a bit more seriously
	mov ax, 0
	mov [0x40], ax
	mov [0x42], ax
	mov [0x44], ax
	mov [0x46], ax
	mov [0x48], ax
	mov [0x4A], ax
	mov [0x4C], ax
	mov [0x4E], ax
	mov [0x50], ax
	mov [0x52], ax
	mov [0x54], ax
	mov [0x56], ax
	mov ax, [0x2002]
	mov ds, ax
	ss mov ax, [0x2000]

	// fly me to the moon
	jmp 0x0000,0x2004

launch_ram_asm_skip_memory_clear:
	// restore register state
	mov cx, [0x56]
	push cx
	popf
	push ax // clear stack area we just used
	mov	cx, [0x44]
	mov	dx, [0x46]
	mov	sp, [0x48]
	mov	bp, [0x4A]
	mov	di, [0x4E]
	mov	es, [0x52]
	mov	ss, [0x54]

	// fly me to the moon
	jmp 0x0000,0x2004
