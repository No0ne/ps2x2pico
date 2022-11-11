/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 No0ne (https://github.com/No0ne)
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

#define KBCLK 11
#define KBDAT 12
#define LVPWR 13
#define MSCLK 14
#define MSDAT 15

uint8_t const led2ps2[] = { 0, 4, 1, 5, 2, 6, 3, 7 };
uint8_t const mod2ps2[] = { 0x14, 0x12, 0x11, 0x1f, 0x14, 0x59, 0x11, 0x27 };
uint8_t const hid2ps2[] = {
  0x00, 0x00, 0xfc, 0x00, 0x1c, 0x32, 0x21, 0x23, 0x24, 0x2b, 0x34, 0x33, 0x43, 0x3b, 0x42, 0x4b,
  0x3a, 0x31, 0x44, 0x4d, 0x15, 0x2d, 0x1b, 0x2c, 0x3c, 0x2a, 0x1d, 0x22, 0x35, 0x1a, 0x16, 0x1e,
  0x26, 0x25, 0x2e, 0x36, 0x3d, 0x3e, 0x46, 0x45, 0x5a, 0x76, 0x66, 0x0d, 0x29, 0x4e, 0x55, 0x54,
  0x5b, 0x5d, 0x5d, 0x4c, 0x52, 0x0e, 0x41, 0x49, 0x4a, 0x58, 0x05, 0x06, 0x04, 0x0c, 0x03, 0x0b,
  0x83, 0x0a, 0x01, 0x09, 0x78, 0x07, 0x7c, 0x7e, 0x7e, 0x70, 0x6c, 0x7d, 0x71, 0x69, 0x7a, 0x74,
  0x6b, 0x72, 0x75, 0x77, 0x4a, 0x7c, 0x7b, 0x79, 0x5a, 0x69, 0x72, 0x7a, 0x6b, 0x73, 0x74, 0x6c,
  0x75, 0x7d, 0x70, 0x71, 0x61, 0x2f, 0x37, 0x0f, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40,
  0x48, 0x50, 0x57, 0x5f
};
uint8_t const maparray = sizeof(hid2ps2) / sizeof(uint8_t);

bool irq_enabled = true;
bool kbd_enabled = true;
uint8_t kbd_addr = 0;
uint8_t kbd_inst = 0;

bool blinking = false;
bool receive_kbd = false;
bool receive_ms = false;
bool repeating = false;
uint32_t repeat_us = 35000;
uint16_t delay_ms = 250;
alarm_id_t repeater;

uint8_t prev_rpt[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t prev_kbd = 0;
uint8_t resend_kbd = 0;
uint8_t resend_ms = 0;
uint8_t repeat = 0;
uint8_t leds = 0;

#define MS_TYPE_STANDARD  0x00
#define MS_TYPE_WHEEL_3   0x03
#define MS_TYPE_WHEEL_5   0x04

#define MS_MODE_IDLE      0
#define MS_MODE_STREAMING 1

#define MS_INPUT_CMD      0
#define MS_INPUT_SET_RATE 1

uint8_t ms_type = MS_TYPE_STANDARD;
uint8_t ms_mode = MS_MODE_IDLE;
uint8_t ms_input_mode = MS_INPUT_CMD;
uint8_t ms_rate = 100;
uint32_t ms_magic_seq = 0x00;

int64_t repeat_callback(alarm_id_t id, void *user_data) {
  if(repeat) {
    repeating = true;
    return repeat_us;
  }
  
  repeater = 0;
  return 0;
}

void ps2_cycle_clock(uint8_t clkout) {
  sleep_us(20);
  gpio_put(clkout, 0);
  sleep_us(40);
  gpio_put(clkout, 1);
  sleep_us(20);
}

void ps2_set_bit(bool bit, uint8_t clkout, uint8_t datout) {
  gpio_put(datout, bit);
  ps2_cycle_clock(clkout);
}

void ps2_send(uint8_t data, bool channel) {
  uint8_t clkout = channel ? KBCLK : MSCLK;
  uint8_t datout = channel ? KBDAT : MSDAT;
  
  uint8_t timeout = 10;
  sleep_ms(1);
  
  while(timeout) {
    if(gpio_get(clkout) && gpio_get(datout)) {
      
      if(channel) {
        resend_kbd = data;
      } else {
        resend_ms = data;
      }
      
      uint8_t parity = 1;
      irq_enabled = false;
      
      gpio_set_dir(clkout, GPIO_OUT);
      gpio_set_dir(datout, GPIO_OUT);
      ps2_set_bit(0, clkout, datout);
      
      for(uint8_t i = 0; i < 8; i++) {
        ps2_set_bit(data & 0x01, clkout, datout);
        parity = parity ^ (data & 0x01);
        data = data >> 1;
      }
      
      ps2_set_bit(parity, clkout, datout);
      ps2_set_bit(1, clkout, datout);
      
      gpio_set_dir(clkout, GPIO_IN);
      gpio_set_dir(datout, GPIO_IN);
      
      irq_enabled = true;
      return;
      
    }
    
    timeout--;
    sleep_ms(8);
  }
}

void ms_send(uint8_t data) {
  printf("send MS %02x\n", data);
  ps2_send(data, false);
}

void kbd_send(uint8_t data) {
  printf("send KB %02x\n", data);
  ps2_send(data, true);
}

void maybe_send_e0(uint8_t data) {
  if(data == 0x46 ||
     data >= 0x48 && data <= 0x52 ||
     data == 0x54 || data == 0x58 ||
     data == 0x65 || data == 0x66 ||
     data >= 0x81) {
    ps2_send(0xe0, true);
  }
}

void kbd_set_leds(uint8_t data) {
  if(data > 7) data = 0;
  leds = led2ps2[data];
  tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, &leds, sizeof(leds));
}

int64_t blink_callback(alarm_id_t id, void *user_data) {
  if(kbd_addr) {
    if(blinking) {
      kbd_set_leds(7);
      blinking = false;
      return 500000;
    } else {
      kbd_set_leds(0);
    }
  }
  return 0;
}

void process_kbd(uint8_t data) {
  switch(prev_kbd) {
    case 0xed:
      prev_kbd = 0;
      kbd_set_leds(data);
    break;
    
    case 0xf3:
      prev_kbd = 0;
      repeat_us = data & 0x1f;
      delay_ms = data & 0x60;
      
      repeat_us = 35000 + repeat_us * 15000;
      
      if(delay_ms == 0x00) delay_ms = 250;
      if(delay_ms == 0x20) delay_ms = 500;
      if(delay_ms == 0x40) delay_ms = 750;
      if(delay_ms == 0x60) delay_ms = 1000;
    break;
    
    default:
      switch(data) {
        case 0xff:
          kbd_send(0xfa);
          
          kbd_enabled = true;
          blinking = true;
          add_alarm_in_ms(1, blink_callback, NULL, false);
          
          sleep_ms(16);
          kbd_send(0xaa);
          
          return;
        break;
        
        case 0xfe:
          kbd_send(resend_kbd);
          return;
        break;
        
        case 0xee:
          kbd_send(0xee);
          return;
        break;
        
        case 0xf2:
          kbd_send(0xfa);
          kbd_send(0xab);
          kbd_send(0x83);
          return;
        break;
        
        case 0xf3:
        case 0xed:
          prev_kbd = data;
        break;
        
        case 0xf4:
          kbd_enabled = true;
        break;
        
        case 0xf5:
        case 0xf6:
          kbd_enabled = data == 0xf6;
          repeat_us = 35000;
          delay_ms = 250;
          kbd_set_leds(0);
        break;
      }
    break;
  }
  
  kbd_send(0xfa);
}

void process_ms(uint8_t data) {

  if(ms_input_mode == MS_INPUT_SET_RATE) {
    ms_rate = data;  // TODO... need to actually honor the sample rate!
    ms_input_mode = MS_INPUT_CMD;
    ms_send(0xfa);

    ms_magic_seq = (ms_magic_seq << 8) | data;
    if(ms_type == MS_TYPE_STANDARD && ms_magic_seq == 0xc86450) {
      ms_type = MS_TYPE_WHEEL_3;
    } else if (ms_type == MS_TYPE_WHEEL_3 && ms_magic_seq == 0xc8c850) {
      ms_type = MS_TYPE_WHEEL_5;
    }
    printf("  MS magic seq: %06x type: %d\n", ms_magic_seq, ms_type);
    return;
  }

  if(data != 0xf3) {
    ms_magic_seq = 0x00;
  }

  switch(data) {
    case 0xff: // CMD: Reset
      ms_type = MS_TYPE_STANDARD;
      ms_mode = MS_MODE_IDLE;
      ms_rate = 100;

      ms_send(0xfa);
      ms_send(0xaa);
      ms_send(ms_type);
    return;

    case 0xf6: // CMD: Set Defaults
      ms_type = MS_TYPE_STANDARD;
      ms_rate = 100;
    case 0xf5: // CMD: Disable Data Reporting
    case 0xea: // CMD: Set Stream Mode
      ms_mode = MS_MODE_IDLE;
      ms_send(0xfa);
    return;

    case 0xf4: // CMD: Enable Data Reporting
      ms_mode = MS_MODE_STREAMING;
      ms_send(0xfa);
    return;

    case 0xf3: // CMD: Set Sample Rate
      ms_input_mode = MS_INPUT_SET_RATE;
      ms_send(0xfa);
    return;

    case 0xf2: // CMD: Get Device ID
      ms_send(0xfa);
      ms_send(ms_type);
    return;

    case 0xe9: // CMD: Status Request
      ms_send(0xfa);
      ms_send(0x00); // Bit6: Mode, Bit 5: Enable, Bit 4: Scaling, Bits[2,1,0] = Buttons[L,M,R]
      ms_send(0x02); // Resolution
      ms_send(ms_rate); // Sample Rate
    return;

// TODO: Implement (more of) these?
//    case 0xfe: // CMD: Resend
//    case 0xf0: // CMD: Set Remote Mode
//    case 0xee: // CMD: Set Wrap Mode
//    case 0xec: // CMD: Reset Wrap Mode
//    case 0xeb: // CMD: Read Data
//    case 0xe8: // CMD: Set Resolution
//    case 0xe7: // CMD: Set Scaling 2:1
//    case 0xe6: // CMD: Set Scaling 1:1
  }

  ms_send(0xfa);
}

void ps2_receive(bool channel) {
  uint8_t clkin = channel ? KBCLK : MSCLK;
  uint8_t datin = channel ? KBDAT : MSDAT;

  irq_enabled = false;
  board_led_write(1);

  uint8_t bit = 1;
  uint8_t data = 0;
  uint8_t parity = 1;

  gpio_set_dir(clkin, GPIO_OUT);
  ps2_cycle_clock(clkin);

  while(bit) {
    if(gpio_get(datin)) {
      data = data | bit;
      parity = parity ^ 1;
    } else {
      parity = parity ^ 0;
    }

    bit = bit << 1;
    ps2_cycle_clock(clkin);
  }

  parity = gpio_get(datin) == parity;
  ps2_cycle_clock(clkin);

  gpio_set_dir(datin, GPIO_OUT);
  ps2_set_bit(0, clkin, datin);
  gpio_set_dir(clkin, GPIO_IN);
  gpio_set_dir(datin, GPIO_IN);

  irq_enabled = true;
  board_led_write(0);

  if(!parity) {
    if(channel) {
      kbd_send(0xfe);
    } else {
      ms_send(0xfe);
    }
    return;
  }

  if(channel) {
    printf("got KB %02x  ", (unsigned char)data);
    process_kbd(data);
  } else {
    printf("got MS %02x  ", (unsigned char)data);
    process_ms(data);
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  printf("HID device address = %d, instance = %d is mounted\n", dev_addr, instance);

  switch(tuh_hid_interface_protocol(dev_addr, instance)) {
    case HID_ITF_PROTOCOL_KEYBOARD:
      printf("HID Interface Protocol = Keyboard\n");
      
      kbd_addr = dev_addr;
      kbd_inst = instance;
      
      blinking = true;
      add_alarm_in_ms(1, blink_callback, NULL, false);
      
      tuh_hid_receive_report(dev_addr, instance);
    break;
    
    case HID_ITF_PROTOCOL_MOUSE:
      printf("HID Interface Protocol = Mouse\n");
      //tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
      tuh_hid_receive_report(dev_addr, instance);
    break;
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  
  switch(tuh_hid_interface_protocol(dev_addr, instance)) {
    case HID_ITF_PROTOCOL_KEYBOARD:
      
      if(!kbd_enabled || report[1] != 0) {
        tuh_hid_receive_report(dev_addr, instance);
        return;
      }
      
      board_led_write(1);
      
      if(report[0] != prev_rpt[0]) {
        uint8_t rbits = report[0];
        uint8_t pbits = prev_rpt[0];
        
        for(uint8_t j = 0; j < 8; j++) {
          
          if((rbits & 0x01) != (pbits & 0x01)) {
            if(j > 2 && j != 5) kbd_send(0xe0);
            
            if(rbits & 0x01) {
              kbd_send(mod2ps2[j]);
            } else {
              kbd_send(0xf0);
              kbd_send(mod2ps2[j]);
            }
          }
          
          rbits = rbits >> 1;
          pbits = pbits >> 1;
          
        }
        
        prev_rpt[0] = report[0];
      }
      
      for(uint8_t i = 2; i < 8; i++) {
        if(prev_rpt[i]) {
          bool brk = true;
          
          for(uint8_t j = 2; j < 8; j++) {
            if(prev_rpt[i] == report[j]) {
              brk = false;
              break;
            }
          }
          
          if(brk && report[i] < maparray) {
            if(prev_rpt[i] == 0x48) continue;
            if(prev_rpt[i] == repeat) repeat = 0;
            
            maybe_send_e0(prev_rpt[i]);
            kbd_send(0xf0);
            kbd_send(hid2ps2[prev_rpt[i]]);
          }
        }
        
        if(report[i]) {
          bool make = true;
          
          for(uint8_t j = 2; j < 8; j++) {
            if(report[i] == prev_rpt[j]) {
              make = false;
              break;
            }
          }
          
          if(make && report[i] < maparray) {
            if(report[i] == 0x48) {
              
              if(report[0] & 0x1 || report[0] & 0x10) {
                kbd_send(0xe0); kbd_send(0x7e); kbd_send(0xe0); kbd_send(0xf0); kbd_send(0x7e);
              } else {
                kbd_send(0xe1); kbd_send(0x14); kbd_send(0x77); kbd_send(0xe1);
                kbd_send(0xf0); kbd_send(0x14); kbd_send(0xf0); kbd_send(0x77);
              }
              
              continue;
            }
            
            repeat = report[i];
            if(repeater) cancel_alarm(repeater);
            repeater = add_alarm_in_ms(delay_ms, repeat_callback, NULL, false);
            
            maybe_send_e0(report[i]);
            kbd_send(hid2ps2[report[i]]);
          }
        }
        
        prev_rpt[i] = report[i];
      }
      
      tuh_hid_receive_report(dev_addr, instance);
      board_led_write(0);
    break;
    
    case HID_ITF_PROTOCOL_MOUSE:
      board_led_write(1);
      
      printf("%02x %02x %02x %02x\n", report[0], report[1], report[2], report[3]);

      if(ms_mode == MS_MODE_STREAMING) {

        uint8_t s = report[0] + 8;
        uint8_t x = report[1] & 0x7f;
        uint8_t y = report[2] & 0x7f;
        uint8_t z = report[3] & 7;

        if(report[1] >> 7) {
          s += 0x10;
          x += 0x80;
        }

        if(report[2] >> 7) {
          y = 0x80 - y;
        } else if(y) {
          s += 0x20;
          y = 0x100 - y;
        }

        ms_send(s);
        ms_send(x);
        ms_send(y);

        if (ms_type == MS_TYPE_WHEEL_3 || ms_type == MS_TYPE_WHEEL_5) {
          // TODO: add proper support for buttons 4 & 5

          if(report[3] >> 7) {
            z = 0x8 - z;
          } else if(z) {
            z = 0x100 - z;
          }

          ms_send(z);
        }
      }

      tuh_hid_receive_report(dev_addr, instance);
      board_led_write(0);
    break;
  }
  
}

void irq_callback(uint gpio, uint32_t events) {
  if(irq_enabled) {
    if(gpio == KBCLK && !gpio_get(KBDAT)) {
      printf("IRQ KB  ");
      receive_kbd = true;
    }
    
    if(gpio == MSCLK && !gpio_get(MSDAT)) {
      printf("IRQ MS  ");
      receive_ms = true;
    }
  }
}

void main() {
  board_init();
  printf("ps2x2pico-0.4\n");
  
  gpio_init(KBCLK);
  gpio_init(KBDAT);
  gpio_init(MSCLK);
  gpio_init(MSDAT);
  gpio_init(LVPWR);
  gpio_set_dir(LVPWR, GPIO_OUT);
  gpio_put(LVPWR, 1);
  
  gpio_set_irq_enabled_with_callback(KBCLK, GPIO_IRQ_EDGE_RISE, true, &irq_callback);
  gpio_set_irq_enabled_with_callback(MSCLK, GPIO_IRQ_EDGE_RISE, true, &irq_callback);
  tusb_init();
  
  while(true) {
    tuh_task();
    
    if(repeating) {
      repeating = false;
      
      if(repeat) {
        maybe_send_e0(repeat);
        kbd_send(hid2ps2[repeat]);
      }
    }
    
    if(receive_kbd) {
      receive_kbd = false;
      ps2_receive(true);
    }
    
    if(receive_ms) {
      receive_ms = false;
      ps2_receive(false);
    }
    
  }
}
