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
#include <string.h>
#include <wonderful.h>
#include <ws.h>
#include <wsx/zx0.h>
#include "driver.h"
#include "lang.h"
#include "settings.h"
#include "sram.h"
#include "ui.h"
#include "util.h"
#include "ws/hardware.h"
#include "ws/system.h"
#include "xmodem.h"
#include "../obj/assets/wsmonitor_zx0.h"

typedef enum {
    MENU_TOOL_BFBCODE_XM,
    MENU_TOOL_SRAMCODE_XM,
    MENU_TOOL_WSMONITOR_RAM,
    MENU_TOOL_IPL_SRAM
} ui_tool_id_t;

static uint16_t __far ui_tool_lks[] = {
    LK_UI_TOOLS_BFBCODE_XM,
    LK_UI_TOOLS_SRAMCODE_XM,
    LK_UI_TOOLS_WSMONITOR_RAM,
    LK_UI_TOOLS_IPL_SRAM
};
static void ui_tool_menu_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    strncpy(buf, lang_keys[ui_tool_lks[entry_id]], buf_len);
    if (entry_id >= MENU_TOOL_IPL_SRAM) {
        buf_right[0] = '>';
        buf_right[1] = '>';
        buf_right[2] = 0;
    }
}

void ui_tool_xmodem_ui_message(uint16_t lk_msg) {
    ui_fill_line(13, 0);
    ui_puts_centered(false, 13, 0, lang_keys[lk_msg]);
}

void ui_tool_xmodem_ui_step(uint32_t bytes) {
    uint8_t line = inportb(IO_LCD_LINE);
    if (line >= 136 || line < 72) {
        ui_fill_line(14, 0);
        ui_bg_printf_centered(14, 0, lang_keys[LK_UI_XMODEM_BYTE_PROGRESS], bytes);
    }
    ui_step_work_indicator();
}

static void ui_tool_sramcode_bfb() {
    uint8_t buffer[128];

    ui_reset_main_screen();
    ui_puts_centered(false, 2, 0, lang_keys[LK_UI_XMODEM_RECEIVE]);
    ui_puts_centered(false, 3, 0, lang_keys[LK_UI_XMODEM_PRESS_B_TO_CANCEL]);

    uint8_t __far* code_start_ptr;
    uint8_t __far* code_ptr = buffer;
    uint16_t code_blocks_left = 0xFFFF;
    bool code_offset0 = false;
    bool active = true;

    xmodem_open_default();
    if (xmodem_recv_start() == XMODEM_OK) {
        while (active) {
            uint8_t result = xmodem_recv_block(code_ptr);
            switch (result) {
                case XMODEM_COMPLETE:
                    launch_ram(code_start_ptr);
                    break;
                case XMODEM_SELF_CANCEL:
                case XMODEM_CANCEL:
                    active = false;
                    break;
                case XMODEM_OK:
                    if (code_blocks_left == 0xFFFF) {
                        ui_tool_xmodem_ui_message(LK_UI_XMODEM_IN_PROGRESS);

                        if (buffer[0] != 'b' || buffer[1] != 'F') {
                            ui_tool_xmodem_ui_message(LK_UI_XMODEM_INVALID_FILE);
                            active = false;
                            break;
                        } else {
                            uint16_t code_start = *((uint16_t *) (buffer + 2));
                            if (code_start == 0xFFFF) {
                                code_start = 0x6800;
                                code_start_ptr = MK_FP(0x0680, 0x0000);
                                code_offset0 = true;
                            } else {
                                code_start_ptr = MK_FP(0x0000, code_start);
                            }
                            if (code_start < 0x6800 || code_start > 0xFD80) {
                                ui_tool_xmodem_ui_message(LK_UI_XMODEM_INVALID_FILE);
                                active = false;
                                break;
                            } else {
                                code_blocks_left = (0xFE00 - code_start - 124) >> 7;
                                code_ptr = code_start_ptr;
                                memcpy(code_ptr, buffer + 4, 124);
                                code_ptr += 124;
                                xmodem_recv_ack();
                                break;
                            }
                        }
                    } else if (code_blocks_left--) {
                        code_ptr += 128;
                        xmodem_recv_ack();
                        break;
                    }
                    // fall through to XMODEM_ERROR
                case XMODEM_ERROR:
                    ui_tool_xmodem_ui_message(LK_UI_XMODEM_ERROR);
                    active = false;
                    break;
            }
        }
    }

    ui_clear_work_indicator();
    xmodem_close();
    while (!xmodem_poll_exit()) cpu_halt();
}

static void ui_tool_sramcode_xm() {
    sram_unload();

    ui_reset_main_screen();
    ui_puts_centered(false, 2, 0, lang_keys[LK_UI_XMODEM_RECEIVE]);
    ui_puts_centered(false, 3, 0, lang_keys[LK_UI_XMODEM_PRESS_B_TO_CANCEL]);

    uint8_t __far* sram_ptr = MK_FP(0x1000, 0x0010);
    uint16_t sram_incrs = 0;
    bool active = true;

    xmodem_open_default();
    if (xmodem_recv_start() == XMODEM_OK) {
        while (active) {
            uint8_t result = xmodem_recv_block(sram_ptr);
            switch (result) {
                case XMODEM_COMPLETE:
                    launch_ram(MK_FP(0x1000, 0x0010));
                    break;
                case XMODEM_SELF_CANCEL:
                case XMODEM_CANCEL:
                    active = false;
                    break;
                case XMODEM_OK:
                    if (sram_incrs == 0) {
                        ui_tool_xmodem_ui_message(LK_UI_XMODEM_IN_PROGRESS);
                    }
                    sram_incrs++;
                    if (sram_incrs < 512) {
                        ui_tool_xmodem_ui_step((uint32_t) sram_incrs << 7);
                        sram_ptr += 128;
                        xmodem_recv_ack();
                        break;
                    }
                    // fall through to XMODEM_ERROR
                case XMODEM_ERROR:
                    ui_tool_xmodem_ui_message(LK_UI_XMODEM_ERROR);
                    active = false;
                    break;
            }
        }
    }

    ui_clear_work_indicator();
    xmodem_close();
    while (!xmodem_poll_exit()) cpu_halt();
}

void ui_tools(void) {
    uint8_t menu_list[16];
    uint8_t i = 0;
    if (ws_system_color_active()) menu_list[i++] = MENU_TOOL_BFBCODE_XM;
    if ((_CS & 0xF000) != 0x1000) menu_list[i++] = MENU_TOOL_SRAMCODE_XM;
    menu_list[i++] = MENU_TOOL_WSMONITOR_RAM;
#ifndef TARGET_flash_masta
    if (!(inportb(IO_SYSTEM_CTRL1) & SYSTEM_CTRL1_IPL_LOCKED)) menu_list[i++] = MENU_TOOL_IPL_SRAM;
#endif
    menu_list[i++] = MENU_ENTRY_END;

    ui_menu_state_t menu = {
        .list = menu_list,
        .build_line_func = ui_tool_menu_build_line,
        .flags = 0
    };
    ui_menu_init(&menu);

    uint16_t result = ui_menu_select(&menu);
    switch (result) {
        case MENU_TOOL_BFBCODE_XM: ui_tool_sramcode_bfb(); break;
        case MENU_TOOL_SRAMCODE_XM: ui_tool_sramcode_xm(); break;
        case MENU_TOOL_WSMONITOR_RAM: {
            wait_for_vblank();
            outportw(IO_DISPLAY_CTRL, 0);
            wsx_zx0_decompress((uint8_t*) 0x3000, wsmonitor);
            launch_ram(MK_FP(0x0300, 0x0000));
        } break;
#ifndef TARGET_flash_masta
        case MENU_TOOL_IPL_SRAM: {
            sram_unload();
            for (int i = 0; i < 8; i++) {
                outportb(IO_BANK_RAM, 0xF8 | i);
                if (inportb(IO_SYSTEM_CTRL1) & SYSTEM_CTRL1_COLOR) {
                    memcpy(MK_FP(0x1000, 0x0000), MK_FP(0xFE00, 0x0000), 0x2000);
                } else {
                    memcpy(MK_FP(0x1000, 0x0000), MK_FP(0xFF00, 0x0000), 0x1000);
                }
            }
            ui_dialog_run(0, 0, LK_DIALOG_SUCCESS, LK_DIALOG_OK);
        } break;
#endif
    }
}
