#include "hardware/gpio.h"
#include "bsp/board.h"
#include "tusb.h"

#define KBCLK 11
#define KBDAT 12
#define LVPWR 13
#define MSCLK 14
#define MSDAT 15

#define CLKFULL 40
#define CLKHALF 20
#define DTDELAY 1000

alarm_id_t repeater;
uint32_t repeatus = 35000;
uint16_t delayms = 250;
uint8_t repeat = 0;
bool repeating = false;
bool receiving = false;
bool sending = false;
bool enabled = true;
bool blink = false;
bool ms2send = false;
bool ms2recv = false;

uint8_t kbd_addr;
uint8_t kbd_inst;

uint8_t prevhid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t prevps2 = 0;
uint8_t lastps2 = 0;
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

bool ps2_is_e0(uint8_t data) {
  return data == 0x46 ||
         data >= 0x48 && data <= 0x52 ||
         data == 0x54 || data == 0x58 ||
         data == 0x65 || data == 0x66;
}

int64_t blink_callback(alarm_id_t id, void *user_data) {
  if(blink) {
    blink = false;
    add_alarm_in_ms(1000, blink_callback, NULL, false);
    uint8_t static value = 7; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1);
  } else {
    uint8_t static value = 0; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1);
  }
}

bool ps2_send(uint8_t data) {
  sleep_us(DTDELAY);
  printf("send KB %02x\n", data);
  
  if(!gpio_get(KBCLK)) return false;
  if(!gpio_get(KBDAT)) return false;
  
  lastps2 = data;
  uint8_t parity = 1;
  sending = true;
  
  gpio_set_dir(KBCLK, GPIO_OUT);
  gpio_set_dir(KBDAT, GPIO_OUT);
  
  gpio_put(KBDAT, 0); sleep_us(CLKHALF);
  gpio_put(KBCLK, 0); sleep_us(CLKFULL);
  gpio_put(KBCLK, 1); sleep_us(CLKHALF);
  
  for(uint8_t i = 0; i < 8; i++) {
    gpio_put(KBDAT, data & 0x01); sleep_us(CLKHALF);
    gpio_put(KBCLK, 0); sleep_us(CLKFULL);
    gpio_put(KBCLK, 1); sleep_us(CLKHALF);
  
    parity = parity ^ (data & 0x01);
    data = data >> 1;
  }
  
  gpio_put(KBDAT, parity); sleep_us(CLKHALF);
  gpio_put(KBCLK, 0); sleep_us(CLKFULL);
  gpio_put(KBCLK, 1); sleep_us(CLKHALF);
  
  gpio_put(KBDAT, 1); sleep_us(CLKHALF);
  gpio_put(KBCLK, 0); sleep_us(CLKFULL);
  gpio_put(KBCLK, 1); sleep_us(CLKHALF);
  
  gpio_set_dir(KBCLK, GPIO_IN);
  gpio_set_dir(KBDAT, GPIO_IN);
  
  sending = false;
  return true;
}

void ps2_receive() {
  sending = true;
  uint16_t data = 0x00;
  uint16_t bit = 0x01;
  
  uint8_t cp = 1;
  uint8_t rp = 0;
  
  sleep_us(CLKHALF);
  gpio_set_dir(KBCLK, GPIO_OUT);
  gpio_put(KBCLK, 0); sleep_us(CLKFULL);
  gpio_put(KBCLK, 1); sleep_us(CLKHALF);

  while(bit < 0x0100) {
    if(gpio_get(KBDAT)) {
      data = data | bit;
      cp = cp ^ 1;
    } else {
      cp = cp ^ 0;
    }

    bit = bit << 1;
    
    sleep_us(CLKHALF);
    gpio_put(KBCLK, 0); sleep_us(CLKFULL);
    gpio_put(KBCLK, 1); sleep_us(CLKHALF);
  }

  rp = gpio_get(KBDAT);

  sleep_us(CLKHALF);
  gpio_put(KBCLK, 0); sleep_us(CLKFULL);
  gpio_put(KBCLK, 1); sleep_us(CLKHALF);

  sleep_us(CLKHALF);
  gpio_set_dir(KBDAT, GPIO_OUT);
  gpio_put(KBDAT, 0);
  gpio_put(KBCLK, 0); sleep_us(CLKFULL);
  gpio_put(KBCLK, 1); sleep_us(CLKHALF);
  
  gpio_set_dir(KBCLK, GPIO_IN);
  gpio_set_dir(KBDAT, GPIO_IN);
  
  uint8_t received = data & 0x00ff;
  sending = false;
  
  if(rp == cp) {
    
    printf("got KB %02x  ", (unsigned char)received);
    
    if(received == 0xff) {
      while(!ps2_send(0xaa));
      enabled = true;
      blink = true;
      add_alarm_in_ms(100, blink_callback, NULL, false);
      return;
      
    } else if(received == 0xee) {
      ps2_send(0xee);
      return;
      
    } else if(received == 0xfe) {
      ps2_send(lastps2);
      return;
    
    } else if(received == 0xf2) {
      ps2_send(0xfa);
      ps2_send(0xab);
      ps2_send(0x83);
      return;
    
    } else if(received == 0xf4) {
      enabled = true;
      
    } else if(received == 0xf5 || received == 0xf6) {
      enabled = received == 0xf6;
      repeatus = 35000;
      delayms = 250;
      uint8_t static value = 0; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1);
    }
    
    if(prevps2 == 0xf3) {
      repeatus = received & 0x1f;
      delayms = received & 0x60;
      
      repeatus = 35000 + repeatus * 15000;
      
      if(delayms == 0x00) delayms = 250;
      if(delayms == 0x20) delayms = 500;
      if(delayms == 0x40) delayms = 750;
      if(delayms == 0x60) delayms = 1000;
      
    } else if(prevps2 == 0xed) {
      if(received == 1) { uint8_t static value = 4; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); } else
      if(received == 2) { uint8_t static value = 1; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); } else
      if(received == 3) { uint8_t static value = 5; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); } else
      if(received == 4) { uint8_t static value = 2; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); } else
      if(received == 5) { uint8_t static value = 6; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); } else
      if(received == 6) { uint8_t static value = 3; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); } else
      if(received == 7) { uint8_t static value = 7; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); } else
                        { uint8_t static value = 0; tuh_hid_set_report(kbd_addr, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, (void*)&value, 1); }
    }
    
    prevps2 = received;
    ps2_send(0xfa);
    
  } else {
    ps2_send(0xfe);
  }
}

bool ms2_send(uint8_t data) {
  sleep_us(DTDELAY);
  printf("send MS %02x\n", data);
  
  if(!gpio_get(MSCLK)) return false;
  if(!gpio_get(KBDAT)) return false;
  
  ms2send = true;
  uint8_t parity = 1;
  
  gpio_set_dir(MSCLK, GPIO_OUT);
  gpio_set_dir(MSDAT, GPIO_OUT);
  
  gpio_put(MSDAT, 0); sleep_us(CLKHALF);
  gpio_put(MSCLK, 0); sleep_us(CLKFULL);
  gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  
  for(uint8_t i = 0; i < 8; i++) {
    gpio_put(MSDAT, data & 0x01); sleep_us(CLKHALF);
    gpio_put(MSCLK, 0); sleep_us(CLKFULL);
    gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  
    parity = parity ^ (data & 0x01);
    data = data >> 1;
  }
  
  gpio_put(MSDAT, parity); sleep_us(CLKHALF);
  gpio_put(MSCLK, 0); sleep_us(CLKFULL);
  gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  
  gpio_put(MSDAT, 1); sleep_us(CLKHALF);
  gpio_put(MSCLK, 0); sleep_us(CLKFULL);
  gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  
  gpio_set_dir(MSCLK, GPIO_IN);
  gpio_set_dir(MSDAT, GPIO_IN);
  
  ms2send = false;
  return true;
}

void ms2_receive() {
  ms2send = true;
  uint16_t data = 0x00;
  uint16_t bit = 0x01;
  
  uint8_t cp = 1;
  uint8_t rp = 0;
  
  sleep_us(CLKHALF);
  gpio_set_dir(MSCLK, GPIO_OUT);
  gpio_put(MSCLK, 0); sleep_us(CLKFULL);
  gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  
  while(bit < 0x0100) {
    if(gpio_get(MSDAT)) {
      data = data | bit;
      cp = cp ^ 1;
    } else {
      cp = cp ^ 0;
    }
  
    bit = bit << 1;
    
    sleep_us(CLKHALF);
    gpio_put(MSCLK, 0); sleep_us(CLKFULL);
    gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  }
  
  rp = gpio_get(MSDAT);
  
  sleep_us(CLKHALF);
  gpio_put(MSCLK, 0); sleep_us(CLKFULL);
  gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  
  sleep_us(CLKHALF);
  gpio_set_dir(MSDAT, GPIO_OUT);
  gpio_put(MSDAT, 0);
  gpio_put(MSCLK, 0); sleep_us(CLKFULL);
  gpio_put(MSCLK, 1); sleep_us(CLKHALF);
  
  gpio_set_dir(MSCLK, GPIO_IN);
  gpio_set_dir(MSDAT, GPIO_IN);
  
  uint8_t received = data & 0x00ff;
  ms2send = false;
  
  if(rp == cp) {
    printf("got MS %02x  ", (unsigned char)received);
    
    if(received == 0xff) {
      while(!ms2_send(0xfa));
      while(!ms2_send(0xaa));
      while(!ms2_send(0x00));
      return;
    }
    
    if(received == 0xf2) {
      while(!ms2_send(0xfa));
      while(!ms2_send(0x03));
      return;
    }
    
    if(received == 0xe9) {
      while(!ms2_send(0xfa));
      while(!ms2_send(0x00));
      while(!ms2_send(0x02));
      while(!ms2_send(0x64));
      return;
    }
    
    ms2_send(0xfa);
    
  } else {
    ms2_send(0xfe);
  }
}

int64_t repeat_callback(alarm_id_t id, void *user_data) {
  if(repeat) {
    repeating = true;
    return repeatus;
  }
  
  repeater = 0;
  return 0;
}

void irq_callback(uint gpio, uint32_t events) {  
  if(gpio == KBCLK && !sending && !gpio_get(KBDAT)) {
    printf("IRQ KB  ");
    receiving = true;
  }
  
  if(gpio == MSCLK && !ms2send && !gpio_get(MSDAT)) {
    printf("IRQ MS  ");
    ms2recv = true;
  }
}

void main() {
  board_init();
  printf("ps2x2pico-0.2\n");
  
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
        if(ps2_is_e0(repeat)) ps2_send(0xe0);
        ps2_send(hid2ps2[repeat]);
      }
    }
    
    if(receiving) {
      receiving = false;
      ps2_receive();
    }
    
    if(ms2recv) {
      ms2recv = false;
      ms2_receive();
    }
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  printf("HID device address = %d, instance = %d is mounted\n", dev_addr, instance);
  
  switch(tuh_hid_interface_protocol(dev_addr, instance)) {
    case HID_ITF_PROTOCOL_KEYBOARD:
      printf("HID Interface Protocol = Keyboard\n");
      tuh_hid_receive_report(dev_addr, instance);
      
      blink = true;
      add_alarm_in_ms(100, blink_callback, NULL, false);
      
      kbd_addr = dev_addr;
      kbd_inst = instance;
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
  
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  
  if(itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
    board_led_write(1);
    
    printf("%02x %02x %02x %02x\n", report[0], report[1], report[2], report[3]);
    
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
    
    if(report[3] >> 7) {
      z = 0x8 - z;
    } else if(z) {
      z = 0x100 - z;
    }
    
    ms2_send(s); ms2_send(x);
    ms2_send(y); ms2_send(z);
    board_led_write(0);
  }
  
  if(enabled && itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
    board_led_write(1);
    
    if(report[0] != prevhid[0]) {
      uint8_t rbits = report[0];
      uint8_t pbits = prevhid[0];
      
      for(uint8_t j = 0; j < 8; j++) {
        
        if((rbits & 0x01) != (pbits & 0x01)) {
          if(j > 2 && j != 5) ps2_send(0xe0);
          
          if(rbits & 0x01) {
            ps2_send(mod2ps2[j]);
          } else {
            ps2_send(0xf0);
            ps2_send(mod2ps2[j]);
          }
        }
        
        rbits = rbits >> 1;
        pbits = pbits >> 1;
        
      }
      
      prevhid[0] = report[0];
    }
    
    for(uint8_t i = 2; i < 8; i++) {
      if(prevhid[i]) {
        bool brk = true;
        
        for(uint8_t j = 2; j < 8; j++) {
          if(prevhid[i] == report[j]) {
            brk = false;
            break;
          }
        }
        
        if(brk) {
          if(prevhid[i] == 0x48) continue;
          repeat = 0;
          
          if(ps2_is_e0(prevhid[i])) ps2_send(0xe0);
          ps2_send(0xf0);
          ps2_send(hid2ps2[prevhid[i]]);
        }
      }
      
      if(report[i]) {
        bool make = true;
        
        for(uint8_t j = 2; j < 8; j++) {
          if(report[i] == prevhid[j]) {
            make = false;
            break;
          }
        }
        
        if(make) {
          if(report[i] == 0x48) {
            ps2_send(0xe1); ps2_send(0x14); ps2_send(0x77); ps2_send(0xe1);
            ps2_send(0xf0); ps2_send(0x14); ps2_send(0xf0); ps2_send(0x77);
            continue;
          }
          
          repeat = report[i];
          if(repeater) cancel_alarm(repeater);
          repeater = add_alarm_in_ms(delayms, repeat_callback, NULL, false);
          
          if(ps2_is_e0(report[i])) ps2_send(0xe0);
          ps2_send(hid2ps2[report[i]]);
        }
      }
      
      prevhid[i] = report[i];
    }
    
    board_led_write(0);
  }

  tuh_hid_receive_report(dev_addr, instance);
  
}