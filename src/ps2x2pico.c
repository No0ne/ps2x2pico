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
#include "ps2x2pico.h"
#include "bsp/board_api.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pio_usb.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

int main() {
  set_sys_clock_khz(120000, true);
  board_init();
  printf("\n%s-%s\n", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);
  
  //gpio_init(LVOUT);
  //gpio_init(LVIN);
  //gpio_set_dir(LVOUT, GPIO_OUT);
  //gpio_set_dir(LVIN, GPIO_OUT);
  //gpio_put(LVOUT, 1);
  //gpio_put(LVIN, 1);

  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = 2;
  tuh_configure(0, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);



  tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
  tusb_init();
  kb_init(KBOUT, 0); //KBIN);
  ms_init(MSOUT, 0); //MSIN);

  pio_usb_host_add_port(4, PIO_USB_PINOUT_DPDM);

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
