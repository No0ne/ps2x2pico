/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 No0ne (https://github.com/No0ne)
 *           (c) 2023 Dustin Hoffman
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
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "tusb.h"
#include "hardware/pio.h"
#include "pico/util/queue.h"

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

void kb_init(u8 gpio_out, u8 gpio_in);
void kb_send_key(u8 key, bool is_key_pressed, u8 modifiers);
void tuh_kb_set_leds(u8 leds);
bool kb_task();

void ms_init(u8 gpio_out, u8 gpio_in);
void ms_send_movement(u8 buttons, s8 x, s8 y, s8 z);
bool ms_task();


u32 ps2_frame(u8 byte);
typedef void (*rx_callback)(u8 byte, u8 prev_byte);

typedef struct {
  PIO pio;
  uint sm;
  queue_t qbytes;
  queue_t qpacks;
  rx_callback rx;
  u8 last_rx;
  u8 last_tx;
  u8 sent;
  u8 busy;
} ps2out;

void ps2out_init(ps2out* this, PIO pio, u8 data_pin, rx_callback rx);
void ps2out_task(ps2out* this);


typedef struct {
  PIO pio;
  uint sm;
  u8 state;
  u8 byte;
} ps2in;

void ps2in_init(ps2in* this, PIO pio, u8 data_pin);
void ps2in_task(ps2in* this, ps2out* out);
void ps2in_reset(ps2in* this);
void ps2in_set(ps2in* this, u8 command, u8 byte);


#define KB_EXT_PFX_E0 0xe0 // This is the extended code prefix used in sets 1 and 2
#define KB_BREAK_2_3 0xf0 // The prefix 0xf0 is the break code prefex in sets 2 and 3 (is send when key is released)
#define HID2PS2_IDX_MAX 0x73
#define IS_VALID_KEY(key) (key <= HID2PS2_IDX_MAX || (key >= HID_KEY_CONTROL_LEFT && key <= HID_KEY_GUI_RIGHT))
#define IS_MOD_KEY(key) (key >= HID_KEY_CONTROL_LEFT && key <= HID_KEY_GUI_RIGHT)

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
extern u8 const break_make_1[];
extern u8 const pause_make_1[];
extern u8 const prt_scn_make_2[];
extern u8 const prt_scn_break_2[];
extern u8 const break_make_2[];
extern u8 const pause_make_2[];
