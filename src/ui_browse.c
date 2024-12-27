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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ws.h>
#include "config.h"
#include "driver.h"
#include "input.h"
#include "lang.h"
#include "settings.h"
#include "sram.h"
#include "ui.h"
#include "util.h"
#include "ww.h"

#ifdef USE_SLOT_SYSTEM

#define BROWSE_SUB_LAUNCH 0
#define BROWSE_SUB_INFO 1
#define BROWSE_SUB_RENAME 2
#define BROWSE_SUB_INSTALL_WW 3
#define BROWSE_SUB_MANAGE_WW 4

#define WW_MANAGE_SUB_UPDATE_FULL 0
#define WW_MANAGE_SUB_UPDATE_OS 1
#define WW_MANAGE_SUB_UPDATE_BIOS 2
#define WW_MANAGE_SUB_ERASE_FS 3

// in Mbits
static const uint8_t __far rom_size_table[] = {
    1, 2, 4, 8, 16, 24, 32, 48, 62, 128
};

#define CART_METADATA_SIZE 6
typedef enum {
    CART_TYPE_NORMAL = 0,
    CART_TYPE_WW_ATHENABIOS = 1,
    CART_TYPE_EMPTY = 2
} cart_type_t;
typedef struct __attribute__((packed)) {
    uint8_t type;
    union {
        struct __attribute__((packed))  {
            uint8_t major;
            uint8_t minor;
            uint8_t patch;
        } ww_athenabios;
        struct __attribute__((packed))  {
            uint8_t id;
            uint8_t version;
            uint16_t checksum;
            uint8_t publisher;
        } normal;
    };
} cart_metadata_t;
_Static_assert(sizeof(cart_metadata_t) == CART_METADATA_SIZE, "cart_metadata_t size error");

typedef struct __attribute__((packed)) {
    union {
        struct __attribute__((packed)) {
            uint32_t magic;
            uint8_t major;
            uint8_t minor;
            uint8_t patch;
        } ww_athenabios;
        uint8_t ffe0[16];
    };
    union {
        struct __attribute__((packed))  {
            uint8_t entrypoint[5];
            uint8_t maintenance;
            uint8_t publisher_id;
            uint8_t color;
            uint8_t id;
            uint8_t version;
            uint8_t rom_size;
            uint8_t save_type;
            uint8_t flags;
            uint8_t mapper;
            uint16_t checksum;
        };
        uint8_t fff0[16];
    };
} cart_header_t;
_Static_assert(sizeof(cart_header_t) == 32, "cart_header_t size error");

#define CART_METADATA_GET(tbl, id) ((cart_metadata_t*) (((uint8_t*) (tbl)) + ((id) * CART_METADATA_SIZE)))
#define ENTRY_ID_MAX 128

static void ui_browse_menu_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    char buf_name[28];
    cart_metadata_t *cart_metadata = CART_METADATA_GET(userdata, entry_id);

    if (entry_id < ENTRY_ID_MAX) {
        if (entry_id < GAME_SLOTS && settings_local.slot_name[entry_id][0] >= 0x20) {
            _nmemcpy(buf_name, settings_local.slot_name[entry_id] + 1, 23);
            buf_name[23] = 0;
        } else if (settings_local.flags1 & SETT_FLAGS1_HIDE_SLOT_IDS) {
            buf_name[0] = 0;
        } else if (cart_metadata->type == CART_TYPE_NORMAL) {
            snprintf(buf_name, sizeof(buf_name), lang_keys[LK_UI_BROWSE_SLOT_DEFAULT_NAME],
                (uint16_t) cart_metadata->normal.publisher,
                (uint16_t) cart_metadata->normal.id,
                (uint16_t) cart_metadata->normal.version,
                (uint16_t) cart_metadata->normal.checksum
            );
        } else if (cart_metadata->type == CART_TYPE_WW_ATHENABIOS) {
            snprintf(buf_name, sizeof(buf_name), lang_keys[LK_UI_BROWSE_SLOT_DEFAULT_WW_ATHENABIOS_NAME],
                (uint16_t) cart_metadata->ww_athenabios.major,
                (uint16_t) cart_metadata->ww_athenabios.minor,
                (uint16_t) cart_metadata->ww_athenabios.patch);
        } else if (cart_metadata->type == CART_TYPE_EMPTY) {
            buf_name[0] = 0;
        }
        uint8_t sub_slot = entry_id >> 4;
        entry_id &= 0xF;
        if (settings_local.slot_type[entry_id] == SLOT_TYPE_8M_2M) sub_slot >>= 2;
        snprintf(buf, buf_len, lang_keys[LK_UI_BROWSE_SLOT], (uint16_t) (entry_id + 1), sub_slot == 0 ? ' ' : ('A' + sub_slot - 1), ((const char __far*) buf_name));
    }
}

static uint16_t __far browse_sub_lks[] = {
    LK_UI_BROWSE_POPUP_LAUNCH,
    LK_UI_BROWSE_POPUP_INFO,
    LK_UI_BROWSE_POPUP_RENAME,
    LK_UI_BROWSE_POPUP_INSTALL_WW,
    LK_UI_BROWSE_POPUP_MANAGE
};

static void ui_browse_submenu_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len) {
    strncpy(buf, lang_keys[browse_sub_lks[entry_id]], buf_len);
}

static uint16_t __far ww_manage_sub_lks[] = {
    LK_UI_WW_UPDATE_FULL,
    LK_UI_WW_UPDATE_OS,
    LK_UI_WW_UPDATE_BIOS,
    LK_UI_WW_ERASE_FS
};

static void ui_ww_manage_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    strncpy(buf, lang_keys[ww_manage_sub_lks[entry_id]], buf_len);
}

static void ui_browse_save_select_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    snprintf(buf, buf_len, lang_keys[LK_UI_BROWSE_USE_SRAM], entry_id + 'A');
}

static bool ui_read_rom_header(cart_header_t *buffer, uint8_t slot, uint8_t bank) {
    return driver_read_slot(buffer, slot, bank, 0xFFE0, 32);
}

static inline bool ui_read_rom_header_from_entry(cart_header_t *buffer, uint8_t entry_id) {
    return ui_read_rom_header(buffer, entry_id & 0x0F, 0xFF - (entry_id & 0xF0));
}

static bool is_valid_rom_header(cart_header_t *buffer) {
    // is the first byte a valid jump call?
    if (buffer->entrypoint[0] == 0xEA || buffer->entrypoint[0] == 0x9A) {
        // are maintenance bits not set?
        if (!(buffer->maintenance & 0x0F)) {
            // is the target plausibly outside of internal RAM?
            uint16_t dest_high = *((uint16_t*) (buffer->entrypoint + 3));
            if (dest_high != 0xFFFF && dest_high != 0x0000) {
                return true;
            }
        }
    }
    return false;
}

static uint8_t iterate_carts(uint8_t *menu_list, uint8_t *cart_metadata_tbl, uint8_t i) {
    cart_header_t header;

    ui_step_work_indicator();
    driver_unlock();
    for (uint8_t slot = 0; slot < GAME_SLOTS; slot++) {
        ui_step_work_indicator();
        if (settings_local.slot_type[slot] == SLOT_TYPE_UNUSED) {
            continue;
        }

        if (driver_get_launch_slot() == slot) {
            // if booted from RAM/SRAM, don't skip launch slot
            if (_CS >= 0x2000) {
                continue;
            }
        }

        int16_t bank = 0xFF;
        while (bank >= 0x80) {
            uint8_t entry_id = slot | ((bank & 0xF0) ^ 0xF0);
            cart_metadata_t *cart_metadata = CART_METADATA_GET(cart_metadata_tbl, entry_id);
            cart_metadata->type = CART_TYPE_EMPTY;

            _nmemset(&header, 0xFF, sizeof(header));
            if (ui_read_rom_header(&header, slot, bank)) {
                if (is_valid_rom_header(&header)) {
                    if (header.ww_athenabios.magic == WW_ATHENABIOS_MAGIC) {
                        cart_metadata->type = CART_TYPE_WW_ATHENABIOS;
                        cart_metadata->ww_athenabios.major = header.ww_athenabios.major;
                        cart_metadata->ww_athenabios.minor = header.ww_athenabios.minor;
                        cart_metadata->ww_athenabios.patch = header.ww_athenabios.patch;
                    } else {
                        cart_metadata->type = CART_TYPE_NORMAL;
                        cart_metadata->normal.id = header.id;
                        cart_metadata->normal.version = header.version;
                        cart_metadata->normal.checksum = header.checksum;
                        cart_metadata->normal.publisher = header.publisher_id;
                    }
                }
            }

            if (cart_metadata->type != CART_TYPE_EMPTY || !(settings_local.flags1 & SETT_FLAGS1_HIDE_EMPTY_SLOTS)) {
                menu_list[i++] = entry_id;
            }

            uint8_t min_size_banks = 128;
            if (settings_local.slot_type[slot] == SLOT_TYPE_8M_2M  ) min_size_banks = 64;
            if (settings_local.slot_type[slot] == SLOT_TYPE_8M_512K) min_size_banks = 16;

            uint16_t size_banks = 0;
            if (cart_metadata->type != CART_TYPE_EMPTY && header.rom_size < sizeof(rom_size_table)) {
                size_banks = ((uint16_t) rom_size_table[header.rom_size]) * 2;
            }
            if (size_banks < min_size_banks) size_banks = min_size_banks;
            bank -= size_banks;
        }
    }
    driver_lock();

    return i;
}

void ui_browse_info(uint8_t slot) {
    char buf[32], buf2[24];
    cart_header_t rom_header;

    driver_unlock();
    ui_reset_main_screen();
    ui_read_rom_header_from_entry(&rom_header, slot);
    driver_lock();
    input_wait_clear();

    snprintf(buf, sizeof(buf), lang_keys[LK_UI_BROWSE_INFO_ID_VAL],
        (uint16_t) rom_header.publisher_id,
        (uint16_t) rom_header.id,
        (uint16_t) rom_header.version);
    ui_bg_printf(0, 1, 0, lang_keys[LK_UI_BROWSE_INFO_ID], (const char __far*) buf);

    if (rom_header.rom_size >= sizeof(rom_size_table)) {
        strncpy(buf, lang_keys[LK_UI_BROWSE_INFO_UNKNOWN], sizeof(buf));
    } else {
        snprintf(buf, sizeof(buf), lang_keys[LK_UI_BROWSE_INFO_ROM_SIZE_MBIT],
            (uint16_t) rom_size_table[rom_header.rom_size]);
    }

    ui_bg_printf(0, 2, 0, lang_keys[LK_UI_BROWSE_INFO_ROM_SIZE], (const char __far*) buf);

    ui_puts(false, 0, 3, 0, lang_keys[LK_UI_BROWSE_INFO_SAVE_TYPE]);
    uint8_t save_str_x = 1 + strlen(lang_keys[LK_UI_BROWSE_INFO_SAVE_TYPE]);
    uint8_t save_str_y = 3;
    if (rom_header.save_type == 0x00) {
        ui_puts(false, save_str_x, save_str_y, 0, lang_keys[LK_UI_BROWSE_INFO_NONE]);
    } else {
        if (rom_header.save_type & 0x0F) {
            uint16_t kbit = 0;
            switch (rom_header.save_type & 0x0F) {
            case 0x1: kbit = 64; break;
            case 0x2: kbit = 256; break;
            case 0x3: kbit = 1024; break;
            case 0x4: kbit = 2048; break;
            case 0x5: kbit = 4096; break;
            }
            if (kbit == 0) {
                strncpy(buf, lang_keys[LK_UI_BROWSE_INFO_UNKNOWN], sizeof(buf));
            } else {
                snprintf(buf, sizeof(buf), lang_keys[LK_UI_BROWSE_INFO_DECIMAL], kbit);
            }
            ui_bg_printf(save_str_x, save_str_y++, 0, lang_keys[LK_UI_BROWSE_INFO_SAVE_TYPE_SRAM], (const char __far*) buf);
        }
        if (rom_header.save_type & 0xF0) {
            uint16_t kbit = 0;
            switch (rom_header.save_type >> 4) {
            case 0x1: kbit = 1; break;
            case 0x2: kbit = 16; break;
            case 0x5: kbit = 8; break;
            }
            if (kbit == 0) {
                strncpy(buf, lang_keys[LK_UI_BROWSE_INFO_UNKNOWN], sizeof(buf));
            } else {
                snprintf(buf, sizeof(buf), lang_keys[LK_UI_BROWSE_INFO_DECIMAL], kbit);
            }
            ui_bg_printf(save_str_x, save_str_y++, 0, lang_keys[LK_UI_BROWSE_INFO_SAVE_TYPE_EEPROM], (const char __far*) buf);
        }
    }

    ui_bg_printf(0, 5, 0, lang_keys[LK_UI_BROWSE_INFO_COLOR], (const char __far*) lang_keys[
        rom_header.color > 1 ? LK_UI_BROWSE_INFO_UNKNOWN : (rom_header.color ? LK_CONFIG_YES : LK_CONFIG_NO)
    ]);
    ui_bg_printf(0, 6, 0, lang_keys[LK_UI_BROWSE_INFO_ORIENTATION], (const char __far*) lang_keys[
        rom_header.flags & 0x01 ? LK_UI_BROWSE_INFO_VERTICAL : LK_UI_BROWSE_INFO_HORIZONTAL
    ]);

    ui_bg_printf(0, 8, 0, lang_keys[LK_UI_BROWSE_INFO_MAPPER], (const char __far*) lang_keys[
        rom_header.mapper > 1 ? LK_UI_BROWSE_INFO_UNKNOWN : (rom_header.mapper ? LK_UI_BROWSE_INFO_MAPPER_2003 : LK_UI_BROWSE_INFO_MAPPER_2001)
    ]);
    ui_bg_printf(0, 9, 0, lang_keys[LK_UI_BROWSE_INFO_EEPROM], (const char __far*) lang_keys[
        rom_header.version & 0x80 ? LK_CONFIG_YES : LK_CONFIG_NO
    ]);

    ui_puts(false, 0, 11, 0, lang_keys[(rom_header.flags & 0x04) ? LK_UI_BROWSE_INFO_ROM_SPEED_1 : LK_UI_BROWSE_INFO_ROM_SPEED_3]);
    ui_bg_printf(0, 12, 0, lang_keys[LK_UI_BROWSE_INFO_ROM_BUS_SIZE], (uint16_t) ((rom_header.flags & 0x02) ? 8 : 16));

    ui_bg_printf(0, 14, 0, lang_keys[LK_UI_BROWSE_INFO_CHECKSUM], (uint16_t) rom_header.checksum);

    while (ui_poll_events()) {
        wait_for_vblank();
        if (input_pressed & (KEY_A | KEY_B)) return;
    }
}

__attribute__((noinline))
static uint8_t ui_browse_inner(uint8_t *entry_id) {
    uint8_t menu_list[256];
    uint8_t cart_metadata[ENTRY_ID_MAX * CART_METADATA_SIZE];
    uint8_t i = 0;

    i = iterate_carts(menu_list, cart_metadata, i);
    menu_list[i++] = MENU_ENTRY_END;

    ui_menu_state_t menu = {
        .list = menu_list,
        .build_line_func = ui_browse_menu_build_line,
        .build_line_data = cart_metadata,
        .flags = MENU_B_AS_ACTION
    };
    ui_menu_init(&menu);

    uint16_t result = ui_menu_select(&menu);
    uint16_t subaction = 0;
    if ((result & 0xFF) < ENTRY_ID_MAX) {
        *entry_id = result;
        uint8_t slot_type = settings_local.slot_type[result & 0x0F];
        bool is_ww = CART_METADATA_GET(cart_metadata, result & 0xFF)->type == CART_TYPE_WW_ATHENABIOS;
        if ((result & 0xFF00) == MENU_ACTION_B) {
            ui_popup_menu_state_t popup_menu = {
                .list = menu_list,
                .build_line_func = ui_browse_submenu_build_line,
                .flags = 0
            };
            i = 0;
            menu_list[i++] = BROWSE_SUB_LAUNCH;
            if (is_ww) menu_list[i++] = BROWSE_SUB_MANAGE_WW;
            menu_list[i++] = BROWSE_SUB_INFO;
            menu_list[i++] = BROWSE_SUB_RENAME;
            if (!is_ww && (slot_type == SLOT_TYPE_SOFT || slot_type == SLOT_TYPE_8M_2M)) menu_list[i++] = BROWSE_SUB_INSTALL_WW;
            menu_list[i++] = MENU_ENTRY_END;
            subaction = ui_popup_menu_run(&popup_menu);
        }
        result &= 0xFF;

        if (subaction == BROWSE_SUB_LAUNCH) {
            ui_reset_main_screen();

            cart_header_t *header = (cart_header_t*) (menu_list + 128);
            _nmemset(menu_list, 0xFF, 16);
            ui_read_rom_header_from_entry(header, result);

            // does the game use save data?
            if (header->save_type != 0 && _CS >= 0x2000) {
                // figure out SRAM slots
                i = 0;
                for (uint8_t k = 0; k < SRAM_SLOTS; k++) {
                    if (settings_local.sram_slot_mapping[k] == (result & 0x0F)) {
                        menu_list[i++] = k;
                    }
                }
                uint8_t sram_slot = 0xFF;
                if (!is_ww && i > 1) {
                    menu_list[i++] = MENU_ENTRY_END;
                    menu.build_line_func = ui_browse_save_select_build_line;
                    menu.flags = MENU_B_AS_BACK;
                    ui_menu_init(&menu);
                    uint16_t result_sram = ui_menu_select(&menu);
                    if (result_sram == MENU_ENTRY_END) {
                        return 0;
                    } else {
                        sram_slot = menu_list[result_sram & 0xFF];
                    }
                } else if (i > 0) {
                    sram_slot = menu_list[0];
                }

                uint8_t offset_size = SRAM_OFFSET_SIZE_DEFAULT;
                if (slot_type == SLOT_TYPE_8M_2M  ) offset_size = SRAM_OFFSET_SIZE((result >= 0x10) ? 4 : 0, 4);
                if (slot_type == SLOT_TYPE_8M_512K) offset_size = SRAM_OFFSET_SIZE((result >> 4) & 7, 1);
                
                sram_switch_to_slot(sram_slot, offset_size);
            } else {
                if (settings_local.active_sram_slot == SRAM_SLOT_FIRST_BOOT) {
                    settings_local.active_sram_slot = SRAM_SLOT_NONE;
                    settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
                    settings_mark_changed();
                }
            }

            // does the game leave IEEPROM unlocked?
            if (!(header->version & 0x80)) {
                // lock IEEPROM
                outportw(IO_IEEP_CTRL, IEEP_PROTECT);
            }

            input_wait_clear();
            launch_slot(result & 0x0F, 0xFF - (result & 0xF0));
        } else if (subaction == BROWSE_SUB_INFO) {
            ui_browse_info(result);
        } else if (subaction == BROWSE_SUB_RENAME) {
            if (result < GAME_SLOTS) {
                if (settings_local.slot_name[result][0] < 0x20) {
                    menu_list[0] = 0;
                } else {
                    _nmemcpy(menu_list, settings_local.slot_name[result] + 1, 23);
                }
                if (ui_osk_run(0, (char*) menu_list, 23)) {
                    _nmemset(settings_local.slot_name[result], 0, 24);
                    if (menu_list[0] != 0) {
                        settings_local.slot_name[result][0] = 0x20;
                        strncpy((char*) (settings_local.slot_name[result] + 1), (char*) menu_list, 23);
                    }
                    settings_mark_changed();
                }
            }
        } else if (subaction == BROWSE_SUB_INSTALL_WW || subaction == BROWSE_SUB_MANAGE_WW) {
            return subaction;
        }
    }

    return 0;
}

void ui_browse(void) {
    // ui_browse_inner() uses ~0.8KB of stack, so it's separated out
    // keeping in mind other functions which require a lot of stack
    uint8_t entry_id;
    uint8_t subaction = ui_browse_inner(&entry_id);
    uint8_t slot = entry_id & 0x0F;
    uint8_t bank = 0xF0 - (entry_id & 0xF0);
    uint8_t menu_list[8];

    if (subaction == BROWSE_SUB_INSTALL_WW) {
        if (ui_dialog_run(0, 1, LK_DIALOG_WW_INSTALL, LK_DIALOG_YES_NO) == 0) {
            ww_ui_erase_userdata(slot, bank);
            ww_ui_install_full(slot, bank);
        }
    } else if (subaction == BROWSE_SUB_MANAGE_WW) {
        ui_reset_main_screen();
        ui_menu_state_t menu = {
            .list = menu_list,
            .build_line_func = ui_ww_manage_build_line,
            .build_line_data = menu_list,
            .flags = MENU_B_AS_BACK
        };
        int i = 0;
        menu_list[i++] = WW_MANAGE_SUB_UPDATE_FULL;
        menu_list[i++] = WW_MANAGE_SUB_UPDATE_OS;
        menu_list[i++] = WW_MANAGE_SUB_UPDATE_BIOS;
        menu_list[i++] = WW_MANAGE_SUB_ERASE_FS;
        menu_list[i++] = MENU_ENTRY_END;
        ui_menu_init(&menu);
        uint16_t result = ui_menu_select(&menu);
        if (result == WW_MANAGE_SUB_UPDATE_FULL) {
            ww_ui_install_full(slot, bank);
        } else if (result == WW_MANAGE_SUB_UPDATE_BIOS) {
            ww_ui_install_bios(slot, bank);
        } else if (result == WW_MANAGE_SUB_UPDATE_OS) {
            ww_ui_install_os(slot, bank);
        } else if (result == WW_MANAGE_SUB_ERASE_FS) {
            if (ui_dialog_run(0, 1, LK_DIALOG_CONFIRM, LK_DIALOG_YES_NO) == 0) {
                ww_ui_erase_userdata(slot, bank);
            }
        }
    }
}

#endif
