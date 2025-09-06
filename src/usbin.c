/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 No0ne (https://github.com/No0ne)
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

struct {
  u8 report_count;
  hid_report_info_t report_info[MAX_REPORT];
  u8 dev_addr;
  u8 modifiers;
  u8 boot[MAX_BOOT];
  u8 nkro[MAX_NKRO];
  bool leds;
} hid_info[CFG_TUH_HID];

ms_items_t ms_items;

u8 kb_set_led = 0;
u8 kb_inst_loop = 0;
u8 kb_last_dev = 0;

s64 kb_set_led_callback() {
  if(hid_info[kb_inst_loop].leds && kb_last_dev != hid_info[kb_inst_loop].dev_addr) {
    tuh_hid_set_report(hid_info[kb_inst_loop].dev_addr, kb_inst_loop, 0, HID_REPORT_TYPE_OUTPUT, &kb_set_led, 1);
    kb_last_dev = hid_info[kb_inst_loop].dev_addr;
  }

  kb_inst_loop++;

  if(kb_inst_loop == CFG_TUH_HID) {
    kb_inst_loop = 0;
    kb_last_dev = 0;
    return 0;
  }

  return 1000;
}

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
            if(ri_report_usage_count > ri_report_count) {
              info->num_items += ri_report_usage_count - ri_report_count;
            }
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
            ri_report_usage_count = 0;
            ri_report_count = 0;
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

  char* hidprotostr = "generic";
  if(hid_if_proto == HID_ITF_PROTOCOL_KEYBOARD) hidprotostr = "keyboard";
  if(hid_if_proto == HID_ITF_PROTOCOL_MOUSE) hidprotostr = "mouse";

  printf("\nHID(%d,%d,%s) mounted\n", dev_addr, instance, hidprotostr);
  printf(" VID: %04x  PID: %04x\n", vid, pid);

  hid_info[instance].report_count = hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
  printf(" HID has %u reports\n", hid_info[instance].report_count);

  if(!tuh_hid_receive_report(dev_addr, instance)) {
    printf(" ERROR: Could not register for HID(%d,%d,%s)!\n", dev_addr, instance, hidprotostr);
  } else {
    printf(" HID(%d,%d,%s) registered for reports\n", dev_addr, instance, hidprotostr);

    if(hid_if_proto == HID_ITF_PROTOCOL_MOUSE) {
      hid_info[instance].leds = false;
    } else {
      hid_info[instance].dev_addr = dev_addr;
      hid_info[instance].modifiers = 0;
      memset(hid_info[instance].boot, 0, MAX_BOOT);
      memset(hid_info[instance].nkro, 0, MAX_NKRO);
      hid_info[instance].leds = true;
    }

    board_led_write(1);
  }
}

void tuh_hid_umount_cb(u8 dev_addr, u8 instance) {
  printf("HID(%d,%d) unmounted\n", dev_addr, instance);
  hid_info[instance].dev_addr = 0;
  hid_info[instance].leds = false;
}

void tuh_hid_report_received_cb(u8 dev_addr, u8 instance, u8 const* report, u16 len) {
  u8 const rpt_count = hid_info[instance].report_count;
  hid_report_info_t *rpt_infos = hid_info[instance].report_info;
  hid_report_info_t *rpt_info = NULL;

  if(rpt_count == 1 && rpt_infos[0].report_id == 0) {
    rpt_info = &rpt_infos[0];
  } else {
    u8 const rpt_id = report[0];
    for(u8 i = 0; i < rpt_count; i++) {
      if(rpt_id == rpt_infos[i].report_id) {
        rpt_info = &rpt_infos[i];
        break;
      }
    }
    report++;
    len--;
  }

  if(!rpt_info) return;
  board_led_write(1);
  tuh_hid_receive_report(dev_addr, instance);

  if(tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_MOUSE) {

    if(tuh_hid_get_protocol(dev_addr, instance) == HID_PROTOCOL_BOOT) {
      ms_send_movement(report[0], report[1], report[2], report[3]);

    } else if(rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP && rpt_info->usage == HID_USAGE_DESKTOP_MOUSE) {
      ms_setup(rpt_info);
      ms_report_receive(report, len);

    } else {
      printf("UKNOWN mouse  usage_page: %02x  usage: %02x\n", rpt_info->usage_page, rpt_info->usage);
    }

  } else {

    if(rpt_info->usage_page != HID_USAGE_PAGE_DESKTOP || rpt_info->usage != HID_USAGE_DESKTOP_KEYBOARD) {
      printf("UNKNOWN key  usage_page: %02x  usage: %02x\n", rpt_info->usage_page, rpt_info->usage);
      return;
    }

    if(report[0] != hid_info[instance].modifiers) {
      for(u8 i = 0; i < 8; i++) {
        if((report[0] >> i & 1) != (hid_info[instance].modifiers >> i & 1)) {
          kb_send_key(i + HID_KEY_CONTROL_LEFT, report[0] >> i & 1);
        }
      }

      hid_info[instance].modifiers = report[0];
    }

    report++;
    len--;

    if(len > 12 && len < 31) {
      for(u8 i = 0; i < len && i < MAX_NKRO; i++) {
        for(u8 j = 0; j < 8; j++) {
          if((report[i] >> j & 1) != (hid_info[instance].nkro[i] >> j & 1)) {
            kb_send_key(i*8+j, report[i] >> j & 1);
          }
        }
      }

      memcpy(hid_info[instance].nkro, report, len > MAX_NKRO ? MAX_NKRO : len);
      return;
    }

    switch(len) {
      case 8:
      case 7:
        report++;
        // fall through
      case 6:
        for(u8 i = 0; i < MAX_BOOT; i++) {
          if(hid_info[instance].boot[i]) {
            bool brk = true;

            for(u8 j = 0; j < MAX_BOOT; j++) {
              if(hid_info[instance].boot[i] == report[j]) {
                brk = false;
                break;
              }
            }

            if(brk) kb_send_key(hid_info[instance].boot[i], false);
          }
        }

        for(u8 i = 0; i < MAX_BOOT; i++) {
          if(report[i]) {
            bool make = true;

            for(u8 j = 0; j < MAX_BOOT; j++) {
              if(report[i] == hid_info[instance].boot[j]) {
                make = false;
                break;
              }
            }

            if(make) kb_send_key(report[i], true);
          }
        }

        memcpy(hid_info[instance].boot, report, MAX_BOOT);
      return;
    }

    printf("UNKNOWN keyboard  len: %d\n", len);
  }
}
