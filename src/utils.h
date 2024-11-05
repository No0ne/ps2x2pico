/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 No0ne (https://github.com/No0ne)
 *           (c) 2024 Bernd Strobel
 *           (c) 2024 pdaxrom (https://github.com/pdaxrom)
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

#define MAX_REPORT 4
#define MAX_REPORT_ITEMS 32

typedef struct {
  u16 page;
  u16 usage;
} hid_usage_t;

typedef struct {
  u32 type;
  u8 exponent;
} hid_unit_t;

typedef struct {
  s32 min;
  s32 max;
} hid_minmax_t;

typedef struct {
  hid_usage_t usage;
  hid_unit_t unit;
  hid_minmax_t logical;
  hid_minmax_t physical;
} hid_report_item_attributes_t;

typedef struct {
  u16 bit_offset;
  u8 bit_size;
  u8 item_type;
  u16 item_flags;
  hid_report_item_attributes_t attributes;
} hid_report_item_t;

typedef struct {
  u8 report_id;
  u8 usage;
  u16 usage_page;
  u8 num_items;
  hid_report_item_t	item[MAX_REPORT_ITEMS];
} hid_report_info_t;

typedef struct {
  const hid_report_item_t *x;
  const hid_report_item_t *y;
  const hid_report_item_t *wheel;
  const hid_report_item_t *acpan;
  const hid_report_item_t *lb;
  const hid_report_item_t *mb;
  const hid_report_item_t *rb;
  const hid_report_item_t *bw;
  const hid_report_item_t *fw;
} ms_items_t;

bool hid_parse_find_bit_item_by_page(hid_report_info_t* report_info_arr, u8 type, u16 page, u8 bit, const hid_report_item_t **item);
bool hid_parse_find_item_by_usage(hid_report_info_t* report_info_arr, u8 type, u16 usage, const hid_report_item_t **item);
bool hid_parse_get_item_value(const hid_report_item_t *item, const u8 *report, u8 len, s32 *value);

s32 to_signed_value(const hid_report_item_t *item, const u8 *report, u16 len);
s8 to_signed_value8(const hid_report_item_t *item, const u8 *report, u16 len);
bool to_bit_value(const hid_report_item_t *item, const u8 *report, u16 len);

u8 hid_parse_report_descriptor(hid_report_info_t* report_info_arr, u8 arr_count, u8 const* desc_report, u16 desc_len);
void mouse_setup(hid_report_info_t *info);
void print_utf16(u16 *temp_buf, size_t buf_len);