
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 No0ne (https://github.com/No0ne)
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

#define KB_EXT_PFX_E0 0xe0 // This is the extended code prefix used in sets 1 and 2
#define KB_BREAK_2_3 0xf0 // The prefix 0xf0 is the break code prefex in sets 2 and 3 (is send when key is released)
#define HID2PS2_IDX_MAX 0x73
#define MOD2PS2_IDX_MAX 7
#define IS_VALID_KEY(key) (key <= HID2PS2_IDX_MAX)

extern u8 const ext_code_keys_1_2[]; // keys in this list need to have KB_EXT_PFX_E0 sent before their actual code
extern u8 const ext_code_modifier_keys_1_2[]; // keys in this list need to have KB_EXT_PFX_E0 sent before their actual code
extern u8 const mod2ps2_1[];
extern u8 const mod2ps2_2[];
extern u8 const mod2ps2_3[];
extern u8 const hid2ps2_1[];
extern u8 const hid2ps2_2[];
extern u8 const hid2ps2_3[];
extern u8 const prt_scn_make_1[];
extern u8 const prt_scn_break_1[];
extern u8 const pause_make_1[];
extern u8 const prt_scn_make_2[];
extern u8 const prt_scn_break_2[];
extern u8 const pause_make_2[];