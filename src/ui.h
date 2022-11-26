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

#ifndef __UI_H__
#define __UI_H__

#include <stdbool.h>
#include <stdint.h>
#include "../res/lang.h"

extern const char __far* const __far* lang_keys;

void ui_init(void);
void ui_show(void);
void ui_hide(void);
void ui_reset_main_screen(void);
void ui_scroll(int8_t offset);
void ui_putc(bool alt_screen, uint8_t x, uint8_t y, uint16_t chr, uint8_t color);
void ui_puts(bool alt_screen, uint8_t x, uint8_t y, const char __far* buf, uint8_t color);
void ui_puts_centered(bool alt_screen, uint8_t y, const char __far* buf, uint8_t color);
__attribute__((format(printf, 5, 6))) void ui_printf(bool alt_screen, uint8_t x, uint8_t y, uint8_t color, const char __far* format, ...);
__attribute__((format(printf, 4, 5))) void ui_printf_centered(bool alt_screen, uint8_t y, uint8_t color, const char __far* format, ...);

#define UI_GLYPH_ARROW_RIGHT 16
#define UI_GLYPH_ARROW_LEFT 17
#define UI_GLYPH_TRIANGLE_UR 272

// Tabs

extern uint8_t ui_current_tab;
void ui_set_current_tab(uint8_t tab);
bool ui_poll_events(void);

#define UI_TAB_BROWSE 0
#define UI_TAB_TOOLS 1
#define UI_TAB_SETTINGS 2
#define UI_TAB_ABOUT 3
#define UI_TAB_TOTAL 4

// Work indicator

void ui_step_work_indicator(void);
void ui_clear_work_indicator(void);

// Menu system

#define MENU_ENTRY_END 255

typedef void (*ui_menu_draw_line_func)(uint8_t entry_id, uint8_t y, uint8_t color);

uint8_t ui_menu_select(uint8_t *menu_list, ui_menu_draw_line_func draw_line_func);

// Tab implementations

void ui_about(void); // ui_about.c
void ui_browse(void); // ui_browse.c
void ui_settings(void); // ui_settings.c
void ui_tools(void); // ui_tools.c

#endif