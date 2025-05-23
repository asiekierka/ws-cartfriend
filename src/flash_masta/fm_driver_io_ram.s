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
#include <ws.h>

	.arch	i186
	.code16
	.intel_syntax noprefix
	.global driver_read_slot
	.global driver_write_slot
	.global driver_erase_bank
	.global driver_launch_slot
	.global fm_initial_slot
	.global _fm_unlock_refcount

	.section .text
	.align 2
driver_launch_slot_relocated:
	mov ax, [0x40]
	mov [0x2000], ax
	mov ax, [0x50]
	mov [0x2002], ax

	// initialize ROM banks
	mov al, cl
	or al, 0xF
	out IO_BANK_ROM0, al
	out IO_BANK_ROM1, al
	ror al, 4
	out IO_BANK_ROM_LINEAR, al
	out IO_BANK_RAM, al

	// lock cartridge
	mov al, 0x08
	out IO_CART_GPO_DATA, al
	xor ax, ax
	out IO_CART_GPO_CTRL, al

	// the memory clears will take the necessary time
	mov di, 0x2200
	mov cx, ((0x4000 - 0x2200) >> 1)
	rep stosw

	// clear (almost) all memory we can (rep stosw in relocated part)
	mov di, ax
	mov cx, (0x40 >> 1)
	rep stosw

	mov di, 0x58
	mov cx, ((0x2000 - 0x58) >> 1)
	rep stosw

	// restore partial register state
	mov	bx, [0x42]
	mov [0x42], ax
	mov	dx, [0x46]
	mov [0x46], ax
	mov	si, [0x4C]
	mov [0x4C], ax

	// mov di, offset ((driver_launch_slot_end + 1 - driver_launch_slot_relocated + 0x2000) & 0xFFFE)
	// mov cx, offset ((0x4000 >> 1) - ((driver_launch_slot_end + 1 - driver_launch_slot_relocated + 0x2000) >> 1))
	// rep stosw

driver_launch_slot_restore:
	// restore register state
	mov cx, [0x56]
	push cx
	popf
	push ax // clear stack area we just used
	mov	cx, [0x44]
	mov	sp, [0x48]
	mov	bp, [0x4A]
	mov	di, [0x4E]
	mov	es, [0x52]
	mov	ss, [0x54]

	// take clearing a bit more seriously
	mov ax, 0
	mov [0x40], ax
	mov [0x44], ax
	mov [0x48], ax
	mov [0x4A], ax
	mov [0x4E], ax
	mov [0x50], ax
	mov [0x52], ax
	mov [0x54], ax
	mov [0x56], ax
	mov ax, [0x2002]
	mov ds, ax
	mov ax, [0x2000]

	// fly me to the moon
	jmp 0xFFFF,0x0000
driver_launch_slot_end:

	.section .data
	.align 2
driver_launch_slot:
	push cx

	// move relocated driver to what i think is the safest place
	// do this before clearing banks - we can keep the code in ROM this way
	//mov   ax, offset "driver_launch_slot_relocated!"
	.byte   0xB8
	.reloc  ., R_386_SEG16, "driver_launch_slot_relocated!"
	.word   0
	//
	mov ds, ax
	mov si, offset driver_launch_slot_relocated
	xor ax, ax
	mov es, ax

	mov di, 0x2000
	mov cx, 0x100
	rep stosw

	mov di, 0x2000
	mov cx, offset ((driver_launch_slot_end - driver_launch_slot_relocated + 1) >> 1)
	rep movsw
	
	pop cx

	// restore DS
	// xor ax, ax - done above
	mov ds, ax

	mov byte ptr [_driver_current_slot], 0xFF
	push 0x2000
	jmp _driver_switch_slot

_rtc_wait_ready:
	push ax
	.balign 2, 0x90
1:
	in al, IO_CART_RTC_CTRL
	test al, (CART_RTC_READY | CART_RTC_ACTIVE)
	jz 2f // The "not ready and not active" state also allows writing to the RTC.
	jns 1b // If not ready, keep waiting.
2:
	pop ax
	ret

_rtc_write_data_al:
	call _rtc_wait_ready
	out IO_CART_RTC_DATA, al
	ret

// dx = slot
_driver_switch_slot:
	push ax
_driver_switch_slot1:
	ss mov al, [_driver_current_slot]
	cmp al, dl
	je _driver_switch_slot_equal
	ss mov [_driver_current_slot], dl

	// Use RTC protocol to change the current bank.
	mov al, 0xA0
	call _rtc_write_data_al
	mov al, 0x14
	out IO_CART_RTC_CTRL, al
	mov al, dl
	call _rtc_write_data_al
	xor al, al
	call _rtc_write_data_al
	call _rtc_write_data_al
	call _rtc_write_data_al
	call _rtc_write_data_al
	call _rtc_write_data_al
	call _rtc_wait_ready
_driver_change_loop:
	push cx
/*	mov ch, 0
	mov cl, byte ptr [settings_local + 424]
	mov ax, cx
	shl cx, 2
	add cx, ax
	shl cx, 7 */
	mov cx, (615 * 12) // ~12 ms (15 ms known to be reliable)
	.balign 2, 0x90
1:
	loop 1b
	pop cx

_driver_switch_slot_equal:
	pop ax
	ret

// preserves AX
// CX = bank
_driver_switch_slot_bank1:
	cli
	push ax
	in al, IO_BANK_ROM1
	ss mov [_driver_bank_temp], al
	mov al, cl
	out IO_BANK_ROM1, al
	jmp _driver_switch_slot1

// clobbers AX, DL
_driver_unswitch_slot_bank1:
	ss mov al, [_driver_bank_temp]
	out IO_BANK_ROM1, al
_driver_unswitch_slot:
	ss mov dl, [fm_initial_slot]
	call _driver_switch_slot
	sti
	ret

// preserves AX
_driver_switch_slot_sram:
	cli
	push ax
	in al, IO_BANK_RAM
	ss mov [_driver_bank_temp], al
	mov al, 1
	out IO_CART_FLASH, al
	mov al, cl
	out IO_BANK_RAM, al
	jmp _driver_switch_slot1

// clobbers AX, DL
_driver_unswitch_slot_sram:
	xor ax, ax
	out IO_CART_FLASH, al
	ss mov al, [_driver_bank_temp]
	out IO_BANK_RAM, al
	jmp _driver_unswitch_slot

_driver_reset_flash:
	push ds
	mov ax, 0x1000
	mov ds, ax
	mov byte ptr [0xAAA], 0xAA
	mov byte ptr [0x555], 0x55
	mov byte ptr [0x000], 0xF0
	pop ds
	ret

	.align 2
driver_read_slot:
	push	si
	push	di
	push	ds
	push	es
	push	bp
	mov	bp, sp

	mov di, ax
	call _driver_switch_slot_bank1

	mov bx, 0x3000
	mov	ds, bx
	xor bx, bx
	mov es, bx

	mov si, [bp + 14]
_drs_part2:
	mov	cx, [bp + 16]
	shr	cx, 1
	cld
	rep	movsw
	jnc	2f
	movsb
2:
	pop	bp
	pop	es
	pop	ds

	call _driver_unswitch_slot_bank1
	mov al, 1

	pop	di
	pop	si

	call driver_slot_finish_error_check
	retf 0x4

	.align 2
driver_write_slot:
	push	si
	push	di
	push	ds
	push	es
	push	bp
	mov	bp, sp

	mov si, ax
	call _driver_switch_slot_sram

	mov di, [bp + 14]
	mov	cx, [bp + 16]

	xor bx, bx
	mov ds, bx
	mov bx, 0x1000
	mov es, bx
	mov bx, 0xAAA

	// start bypass
	mov byte ptr es:[bx], 0xAA
	mov byte ptr es:[0x555], 0x55
	mov byte ptr es:[bx], 0x20

	cld

	// check if we can write buffered
	// we need to be within one 512-byte block
	// and length needs to be <= 256
	cmp cx, 256
	ja _dws_write_slow

	mov ax, di
	add ax, cx
	dec ax
	xor ax, di
	and ah, 0xFE
	jnz _dws_write_slow

1:
	mov al, byte ptr [settings_local + 423]
	test al, 0x02
	jnz _dws_write_slow

_dws_write_fast:
	xor bx, bx // clear BX (block address)
	dec cx

	// start write
	mov byte ptr es:[bx], 0x25
	mov byte ptr es:[bx], cl

	shr cx, 1
	.balign 2, 0x90
1:
	rep movsw
	jnc 2f
	movsb
2:
	movsb

	// confirm write
	mov byte ptr es:[bx], 0x29

	// cx is 0
	inc cx
	jmp dws_driver_flash_busyloop_until_done

_dws_write_slow:
	.balign 2, 0x90
1:
	mov byte ptr es:[bx], 0xA0
	movsb
dws_driver_flash_busyloop_until_done:
	nop
	nop
	mov al, byte ptr es:[di]
	nop
	nop
	cmp al, byte ptr es:[di]
	jne dws_driver_flash_busyloop_until_done
	loop 1b // 5 cycles

dws_driver_flash_done:
	// stop bypass
	mov byte ptr es:[bx], 0x90
	mov byte ptr es:[bx], 0x00

	// reset
	call _driver_reset_flash

	pop	bp
	pop	es
	pop	ds

	call _driver_unswitch_slot_sram

	pop	di
	pop	si

	call driver_slot_finish_error_check
	mov al, 1
	retf 0x4

	.align 2
driver_erase_bank:
	test cl, 1
	jnz driver_erase_bank_finish

	push ds
	push si

	call _driver_switch_slot_sram

	// execute erase command
	mov bx, 0x1000
	mov ds, bx
	mov bx, 0xAAA
	mov si, 0x555

	mov byte ptr [bx], 0xAA
	mov byte ptr [si], 0x55
	mov byte ptr [bx], 0x80
	mov byte ptr [bx], 0xAA
	mov byte ptr [si], 0x55
	xor bx, bx
	mov byte ptr [bx], 0x30

	.balign 2, 0x90
deb_driver_flash_busyloop_until_done:
	nop
	nop
	nop
	mov al, byte ptr [bx]
	nop
	nop
	nop
	cmp al, byte ptr [bx] // DQ2 and/or DQ6 toggles if status register
	jne deb_driver_flash_busyloop_until_done

	call _driver_reset_flash

	call _driver_unswitch_slot_sram

	pop si
	pop ds

	call driver_slot_finish_error_check
driver_erase_bank_finish:
	mov al, 1
	retf

	.align 2
// check if the slot was correctly remounted
// this is pretty bare-bones and could be better
// clobbers ax
driver_slot_finish_error_check:
// if BIOS unlocked, skip check
// TODO: implement alternate check
	in al, IO_SYSTEM_CTRL1
	test al, SYSTEM_CTRL1_IPL_LOCKED
	jz driver_slot_finish_error_check_skip

	push ds
	push si

	mov ax, 0xFFFF
	mov ds, ax
	mov si, 0x0005

	lodsw
	cmp ax, 0xAA00
	mov al, 0x50
	jne driver_write_error

	lodsw
	cmp ax, 0x5501
	mov al, 0x51
	jne driver_write_error

	pop si
	pop ds
driver_slot_finish_error_check_skip:
	ret

// 1-byte value in AL => 4 bytes in ES:DI (for display)
// clobbers AX
driver_emergency_error_handler_hex2int:
	mov ah, 0
	push ax
	shr al, 4
	call hex2int_write
	pop ax
	and al, 0x0F
	call hex2int_write
	ret

hex2int_write:
	add al, 48
	cmp al, 58
	jb hex2int_09
	add al, 7
hex2int_09:
	stosw
	ret

// AL has initial error number
driver_write_error:
	xor bx, bx
	mov es, bx
	mov di, 0x3800
	call driver_emergency_error_handler_hex2int
	in al, IO_BANK_ROM_LINEAR
	call driver_emergency_error_handler_hex2int
	in al, IO_BANK_RAM
	call driver_emergency_error_handler_hex2int
	in al, IO_CART_FLASH
	call driver_emergency_error_handler_hex2int
	in ax, IO_DISPLAY_CTRL
	or al, (DISPLAY_SCR1_ENABLE | DISPLAY_SCR2_ENABLE)
	out IO_DISPLAY_CTRL, ax
_end:
	cli
	xor ax, ax
	out IO_HWINT_ENABLE, al
	// lock IEEPROM
	mov al, IEEP_PROTECT
	out IO_IEEP_CTRL, ax
1:
	hlt
	jmp 1b

	.section .bss
_driver_bank_temp:
	.byte 0
_driver_current_slot:
	.byte 0
_fm_unlock_refcount:
	.byte 0
fm_initial_slot:
	.byte 0
	.byte 0
	.byte 0
	.byte 0
	.byte 0
	.byte 0
	.byte 0
