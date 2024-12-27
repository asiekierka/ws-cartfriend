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
#include <ws.h>
#include "config.h"
#include "driver.h"
#include "error.h"
#include "lang.h"
#include "settings.h"
#include "sram.h"
#include "ui.h"
#include "util.h"

// Saving a 1KB settings block across 127 slots allows for wear leveling.
// Given that one block of WSFM flash is rated for 100,000 erases, this should give >12 million
// settings changes over the lifespan of the device.
// (I'd have preferred an SD card slot, but you gotta work with what you gotta work with.)

#define SETTINGS_BANK 0xF4
#define LEGACY_SETTINGS_BANK 0xF8

uint8_t settings_slot;
settings_t settings_local;
bool settings_changed;
bool settings_location_legacy;
bool settings_first_boot = false;
const char __far settings_magic[4] = {'w', 'f', 'C', 'F'};

void settings_reset(void) {
    _nmemset(((uint8_t*) &settings_local) + sizeof(settings_magic), 0, sizeof(settings_local) - sizeof(settings_magic));
    memcpy(settings_local.magic, settings_magic, sizeof(settings_magic));
    settings_local.version = SETTINGS_VERSION;

    // game/SRAM slots
    uint8_t sram_slot = 0;
    for (uint8_t i = 0; i < GAME_SLOTS; i++) {
        if (i == driver_get_launch_slot()) {
            settings_local.slot_type[i] = SLOT_TYPE_LAUNCHER;
        } else {
            settings_local.slot_type[i] = SLOT_TYPE_SOFT;
            if (sram_slot < GAME_SLOTS) {
                settings_local.sram_slot_mapping[sram_slot++] = i;
            }
        }
        settings_local.slot_name[i][0] = 0;
        settings_local.slot_name[i][1] = 0;
    }
    while (sram_slot < SRAM_SLOTS) {
        settings_local.sram_slot_mapping[sram_slot++] = 0xFF;
    }
    settings_local.active_sram_slot = SRAM_SLOT_FIRST_BOOT;
    settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
    settings_local.color_theme = 0x02;

    settings_slot = 127;
    settings_changed = true;
}

static inline uint16_t settings_calculate_crc(void) {
    return crc16((const char*) &settings_local, sizeof(settings_local), 1022);
}

static void settings_migrate(void) {
    if (settings_local.version > SETTINGS_VERSION) {
        ui_show();
        ui_dialog_run(0, 0, LK_DIALOG_SETTINGS_TOO_NEW, LK_DIALOG_OK);
        settings_reset();
        return;
    }

    if (settings_local.version < 1) {
        settings_local.color_theme = 0;
    }
    if (settings_local.version < 2) {
        for (uint8_t i = 0; i < GAME_SLOTS; i++) {
            settings_local.slot_name[i][0] = 0;
            settings_local.slot_name[i][1] = 0;
        }
    }

    if (settings_local.version < 3) {
        settings_local.flags1 = 0;
    }

    if (settings_local.version < 5) {
        settings_local.language = 0;
    }

    if (settings_local.version < 6) {
        settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
    }

    settings_local.version = SETTINGS_VERSION;
}

static bool try_settings_load(uint8_t settings_bank, uint8_t slot_start, uint8_t slot_end) {
    if (driver_get_launch_slot() != 0xFF) {
        settings_slot = slot_end;
        while (true) {
            uint8_t bank = settings_bank + (settings_slot >> 6);
            uint16_t offset = settings_slot << 10;
            _nmemset(&settings_local, 0, 6);
            driver_read_slot(&settings_local, driver_get_launch_slot(), bank, offset, 6);

            if (!memcmp(settings_magic, &settings_local, 4)) {
                bool read_ok = driver_read_slot(((uint8_t*) &settings_local) + 6, driver_get_launch_slot(), bank, offset + 6, sizeof(settings_local) - 6);
                uint16_t settings_crc;
                read_ok &= driver_read_slot(&settings_crc, driver_get_launch_slot(), bank, offset + 1022, 2);
                if (read_ok) {
                    uint16_t settings_crc_calculated = settings_calculate_crc();
                    // TODO: check settings CRC
                    return true;
                }
            }
            
            if (settings_slot == slot_start) return false;
            settings_slot--;
        }
    }
    return false;
}

static const uint8_t bootstrap_data[] = {
    // 16 bytes of NOP sled
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    // jmp far e0000:0000
    0xea, 0x00, 0x00, 0x00, 0xe0
};

void settings_erase_slots(void) {
    driver_erase_bank(0, driver_get_launch_slot(), SETTINGS_BANK);
    driver_write_slot(bootstrap_data, driver_get_launch_slot(), SETTINGS_BANK, 0, sizeof(bootstrap_data));
    settings_slot = 1;
}

void settings_load(void) {
    settings_changed = false;
    settings_location_legacy = false;

#ifndef USE_SLOT_SYSTEM
    settings_reset();
    return;
#else
    if (try_settings_load(SETTINGS_BANK, 1, 127)) {
        settings_migrate();
        settings_first_boot = false;
        return;
    }

    // CartFriend <= 0.1.4 stores settings in bank 0xF8
    if (try_settings_load(LEGACY_SETTINGS_BANK, 0, 127)) {
        settings_location_legacy = true;
        settings_migrate();
        
        // init UI
        settings_refresh();
	    ui_set_current_tab(0);
        wait_for_vblank();
        ui_show();
        outportb(IO_LCD_SEG, LCD_SEG_ORIENT_H);
		ui_reset_main_screen();

        // load bank 14 to SRAM -> set settings location to new -> unload bank 14 from SRAM
        sram_switch_to_slot(14, SRAM_OFFSET_SIZE_DEFAULT);
        settings_location_legacy = false;
        sram_unload();

        // force new slot write
        settings_slot = 127;
        settings_changed = true;
        settings_save();
        settings_first_boot = false;
        return;
    }

    // New install.
    settings_reset();
    settings_first_boot = true;
#endif
}


void settings_refresh(void) {
	ui_update_theme(settings_local.color_theme);
    ui_set_current_tab(ui_current_tab);
}

void settings_mark_changed(void) {
    settings_changed = true;
    ui_update_indicators();
}

void settings_save(void) {
#ifdef USE_SLOT_SYSTEM
    if (!settings_changed) return;
    if (driver_get_launch_slot() == 0xFF) return;

    ui_step_work_indicator();

    if (settings_slot >= 127) {
        settings_erase_slots();
    } else {
        settings_slot++;
    }

    uint8_t active_sram_slot = settings_local.active_sram_slot;
    uint8_t active_sram_offset_size = settings_local.active_sram_offset_size;
    if (active_sram_slot == SRAM_SLOT_FIRST_BOOT) {
        settings_local.active_sram_slot = SRAM_SLOT_NONE;
        settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
    }

    uint8_t bank = SETTINGS_BANK + (settings_slot >> 6);
    uint16_t offset = settings_slot << 10;
    // write settings data
    driver_write_slot(&settings_local, driver_get_launch_slot(), bank, offset, sizeof(settings_local));
    // write settings CRC
    uint16_t settings_crc = settings_calculate_crc();
    driver_write_slot(&settings_crc, driver_get_launch_slot(), bank, offset + 1022, 2);

    settings_local.active_sram_slot = active_sram_slot;
    settings_local.active_sram_offset_size = active_sram_offset_size;

    ui_clear_work_indicator();
    settings_changed = false;
    ui_update_indicators();
#endif
}
