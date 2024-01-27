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

#include "ps2phy.h"
ps2phy ms_phy;

bool ms_streaming = false;
u32 ms_magic_seq = 0;
u8 ms_type = 0;

void ms_send(u8 byte) {
  queue_try_add(&ms_phy.qbytes, &byte);
}

void ms_send_packet(u8 buttons, s8 x, s8 y, s8 h, s8 v) {
  if(ms_streaming) {
    u8 byte1 = 0x8 | (buttons & 0x7);
    s8 byte2 = x;
    s8 byte3 = 0x100 - y;
    s8 byte4 = 0x100 - v;
    
    if(byte2 < 0) byte1 |= 0x10;
    if(byte3 < 0) byte1 |= 0x20;
    
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
        byte4 &= 0xf;
        byte4 |= (buttons << 1) & 0x30;
      }
      
      ms_send(byte4);
    }
  }
}

void ms_usb_receive(u8 const* report) {
  ms_send_packet(report[0], report[1], report[2], report[3], report[3]);
}

void ms_receive(u8 byte, u8 prev_byte) {
  switch (prev_byte) {
    case 0xf3: // Set Sample Rate
      ms_magic_seq = ((ms_magic_seq << 8) | byte) & 0xffffff;
      
      if(ms_type == 0 && ms_magic_seq == 0xc86450) {
        ms_type = 3;
      } else if(ms_type == 3 && ms_magic_seq == 0xc8c850) {
        ms_type = 4;
      }
    break;
    
    default:
      switch (byte) {
        case 0xff: // Reset
          ms_streaming = false;
          ms_type = 0;
          
          ms_send(0xfa);
          ms_send(0xaa);
          ms_send(ms_type);
        return;
        
        case 0xf6: // Set Defaults
          ms_streaming = false;
          ms_type = 0;
        break;
        
        case 0xf5: // Disable Data Reporting
        case 0xea: // Set Stream Mode
          ms_streaming = false;
        break;
        
        case 0xf4: // Enable Data Reporting
          ms_streaming = true;
        break;
        
        case 0xf2: // Get Device ID
          ms_send(0xfa);
          ms_send(ms_type);
        return;
        
        case 0xe9: // Status Request
          ms_send(0xfa);
          ms_send(0x00); // Bit6: Mode, Bit 5: Enable, Bit 4: Scaling, Bits[2,1,0] = Buttons[L,M,R]
          ms_send(0x02); // Resolution
          ms_send(100);  // Sample Rate
        return;
        
        // TODO: Implement (more of) these?
        // case 0xf0: // Set Remote Mode
        // case 0xee: // Set Wrap Mode
        // case 0xec: // Reset Wrap Mode
        // case 0xeb: // Read Data
        // case 0xe8: // Set Resolution
        // case 0xe7: // Set Scaling 2:1
        // case 0xe6: // Set Scaling 1:1
      }
    break;
  }
  
  ms_send(0xfa);
}

bool ms_task() {
  ps2phy_task(&ms_phy);
  return ms_streaming && !ms_phy.busy;
}

void ms_init(u8 gpio) {
  ps2phy_init(&ms_phy, pio0, gpio, &ms_receive);
}
