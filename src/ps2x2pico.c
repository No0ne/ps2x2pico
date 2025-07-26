/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 No0ne (https://github.com/No0ne)
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

#include "ps2x2pico.h"
#include "bsp/board_api.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"

#include "pico/util/queue.h"
#include "ps2out.pio.h"

#include "pio_usb.h"
#include "pico/stdlib.h"

u8 phy_last_rx = 0;
u8 phy_last_tx = 0;
u8 phy_sent = 0;
u8 phy_packet[9];
queue_t phy_packets;

u32 ps2_frame(u8 byte) {
  bool parity = 1;
  for(u8 i = 0; i < 8; i++) {
    parity = parity ^ (byte >> i & 1);
  }
  return ((1 << 10) | (parity << 9) | (byte << 1)) ^ 0x7ff;
}

void kb_send_key(u8 key, bool state) {
  if(!state) pio_sm_put(pio0, 0, ps2_frame(0xf0));
  printf("%02x ", key);
  pio_sm_put(pio0, 0, ps2_frame(0x77));
}

void ms_send_movement(u8 buttons, s8 x, s8 y, s8 z) {
  printf("ms %02x %02x %02x %02x\n", buttons, x, y, z);
}

void kb_init(u8 gpio_out, u8 gpio_in) {
  ps2write_program_init(pio0, 0, pio_add_program(pio0, &ps2write_program), KBOUT);
  ps2read_program_init(pio0, 1, pio_add_program(pio0, &ps2read_program), KBOUT);
  queue_init(&phy_packets, 9, 32);
}

int main() {
  set_sys_clock_khz(120000, true);

  //board_init();
  //board_led_write(1);
  stdio_init_all();
  printf("\n%s-%s\n", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);

  gpio_init(LVOUT);
  gpio_init(LVIN);
  gpio_set_dir(LVOUT, GPIO_OUT);
  gpio_set_dir(LVIN, GPIO_OUT);
  gpio_put(LVOUT, 1);
  gpio_put(LVIN, 1);

  tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
  tuh_init(0);

  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = 20;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
  tuh_init(1);

  /*tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
  tuh_init(BOARD_TUH_RHPORT);
  kb_init(KBOUT, KBIN);*/
  //ms_init(MSOUT, MSIN);

  //kb_init();
  //add_alarm_in_ms(500, kb_set_led_callback, NULL, false);

  while(1) {
    tuh_task();
    //kb_task();
    //ms_task();
  }
}

void reset() {
  printf("\n\n *** PANIC via tinyusb: watchdog reset!\n\n");
  //watchdog_enable(100, false);
}
