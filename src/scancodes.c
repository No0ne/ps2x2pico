/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 No0ne (https://github.com/No0ne)
 *           (c) 2023 Dustin Hoffman
 *           (c) 2024 Bernd Strobel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include "ps2x2pico.h"
#include "tusb.h"

u8 const ext_code_keys_1_2[] = {
  HID_KEY_INSERT,
  HID_KEY_HOME,
  HID_KEY_PAGE_UP,
  HID_KEY_DELETE,
  HID_KEY_END,
  HID_KEY_PAGE_DOWN,
  HID_KEY_ARROW_RIGHT,
  HID_KEY_ARROW_LEFT,
  HID_KEY_ARROW_DOWN,
  HID_KEY_ARROW_UP,
  HID_KEY_KEYPAD_DIVIDE,
  HID_KEY_KEYPAD_ENTER,
  HID_KEY_APPLICATION,
  HID_KEY_POWER,
  0 // End marker
};

u8 const ext_code_modifier_keys_1_2[] = {
  HID_KEY_GUI_LEFT,
  HID_KEY_CONTROL_RIGHT,
  HID_KEY_ALT_RIGHT,
  HID_KEY_GUI_RIGHT,
  0
};

u8 const prt_scn_make_1[]  = { 0xe0, 0x2a, 0xe0, 0x37, 0 };
u8 const prt_scn_break_1[] = { 0xe0, 0xb7, 0xe0, 0xaa, 0 };
u8 const break_make_1[]    = { 0xe0, 0x46, 0xe0, 0xc6, 0 };
u8 const pause_make_1[]    = { 0xe1, 0x1d, 0x45, 0xe1, 0x9d, 0xc5, 0 };

u8 const prt_scn_make_2[]  = { 0xe0, 0x12, 0xe0, 0x7c, 0 };
u8 const prt_scn_break_2[] = { 0xe0, 0xf0, 0x7c, 0xe0, 0xf0, 0x12, 0 };
u8 const break_make_2[]    = { 0xe0, 0x7e, 0xe0, 0xf0, 0x7e, 0 };
u8 const pause_make_2[]    = { 0xe1, 0x14, 0x77, 0xe1, 0xf0, 0x14, 0xf0, 0x77, 0 };

// break codes in set 1 are created from the make code by adding 128 (0x80) or in other words set the msb.
// this is true even for the extended codes (but 0xe0 stays 0xe0) and the special multi byte codes for print screen and pause
u8 const mod2ps2_1[] = {
  0x1d, // L CTRL
  0x2a, // L SHFT
  0x38, // L ALT
  0x5b, // L GUI
  0x1d, // R CTRL
  0x36, // R SHFT
  0x38, // R ALT
  0x5c  // R GUI
};

u8 const mod2ps2_2[] = {
  0x14, // L CTRL
  0x12, // L SHFT
  0x11, // L ALT
  0x1f, // L WIN
  0x14, // R CTRL
  0x59, // R SHFT
  0x11, // R ALT
  0x27  // R WIN
};

u8 const mod2ps2_3[] = {
  0x11, // L CTRL
  0x12, // L SHFT
  0x19, // L ALT
  0x8b, // L WIN
  0x58, // R CTRL
  0x59, // R SHFT
  0x39, // R ALT
  0x8c  // R WIN
};

// break codes in set 1 are created from the make code by adding 128 (0x80) or in other words set the msb.
// this is true even for the extended codes (but 0xe0 stays 0xe0) and the special multi byte codes for print screen and pause
u8 const hid2ps2_1[] = {
  // 0x00 - 0x0f
  0, 0, 0, 0, // NONE
  0x1e, // A
  0x30, // B
  0x2e, // C
  0x20, // D
  0x12, // E
  0x21, // F
  0x22, // G
  0x23, // H
  0x17, // I
  0x24, // J
  0x25, // K
  0x26, // L
  // 0x10 - 0x1f
  0x32, // M
  0x31, // N
  0x18, // O
  0x19, // P
  0x10, // Q
  0x13, // R
  0x1f, // S
  0x14, // T
  0x16, // U
  0x2f, // V
  0x11, // W
  0x2d, // X
  0x15, // Y
  0x2c, // Z
  0x02, // 1
  0x03, // 2
  // 0x20 - 0x2f
  0x04, // 3
  0x05, // 4
  0x06, // 5
  0x07, // 6
  0x08, // 7
  0x09, // 8
  0x0a, // 9
  0x0b, // 0
  0x1c, // ENTER
  0x01, // ESC
  0x0e, // BKSP
  0x0f, // TAB
  0x39, // SPACE
  0x0c, // -
  0x0d, // =
  0x1a, // [
  // 0x30 - 0x3f
  0x1b, // ]
  0x2b, // BACKSLASH
  0x2b, // EUROPE_1
  0x27, // ;
  0x28, // ' APOSTROPHE
  0x29, // ` GRAVE
  0x33, // ,
  0x34, // .
  0x35, // /
  0x3a, // CAPS
  0x3b, // F1
  0x3c, // F2
  0x3d, // F3
  0x3e, // F4
  0x3f, // F5
  0x40, // F6
  // 0x40 - 0x4f
  0x41, // F7
  0x42, // F8
  0x43, // F9
  0x44, // F10
  0x57, // F11
  0x58, // F12
  0x00, // E0,2A,E0,37 // PRNT SCRN // break E0,B7,E0,AA
  0x46, // SCROLL
  0x00, // E1,1D,45,E1,9D,C5 // PAUSE, no break codes!
  0x52, // E0 INSERT
  0x47, // E0 HOME
  0x49, // E0 PG UP
  0x53, // E0 DELETE
  0x4f, // E0 END
  0x51, // E0 PG DN
  0x4d, // E0 R ARROW
  // 0x50 - 0x5f
  0x4b, // E0 L ARROW
  0x50, // E0 D ARROW
  0x48, // E0 U ARROW
  0x45, // NUM
  0x35, // E0 KP /
  0x37, // KP *
  0x4a, // KP -
  0x4e, // KP +
  0x1c, // E0 KP EN
  0x4f, // KP 1
  0x50, // KP 2
  0x51, // KP 3
  0x4b, // KP 4
  0x4c, // KP 5
  0x4d, // KP 6
  0x47, // KP 7
  // 0x60 - 0x6f
  0x48, // KP 8
  0x49, // KP 9
  0x52, // KP 0
  0x53, // KP .
  0x56, // EUROPE_2
  0x5d, // E0 APPS
  0x5e, // E0 POWER
  0x49, // KEYPAD_EQUAL
  0x64, // F13
  0x65, // F14
  0x66, // F15
  0x67, // F16
  0x68, // F17
  0x69, // F18
  0x6a, // F19
  0x6b, // F20
  // 0x70 - 0x73
  0x6c, // F21
  0x6d, // F22
  0x6e, // F23
  0x76  // F24
};

u8 const hid2ps2_2[] = {
  // 0x00 - 0x0f
  0, 0, 0, 0, // NONE
  0x1c, // A
  0x32, // B
  0x21, // C
  0x23, // D
  0x24, // E
  0x2b, // F
  0x34, // G
  0x33, // H
  0x43, // I
  0x3b, // J
  0x42, // K
  0x4b, // L
  // 0x10 - 0x1f
  0x3a, // M
  0x31, // N
  0x44, // O
  0x4d, // P
  0x15, // Q
  0x2d, // R
  0x1b, // S
  0x2c, // T
  0x3c, // U
  0x2a, // V
  0x1d, // W
  0x22, // X
  0x35, // Y
  0x1a, // Z
  0x16, // 1
  0x1e, // 2
  // 0x20 - 0x2f
  0x26, // 3
  0x25, // 4
  0x2e, // 5
  0x36, // 6
  0x3d, // 7
  0x3e, // 8
  0x46, // 9
  0x45, // 0
  0x5a, // ENTER
  0x76, // ESC
  0x66, // BACKSPACE
  0x0d, // TAB
  0x29, // SPACE
  0x4e, // -
  0x55, // =
  0x54, // [
  // 0x30 - 0x3f
  0x5b, // ]
  0x5d, // BACKSLASH
  0x5d, // EUROPE_1
  0x4c, // ;
  0x52, // ' APOSTROPHE
  0x0e, // ` GRAVE
  0x41, // ,
  0x49, // .
  0x4a, // /
  0x58, // CAPS
  0x05, // F1
  0x06, // F2
  0x04, // F3
  0x0c, // F4
  0x03, // F5
  0x0b, // F6
  // 0x40 - 0x4f
  0x83, // F7
  0x0a, // F8
  0x01, // F9
  0x09, // F10
  0x78, // F11
  0x07, // F12
  0x7c, // PRNT SCRN
  0x7e, // SCROLL
  0x7e, // PAUSE
  0x70, // INSERT
  0x6c, // HOME
  0x7d, // PG UP
  0x71, // DELETE
  0x69, // END
  0x7a, // PD DN
  0x74, // R ARROW
  // 0x50 - 0x5f
  0x6b, // L ARROW
  0x72, // D ARROW
  0x75, // U ARROW
  0x77, // NUM
  0x4a, // KP /
  0x7c, // KP *
  0x7b, // KP -
  0x79, // KP +
  0x5a, // KP EN
  0x69, // KP 1
  0x72, // KP 2
  0x7a, // KP 3
  0x6b, // KP 4
  0x73, // KP 5
  0x74, // KP 6
  0x6c, // KP 7
  // 0x60 - 0x6f
  0x75, // KP 8
  0x7d, // KP 9
  0x70, // KP 0
  0x71, // KP .
  0x61, // EUROPE_2
  0x2f, // APPS
  0x37, // POWER
  0x0f, // KEYPAD_EQUAL
  0x08, // F13
  0x10, // F14
  0x18, // F15
  0x20, // F16
  0x28, // F17
  0x30, // F18
  0x38, // F19
  0x40, // F20
  // 0x70 - 0x73
  0x48, // F21
  0x50, // F22
  0x57, // F23
  0x5f  // F24
};

u8 const hid2ps2_3[] = {
  // 0x00 - 0x0f
  0, 0, 0, 0, // NONE
  0x1C,       // A
  0x32,       // B
  0x21,       // C
  0x23,       // D
  0x24,       // E
  0x2B,       // F
  0x34,       // G
  0x33,       // H
  0x43,       // I
  0x3B,       // J
  0x42,       // K
  0x4B,       // L
  // 0x10 - 0x1f
  0x3A,       // M
  0x31,       // N
  0x44,       // O
  0x4D,       // P
  0x15,       // Q
  0x2D,       // R
  0x1B,       // S
  0x2C,       // T
  0x3C,       // U
  0x2A,       // V
  0x1D,       // W
  0x22,       // X
  0x35,       // Y
  0x1A,       // Z
  0x16,       // 1
  0x1E,       // 2
  // 0x20 - 0x2f
  0x26,       // 3
  0x25,       // 4
  0x2E,       // 5
  0x36,       // 6
  0x3D,       // 7 ?
  0x3E,       // 8
  0x46,       // 9
  0x45,       // 0
  0x5A,       // ENTER
  0x08,       // ESC
  0x66,       // BKSP
  0x0D,       // TAB
  0x29,       // SPACE
  0x4E,       // -
  0x55,       // =
  0x54,       // [
  // 0x30 - 0x3f
  0x5B,       // ]
  0x5C,       // BACKSLASH
  0x53,       // EUROPE_1
  0x4C,       // ;
  0x52,       // ' APOSTROPHE
  0x0E,       // ` GRAVE
  0x41,       // ,
  0x49,       // .
  0x4A,       // /
  0x14,       // CAPS
  0x07,       // F1
  0x0F,       // F2
  0x17,       // F3
  0x1F,       // F4
  0x27,       // F5
  0x2F,       // F6
  // 0x40 - 0x4f
  0x37,       // F7
  0x3F,       // F8
  0x47,       // F9
  0x4F,       // F10
  0x56,       // F11
  0x5E,       // F12
  0x57,       // PRNT SCRN
  0x5F,       // SCROLL
  0x62,       // PAUSE
  0x67,       // INSERT
  0x6E,       // HOME
  0x6F,       // PG UP
  0x64,       // DELETE
  0x65,       // END
  0x6D,       // PG DN
  0x6A,       // R ARROW
  // 0x50 - 0x5f
  0x61,       // L ARROW
  0x60,       // D ARROW
  0x63,       // U ARROW
  0x76,       // NUM
  0x77,       // KP /
  0x7E,       // KP *
  0x84,       // KP -
  0x7C,       // KP +
  0x79,       // KP EN
  0x69,       // KP 1
  0x72,       // KP 2
  0x7A,       // KP 3
  0x6B,       // KP 4
  0x73,       // KP 5
  0x74,       // KP 6
  0x6C,       // KP 7
  // 0x60 - 0x6f
  0x75,       // KP 8
  0x7D,       // KP 9
  0x70,       // KP 0
  0x71,       // KP .
  0x13,       // EUROPE_2
  0x00,       // APPS
  0x00,       // POWER
  0x00,       // KEYPAD_EQUAL
  0x00,       // F13
  0x00,       // F14
  0x00,       // F15
  0x00,       // F16
  0x00,       // F17
  0x00,       // F18
  0x00,       // F19
  0x00,       // F20
  // 0x70 - 0x73
  0x00,       // F21
  0x00,       // F22
  0x00,       // F23
  0x00        // F24
};
