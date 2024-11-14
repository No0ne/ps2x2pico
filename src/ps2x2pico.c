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
#include <string.h>

#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "utils.h"
#include "ps2x2pico.h"

u8 kb_addr = 0;
u8 kb_inst = 0;
u8 kb_leds = 0;
ms_items_t ms_items;
bool ms_inited = false;
char device_str[50];
char manufacturer_str[50];

struct {
  u8 report_count;
  hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

void ms_setup(hid_report_info_t *info) {
  ms_items_t *items = &ms_items;
  memset(items, 0, sizeof(ms_items_t));
  hid_parse_find_item_by_usage(info, RI_MAIN_INPUT, HID_USAGE_DESKTOP_X, &items->x);
  hid_parse_find_item_by_usage(info, RI_MAIN_INPUT, HID_USAGE_DESKTOP_Y, &items->y);
  hid_parse_find_item_by_usage(info, RI_MAIN_INPUT, HID_USAGE_DESKTOP_WHEEL, &items->wheel);
  //hid_parse_find_item_by_usage(info, RI_MAIN_INPUT, HID_USAGE_CONSUMER_AC_PAN, &items->acpan);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 0, &items->lb);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 1, &items->rb);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 2, &items->mb);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 3, &items->bw);
  hid_parse_find_bit_item_by_page(info, RI_MAIN_INPUT, HID_USAGE_PAGE_BUTTON, 4, &items->fw);
}

void ms_report_receive(u8 dev_addr, u8 instance, u8 const* report, u16 len) {
  u8 const rpt_count = hid_info[instance].report_count;
  hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
  hid_report_info_t* rpt_info = NULL;

  if(rpt_count == 1 && rpt_info_arr[0].report_id == 0) {
    rpt_info = &rpt_info_arr[0];
  } else {
    u8 const rpt_id = report[0];
    for(u8 i=0; i < rpt_count; i++) {
      if(rpt_id == rpt_info_arr[i].report_id) {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }
    report++;
    len--;
  }

  if(!rpt_info) return;
  if(rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP && rpt_info->usage == HID_USAGE_DESKTOP_MOUSE) {
    ms_items_t *items = &ms_items;
    u8 buttons = 0;
    s8 x, y, z;

    if(!ms_inited) {
      ms_setup(rpt_info);
      ms_inited = true;
    }

    if(to_bit_value(items->lb, report, len)) buttons |= 0x01;
    if(to_bit_value(items->rb, report, len)) buttons |= 0x02;
    if(to_bit_value(items->mb, report, len)) buttons |= 0x04;
    if(to_bit_value(items->bw, report, len)) buttons |= 0x08;
    if(to_bit_value(items->fw, report, len)) buttons |= 0x10;

    x = to_signed_value8(items->x, report, len);
    y = to_signed_value8(items->y, report, len);
    z = to_signed_value8(items->wheel, report, len);
    //to_signed_value8(items->acpan, report, len);

    ms_send_movement(buttons, x, y, z);
  }
}

void tuh_kb_set_leds(u8 leds) {
  if(kb_addr) {
    kb_leds = leds;
    printf("HID(%d,%d): LEDs = %d\n", kb_addr, kb_inst, kb_leds);
    tuh_hid_set_report(kb_addr, kb_inst, 0, HID_REPORT_TYPE_OUTPUT, &kb_leds, sizeof(kb_leds));
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

  char* hidprotostr;
  switch (hid_if_proto) {
    case HID_ITF_PROTOCOL_NONE:
      hidprotostr = "NONE";
      break;
    case HID_ITF_PROTOCOL_KEYBOARD:
      hidprotostr = "KEYBOARD";
      break;
    case HID_ITF_PROTOCOL_MOUSE:
      hidprotostr = "MOUSE";
      //tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
      hid_info[instance].report_count = hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
      printf("HID has %u reports\n", hid_info[instance].report_count);
      break;
    default:
      hidprotostr = "UNKNOWN";
      break;
  };

  printf("HID(%d,%d,%s) mounted\n", dev_addr, instance, hidprotostr);
  printf(" ID: %04x:%04x\n", vid, pid);
 
  u16 temp_buf[128];

  printf(" Manufacturer: ");
  if(XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(dev_addr, 0x0409, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\n");

  printf(" Product:      ");
  if(XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(dev_addr, 0x0409, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\n\n");

  if(hid_if_proto == HID_ITF_PROTOCOL_KEYBOARD || hid_if_proto == HID_ITF_PROTOCOL_MOUSE) {
    if(!tuh_hid_receive_report(dev_addr, instance)) {
      printf("ERROR: Could not register for HID(%d,%d,%s)!\n", dev_addr, instance, hidprotostr);
    } else {
      printf("HID(%d,%d,%s) registered for reports\n", dev_addr, instance, hidprotostr);
      if(hid_if_proto == HID_ITF_PROTOCOL_KEYBOARD) {
          // TODO: This needs to be addressed if we want to have multiple connected kbds working correctly! 
          // Only relevant for KB LEDS though.
          // Could be a list of all connected kbds, so we could set the LEDs on each.
          kb_addr = dev_addr;
          kb_inst = instance;
      }
      board_led_write(1);
    }
  }
}

void tuh_hid_umount_cb(u8 dev_addr, u8 instance) {
  printf("HID(%d,%d) unmounted\n", dev_addr, instance);
  board_led_write(0);
  
  if(dev_addr == kb_addr && instance == kb_inst) {
    kb_addr = 0;
    kb_inst = 0;
  } else {
    ms_inited = false;
  }
  //tuh_deinit(TUH_OPT_RHPORT);
  //printf("deinit(%d)\n", TUH_OPT_RHPORT);
  //tusb_init();
  //printf("init()\n");
}

void tuh_hid_report_received_cb(u8 dev_addr, u8 instance, u8 const* report, u16 len) {
  switch(tuh_hid_interface_protocol(dev_addr, instance)) {
    case HID_ITF_PROTOCOL_KEYBOARD:
      kb_usb_receive(report, len);
      tuh_hid_receive_report(dev_addr, instance);
    break;
    
    case HID_ITF_PROTOCOL_MOUSE:
      if(tuh_hid_get_protocol(dev_addr, instance) == HID_PROTOCOL_BOOT) {
        ms_usb_receive(report);
        tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
      } else {
        ms_report_receive(dev_addr, instance, report, len);
      }
      tuh_hid_receive_report(dev_addr, instance);
    break;
  }
}

int main() {
  board_init();
  printf("\n%s-%s\n", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);
  
  gpio_init(LVOUT);
  gpio_init(LVIN);
  gpio_set_dir(LVOUT, GPIO_OUT);
  gpio_set_dir(LVIN, GPIO_OUT);
  gpio_put(LVOUT, 1);
  gpio_put(LVIN, 1);
  
  tusb_init();
  kb_init(KBOUT, KBIN);
  ms_init(MSOUT, MSIN);
  
  while(1) {
    tuh_task();
    kb_task();
    ms_task();
  }
}

void reset() {
  printf("\n\n *** PANIC via tinyusb: watchdog reset!\n\n");
  watchdog_enable(100, false);
}
