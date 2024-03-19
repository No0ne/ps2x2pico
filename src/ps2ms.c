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

#include "tusb.h"
#include "ps2out.h"
#include "ps2in.h"
ps2out ms_out;
ps2in ms_in;

bool ms_streaming = false;
bool ms_ismoving = false;
u32 ms_magic_seq = 0;
u8 ms_type = 0;
u8 ms_rate = 100;
u8 ms_db = 0;
s16 ms_dx = 0;
s16 ms_dy = 0;
s8 ms_dz = 0;

void ms_reset() {
  ms_ismoving = false;
  ms_db = 0;
  ms_dx = 0;
  ms_dy = 0;
  ms_dz = 0;
}

void ms_send(u8 byte) {
  queue_try_add(&ms_out.qbytes, &byte);
}

s64 ms_reset_callback() {
  ms_send(0xaa);
  ms_send(ms_type);
  return 0;
}

u8 ms_clamp_xyz(s16 xyz) {
  if(xyz < -255) return 1;
  if(xyz > 255) return 255;
  return xyz;
}

s16 ms_remain_xyz(s16 xyz) {
  if(xyz < -255) return xyz + 255;
  if(xyz > 255) return xyz - 255;
  return 0;
}

void ms_send_packet(u8 buttons, s16 x, s16 y, s8 h, s8 v) {
  if(!ms_streaming) return;
  
  if(!buttons && !x && !y && !h && !v) {
    if(!ms_ismoving) return;
    ms_ismoving = false;
  } else {
    ms_ismoving = true;
  }
  
  u8 byte1 = 0x08 | (buttons & 0x07);
  u8 byte2 = ms_clamp_xyz(x);
  u8 byte3 = 0x100 - ms_clamp_xyz(y);
  s8 byte4 = 0x100 - v;
  
  if(x < 0) byte1 |= 0x10;
  if(y > 0) byte1 |= 0x20;
  if(byte2 == 0xaa) byte2 = 0xab;
  if(byte3 == 0xaa) byte3 = 0xab;
  
  ms_send(byte1);
  ms_send(byte2);
  ms_send(byte3);
  
  if(ms_type == 3 || ms_type == 4) {
    if(h == v) {
      if(byte4 < -8) byte4 = -8;
      if(byte4 > 7) byte4 = 7;
      if(byte4 < 0) byte4 |= 0xf8;
    } else {
      if(v < 0) byte4 = 0x01;
      if(v > 0) byte4 = 0xff;
      if(h < 0) byte4 = 0x02;
      if(h > 0) byte4 = 0xfe;
    }
    
    if(ms_type == 4) {
      byte4 &= 0x0f;
      byte4 |= (buttons << 1) & 0x30;
    }
    
    ms_send(byte4);
  }
}

s64 ms_send_callback() {
  if(!ms_streaming) return 0;
  
  if(!ms_phy.busy) {
    ms_send_packet(ms_db, ms_dx, ms_dy, ms_dz, ms_dz);
    ms_dx = ms_remain_xyz(ms_dx);
    ms_dy = ms_remain_xyz(ms_dy);
    ms_dz = 0;
  }
  
  return 1000000 / ms_rate;
}

void ms_usb_receive(u8 const* report) {
  ms_db = report[0];
  ms_dx += (s8)report[1];
  ms_dy += (s8)report[2];
  ms_dz += (s8)report[3];
}

void ms_receive(u8 byte, u8 prev_byte) {
  switch (prev_byte) {
    case 0xf3: // Set Sample Rate
      ms_rate = byte;
      ms_magic_seq = ((ms_magic_seq << 8) | ms_rate) & 0xffffff;
      
      if(ms_type == 0 && ms_magic_seq == 0xc86450) {
        ms_type = 3;
      } else if(ms_type == 3 && ms_magic_seq == 0xc8c850) {
        ms_type = 4;
      }
      
      ms_reset();
    break;
    
    default:
      switch(byte) {
        case 0xff: // Reset
          add_alarm_in_ms(100, ms_reset_callback, NULL, false);
          ms_type = 0;
        case 0xf6: // Set Defaults
          ms_rate = 100;
        case 0xf5: // Disable Data Reporting
          ms_streaming = false;
          ms_reset();
        break;
        
        case 0xf4: // Enable Data Reporting
          ms_streaming = true;
          ms_reset();
          add_alarm_in_ms(100, ms_send_callback, NULL, false);
        break;
        
        case 0xf2: // Get Device ID
          ms_send(0xfa);
          ms_send(ms_type);
          ms_reset();
        return;
        
        case 0xeb: // Read Data
          ms_ismoving = true;
        break;
        
        case 0xe9: // Status Request
          ms_send(0xfa);
          ms_send(ms_streaming << 5); // Bit6: Mode, Bit 5: Enable, Bit 4: Scaling, Bits[2,1,0] = Buttons[L,M,R]
          ms_send(0x02); // Resolution
          ms_send(ms_rate); // Sample Rate
        return;
        
        default:
          ms_reset();
        break;
      }
    break;
  }
  
  ms_send(0xfa);
}

bool ms_task() {
  ps2out_task(&ms_out);
  ps2in_task(&ms_in, &ms_out);
  return ms_streaming && !ms_out.busy;
}

void ms_init(u8 gpio_out, u8 gpio_in) {
  ps2out_init(&ms_out, pio0, gpio_out, &ms_receive);
  ps2in_init(&ms_in, pio1, gpio_in);
  ms_reset_callback();
}
