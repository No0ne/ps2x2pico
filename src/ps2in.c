/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 No0ne (https://github.com/No0ne)
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
#include "ps2in.pio.h"

s8 ps2in_prog = -1;
u8 ps2in_msi = 0;
u8 ps2in_msb[] = { 0, 0, 0, 0 };

void ps2in_init(ps2in* this, PIO pio, u8 data_pin) {
  if(ps2in_prog == -1) {
    ps2in_prog = pio_add_program(pio, &ps2in_program);
  }

  this->sm = pio_claim_unused_sm(pio, true);
  ps2in_program_init(pio, this->sm, ps2in_prog, data_pin);
  this->pio = pio;
  this->state = 0;
}

void ps2in_task(ps2in* this, ps2out* out) {
  if(!pio_sm_is_rx_fifo_empty(this->pio, this->sm)) {
    u32 fifo = pio_sm_get(this->pio, this->sm) >> 23;
    
    bool parity = 1;
    for(u8 i = 0; i < 8; i++) {
      parity = parity ^ (fifo >> i & 1);
    }
    
    if(parity != fifo >> 8) {
      pio_sm_put(this->pio, this->sm, ps2_frame(0xfe));
      return;
    }
    
    u8 byte = fifo;
    //printf("** ps2in  sm %02x  byte %02x\n", this->sm, byte);
    
    //if((fifo & 0xff) == 0xfe) {
    //  pio_sm_put(this->pio, this->sm, ps2_frame(this->last_tx));
    //  return;
    //}
    
    if(byte == 0xaa && this->state) {
      this->state = this->sm ? 2 : 10;
      //printf("** ps2in  sm %02x  reset successful!\n", this->sm);
    }
    
    if(this->sm == 0) {
      if(byte == 0xfa && this->state == 9) {
        this->state = 10;
        pio_sm_put(this->pio, this->sm, ps2_frame(this->byte));
      }
      
      if(byte != 0xfa && this->state == 10) {
        queue_try_add(&out->qbytes, &byte);
      }
    }
    
    if(this->sm == 1) {
      if(this->state == 10) {
        
        ps2in_msb[ps2in_msi] = byte;
        ps2in_msi++;
        
        /*if(ps2in_msi == 3) {
          ps2in_msi = 0;
          ms_send_movement(/ *ps2in_msb[0] & 0x7* /0, ps2in_msb[1], 0x100 - ps2in_msb[2], 0);
          printf(" %02x %02x \n", ps2in_msb[1], ps2in_msb[2]);
        }*/
        
        if(ps2in_msi == 4) {
          ps2in_msi = 0;
          ms_send_movement(ps2in_msb[0] & 0x7, ps2in_msb[1], 0x100 - ps2in_msb[2], 0x100 - ps2in_msb[3]);
        }
        
      } else {
        
        if(byte == 0xfa && this->state == 9) {
          ps2in_msi = 0;
          this->state = 10;
        }
        
        u8 init[] = { 0xaa, 0x00, 0xf3, 0xc8, 0xf3, 0x64, 0xf3, 0x50, 0xf4 };
        
        if((byte == init[1] && this->state == 2) || (byte == 0xfa && this->state > 2 && this->state < 9)) {
          pio_sm_put(this->pio, this->sm, ps2_frame(init[this->state]));
          this->state++;
        }
        
      }
    }
  }
}

void ps2in_reset(ps2in* this) {
  this->state = 1;
  //printf("** ps2in reset sm %02x\n", this->sm);
  pio_sm_put(this->pio, this->sm, ps2_frame(0xff));
}

void ps2in_set(ps2in* this, u8 command, u8 byte) {
  if(this->state == 10) {
    //printf("** ps2in  cmd %02x  byte %02x\n", command, byte);
    pio_sm_put(this->pio, this->sm, ps2_frame(command));
    this->byte = byte;
    this->state = 9;
  }
}
