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
#include "utils.h"
#include "tusb.h"

bool hid_parse_find_bit_item_by_page(hid_report_info_t* report_info_arr, u8 type, u16 page, u8 bit, const hid_report_item_t **item) {
  for(int i = 0; i < report_info_arr->num_items; i++) {
    if(report_info_arr->item[i].item_type == type && report_info_arr->item[i].attributes.usage.page == page) {
      if(item) {
        if(i+bit < report_info_arr->num_items && report_info_arr->item[i+bit].item_type == type && report_info_arr->item[i+bit].attributes.usage.page == page) {
          *item = &report_info_arr->item[i + bit];
        } else {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

bool hid_parse_find_item_by_usage(hid_report_info_t* report_info_arr, u8 type, u16 usage, const hid_report_item_t **item) {
  for(int i = 0; i < report_info_arr->num_items; i++) {
    if(report_info_arr->item[i].item_type == type && report_info_arr->item[i].attributes.usage.usage == usage) {
      if(item) {
        *item = &report_info_arr->item[i];
      }
      return true;
    }
  }
  return false;
}

bool hid_parse_get_item_value(const hid_report_item_t *item, const u8 *report, u8 len, s32 *value) {
  if(item == NULL || report == NULL) return false;
  u8 boffs = item->bit_offset & 0x07;
  u8 pos = 8 - boffs;
  u8 offs  = item->bit_offset >> 3;
  u32 mask = ~(0xffffffff << item->bit_size);
  s32 val = report[offs++] >> boffs;
  while(item->bit_size > pos) {
    val |= (report[offs++] << pos);
    pos += 8;
  }
  val &= mask;
  if(item->attributes.logical.min < 0) {
    if(val & (1 << (item->bit_size - 1))) {
      val |= (0xffffffff << item->bit_size);
    }
  }
  *value = val;
  return true;
}

s32 to_signed_value(const hid_report_item_t *item, const u8 *report, u16 len) {
  s32 value = 0;
  if(hid_parse_get_item_value(item, report, len, &value)) {
    s32 midval = ((item->attributes.logical.max - item->attributes.logical.min) >> 1) + 1;
    value -= midval;
    value <<= (16 - item->bit_size);
  }
  if(value >  32767) value =  32767;
  if(value < -32767) value = -32767;
  return value;
}

s8 to_signed_value8(const hid_report_item_t *item, const u8 *report, u16 len) {
  s32 value = 0;
  if(hid_parse_get_item_value(item, report, len, &value)) {
      value = (value > 127) ? 127 : (value < -127) ? -127 : value;
  }
  return value;
}

bool to_bit_value(const hid_report_item_t *item, const u8 *report, u16 len) {
  s32 value = 0;
  hid_parse_get_item_value(item, report, len, &value);
  return value ? true : false;
}

u8 hid_parse_report_descriptor(hid_report_info_t* report_info_arr, u8 arr_count, u8 const* desc_report, u16 desc_len) {
  union TU_ATTR_PACKED {
    u8 byte;
    struct TU_ATTR_PACKED {
      u8 size: 2;
      u8 type: 2;
      u8 tag: 4;
    };
  } header;

  tu_memclr(report_info_arr, arr_count*sizeof(tuh_hid_report_info_t));

  u8 report_num = 0;
  hid_report_info_t* info = report_info_arr;

  u16 ri_global_usage_page = 0;
  s32 ri_global_logical_min = 0;
  s32 ri_global_logical_max = 0;
  s32 ri_global_physical_min = 0;
  s32 ri_global_physical_max = 0;
  u8 ri_report_count = 0;
  u8 ri_report_size = 0;
  u8 ri_report_usage_count = 0;

  u8 ri_collection_depth = 0;

  while(desc_len && report_num < arr_count) {
    header.byte = *desc_report++;
    desc_len--;

    u8 const tag  = header.tag;
    u8 const type = header.type;
    u8 const size = header.size;

    u32 data;
    u32 sdata;
    switch(size) {
      case 1: data = desc_report[0]; sdata = ((data & 0x80) ? 0xffffff00 : 0 ) | data; break;
      case 2: data = (desc_report[1] << 8) | desc_report[0]; sdata = ((data & 0x8000) ? 0xffff0000 : 0 ) | data;  break;
      case 3: data = (desc_report[3] << 24) | (desc_report[2] << 16) | (desc_report[1] << 8) | desc_report[0]; sdata = data; break;
      default: data = 0; sdata = 0;
    }

    switch(type) {
      case RI_TYPE_MAIN:
        switch(tag) {
          case RI_MAIN_INPUT:
          case RI_MAIN_OUTPUT:
          case RI_MAIN_FEATURE:
            u16 offset = (info->num_items == 0) ? 0 : (info->item[info->num_items - 1].bit_offset + info->item[info->num_items - 1].bit_size);
            for(int i = 0; i < ri_report_count; i++) {
              if(info->num_items + i < MAX_REPORT_ITEMS) {
                info->item[info->num_items + i].bit_offset = offset;
                info->item[info->num_items + i].bit_size = ri_report_size;
                info->item[info->num_items + i].item_type = tag;
                info->item[info->num_items + i].attributes.logical.min = ri_global_logical_min;
                info->item[info->num_items + i].attributes.logical.max = ri_global_logical_max;
                info->item[info->num_items + i].attributes.physical.min = ri_global_physical_min;
                info->item[info->num_items + i].attributes.physical.max = ri_global_physical_max;
                info->item[info->num_items + i].attributes.usage.page = ri_global_usage_page;
                if(ri_report_usage_count != ri_report_count && ri_report_usage_count > 0) {
                  if(i >= ri_report_usage_count) {
                    info->item[info->num_items + i].attributes.usage = info->item[info->num_items + i - 1].attributes.usage;
                  }
                }
              }
              offset += ri_report_size;
            }
            info->num_items += ri_report_count;
            ri_report_usage_count = 0;
          break;

          case RI_MAIN_COLLECTION:
            ri_collection_depth++;
          break;

          case RI_MAIN_COLLECTION_END:
            ri_collection_depth--;
            if(ri_collection_depth == 0) {
              info++;
              report_num++;
            }
          break;
        }
      break;

      case RI_TYPE_GLOBAL:
        switch(tag) {
          case RI_GLOBAL_USAGE_PAGE:
            if(ri_collection_depth == 0) {
              info->usage_page = data;
            }
            ri_global_usage_page = data;
          break;

          case RI_GLOBAL_LOGICAL_MIN:
            ri_global_logical_min = sdata;
          break;
          case RI_GLOBAL_LOGICAL_MAX:
            ri_global_logical_max = sdata;
          break;
          case RI_GLOBAL_PHYSICAL_MIN:
            ri_global_physical_min = sdata;
          break;
          case RI_GLOBAL_PHYSICAL_MAX:
            ri_global_physical_max = sdata;
          break;
          case RI_GLOBAL_REPORT_ID:
            info->report_id = data;
          break;
          case RI_GLOBAL_REPORT_SIZE:
            ri_report_size = data;
          break;
          case RI_GLOBAL_REPORT_COUNT:
            ri_report_count = data;
          break;
        }
      break;

      case RI_TYPE_LOCAL:
        if(tag == RI_LOCAL_USAGE) {
          if(ri_collection_depth == 0) {
            info->usage = data;
          } else {
            if(ri_report_usage_count < MAX_REPORT_ITEMS) {
              info->item[info->num_items + ri_report_usage_count].attributes.usage.usage = data;
              ri_report_usage_count++;
            }
          }
        }
      break;
    }

    desc_report += size;
    desc_len    -= size;
  }

  return report_num;
}

void convert_utf16le_to_utf8(const u16 *utf16, size_t utf16_len, u8 *utf8, size_t utf8_len) {
  // TODO: Check for runover.
  (void)utf8_len;
  // Get the UTF-16 length out of the data itself.
  for(size_t i = 0; i < utf16_len; i++) {
    u16 chr = utf16[i];
    if(chr < 0x80) {
      *utf8++ = chr & 0xffu;
    } else if(chr < 0x800) {
      *utf8++ = (u8)(0xc0 | (chr >> 6 & 0x1f));
      *utf8++ = (u8)(0x80 | (chr >> 0 & 0x3f));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (u8)(0xe0 | (chr >> 12 & 0x0f));
      *utf8++ = (u8)(0x80 | (chr >> 6 & 0x3f));
      *utf8++ = (u8)(0x80 | (chr >> 0 & 0x3f));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
int count_utf8_bytes(const u16 *buf, size_t len) {
  size_t total_bytes = 0;
  for(size_t i = 0; i < len; i++) {
    u16 chr = buf[i];
    if(chr < 0x80) {
      total_bytes += 1;
    } else if (chr < 0x800) {
      total_bytes += 2;
    } else {
      total_bytes += 3;
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
  return (int)total_bytes;
}

void print_utf16(u16 *temp_buf, size_t buf_len) {
  if((temp_buf[0] & 0xff) == 0) return;
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(u16);
  size_t utf8_len = (size_t) count_utf8_bytes(temp_buf + 1, utf16_len);
  convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (u8 *) temp_buf, sizeof(u16) * buf_len);
  ((u8*) temp_buf)[utf8_len] = '\0';
  printf("%s", (char*)temp_buf);
}
