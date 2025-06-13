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

#include <stdint.h>
#include <string.h>
#include <ws.h>
#include "input.h"
#include "lang.h"
#include "ui.h"
#include "util.h"

#define SPRITE_TABLE ((uint8_t*) 0x3C80)

extern uint8_t fm_initial_slot[7];
static const int8_t __far sin_table[128] = {0, 1, 3, 4, 6, 7, 9, 10, 12, 14, 15, 17, 18, 20, 21, 23, 24, 26, 28, 29, 31, 32, 34, 35, 37, 38, 40, 41, 43, 44, 46, 47, 48, 50, 51, 53, 54, 56, 57, 58, 60, 61, 63, 64, 65, 67, 68, 69, 71, 72, 73, 74, 76, 77, 78, 79, 81, 82, 83, 84, 85, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 108, 109, 110, 111, 112, 112, 113, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 120, 121, 121, 122, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127};

static int16_t sin(int16_t v) {
    switch ((v >> 7) & 0x03) {
    case 0: return sin_table[v & 0x7F];
    case 1: return sin_table[127 - (v & 0x7F)];
    case 2: return -sin_table[v & 0x7F];
    case 3: return -sin_table[127 - (v & 0x7F)];
    }
}
static inline int16_t cos(int16_t v) {
    return sin(v + 128);
}

static const char cartfriend_name[] = "CartFriend " VERSION;

void ui_about(void) {
    ui_puts_centered(false, 2, 0, cartfriend_name);
#ifdef TARGET_flash_masta
    outportb(0xCE, 0xAA);
    bool is_rev5 = inportb(0xCE) == 0xAA;
    ui_puts_centered(false, 3, 0, lang_keys[is_rev5 ? LK_UI_FM_REV5 : LK_UI_FM_REV4]);
#else
    ui_puts_centered(false, 3, 0, lang_keys[LK_UI_GENERIC]);
#endif

    // ui_puts_centered(false, 3, lang_keys[LK_RELEASE_DATE], 0);
    /* ui_bg_printf_centered(3, 0, lang_keys[LK_UI_LOADED_FROM],
        (int) fm_initial_slot[0],
        (int) fm_initial_slot[1],
        (int) fm_initial_slot[2],
        (int) fm_initial_slot[3]); */
    ui_puts_centered(false, 12, 0, lang_keys[LK_UI_ABOUT_URL_LINE1]);
    ui_puts_centered(false, 13, 0, lang_keys[LK_UI_ABOUT_URL_LINE2]);

    outportb(IO_SPR_BASE, SPR_BASE(SPRITE_TABLE));
    outportb(IO_SPR_FIRST, 128 >> 2);
    outportb(IO_SPR_COUNT, 8);

    uint16_t rotX = 0;
    uint16_t rotY = 0;
    uint16_t rotZ = 0;
    uint16_t offX = 0;
    int16_t points3D[8][3];

    for (uint8_t i = 0; i < 8; i++) {
        SPRITE_TABLE[(i << 2)] = 7;
        SPRITE_TABLE[(i << 2) + 1] = (4 << 1) | (1 << 5);
    }

    while (ui_poll_events()) {
        wait_for_vblank();

        if (input_held & KEY_UP) {
            rotY -= 9;
        }
        if (input_held & KEY_DOWN) {
            rotY += 9;
        }
        if (input_held & KEY_LEFT) {
            rotX -= 9;
        }
        if (input_held & KEY_RIGHT) {
            rotX += 9;
        }
        if (input_held & KEY_A) {
            rotZ += 9;
        }
        if (input_held & KEY_B) {
            rotZ -= 9;
        }
        if (!(input_held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B))) {
            rotX += 3;
            rotY += 7;
            rotZ += 5;
            offX += 1;
        }

        int16_t sinRX = sin(rotX >> 2);
        int16_t cosRX = cos(rotX >> 2);
        int16_t sinRY = sin(rotY >> 2);
        int16_t cosRY = cos(rotY >> 2);
        int16_t sinRZ = sin(rotZ >> 2);
        int16_t cosRZ = cos(rotZ >> 2);
        int16_t sinOX = sin(offX);
        int16_t x, y, z;

        for (uint8_t i = 0; i < 8; i++) {
            points3D[i][0] = -64 + ((i & 1) ? 0 : 128);
            points3D[i][1] = -64 + ((i & 2) ? 0 : 128);
            points3D[i][2] = -64 + ((i & 4) ? 0 : 128);
        }

        for (uint8_t i = 0; i < 8; i++) {
            // rotate around X
            y = points3D[i][1];
            z = points3D[i][2];
            points3D[i][1] = (y * cosRX - z * sinRX) >> 7;
            points3D[i][2] = (z * cosRX + y * sinRX) >> 7;

            // rotate around Y
            x = points3D[i][0];
            z = points3D[i][2];
            points3D[i][0] = (x * cosRY + z * sinRY) >> 7;
            points3D[i][2] = (z * cosRY - x * sinRY) >> 7;

            // rotate around Z
            x = points3D[i][0];
            y = points3D[i][1];
            points3D[i][0] = (x * cosRZ - y * sinRZ) >> 7;
            points3D[i][1] = (y * cosRZ + x * sinRZ) >> 7;
        }

        for (uint8_t i = 0; i < 8; i++) {
            uint8_t z = (points3D[i][2] >> 2) + 128; // 128-16 .. 128+16
            points3D[i][0] = (points3D[i][0] * z) >> 7;
            points3D[i][1] = (points3D[i][1] * z) >> 7;
            SPRITE_TABLE[(i << 2)] = (z < 118) ? 12 : ((z < 140) ? 7 : 11);
            SPRITE_TABLE[(i << 2) + 2] = (points3D[i][1] >> 2) + 69;
            SPRITE_TABLE[(i << 2) + 3] = (points3D[i][0] >> 2) + (112 + (sinOX >> 1));
        }
        outportw(IO_DISPLAY_CTRL, inportw(IO_DISPLAY_CTRL) | DISPLAY_SPR_ENABLE);
    }

    wait_for_vblank();
    outportw(IO_DISPLAY_CTRL, inportw(IO_DISPLAY_CTRL) & (~DISPLAY_SPR_ENABLE));
    outportb(IO_SPR_FIRST, 0);
    outportb(IO_SPR_COUNT, 0);
}
