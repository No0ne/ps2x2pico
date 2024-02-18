/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 No0ne (https://github.com/No0ne)
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

#include "hardware/gpio.h"
#include "bsp/board.h"
#include "tusb.h"
#include "ps2x2pico.h"

#include "ps2in.pio.h"

u8 kb_addr = 0;
u8 kb_inst = 0;
u8 kb_leds = 0;

void tuh_kb_set_leds(u8 leds) {
  if(kb_addr) {
    kb_leds = leds;
    printf("HID device address = %d, instance = %d, LEDs = %d\n", kb_addr, kb_inst, kb_leds);
    tuh_hid_set_report(kb_addr, kb_inst, 0, HID_REPORT_TYPE_OUTPUT, &kb_leds, sizeof(kb_leds));
  }
}

void tuh_hid_mount_cb(u8 dev_addr, u8 instance, u8 const* desc_report, u16 desc_len) {
  printf("HID device address = %d, instance = %d is mounted\n", dev_addr, instance);

  switch(tuh_hid_interface_protocol(dev_addr, instance)) {
    case HID_ITF_PROTOCOL_KEYBOARD:
      printf("HID Interface Protocol = Keyboard\n");
      
      kb_addr = dev_addr;
      kb_inst = instance;
      kb_reset();
      
      tuh_hid_receive_report(dev_addr, instance);
    break;
    
    case HID_ITF_PROTOCOL_MOUSE:
      printf("HID Interface Protocol = Mouse\n");
      //tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
      tuh_hid_receive_report(dev_addr, instance);
    break;
  }
}

void tuh_hid_umount_cb(u8 dev_addr, u8 instance) {
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
  
  if(dev_addr == kb_addr && instance == kb_inst) {
    kb_addr = 0;
    kb_inst = 0;
  }
}

void tuh_hid_report_received_cb(u8 dev_addr, u8 instance, u8 const* report, u16 len) {
  board_led_write(1);
  
  switch(tuh_hid_interface_protocol(dev_addr, instance)) {
    case HID_ITF_PROTOCOL_KEYBOARD:
      kb_usb_receive(report);
      tuh_hid_receive_report(dev_addr, instance);
    break;
    
    case HID_ITF_PROTOCOL_MOUSE:
      ms_usb_receive(report);
      tuh_hid_receive_report(dev_addr, instance);
    break;
  }
  
  board_led_write(0);
}

int sm;
u8 next;
bool bnext = false;

void main() {
  board_init();
  printf("\n%s-%s\n", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);
  
  gpio_init(LVPWR);
  gpio_set_dir(LVPWR, GPIO_OUT);
  gpio_put(LVPWR, 1);
  
  u8 inpwr = 18;
  gpio_init(inpwr);
  gpio_set_dir(inpwr, GPIO_OUT);
  gpio_put(inpwr, 1);
  
  tusb_init();
  kb_init(KBDAT);
  ms_init(MSDAT);
  
  sm = pio_claim_unused_sm(pio1, true);
  ps2in_program_init(pio1, sm, pio_add_program(pio1, &ps2in_program));
  bool leds = false;
  while(1) {
    tuh_task();
    kb_task();
    ms_task();
    
    if(!pio_sm_is_rx_fifo_empty(pio1, sm)) {
      u8 b = pio_sm_get(pio1, sm) >> 23;
      printf("%08x\n", b);
      
      if(b < 0x80 || b == 0x83 || b == 0x84 || b == 0xe0 || b == 0xf0) {
        kb_send(b);
      }
      
      if(b == 0xfa && bnext) {
        bnext = false;
        pio_sm_put(pio1, sm, ps2phy_frame(next));
      }
      
    }
  }
}

void inreset() {
  pio_sm_put(pio1, sm, ps2phy_frame(0xff));
}

void inleds(u8 byte) {
  pio_sm_put(pio1, sm, ps2phy_frame(0xed));
  next = byte;
  bnext = true;
}

void intmrd(u8 byte) {
  pio_sm_put(pio1, sm, ps2phy_frame(0xf3));
  next = byte;
  bnext = true;
}