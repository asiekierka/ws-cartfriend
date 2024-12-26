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
#include <stdio.h>
#include <string.h>
#include <ws.h>
#include <wsx/planar_unpack.h>
#include "config.h"
#include "driver.h"
#include "input.h"
#include "lang.h"
#include "settings.h"
#include "ui.h"
#include "../res/font_default.h"
#include "util.h"

const char __far* const __far* lang_keys;
#ifdef USE_LOW_BATTERY_WARNING
extern uint8_t ui_low_battery_flag;
#endif
#define MAIN_SCREEN_WIDTH ((settings_local.flags1 & SETT_FLAGS1_WIDE_SCREEN) ? 28 : 27)

static const uint16_t __far theme_colorways[UI_THEME_COUNT][10] = {
    { // Light
        RGB(15, 15, 15), // Background color (Screen 1)
        RGB(0, 0, 0), // Foreground color (Screen 1)
        RGB(11, 11, 11), // Foreground color (Screen 1, light text)
        RGB(8, 8, 8), // Background color (Screen 1, dialog open)
        RGB(6, 6, 6), // Foreground color (Screen 1, dialog open)
        RGB(11, 11, 11), // Background color (Bar)
        RGB(4, 4, 4), // Foreground color (Bar)
        RGB(15, 15, 15), // Background color (Dialog)
        RGB(0, 0, 0), // Foreground color (Dialog)
        RGB(15, 0, 0), // Foreground color (Progress Bar)
    },
    { // Dark
        RGB(0, 0, 0), // Background color (Screen 1)
        RGB(15, 15, 15), // Foreground color (Screen 1)
        RGB(4, 4, 4), // Foreground color (Screen 1, light text)
        RGB(6, 6, 6), // Background color (Screen 1, dialog open)
        RGB(8, 8, 8), // Foreground color (Screen 1, dialog open)
        RGB(4, 4, 4), // Background color (Bar)
        RGB(11, 11, 11), // Foreground color (Bar)
        RGB(0, 0, 0), // Background color (Dialog)
        RGB(15, 15, 15), // Foreground color (Dialog)
        RGB(15, 0, 0), // Foreground color (Progress Bar)
    },
    { // Crystal
        RGB(15, 14, 13), // Background color (Screen 1)
        RGB(4, 3, 1), // Foreground color (Screen 1)
        RGB(12, 11, 10), // Foreground color (Screen 1, light text)
        RGB(8, 7, 7), // Background color (Screen 1, dialog open)
        RGB(7, 6, 6), // Foreground color (Screen 1, dialog open)
        RGB(5, 3, 6), // Background color (Bar)
        RGB(12, 9, 11), // Foreground color (Bar)
        RGB(14, 13, 15), // Background color (Dialog)
        RGB(3, 2, 4), // Foreground color (Dialog)
        RGB(5, 3, 11), // Foreground color (Progress Bar)
    },
};

static uint8_t scroll_y;
bool ui_dialog_open;

void ui_update_theme(uint8_t current_theme) {
    wait_for_vblank();
    if (ws_system_color_active()) {
        const uint16_t __far* colorway = theme_colorways[(current_theme & 0x7F) % UI_THEME_COUNT];
        MEM_COLOR_PALETTE(0)[0] = colorway[ui_dialog_open ? 3 : 0];
        MEM_COLOR_PALETTE(0)[1] = colorway[ui_dialog_open ? 4 : 1];
        MEM_COLOR_PALETTE(1)[0] = MEM_COLOR_PALETTE(0)[1];
        MEM_COLOR_PALETTE(1)[1] = MEM_COLOR_PALETTE(0)[0];
        MEM_COLOR_PALETTE(2)[0] = colorway[5];
        MEM_COLOR_PALETTE(2)[1] = colorway[6];
        MEM_COLOR_PALETTE(3)[0] = MEM_COLOR_PALETTE(2)[1];
        MEM_COLOR_PALETTE(3)[1] = MEM_COLOR_PALETTE(2)[0];
        MEM_COLOR_PALETTE(12)[1] = colorway[1];
        MEM_COLOR_PALETTE(8)[0] = colorway[7];
        MEM_COLOR_PALETTE(8)[1] = colorway[8];
        MEM_COLOR_PALETTE(9)[0] = MEM_COLOR_PALETTE(8)[1];
        MEM_COLOR_PALETTE(9)[1] = MEM_COLOR_PALETTE(8)[0];
        MEM_COLOR_PALETTE(10)[0] = MEM_COLOR_PALETTE(0)[0];
        MEM_COLOR_PALETTE(10)[1] = colorway[9];
        MEM_COLOR_PALETTE(11)[0] = MEM_COLOR_PALETTE(0)[0];
        MEM_COLOR_PALETTE(11)[1] = colorway[ui_dialog_open ? 4 : 2];
    } else {
        if (current_theme & 0x80) {
            // dark mode
            ws_display_set_shade_lut(SHADE_LUT(15, 13, 11, 8, 6, 4, 2, 0));
        } else {
            // light mode
            ws_display_set_shade_lut(SHADE_LUT(0, 2, 4, 6, 8, 11, 13, 15));
        }
        if (ui_dialog_open) {
            outportw(0x20, 4 << 4 | 3);
            outportw(0x22, 3 << 4 | 4);
            outportw(0x36, 4 << 4 | 3);
        } else {
            outportw(0x20, 7 << 4);
            outportw(0x22, 7);
            outportw(0x36, 2 << 4);
        }
        outportw(0x24, 5 << 4 | 2);
        outportw(0x26, 2 << 4 | 5);
        outportw(0x38, 7 << 4);
        outportw(0x30, 7 << 4);
        outportw(0x32, 7);
        outportw(0x34, 4 << 4);
    }
}

void ui_reset_alt_screen(void) {
    for (uint16_t i = 0; i < 32*16; i++) {
        SCREEN2[i + 32] = SCR_ENTRY_PALETTE(7);
    }
}

uint8_t ui_set_language(uint8_t id) {
    if (id >= UI_LANGUAGE_MAX) id = UI_LANGUAGE_EN;
    switch (id) {
    case UI_LANGUAGE_EN: lang_keys = lang_keys_en; break;
    case UI_LANGUAGE_PL: lang_keys = lang_keys_pl; break;
    case UI_LANGUAGE_DE: lang_keys = lang_keys_de; break;
    case UI_LANGUAGE_FR: lang_keys = lang_keys_fr; break;
    }
    return id;
}

void ui_init(void) {
    lang_keys = lang_keys_en;
#ifdef USE_LOW_BATTERY_WARNING
    ui_low_battery_flag = 0;
#endif

    outportw(IO_DISPLAY_CTRL, 0);
    outportb(IO_SPR_FIRST, 0);
    outportb(IO_SPR_COUNT, 0);

    outportb(IO_SCR2_SCRL_X, 0);
    outportb(IO_SCR2_SCRL_Y, 0);

    // set palettes (mono)
    if (ws_system_is_color()) {
        ws_mode_set(WS_MODE_COLOR);
    }
    ui_dialog_open = false;
    ui_update_theme(0);

    // install font @ 0x2000
    wsx_planar_unpack((uint16_t*) 0x2000, _font_default_bin_size, _font_default_bin, WSX_PLANAR_UNPACK_1BPP_TO_2BPP_ZERO(1));

    ui_reset_main_screen();
    ui_reset_alt_screen();

    for (uint16_t i = 0; i < 28; i++) {
        SCREEN2[i] = SCR_ENTRY_PALETTE(2);
        SCREEN2[i + (17 << 5)] = SCR_ENTRY_PALETTE(2);
    }

    outportb(IO_SCR_BASE, SCR1_BASE((uint16_t) SCREEN1) | SCR2_BASE((uint16_t) SCREEN2));
    //ui_puts(true, 28 - strlen(lang_keys[LK_NAME]), 17, 2, lang_keys[LK_NAME]);
    ui_fg_putc(26, 17, UI_GLYPH_PASSAGE, 2);
}

void ui_show(void) {
    outportw(IO_DISPLAY_CTRL, DISPLAY_SCR1_ENABLE | DISPLAY_SCR2_ENABLE);
}

void ui_hide(void) {
    outportw(IO_DISPLAY_CTRL, 0);
}

void ui_reset_main_screen(void) {
    scroll_y = 0;
    outportb(IO_SCR1_SCRL_X, settings_local.flags1 & SETT_FLAGS1_WIDE_SCREEN ? 0 : 252);
    outportb(IO_SCR1_SCRL_Y, 248);
    _nmemset(SCREEN1, 0, 0x800);
}

void ui_scroll(int8_t offset) {
    scroll_y = (scroll_y + offset) & 31;
    outportb(IO_SCR1_SCRL_Y, (scroll_y - 1) << 3);
}

inline void ui_bg_putc(uint8_t x, uint8_t y, uint16_t chr, uint8_t color) {
    uint16_t prefix = SCR_ENTRY_PALETTE(color);
    ws_screen_put_tile(SCREEN1, prefix | chr, x++, y);
}

inline void ui_fg_putc(uint8_t x, uint8_t y, uint16_t chr, uint8_t color) {
    uint16_t prefix = SCR_ENTRY_PALETTE(color);
    ws_screen_put_tile(SCREEN2, prefix | chr, x++, y);
}

void ui_fill_line(uint8_t y, uint8_t color) {
    uint16_t prefix = SCR_ENTRY_PALETTE(color);
    uint16_t *screen = SCREEN1 + (y << 5);
    for (uint8_t i = 0; i < 32; i++) {
        *(screen++) = prefix;
    }
}

void ui_puts(bool alt_screen, uint8_t x, uint8_t y, uint8_t color, const char __far* buf) {
    uint16_t prefix = SCR_ENTRY_PALETTE(color);
    uint16_t *screen = alt_screen ? SCREEN2 : SCREEN1;
    while (*buf != '\0') {
        if (*buf == '\x05') {
            if (color == 0) prefix = SCR_ENTRY_PALETTE(UI_PAL_LIGHT);
            buf++;
            continue;
        }
        ws_screen_put_tile(screen, prefix | ((uint8_t) *(buf++)), x++, y);
        if (x == 28) return;
    }
}

void ui_puts_centered(bool alt_screen, uint8_t y, uint8_t color, const char __far* buf) {
    uint8_t x = ((alt_screen ? 28 : MAIN_SCREEN_WIDTH) - strlen(buf)) >> 1;
    ui_puts(alt_screen, x, y, color, buf);
}

void ui_bg_printf(uint8_t x, uint8_t y, uint8_t color, const char __far* format, ...) {
    char buf[33];
    va_list val;
    va_start(val, format);
    vsnprintf(buf, sizeof(buf), format, val);
    va_end(val);
    ui_puts(false, x, y, color, buf);
}

void ui_bg_printf_centered(uint8_t y, uint8_t color, const char __far* format, ...) {
    char buf[33];
    va_list val;
    va_start(val, format);
    vsnprintf(buf, sizeof(buf), format, val);
    va_end(val);
    uint8_t x = (MAIN_SCREEN_WIDTH - strlen(buf)) >> 1;
    ui_puts(false, x, y, color, buf);
}

void ui_bg_printf_right(uint8_t x, uint8_t y, uint8_t color, const char __far* format, ...) {
    char buf[33];
    va_list val;
    va_start(val, format);
    int len = vsnprintf(buf, sizeof(buf), format, val);
    va_end(val);
    ui_puts(false, x + 1 - len, y, color, buf);
}

// Tabs

static uint16_t __far ui_tabs_to_lks[] = {
#ifdef USE_SLOT_SYSTEM
    LK_UI_HEADER_BROWSE,
#endif
    LK_UI_HEADER_TOOLS,
    LK_UI_HEADER_SETTINGS,
    LK_UI_HEADER_ABOUT,
    LK_TOTAL
};
uint8_t ui_current_tab;

void ui_set_current_tab(uint8_t tab) {
    uint8_t x = 0;
    bool active = true;
    const char __far* text = lang_keys[ui_tabs_to_lks[tab]];
    bool finished = false;

    ui_current_tab = tab;
    
    while (x < 28) {
        if (text != NULL && ((*text) != 0)) {
            ui_fg_putc(x++, 0, (uint8_t) *(text++), active ? 3 : 2);
        } else if (finished) {
            ui_fg_putc(x++, 0, 0, 2);
        } else if (*text == 0) {
            ui_fg_putc(x++, 0,
                active ? UI_GLYPH_TRIANGLE_UR : 0,
                2
            );
            ui_fg_putc(x++, 0, 0, 2);

            active = false;
            uint16_t lk = ui_tabs_to_lks[++tab];
            if (lk == LK_TOTAL) {
                finished = true;
            } else {
                text = lang_keys[lk];
            }
        } else {
            finished = true;
        }
    }

    if (!finished) {
        ui_fg_putc(26, 0, 0, 2);
        ui_fg_putc(27, 0, UI_GLYPH_ARROW_RIGHT, 2);
    }

    if (ui_current_tab >= 1) {
        ui_fg_putc(25, 0, 0, 2);
        ui_fg_putc(26, 0, UI_GLYPH_ARROW_LEFT, 2);
        if (finished) {
            ui_fg_putc(27, 0, 0, 2);
        }
    }
}

#define UI_WORK_INDICATOR_X 24

void ui_update_indicators(void) {
    ui_fg_putc(0, 17, settings_local.active_sram_slot < SRAM_SLOTS ? UI_GLYPH_SRAM_ACTIVE : 0, 2);
    // 1, 17 - low battery glyph (set elsewherer)
#ifdef USE_SLOT_SYSTEM
    ui_fg_putc(2, 17, settings_changed ? UI_GLYPH_SETTINGS_CHANGED : 0, 2);
#endif
    ui_fg_putc(3, 17, (inportb(IO_IEEP_CTRL) & IEEP_PROTECT) ? UI_GLYPH_EEPROM_UNLOCKED : 0, 2);
}

bool ui_poll_events(void) {
    input_update();
    ui_update_indicators();

#ifdef USE_LOW_BATTERY_WARNING
    if (ui_low_battery_flag == 1 && !ui_dialog_open) {
        ui_fg_putc(1, 17, UI_GLYPH_LOW_BATTERY, 2);
        ui_low_battery_flag = 2;
        ui_dialog_run(0, 0, LK_DIALOG_LOW_BATTERY, LK_DIALOG_OK);
    }
#endif

    if (input_pressed & KEY_ALEFT) {
        if (ui_current_tab > 0) {
            ui_set_current_tab(ui_current_tab - 1);
            wait_for_vblank();
            return false;
        }
    } else if (input_pressed & KEY_ARIGHT) {
        if (ui_current_tab < (UI_TAB_TOTAL - 1)) {
            ui_set_current_tab(ui_current_tab + 1);
            wait_for_vblank();
            return false;
        }
    } else if (input_pressed & KEY_PCV2_PASS) {
        if (ui_current_tab < (UI_TAB_TOTAL - 1)) {
            ui_set_current_tab(ui_current_tab + 1);
        } else {
            ui_set_current_tab(0);
        }
        wait_for_vblank();
        return false;
    }

    return true;
}

// Work indicator

extern uint16_t vbl_ticks;
uint8_t ui_work_indicator;
uint16_t ui_work_indicator_vbl_ticks;
static const uint8_t __far ui_work_table[] = {
    'q', 'd', 'b', 'p'
};

void ui_step_work_indicator(void) {
    uint16_t ticks = vbl_ticks >> 2;
    if (ui_work_indicator_vbl_ticks == 0xFF || (ui_work_indicator_vbl_ticks != ticks)) {
        ui_fg_putc(UI_WORK_INDICATOR_X, 17, ui_work_table[ui_work_indicator], 2);
        ui_work_indicator = (ui_work_indicator + 1) & 3;
        ui_work_indicator_vbl_ticks = ticks;
    }
}

void ui_clear_work_indicator(void) {
    ui_work_indicator = 0;
    ui_work_indicator_vbl_ticks = 0xFF;
    ui_fg_putc(UI_WORK_INDICATOR_X, 17, ' ', 2);
}

// Menu system

static void ui_menu_draw_line(ui_menu_state_t *menu, uint8_t pos, uint8_t color) {
    if (menu->list[pos] == MENU_ENTRY_DIVIDER) {
        ws_screen_fill_tiles(SCREEN1, 196, 0, pos, 32, 1);
        return;
    }

    char buf[31];
    char buf_right[31];
    buf[0] = 0; buf_right[0] = 0;

    menu->build_line_func(menu->list[pos], menu->build_line_data, buf, 30, buf_right, 30);
    if (buf[0] != 0) {
        ui_puts(false, 0, pos, color, buf);
    }
    if (buf_right[0] != 0) {
        ui_puts(false, MAIN_SCREEN_WIDTH - strlen(buf_right), pos, color, buf_right);
    }
}

static void ui_menu_redraw(ui_menu_state_t *menu) {
    if (menu->height > 0) {
        for (uint8_t i = 0; i < 16; i++) {
            if (i >= menu->height) break;
            ui_menu_draw_line(menu, menu->y + i, 0);
        }
        ui_fill_line(menu->pos, 1);
        ui_menu_draw_line(menu, menu->pos, 1);
    }
}

static uint8_t ui_menu_list_move_pos(uint8_t *list, int8_t delta, uint8_t pos, uint8_t height) {
    int last_pos;
    int new_pos = pos;
    do {
        last_pos = new_pos;
        new_pos = new_pos + delta;
        if (new_pos < 0) new_pos = 0;
        else if (new_pos >= height) new_pos = height - 1;
    } while (last_pos != new_pos && (list[new_pos] == MENU_ENTRY_DIVIDER));

    return new_pos;
}

static void ui_menu_move(ui_menu_state_t *menu, int8_t delta) {
    uint8_t new_pos = ui_menu_list_move_pos(menu->list, delta, menu->pos, menu->height);
    if (new_pos == menu->pos) return;

    // draw lines
    ui_fill_line(menu->pos, 0);
    ui_menu_draw_line(menu, menu->pos, 0);
    menu->pos = new_pos;
    ui_fill_line(menu->pos, 1);
    ui_menu_draw_line(menu, menu->pos, 1);

    // adjust scroll
    int new_y = menu->pos - 8;
    if (new_y < 0) new_y = 0;
    else if (new_y >= menu->y_max) new_y = menu->y_max;
    int scroll_delta = new_y - menu->y;
    if (scroll_delta != 0) {
        ui_scroll(scroll_delta);
        menu->y = new_y;

        // TODO: optimize
        ui_menu_redraw(menu);
    }
}

void ui_menu_init(ui_menu_state_t *menu) {
    menu->height = u8_arraylist_len(menu->list);
    menu->pos = 0;
    menu->y = 0;
    menu->y_max = menu->height - 16;
    if (menu->y_max > menu->height) menu->y_max = 0;
}

uint16_t ui_menu_select(ui_menu_state_t *menu) {
    ui_clear_work_indicator();
    ui_menu_redraw(menu);

    uint16_t result = MENU_ENTRY_END;
    while (ui_poll_events()) {
        if (input_pressed & KEY_UP) {
            ui_menu_move(menu, -1);
        }
        if (input_pressed & KEY_DOWN) {
            ui_menu_move(menu, 1);
        }
        wait_for_vblank();
        uint8_t curr_entry = menu->list[menu->pos];
        if (menu->flags & MENU_SEND_LEFT_RIGHT) {
            if (input_pressed & KEY_LEFT) {
                result = curr_entry | MENU_ACTION_LEFT;
                break;
            }
            if (input_pressed & KEY_RIGHT) {
                result = curr_entry | MENU_ACTION_RIGHT;
                break;
            }
        }
        if (menu->flags & MENU_B_AS_BACK) {
            if (input_pressed & KEY_B) {
                break;
            }
        }
        if (menu->flags & MENU_B_AS_ACTION) {
            if (input_pressed & KEY_B) {
                result = curr_entry | MENU_ACTION_B;
                break;
            }
        }
        if (input_pressed & KEY_A) {
            result = curr_entry;
            break;
        }
    }

    return result;
}

// Popup menu system

static uint8_t ui_popup_menu_strlen(ui_popup_menu_state_t *menu, uint8_t i) {
    char buf[31];
    buf[0] = 0;

    menu->build_line_func(menu->list[i], menu->build_line_data, buf, 30);
    return strlen(buf);
}

static void ui_popup_menu_draw_line(ui_popup_menu_state_t *menu, uint8_t pos, uint8_t color) {
    if (menu->list[pos] == MENU_ENTRY_DIVIDER) {
        ws_screen_fill_tiles(SCREEN2, 196 | SCR_ENTRY_PALETTE(8), menu->x, menu->y + pos, menu->width, 1);
        return;
    }

    char buf[31];
    buf[0] = 0;

    menu->build_line_func(menu->list[pos], menu->build_line_data, buf, 30);
    ws_screen_fill_tiles(SCREEN2, SCR_ENTRY_PALETTE(color), menu->x, menu->y + pos, menu->width, 1);    
    if (buf[0] != 0) {
        ui_puts(true, menu->x + 1, menu->y + pos, color, buf);
    }
}

uint16_t ui_popup_menu_run(ui_popup_menu_state_t *menu) {
    ui_clear_work_indicator();

    // init
    menu->width = 0;
    menu->height = u8_arraylist_len(menu->list);
    for (uint8_t i = 0; i < menu->height; i++) {
        uint8_t new_width = ui_popup_menu_strlen(menu, i);
        if (new_width > menu->width) {
            menu->width = new_width;
        }
    }
    menu->width += 2;

    menu->x = 28 - menu->width;
    menu->y = 17 - menu->height;
    menu->pos = 0;

    ui_fg_putc(25, 17, ' ', UI_PAL_BARI);
    ui_fg_putc(26, 17, UI_GLYPH_PASSAGE, UI_PAL_BARI);
    ui_fg_putc(27, 17, ' ', UI_PAL_BARI);
    input_wait_clear();

    wait_for_vblank();
    ui_dialog_open = true;
    ui_update_theme(settings_local.color_theme);

    for (uint8_t i = 0; i < menu->height; i++) {
        ui_popup_menu_draw_line(menu, i, i == menu->pos ? UI_PAL_DIALOGI : UI_PAL_DIALOG);
    }

    // select
    uint16_t result = MENU_ENTRY_END;
    while (true) {
        uint8_t old_pos = menu->pos;

        wait_for_vblank();
        input_update();

        if (input_pressed & KEY_UP) {
            menu->pos = ui_menu_list_move_pos(menu->list, -1, menu->pos, menu->height);
        }
        if (input_pressed & KEY_DOWN) {
            menu->pos = ui_menu_list_move_pos(menu->list, 1, menu->pos, menu->height);
        }

        if (old_pos != menu->pos) {
            ui_popup_menu_draw_line(menu, old_pos, UI_PAL_DIALOG);
            ui_popup_menu_draw_line(menu, menu->pos, UI_PAL_DIALOGI);
        }

        uint8_t curr_entry = menu->list[menu->pos];
        if (input_pressed & KEY_B) {
            break;
        }
        if (input_pressed & KEY_A) {
            result = curr_entry;
            break;
        }
    }

    wait_for_vblank();
    ui_reset_alt_screen();

    ui_fg_putc(25, 17, ' ', UI_PAL_BAR);
    ui_fg_putc(26, 17, UI_GLYPH_PASSAGE, UI_PAL_BAR);
    ui_fg_putc(27, 17, ' ', UI_PAL_BAR);

    input_wait_clear();
    wait_for_vblank();

    ui_dialog_open = false;
    ui_update_theme(settings_local.color_theme);

    return result;
}

// Progress bar

void ui_pbar_init(ui_pbar_state_t *state) {
    state->step = 0;
    state->step_last = 0;

    if ((settings_local.flags1 & SETT_FLAGS1_WIDE_SCREEN) && state->x == 0 && state->width == 27) {
        state->x += 1;
        state->width -= 1;
    }
}

void ui_pbar_draw(ui_pbar_state_t *state) {
    uint16_t step_count = state->width * 8;
    uint16_t step_current = (((uint32_t) state->step) * step_count) / state->step_max;
    if (state->step_last != step_current) {
        uint8_t i = 0;
        uint8_t x = state->x;
        for (i = 8; i <= step_current; i += 8) {
            ui_bg_putc(x++, state->y, 219, UI_PAL_PBAR);
        }
        step_current &= 7;
        if (step_current > 0) {
            ui_bg_putc(x, state->y, step_current + UI_GLYPH_HORIZONTAL_PBAR, UI_PAL_PBAR);
        }
    }
}

// Dialog

static uint8_t sep_count_lines(const char __far* s, uint8_t *max_width) {
    uint8_t line_width = 0;
    uint8_t line_count = 1;
    while (*s != '\0') {
        if (*s == '|') {
            if (line_width > *max_width) {
                *max_width = line_width;
            }
            line_width = 0;
            line_count++;
        } else {
            line_width++;
        }
        s++;
    }
    if (line_width > *max_width) {
        *max_width = line_width;
    }
    return line_count;
}

static void sep_draw(const char __far* s, uint8_t x, uint8_t y, uint8_t highlight_line, bool left_aligned) {
    char buf[29];
    highlight_line += y;

    uint8_t i = 0;
    while (*s != '\0') {
        if (*s == '|') {
            buf[i] = 0;
            if (left_aligned) {
                ui_puts(true, x, y, y == highlight_line ? 9 : 8, buf);
            } else {
                ui_puts_centered(true, y, y == highlight_line ? 9 : 8, buf);
            }
            i = 0;
            y++;
        } else {
            buf[i++] = *s;
        }
        s++;
    }

    buf[i] = 0;
    if (left_aligned) {
        ui_puts(true, x, y, y == highlight_line ? 9 : 8, buf);
    } else {
        ui_puts_centered(true, y, y == highlight_line ? 9 : 8, buf);
    }
}

uint8_t ui_dialog_run(uint16_t flags, uint8_t initial_option, uint16_t lk_question, uint16_t lk_options) {
    wait_for_vblank();
    ui_dialog_open = true;
    ui_update_theme(settings_local.color_theme);

    uint8_t max_line_width = 0;
    uint8_t line_count = sep_count_lines(lang_keys[lk_question], &max_line_width);
    uint8_t option_count = sep_count_lines(lang_keys[lk_options], &max_line_width);

    uint8_t width = ((max_line_width + 2) + 1) & 0xFE;
    uint8_t height = ((line_count + option_count + 3) + 1) & 0xFE;
    uint8_t x = 14 - ((width + 1) >> 1);
    uint8_t y = 9 - ((height + 1) >> 1);
    ws_screen_fill_tiles(SCREEN2, SCR_ENTRY_PALETTE(UI_PAL_DIALOG), x, y, width, height);
    uint8_t selected_option = initial_option;

    // draw text
    sep_draw(lang_keys[lk_question], x + 1, y + 1, 0xFF, true);
    while (true) {
        sep_draw(lang_keys[lk_options], x + 1, y + height - option_count - 1, selected_option, false);

        wait_for_vblank();
        input_update();
        if (input_pressed & KEY_UP) {
            if (selected_option > 0) {
                selected_option--;
            }
        } else if (input_pressed & KEY_DOWN) {
            if (selected_option < (option_count - 1)) {
                selected_option++;
            }
        } else if (input_pressed & KEY_A) {
            break;
        } else if (input_pressed & KEY_B) {
            selected_option = 0xFF;
            break;
        }
    }

    wait_for_vblank();
    ui_reset_alt_screen();

    input_wait_clear();
    wait_for_vblank();

    ui_dialog_open = false;
    ui_update_theme(settings_local.color_theme);

    return selected_option;
}

