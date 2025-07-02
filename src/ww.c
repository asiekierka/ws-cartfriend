/**
 * Copyright (c) 2024 Adrian Siekierka
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

#include <string.h>
#include <ws.h>
#include <wsx/zx0.h>
#include "ww.h"
#include "config.h"
#include "driver.h"
#include "error.h"
#include "input.h"
#include "lang.h"
#include "settings.h"
#include "sram.h"
#include "ui.h"
#include "util.h"
#include "ws/cartridge.h"
#include "xmodem.h"
#include "../obj/assets/athenabios_zx0.h"

#ifdef USE_SLOT_SYSTEM
#define COPY_BOTH 0
#define COPY_BIOS_ONLY 1
#define COPY_OS_ONLY 2

static void ww_copy_to_sram(uint16_t slot, uint16_t bank, uint8_t mode) {
    uint8_t buffer[1024];

    ui_pbar_state_t pbar = {
        .x = 0,
        .y = 13,
        .width = 27,
        .step = 0,
        .step_max = mode == COPY_BOTH ? 128 : 64
    };
    ui_pbar_init(&pbar);

    ui_reset_main_screen();
    ui_puts_centered(false, 2, 0, lang_keys[LK_UI_WW_INSTALL_START]);

    driver_unlock();
    for (int i = (mode == COPY_BIOS_ONLY ? 64 : 0); i < (mode == COPY_OS_ONLY ? 64 : 128); i++) {
        ui_pbar_draw(&pbar);
        ui_step_work_indicator();
        pbar.step++;

        driver_read_slot(buffer, slot, bank | 0xE | (i >> 6), (i << 10), sizeof(buffer));

        ws_bank_ram_set(i >> 6);
        memcpy(MK_FP(0x1000, i << 10), buffer, sizeof(buffer));
    }
    driver_lock();
    
    ui_clear_work_indicator();
}

static void ww_flash_from_sram(uint16_t slot, uint16_t bank) {
    uint8_t buffer[1024];

    ui_pbar_state_t pbar = {
        .x = 0,
        .y = 13,
        .width = 27,
        .step_max = 128
    };
    ui_pbar_init(&pbar);

    ui_reset_main_screen();
    ui_puts_centered(false, 2, 0, lang_keys[LK_UI_WW_BIOS_FLASH]);

    driver_unlock();
    driver_erase_bank(0, slot, bank | 0xE);
    for (int i = 0; i < 128; i++) {
        pbar.step++;
        ui_pbar_draw(&pbar);

        ws_bank_ram_set(i >> 6);
        memcpy(buffer, MK_FP(0x1000, i << 10), sizeof(buffer));

        ui_step_work_indicator();
        driver_write_slot(buffer,       slot, bank | 0xE | (i >> 6), (i << 10),       256);
        ui_step_work_indicator();
        driver_write_slot(buffer + 256, slot, bank | 0xE | (i >> 6), (i << 10) + 256, 256);
        ui_step_work_indicator();
        driver_write_slot(buffer + 512, slot, bank | 0xE | (i >> 6), (i << 10) + 512, 256);
        ui_step_work_indicator();
        driver_write_slot(buffer + 768, slot, bank | 0xE | (i >> 6), (i << 10) + 768, 256);
        ui_step_work_indicator();
        driver_read_slot( buffer,       slot, bank | 0xE | (i >> 6), (i << 10),       sizeof(buffer));
        if (memcmp(buffer, MK_FP(0x1000, i << 10), sizeof(buffer))) {
            error_critical(ERROR_CODE_WW_FLASH_FAILED, i);
        }
    }
    driver_lock();

    ui_clear_work_indicator();
}

static void ww_unpack_bios(void) {
    ui_reset_main_screen();
    ui_puts_centered(false, 2, 0, lang_keys[LK_UI_WW_BIOS_UNPACK]);

    ws_bank_ram_set(1);
    wsx_zx0_decompress(MK_FP(0x1000, 0x0000), athenabios);
    if (*((uint32_t __far*) MK_FP(0x1000, 0xFFE0)) != WW_ATHENABIOS_MAGIC)
        error_critical(ERROR_CODE_WW_UNPACK_FAILED, *((uint16_t __far*) MK_FP(0x1000, 0xFFE0)));
}

static bool ww_download_os() {
    uint8_t buffer[128];

    ui_reset_main_screen();

    // Clear OS SRAM area
    ws_bank_ram_set(0);
    memset(MK_FP(0x1000, 0x0000), 0xFF, 16384);
    memset(MK_FP(0x1000, 0x4000), 0xFF, 16384);
    memset(MK_FP(0x1000, 0x8000), 0xFF, 16384);
    memset(MK_FP(0x1000, 0xC000), 0xFF, 16384);

    // Wait for user to initiate send
    ui_puts_centered(false, 6, 0, lang_keys[LK_UI_WW_OS_DOWNLOAD_1]);
    ui_puts_centered(false, 7, 0, lang_keys[LK_UI_WW_OS_DOWNLOAD_2]);
    ui_puts_centered(false, 8, 0, lang_keys[LK_UI_WW_OS_DOWNLOAD_3]);
    ui_puts_centered(false, 9, 0, lang_keys[LK_UI_WW_OS_DOWNLOAD_4]);

    input_wait_key(KEY_A);

    // Receive data
    ui_reset_main_screen();
    ui_puts_centered(false, 2, 0, lang_keys[LK_UI_XMODEM_RECEIVE]);
    ui_puts_centered(false, 3, 0, lang_keys[LK_UI_XMODEM_PRESS_B_TO_CANCEL]);

    uint8_t __far* sram_ptr = MK_FP(0x1000, 0x0000);
    uint16_t sram_incrs = 0;
    bool active = true;

    xmodem_open_default();
    if (xmodem_recv_start() == XMODEM_OK) {
        while (active) {
            uint8_t result = xmodem_recv_block(buffer);
            switch (result) {
                case XMODEM_COMPLETE:
                    return true;
                case XMODEM_SELF_CANCEL:
                case XMODEM_CANCEL:
                    active = false;
                    break;
                case XMODEM_OK: {
                    if (sram_incrs == 0) {
                        ui_tool_xmodem_ui_message(LK_UI_XMODEM_IN_PROGRESS);
                    }
                    // Write to SRAM
                    uint8_t prev_b = 0xFF;
                    for (int i = 0; i < 128; i++) {
                        sram_ptr[i] = buffer[i] ^ prev_b;
                        prev_b = buffer[i];
                    }

                    // Step, acknowledge packet
                    sram_incrs++;
                    if (sram_incrs < 512) {
                        ui_tool_xmodem_ui_step((uint32_t) sram_incrs << 7);
                        sram_ptr += 128;
                        break;
                    }
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

    // clear remaining data
    uint16_t size_blocks = sram_incrs;
    for (; sram_incrs < 512; sram_incrs++) {
         memset(sram_ptr, 0xFF, 128);
         sram_ptr += 128;
    }

    // write footer
    uint8_t __far* sram_footer = MK_FP(0x1000, 0xFFF0);
    sram_footer[0x0] = 0xEA;
    sram_footer[0x1] = 0x00;
    sram_footer[0x2] = 0x00;
    sram_footer[0x3] = 0x00;
    sram_footer[0x4] = 0xE0;
    sram_footer[0x5] = 0x00;
    sram_footer[0x6] = size_blocks;
    sram_footer[0x7] = size_blocks >> 8;

    return false;
}

void ww_ui_erase_userdata(uint16_t slot, uint16_t bank) {
    if (slot == driver_get_launch_slot())
        return;

    sram_unload();

    ui_reset_main_screen();
    ui_puts_centered(false, 2, 0, lang_keys[LK_UI_MSG_ERASE_SRAM]);

    driver_unlock();
    driver_erase_bank(0, slot, bank | 0x8);
    driver_erase_bank(0, slot, bank | 0xA);
    driver_erase_bank(0, slot, bank | 0xC);
    driver_lock();

    for (uint8_t k = 0; k < SRAM_SLOTS; k++) {
        if (settings_local.sram_slot_mapping[k] == (slot & 0x0F)) {
            sram_erase(k, SRAM_OFFSET_SIZE(bank < 0xF0 ? 4 : 0, 4));
        }
    }
}

void ww_ui_install_full(uint16_t slot, uint16_t bank) {
    if (slot == driver_get_launch_slot())
        return;

    sram_unload();
    ww_unpack_bios();
    if (!ww_download_os())
        return;
    ww_flash_from_sram(slot, bank);
}

void ww_ui_install_bios(uint16_t slot, uint16_t bank) {
    if (slot == driver_get_launch_slot())
        return;
    
    sram_unload();
    ww_copy_to_sram(slot, bank, COPY_OS_ONLY);
    ww_unpack_bios();
    ww_flash_from_sram(slot, bank);
}

void ww_ui_install_os(uint16_t slot, uint16_t bank) {
    if (slot == driver_get_launch_slot())
        return;
    
    sram_unload();
    ww_copy_to_sram(slot, bank, COPY_BIOS_ONLY);
    if (!ww_download_os())
        return;
    ww_flash_from_sram(slot, bank);
}
#endif
