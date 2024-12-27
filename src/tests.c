/**
 * Copyright (c) 2023 Adrian Siekierka
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
#include "driver.h"
#include "input.h"
#include "tests.h"
#include "settings.h"
#include "sram.h"
#include "ui.h"
#include "ws/hardware.h"

#ifdef USE_SLOT_SYSTEM
const char __far msg_save_read_write_error1[] = "%02X%04X %02X";

__attribute__((noinline))
static void test_save_read_write_error_emit(uint8_t x, uint8_t y, uint16_t bank, uint16_t offset, uint8_t expected) {
    settings_local.active_sram_slot = 0xFF;
    settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
    outportb(IO_BANK_RAM, 0);
    sram_ui_quiet = false;

    ui_clear_work_indicator();
    ui_update_indicators();

    ui_bg_printf(x, y, 0, msg_save_read_write_error1, bank, offset, expected);
}

__attribute__((always_inline))
static inline bool test_save_read_write_error(uint8_t x, uint8_t y, uint16_t bank, uint16_t offset, uint8_t expected) {
    volatile uint8_t __far *ptr = MK_FP(0x1000, offset);
    if (*ptr != expected) {    
        test_save_read_write_error_emit(x, y, bank, offset, expected);
        return true;
    } else {
        return false;
    }
}

bool test_save_read_write(uint8_t x, uint8_t y, uint8_t slot) {
    settings_local.active_sram_slot = slot;
    settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
    sram_ui_quiet = true;

    // Erase SRAM slot
    sram_erase(slot, SRAM_OFFSET_SIZE_DEFAULT);

    // Write test pattern to SRAM
    for (int i = 0; i < 8; i++) {
        ui_step_work_indicator();
        outportb(IO_BANK_RAM, i);
        volatile uint8_t __far *ptr = MK_FP(0x1000, 0x0000);
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 256; k++, ptr++) {
                *ptr = j + k - i;
            }
        }
    }

    // Compare SRAM
    for (int i = 0; i < 8; i++) {
        ui_step_work_indicator();
        outportb(IO_BANK_RAM, i);
        uint16_t ofs = 0;
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 256; k++, ofs++) {
                if (test_save_read_write_error(x, y, i, ofs, j + k - i)) return false;
            }
        }
    }

    ui_bg_putc(x, y, '.', 0);

    // Read/Write SRAM
    sram_unload();
    ui_bg_putc(x + 1, y, '.', 0);
    sram_switch_to_slot(slot, SRAM_OFFSET_SIZE_DEFAULT);
    ui_bg_putc(x + 2, y, '.', 0);

    // Compare SRAM
    for (int i = 0; i < 8; i++) {
        ui_step_work_indicator();
        outportb(IO_BANK_RAM, i);
        uint16_t ofs = 0;
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 256; k++, ofs++) {
                if (test_save_read_write_error(x, y, i | 0x10, ofs, j + k - i)) return false;
            }
        }
    }

    ui_bg_putc(x + 3, y, '.', 0);

    // Write test pattern to SRAM
    for (int i = 0; i < 8; i++) {
        ui_step_work_indicator();
        outportb(IO_BANK_RAM, i);
        volatile uint8_t __far *ptr = MK_FP(0x1000, 0x0000);
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 256; k++, ptr++) {
                *ptr = (j + k - i) ^ 0xFF;
            }
        }
    }

    // Compare SRAM
    for (int i = 0; i < 8; i++) {
        ui_step_work_indicator();
        outportb(IO_BANK_RAM, i);
        uint16_t ofs = 0;
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 256; k++, ofs++) {
                if (test_save_read_write_error(x, y, i | 0x20, ofs, (j + k - i) ^ 0xFF)) return false;
            }
        }
    }

    ui_bg_putc(x + 4, y, '.', 0);

    // Read/Write SRAM
    sram_unload();
    ui_bg_putc(x + 5, y, '.', 0);
    sram_switch_to_slot(slot, SRAM_OFFSET_SIZE_DEFAULT);
    ui_bg_putc(x + 6, y, '.', 0);

    // Compare SRAM
    for (int i = 0; i < 8; i++) {
        ui_step_work_indicator();
        outportb(IO_BANK_RAM, i);
        uint16_t ofs = 0;
        for (int j = 0; j < 256; j++) {
            for (int k = 0; k < 256; k++, ofs++) {
                if (test_save_read_write_error(x, y, i | 0x30, ofs, (j + k - i) ^ 0xFF)) return false;
            }
        }
    }

    ui_bg_putc(x + 7, y, UI_GLYPH_CHECK, 0);

    // Erase SRAM slot
    sram_erase(slot, SRAM_OFFSET_SIZE_DEFAULT);

    settings_local.active_sram_slot = 0xFF;
    outportb(IO_BANK_RAM, 0);
    sram_ui_quiet = false;

    ui_clear_work_indicator();
    ui_update_indicators();

    return true;
}
#endif
