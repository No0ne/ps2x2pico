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
 
#include "ps2out.h"
#include "ps2in.h"
#include "ps2in.pio.h"

s8 ps2in_prog = -1;

void ps2in_init(ps2in* this, PIO pio, u8 data_pin) {
  if(ps2in_prog == -1) {
    ps2in_prog = pio_add_program(pio, &ps2in_program);
  }

  this->sm = pio_claim_unused_sm(pio, true);
  ps2in_program_init(pio, this->sm, ps2in_prog, data_pin);
  this->pio = pio;
}

void ps2in_task(ps2in* this, ps2out* out) {
  if(!pio_sm_is_rx_fifo_empty(this->pio, this->sm)) {
    u8 byte = pio_sm_get(this->pio, this->sm) >> 23;
    
    printf("%02x %02x\n", this->sm, byte);
    
    if(byte == 0x00 && this->byte_next == 0xaa) {
      pio_sm_put(this->pio, this->sm, ps2_frame(0xf4));
    } else { //if(byte != 0xfa) {
      queue_try_add(&out->qbytes, &byte);
    }
    
    this->byte_next = byte;
    
    /*if(byte < 0x80 || byte == 0x83 || byte == 0x84 || byte == 0xe0 || byte == 0xf0) {
      queue_try_add(&out->qbytes, &byte);
      
    } else if(byte == 0xfa && this->send_next) {
      this->send_next = false;
      pio_sm_put(this->pio, this->sm, ps2_frame(this->byte_next));
    }*/
  }
}

void ps2in_reset(ps2in* this) {
  pio_sm_put(this->pio, this->sm, ps2_frame(0xff));
}

void ps2in_set(ps2in* this, u8 command, u8 byte) {
  pio_sm_put(this->pio, this->sm, ps2_frame(command));
  this->byte_next = byte;
  this->send_next = true;
}
