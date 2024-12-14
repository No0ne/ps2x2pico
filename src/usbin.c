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
#include "bsp/board_api.h"

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
  u8 bit_count;
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
  const hid_report_item_t *z;
  const hid_report_item_t *lb;
  const hid_report_item_t *mb;
  const hid_report_item_t *rb;
  const hid_report_item_t *bw;
  const hid_report_item_t *fw;
} ms_items_t;

u8 kb_leds = 0;
u8 kb_modifiers = 0;
u8 kb_keys[120] = {0};
ms_items_t ms_items;
//char device_str[50];
//char manufacturer_str[50];

struct {
  u8 dev_addr;
  u8 instance;
} keyboards[8];

struct {
  u8 report_count;
  hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

bool hid_parse_find_bit_item_by_page(hid_report_info_t* report_info_arr, u8 type, u16 page, u8 bit, const hid_report_item_t **item) {
  for(u8 i = 0; i < report_info_arr->num_items; i++) {
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
  for(u8 i = 0; i < report_info_arr->num_items; i++) {
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
  (void)len;
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
  if(value > 32767) value = 32767;
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

  tu_memclr(report_info_arr, arr_count * sizeof(tuh_hid_report_info_t));

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

    u16 offset;
    switch(type) {
      case RI_TYPE_MAIN:
        switch(tag) {
          case RI_MAIN_INPUT:
          case RI_MAIN_OUTPUT:
          case RI_MAIN_FEATURE:
            offset = (info->num_items == 0) ? 0 : (info->item[info->num_items - 1].bit_offset + info->item[info->num_items - 1].bit_size);
            for(u8 i = 0; i < ri_report_count; i++) {
              if(info->num_items + i < MAX_REPORT_ITEMS) {
                info->item[info->num_items + i].bit_offset = offset;
                info->item[info->num_items + i].bit_size = ri_report_size;
                info->item[info->num_items + i].bit_count = ri_report_count;
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
    desc_len -= size;
  }

  return report_num;
}

u8 hid_parse_keyboard_modifiers(hid_report_info_t* report_info_arr, const u8 *report, u8 len) {
  u8 modifiers = 0;
  u8 bit = 0;
  for(u8 i = 0; i < report_info_arr->num_items; i++) {
    if(report_info_arr->item[i].item_type == RI_MAIN_INPUT &&
       report_info_arr->item[i].attributes.usage.page == HID_USAGE_PAGE_KEYBOARD &&
       report_info_arr->item[i].bit_size == 1 &&
       report_info_arr->item[i].bit_count == 8) {
      modifiers |= to_bit_value(&report_info_arr->item[i], report, len) << bit;
      bit++;
      if(bit == 8) break;
    }
  }
  return modifiers;
}

bool hid_parse_keyboard_is_nkro(hid_report_info_t* report_info_arr) {
  for(u8 i = 0; i < report_info_arr->num_items; i++) {
    if(report_info_arr->item[i].item_type == RI_MAIN_INPUT &&
       report_info_arr->item[i].attributes.usage.page == HID_USAGE_PAGE_KEYBOARD &&
       report_info_arr->item[i].bit_size == 1 &&
       report_info_arr->item[i].bit_count >= 32) {
      return true;
    }
  }
  return false;
}

/*void convert_utf16le_to_utf8(const u16 *utf16, size_t utf16_len, u8 *utf8, size_t utf8_len) {
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
}*/

void ms_setup(hid_report_info_t *info) {
  ms_items_t *items = &ms_items;
  memset(items, 0, sizeof(ms_items_t));
  hid_parse_find_item_by_usage(info, RI_MAIN_INPUT, HID_USAGE_DESKTOP_X, &items->x);
  hid_parse_find_item_by_usage(info, RI_MAIN_INPUT, HID_USAGE_DESKTOP_Y, &items->y);
  hid_parse_find_item_by_usage(info, RI_MAIN_INPUT, HID_USAGE_DESKTOP_WHEEL, &items->z);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 0, &items->lb);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 1, &items->rb);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 2, &items->mb);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 3, &items->bw);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 4, &items->fw);
}

void ms_report_receive(u8 const* report, u16 len) {
  ms_items_t *items = &ms_items;
  u8 buttons = 0;
  s8 x, y, z;

  if(to_bit_value(items->lb, report, len)) buttons |= 0x01;
  if(to_bit_value(items->rb, report, len)) buttons |= 0x02;
  if(to_bit_value(items->mb, report, len)) buttons |= 0x04;
  if(to_bit_value(items->bw, report, len)) buttons |= 0x08;
  if(to_bit_value(items->fw, report, len)) buttons |= 0x10;

  x = to_signed_value8(items->x, report, len);
  y = to_signed_value8(items->y, report, len);
  z = to_signed_value8(items->z, report, len);

  ms_send_movement(buttons, x, y, z);
}

void kb_report_receive(u8 modifiers, u8 const* report, u16 len) {
  if(modifiers != kb_modifiers) {
    // modifiers have changed
    u8 rbits = modifiers;
    u8 pbits = kb_modifiers;

    for(u8 j = 0; j < 8; j++) {
      if((rbits & 1) != (pbits & 1)) {
        kb_send_key(HID_KEY_CONTROL_LEFT + j, rbits & 1, modifiers);
      }

      rbits = rbits >> 1;
      pbits = pbits >> 1;
    }

    kb_modifiers = modifiers;
  }

  // go over activated non-modifier keys in prev_rpt and
  // check if they are still in the current report
  for(u8 i = 0; i < len; i++) {
    if(kb_keys[i]) {
      bool brk = true;

      for(u8 j = 0; j < len; j++) {
        if(kb_keys[i] == report[j]) {
          brk = false;
          break;
        }
      }

      if(brk) {
        // send break if key not pressed anymore
        kb_send_key(kb_keys[i], false, modifiers);
      }
    }
  }

  // go over activated non-modifier keys in report and check if they were
  // already in prev_rpt.
  for(u8 i = 0; i < len; i++) {
    if(report[i]) {
      bool make = true;

      for(u8 j = 0; j < len; j++) {
        if(report[i] == kb_keys[j]) {
          make = false;
          break;
        }
      }

      // send make if key was in the current report the first time
      if(make) {
        kb_send_key(report[i], true, modifiers);
      }
    }
  }

  memset(kb_keys, 0, sizeof(kb_keys));
  memcpy(kb_keys, report, len);
}

void tuh_kb_set_leds(u8 leds) {
  for(u8 i = 0; i < 8; i++) {
    if(keyboards[i].dev_addr != 0) {
      kb_leds = leds;
      tuh_hid_set_report(keyboards[i].dev_addr, keyboards[i].instance, 0, HID_REPORT_TYPE_OUTPUT, &kb_leds, sizeof(kb_leds));
    }
  }
}

void tuh_hid_mount_cb(u8 dev_addr, u8 instance, u8 const* desc_report, u16 desc_len) {
  // This happens if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE.
  // Consider increasing #define CFG_TUH_ENUMERATION_BUFSIZE 256 in tusb_config.h
  if(desc_report == NULL && desc_len == 0) {
    printf("WARNING: HID(%d,%d) skipped!\n", dev_addr, instance);
    return;
  }

  hid_interface_protocol_enum_t hid_if_proto = tuh_hid_interface_protocol(dev_addr, instance);
  u16 vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  char* hidprotostr = "none";
  if(hid_if_proto == HID_ITF_PROTOCOL_KEYBOARD) hidprotostr = "keyboard";
  if(hid_if_proto == HID_ITF_PROTOCOL_MOUSE) hidprotostr = "mouse";

  printf("\nHID(%d,%d,%s) mounted\n", dev_addr, instance, hidprotostr);
  printf(" VID: %04x  PID: %04x\n", vid, pid);

  hid_info[instance].report_count = hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
  printf(" HID has %u reports\n", hid_info[instance].report_count);

  /*u16 temp_buf[128];

  printf(" Manufacturer: ");
  if(XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(dev_addr, 0x0409, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\n");

  printf(" Product:      ");
  if(XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(dev_addr, 0x0409, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\n\n");*/

  if(!tuh_hid_receive_report(dev_addr, instance)) {
    printf(" ERROR: Could not register for HID(%d,%d,%s)!\n", dev_addr, instance, hidprotostr);
  } else {
    printf(" HID(%d,%d,%s) registered for reports\n", dev_addr, instance, hidprotostr);
    if(hid_if_proto == HID_ITF_PROTOCOL_KEYBOARD) {
      for(u8 i = 0; i < 8; i++) {
        if(keyboards[i].dev_addr == 0 && keyboards[i].instance == 0) {
          keyboards[i].dev_addr = dev_addr;
          keyboards[i].instance = instance;
          break;
        }
      }
    }
    board_led_write(1);
  }
}

void tuh_hid_umount_cb(u8 dev_addr, u8 instance) {
  printf("HID(%d,%d) unmounted\n", dev_addr, instance);
  board_led_write(0);

  for(u8 i = 0; i < 8; i++) {
    if(keyboards[i].dev_addr == dev_addr && keyboards[i].instance == instance) {
      keyboards[i].dev_addr = 0;
      keyboards[i].instance = 0;
      break;
    }
  }
}

void tuh_hid_report_received_cb(u8 dev_addr, u8 instance, u8 const* report, u16 len) {
  hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
  hid_report_info_t* rpt_info = NULL;

  if(hid_info[instance].report_count == 1 && rpt_info_arr[0].report_id == 0) {
    rpt_info = &rpt_info_arr[0];
  } else {
    u8 const rpt_id = report[0];
    for(u8 i = 0; i < hid_info[instance].report_count; i++) {
      if(rpt_id == rpt_info_arr[i].report_id) {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }
    report++;
    len--;
  }

  if(!rpt_info) return;

  if(tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_MOUSE) {

    if(tuh_hid_get_protocol(dev_addr, instance) == HID_PROTOCOL_BOOT) {
      ms_send_movement(report[0], report[1], report[2], report[3]);

    } else if(rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP && rpt_info->usage == HID_USAGE_DESKTOP_MOUSE) {
      ms_setup(rpt_info);
      ms_report_receive(report, len);

    } else {
      printf("mouse unknown  usage_page: %02x  usage: %02x\n", rpt_info->usage_page, rpt_info->usage);
    }

  } else {

    if(tuh_hid_get_protocol(dev_addr, instance) == HID_PROTOCOL_BOOT) {
      u8 modifiers = report[0];
      report++; report++;
      kb_report_receive(modifiers, report, 6);

    } else if(rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP && rpt_info->usage == HID_USAGE_DESKTOP_KEYBOARD) {
      u8 modifiers = hid_parse_keyboard_modifiers(rpt_info, report, len);

      if(hid_parse_keyboard_is_nkro(rpt_info)) {
        u8 current_key = 0;
        u8 newreport[sizeof(kb_keys)] = {0};
        u8 newindex = 0;

        for(u8 i = 1; i < len && i < 16; i++) {
          for(u8 j = 0; j < 8; j++) {
            if(report[i] >> j & 1 && newindex < sizeof(kb_keys)) {
              newreport[newindex] = current_key;
              newindex++;
            }
            current_key++;
          }
        }

        kb_report_receive(modifiers, newreport, sizeof(kb_keys));

      } else if(len == 7) {
        report++;
        kb_report_receive(modifiers, report, 6);

      } else if(len == 8 || len == 9) {
        report++; report++;
        kb_report_receive(modifiers, report, 6);

      } else {
        printf("keyboard unknown  len: %02x\n", len);
      }

    } else {
      printf("keyboard unknown  usage_page: %02x  usage: %02x\n", rpt_info->usage_page, rpt_info->usage);
    }

  }

  tuh_hid_receive_report(dev_addr, instance);
}
