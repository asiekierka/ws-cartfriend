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

#include <stdio.h>
#include <string.h>
#include <ws.h>
#include "config.h"
#include "driver.h"
#include "input.h"
#include "lang.h"
#include "settings.h"
#include "sram.h"
#include "tests.h"
#include "ui.h"
#include "util.h"

typedef enum {
    MENU_OPT_SAVEMAP,
    MENU_OPT_SAVE,
    MENU_OPT_SAVE_MANAGEMENT,
    MENU_OPT_THEME,
    MENU_OPT_SLOTMAP,
    MENU_OPT_UNLOAD_SRAM,
    MENU_OPT_HIDE_SLOT_IDS,
    MENU_OPT_HIDE_EMPTY_SLOTS,
    MENU_OPT_REVERTSETTINGS,
    MENU_OPT_FACTORYRESETSETTINGS,
    MENU_OPT_ADVANCED,
    MENU_OPT_LANGUAGE,
    MENU_OPT_TEXT_WIDTH
} ui_opt_id_t;

static uint16_t __far ui_lang_lks[] = {
    LK_LANG_EN,
    LK_LANG_PL,
    LK_LANG_DE,
    LK_LANG_FR
};

static uint16_t __far ui_opt_lks[] = {
    LK_UI_SETTINGS_SAVEMAP,
    LK_UI_SETTINGS_SAVE,
    LK_UI_SETTINGS_SAVE_MANAGEMENT,
    LK_UI_SETTINGS_THEME,
    LK_UI_SETTINGS_SLOTMAP,
    LK_UI_SETTINGS_UNLOAD_SRAM,
    LK_UI_SETTINGS_HIDE_SLOT_IDS,
    LK_UI_SETTINGS_HIDE_EMPTY_SLOTS,
    LK_UI_SETTINGS_REVERT,
    LK_UI_SETTINGS_FACTORY_RESET,
    LK_UI_SETTINGS_ADVANCED,
    LK_UI_SETTINGS_LANGUAGE,
    LK_UI_SETTINGS_TEXT_WIDTH
};

static uint16_t __far ui_theme_color_lks[] = {
    LK_THEME_C0,
    LK_THEME_C1,
    LK_THEME_C2
};

static void ui_opt_menu_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    if (entry_id == MENU_OPT_SAVE) {
        snprintf(buf, buf_len, lang_keys[settings_changed ? LK_MENU_MARKED : LK_MENU_UNMARKED], lang_keys[ui_opt_lks[entry_id]]);
    } else {
        strncpy(buf, lang_keys[ui_opt_lks[entry_id]], buf_len);
        if (entry_id == MENU_OPT_HIDE_SLOT_IDS) {
            bool yes = settings_local.flags1 & SETT_FLAGS1_HIDE_SLOT_IDS;
            strncpy(buf_right, lang_keys[yes ? LK_CONFIG_YES : LK_CONFIG_NO], buf_right_len);
        } else if (entry_id == MENU_OPT_HIDE_EMPTY_SLOTS) {
            bool yes = settings_local.flags1 & SETT_FLAGS1_HIDE_EMPTY_SLOTS;
            strncpy(buf_right, lang_keys[yes ? LK_CONFIG_YES : LK_CONFIG_NO], buf_right_len);
        } else if (entry_id == MENU_OPT_THEME) {
            if (ws_system_color_active()) {
                strncpy(buf_right, lang_keys[ui_theme_color_lks[settings_local.color_theme & 0x7F]], buf_right_len);
            } else {
                strncpy(buf_right, lang_keys[(settings_local.color_theme & 0x80) ? LK_THEME_M1 : LK_THEME_M0], buf_right_len);
            }
        } else if (entry_id == MENU_OPT_LANGUAGE) {
            strncpy(buf_right, lang_keys[ui_lang_lks[settings_local.language]], buf_right_len);
        } else if (entry_id == MENU_OPT_TEXT_WIDTH) {
            strncpy(buf_right, lang_keys[(settings_local.flags1 & SETT_FLAGS1_WIDE_SCREEN) ? LK_WIDTH_WIDE : LK_WIDTH_NARROW], buf_right_len);
        }

        if (entry_id == MENU_OPT_SLOTMAP || entry_id == MENU_OPT_SAVEMAP || entry_id == MENU_OPT_SAVE_MANAGEMENT || entry_id == MENU_OPT_ADVANCED) {
            buf_right[0] = '>';
            buf_right[1] = '>';
            buf_right[2] = 0;
        }
    }
}

typedef enum {
    MENU_ADV_FORCECARTSRAM,
    MENU_ADV_BUFFERED_WRITES,
    MENU_ADV_UNLOCK_IEEP,
    MENU_ADV_SERIAL_RATE,
    MENU_ADV_FORCE_FAST_SRAM
} ui_adv_id_t;

static uint16_t __far ui_adv_lks[] = {
    LK_UI_SETTINGS_FORCECARTSRAM,
    LK_UI_SETTINGS_BUFFERED_WRITES,
    LK_UI_SETTINGS_UNLOCK_IEEP,
    LK_UI_SETTINGS_SERIAL_RATE,
    LK_UI_SETTINGS_FORCE_FAST_SRAM,
};

static void build_line_yesno(bool yes, char *buf_right, int buf_right_len) {
    strncpy(buf_right, lang_keys[yes ? LK_CONFIG_YES : LK_CONFIG_NO], buf_right_len);
}

static void ui_adv_menu_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    strncpy(buf, lang_keys[ui_adv_lks[entry_id]], buf_len);
    if (entry_id == MENU_ADV_FORCECARTSRAM) {
        build_line_yesno(settings_local.active_sram_slot == SRAM_SLOT_FIRST_BOOT, buf_right, buf_right_len);
    } else if (entry_id == MENU_ADV_BUFFERED_WRITES) {
        build_line_yesno(!(settings_local.flags1 & SETT_FLAGS1_DISABLE_BUFFERED_WRITES), buf_right, buf_right_len);
    } else if (entry_id == MENU_ADV_UNLOCK_IEEP) {
        build_line_yesno(settings_local.flags1 & SETT_FLAGS1_UNLOCK_IEEP_NEXT_BOOT, buf_right, buf_right_len);
    } else if (entry_id == MENU_ADV_SERIAL_RATE) {
        bool is9600 = settings_local.flags1 & SETT_FLAGS1_SERIAL_9600BPS;
        strncpy(buf_right, lang_keys[is9600 ? LK_UI_SETTINGS_SERIAL_RATE_9600 : LK_UI_SETTINGS_SERIAL_RATE_38400], buf_right_len);
    } else if (entry_id == MENU_ADV_FORCE_FAST_SRAM) {
        build_line_yesno(settings_local.flags1 & SETT_FLAGS1_FORCE_FAST_SRAM, buf_right, buf_right_len);
    }
}

static void ui_opt_menu_savemap_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    if (entry_id < SRAM_SLOTS) {
        snprintf(buf, buf_len, lang_keys[entry_id == settings_local.active_sram_slot ? LK_UI_SAVEMAP_SRAM_ACTIVE : LK_UI_SAVEMAP_SRAM], entry_id + 'A');
        uint8_t sram_target = settings_local.sram_slot_mapping[entry_id];
        if (sram_target < GAME_SLOTS) {
            snprintf(buf_right, buf_right_len, lang_keys[LK_UI_SAVEMAP_SLOT], sram_target + 1);
        } else if (sram_target == 0xFF) {
            strncpy(buf_right, lang_keys[LK_UI_SAVEMAP_UNUSED], buf_right_len);
        }
    }
}

static uint8_t ui_savemap_next(uint8_t slot) {
    // 0xFF -> 0
    for (uint8_t i = slot + 1; i < GAME_SLOTS; i++) {
        if (settings_local.slot_type[i] != SLOT_TYPE_LAUNCHER) {
            return i;
        }
    }
    return 0xFF;
}

static uint8_t ui_savemap_prev(uint8_t slot) {
    for (uint8_t i = slot == 0xFF ? GAME_SLOTS : (slot - 1); i != 0xFF; i--) {
        if (settings_local.slot_type[i] != SLOT_TYPE_LAUNCHER) {
            return i;
        }
    }
    return 0xFF;
}

static const uint16_t __far slotmap_lks[] = {
    LK_UI_SLOTMAP_SOFT,
    LK_UI_SLOTMAP_LAUNCHER,
    LK_UI_SLOTMAP_8MB_512KB,
    LK_UI_SLOTMAP_8MB_2MB
};

static void ui_opt_menu_slotmap_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    if (entry_id < GAME_SLOTS) {
        snprintf(buf, buf_len, lang_keys[LK_UI_SLOTMAP_SLOT], entry_id + 1);
        uint8_t slot_type = settings_local.slot_type[entry_id];
        if (slot_type <= SLOT_TYPE_8M_2M) {
            strncpy(buf_right, lang_keys[slotmap_lks[slot_type]], buf_right_len);
        } else if (slot_type == SLOT_TYPE_UNUSED) {
            strncpy(buf_right, lang_keys[LK_UI_SLOTMAP_UNUSED], buf_right_len);
        }
    }
}

static const uint8_t __far slotmap_order[] = {
    SLOT_TYPE_SOFT,
    SLOT_TYPE_8M_512K,
    SLOT_TYPE_8M_2M,
    SLOT_TYPE_UNUSED
};

static uint8_t ui_slotmap_next(uint8_t slot, uint8_t delta) {
    for (uint8_t i = 0; i < sizeof(slotmap_order); i++) {
        if (slotmap_order[i] == slot) {
            return slotmap_order[(i + delta) % sizeof(slotmap_order)];
        }
    }
    return slot;
}

static void ui_opt_menu_erase_sram_build_line(uint8_t entry_id, void *userdata, char *buf, int buf_len, char *buf_right, int buf_right_len) {
    if (entry_id < SRAM_SLOTS) {
        snprintf(buf, buf_len, lang_keys[entry_id == settings_local.active_sram_slot ? LK_UI_ERASE_BLOCK_ACTIVE : LK_UI_ERASE_BLOCK], entry_id + 'A');
    } else if (entry_id == 0xEF) {
        strncpy(buf, lang_keys[LK_UI_ERASE_ALL_SAVE_DATA], buf_len);
    } else if (entry_id == 0xEE) {
        strncpy(buf, lang_keys[LK_UI_ERASE_UNDO_SRAM], buf_len);
    } else if (entry_id == 0xED) {
        strncpy(buf, lang_keys[LK_UI_ERASE_TEST_ALL_SAVE_DATA], buf_len);
    } else if (entry_id == 0xEC) {
        strncpy(buf, lang_keys[LK_UI_ERASE_ALL_SAVE_BLOCKS], buf_len);
    }
}

static void ui_settings_advanced(uint8_t *menu_list) {
    uint8_t i = 0;
    menu_list[i++] = MENU_ADV_FORCE_FAST_SRAM;
    menu_list[i++] = MENU_ADV_BUFFERED_WRITES;
    menu_list[i++] = MENU_ADV_SERIAL_RATE;
    // menu_list[i++] = MENU_ADV_CART_AVR_DELAY;
    menu_list[i++] = MENU_ADV_FORCECARTSRAM;
    menu_list[i++] = MENU_ADV_UNLOCK_IEEP;
    menu_list[i++] = MENU_ENTRY_END;

    ui_menu_state_t menu = {
        .list = menu_list,
        .build_line_func = ui_adv_menu_build_line,
        .flags = MENU_B_AS_BACK
    };
    ui_menu_init(&menu);

Reselect:
    ;
    uint16_t result = ui_menu_select(&menu);

    if (result == MENU_ADV_FORCECARTSRAM) {
        if (settings_local.active_sram_slot != 0xFE) {
            if (ui_dialog_run(0, 1, LK_DIALOG_CONFIRM, LK_DIALOG_YES_NO) == 0) {
                settings_local.active_sram_slot = 0xFE;
                settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
                settings_mark_changed();
            }
        } else {
            goto Reselect;
        }
    } else if (result == MENU_ADV_BUFFERED_WRITES) {
        settings_local.flags1 ^= SETT_FLAGS1_DISABLE_BUFFERED_WRITES;
        settings_mark_changed();
        goto Reselect;
    } else if (result == MENU_ADV_UNLOCK_IEEP) {
        settings_local.flags1 ^= SETT_FLAGS1_UNLOCK_IEEP_NEXT_BOOT;
        settings_mark_changed();
        goto Reselect;
    } else if (result == MENU_ADV_SERIAL_RATE) {
        settings_local.flags1 ^= SETT_FLAGS1_SERIAL_9600BPS;
        settings_mark_changed();
        goto Reselect;
    } else if (result == MENU_ADV_FORCE_FAST_SRAM) {
        settings_local.flags1 ^= SETT_FLAGS1_FORCE_FAST_SRAM;
        settings_mark_changed();
        goto Reselect;
    } /* else if (result == MENU_ADV_CART_AVR_DELAY) {
        settings_local.avr_cart_delay += 5;
        if (settings_local.avr_cart_delay < MINIMUM_AVR_CART_DELAY) {
            settings_local.avr_cart_delay = MINIMUM_AVR_CART_DELAY;
        } else if (settings_local.avr_cart_delay > MAXIMUM_AVR_CART_DELAY) {
            settings_local.avr_cart_delay = MINIMUM_AVR_CART_DELAY;
        }
        settings_mark_changed();
        goto Reselect;
    } */
}

void ui_settings(void) {
    uint8_t menu_list[32];
    uint8_t i = 0;
    menu_list[i++] = MENU_OPT_THEME;
    menu_list[i++] = MENU_OPT_TEXT_WIDTH;
    menu_list[i++] = MENU_OPT_LANGUAGE;
#ifdef USE_SLOT_SYSTEM
    menu_list[i++] = MENU_OPT_HIDE_SLOT_IDS;
    menu_list[i++] = MENU_OPT_HIDE_EMPTY_SLOTS;
#endif
    menu_list[i++] = MENU_ENTRY_DIVIDER;
#ifdef USE_SLOT_SYSTEM
    menu_list[i++] = MENU_OPT_SLOTMAP;
    menu_list[i++] = MENU_OPT_SAVEMAP;
    menu_list[i++] = MENU_OPT_SAVE_MANAGEMENT;
    menu_list[i++] = MENU_OPT_ADVANCED;
    if (settings_local.active_sram_slot < SRAM_SLOTS) {
        menu_list[i++] = MENU_OPT_UNLOAD_SRAM;
    }
    menu_list[i++] = MENU_ENTRY_DIVIDER;
#endif
    menu_list[i++] = MENU_OPT_SAVE;
    if (settings_changed) {
        menu_list[i++] = MENU_OPT_REVERTSETTINGS;
    }
    menu_list[i++] = MENU_OPT_FACTORYRESETSETTINGS;
    menu_list[i++] = MENU_ENTRY_END;

    ui_menu_state_t menu = {
        .list = menu_list,
        .build_line_func = ui_opt_menu_build_line,
        .flags = 0
    };
    ui_menu_init(&menu);
    
Reselect:
    ;
    uint16_t result = ui_menu_select(&menu);
#ifdef USE_SLOT_SYSTEM
    if (result == MENU_OPT_SAVEMAP) {
        i = 0;
        for (uint8_t j = 0; j < SRAM_SLOTS; j++) {
            menu_list[i++] = j;
        }
        menu_list[i] = MENU_ENTRY_END;

        menu.build_line_func = ui_opt_menu_savemap_build_line;
        menu.flags = MENU_SEND_LEFT_RIGHT | MENU_B_AS_BACK;
        ui_menu_init(&menu);
        ui_reset_main_screen();

        while (true) {
            result = ui_menu_select(&menu);
            if (result == MENU_ENTRY_END) {
                break;
            } else if (result & MENU_ACTION_LEFT) {
                settings_local.sram_slot_mapping[result & 0xFF] = ui_savemap_prev(settings_local.sram_slot_mapping[result & 0xFF]);
                settings_mark_changed();
            } else {
                settings_local.sram_slot_mapping[result & 0xFF] = ui_savemap_next(settings_local.sram_slot_mapping[result & 0xFF]);
                settings_mark_changed();
            }
        }
    } else if (result == MENU_OPT_SLOTMAP) {
        i = 0;
        for (uint8_t j = 0; j < GAME_SLOTS; j++) {
            menu_list[i++] = j;
        }
        menu_list[i] = MENU_ENTRY_END;

        menu.build_line_func = ui_opt_menu_slotmap_build_line;
        menu.flags = MENU_SEND_LEFT_RIGHT | MENU_B_AS_BACK;
        ui_menu_init(&menu);
        ui_reset_main_screen();

        while (true) {
            result = ui_menu_select(&menu);
            if (result == MENU_ENTRY_END) {
                break;
            } else if (result & MENU_ACTION_LEFT) {
                settings_local.slot_type[result & 0xFF] = ui_slotmap_next(settings_local.slot_type[result & 0xFF], sizeof(slotmap_order) - 1);
                settings_mark_changed();
            } else {
                settings_local.slot_type[result & 0xFF] = ui_slotmap_next(settings_local.slot_type[result & 0xFF], 1);
                settings_mark_changed();
            }
        }
    } else if (result == MENU_OPT_HIDE_SLOT_IDS) {
        settings_local.flags1 ^= SETT_FLAGS1_HIDE_SLOT_IDS;
        settings_mark_changed();
        goto Reselect;
    } else if (result == MENU_OPT_UNLOAD_SRAM) {
        if (ui_dialog_run(0, 1, LK_DIALOG_CONFIRM, LK_DIALOG_YES_NO) == 0) {
            sram_unload();
            ui_reset_main_screen();
        }
    } else if (result == MENU_OPT_SAVE_MANAGEMENT) {
        i = 0;
        for (uint8_t j = 0; j < SRAM_SLOTS; j++) {
            menu_list[i++] = j;
        }
        menu_list[i++] = 0xEC;
        menu_list[i++] = 0xEF;
        menu_list[i++] = 0xED;
        menu_list[i++] = 0xEE;
        menu_list[i] = MENU_ENTRY_END;

        menu.build_line_func = ui_opt_menu_erase_sram_build_line;
        menu.flags = MENU_B_AS_BACK;
        ui_menu_init(&menu);
        ui_reset_main_screen();

SaveMgmtReselect:
        result = ui_menu_select(&menu);
        if (result == MENU_ENTRY_END) {
            return;
        }

        if (ui_dialog_run(0, 1, LK_DIALOG_CONFIRM, LK_DIALOG_YES_NO) == 0) {
            if (result < SRAM_SLOTS) {
                // if active, erase in-SRAM data too
                if (result == settings_local.active_sram_slot) {
                    sram_erase(SRAM_SLOT_NONE, SRAM_OFFSET_SIZE_DEFAULT);
                    settings_local.active_sram_slot = 0xFF;
                    settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
                    settings_mark_changed();
                }
                // erase slot
                sram_erase(result, SRAM_OFFSET_SIZE_DEFAULT);
            } else if (result == 0xEC || result == 0xEF) {
                // erase all save blocks
                sram_erase(SRAM_SLOT_ALL, SRAM_OFFSET_SIZE_DEFAULT);
                if (result == 0xEF) {
                    // and save data
                    sram_erase(SRAM_SLOT_NONE, SRAM_OFFSET_SIZE_DEFAULT);
                }
                settings_local.active_sram_slot = 0xFF;
                settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
                settings_mark_changed();
            } else if (result == 0xEF) {
                // erase everything
                sram_erase(SRAM_SLOT_ALL, SRAM_OFFSET_SIZE_DEFAULT);
                settings_local.active_sram_slot = 0xFF;
                settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
                settings_mark_changed();
            } else if (result == 0xEE) {
                // discard in-SRAM changes
                settings_local.active_sram_slot = 0xFF;
                settings_local.active_sram_offset_size = SRAM_OFFSET_SIZE_DEFAULT;
                settings_mark_changed();
            } else if (result == 0xED) {
                // erase + test everything
                ui_reset_main_screen();
                ui_puts_centered(false, 1, 0, lang_keys[LK_UI_ERASE_TEST_LINE1]);
                ui_puts_centered(false, 2, 0, lang_keys[LK_UI_PLEASE_WAIT]);
                for (int i = 0; i < SRAM_SLOTS; i++) {
                    ui_bg_putc(13 * (i / 10), 4 + (i % 10), 'A' + i, 0);
                }
                for (int i = 0; i < SRAM_SLOTS; i++) {
                    test_save_read_write(13 * (i / 10) + 2, 4 + (i % 10), i);
                }
                ui_puts_centered(false, 2, 0, lang_keys[LK_UI_PRESS_ANY_KEY]);
                input_wait_any_key();
                settings_mark_changed();
            } else {
                return;
            }
            ui_reset_main_screen();
        } else {
            goto SaveMgmtReselect;
        }
    }
#endif
    if (result == MENU_OPT_THEME) {
        if (ws_system_color_active()) {
            settings_local.color_theme = (settings_local.color_theme & 0x80)
                | (((settings_local.color_theme & 0x7F) + 1) % UI_THEME_COUNT);
        } else {
            settings_local.color_theme ^= 0x80;
        }
        settings_mark_changed();
        ui_update_theme(settings_local.color_theme);
        goto Reselect;
    } else if (result == MENU_OPT_TEXT_WIDTH) {
        settings_local.flags1 ^= SETT_FLAGS1_WIDE_SCREEN;
        settings_refresh();
        ui_reset_main_screen();
        goto Reselect;
    } else if (result == MENU_OPT_LANGUAGE) {
        settings_local.language = ui_set_language(settings_local.language + 1);
        settings_refresh();
        ui_reset_main_screen();
        goto Reselect;
    } else if (result == MENU_OPT_SAVE) {
        settings_save();
        goto Reselect;
    } else if (result == MENU_OPT_ADVANCED) {
        ui_reset_main_screen();
        ui_settings_advanced(menu_list);
    } else if (result == MENU_OPT_REVERTSETTINGS) {
        if (ui_dialog_run(0, 1, LK_DIALOG_CONFIRM, LK_DIALOG_YES_NO) == 0) {
            settings_load();
            settings_refresh();
        }
    } else if (result == MENU_OPT_FACTORYRESETSETTINGS) {
        if (ui_dialog_run(0, 1, LK_DIALOG_CONFIRM, LK_DIALOG_YES_NO) == 0) {
            settings_erase_slots();
            crt0_restart();
        }
    }
}
