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
#include "ps2phy.pio.h"

s8 prog = -1;
bool locked = false;

s64 unlock_callback() {
  locked = false;
  return 0;
}

u32 ps2phy_frame(u8 byte) {
  bool parity = 1;
  for(u8 i = 0; i < 8; i++) {
    parity = parity ^ (byte >> i & 1);
  }
  return ((1 << 10) | (parity << 9) | (byte << 1)) ^ 0x7ff;
}

void ps2phy_init(ps2phy* this, PIO pio, u8 data_pin, rx_callback rx) {
  if(prog == -1) {
    prog = pio_add_program(pio, &ps2phy_program);
  }
  
  queue_init(&this->qbytes, sizeof(u8), 9);
  queue_init(&this->qpacks, sizeof(u8) * 9, 16);
  
  this->sm = pio_claim_unused_sm(pio, true);
  ps2phy_program_init(pio, this->sm, prog, data_pin);
  
  this->pio = pio;
  this->sent = 0;
  this->rx = rx;
  this->last_rx = 0;
  this->last_tx = 0;
  this->busy = 0;
}

void ps2phy_task(ps2phy* this) {
  u8 i = 0;
  u8 byte;
  u8 pack[9];
  
  if(!queue_is_empty(&this->qbytes)) {
    while(i < 9 && queue_try_remove(&this->qbytes, &byte)) {
      i++;
      pack[i] = byte;
    }
    
    pack[0] = i;
    queue_try_add(&this->qpacks, &pack);
  }
  
  if(pio_interrupt_get(this->pio, this->sm)) {
    this->busy = 1;
  } else {
    this->busy &= 2;
  }
  
  if(pio_interrupt_get(this->pio, this->sm + 4)) {
    this->sent--;
    pio_interrupt_clear(this->pio, this->sm + 4);
  }
  
  if(!queue_is_empty(&this->qpacks) && pio_sm_is_tx_fifo_empty(this->pio, this->sm) && !this->busy && !locked) {
    if(queue_try_peek(&this->qpacks, &pack)) {
      if(this->sent == pack[0]) {
        this->sent = 0;
        queue_try_remove(&this->qpacks, &pack);
      } else {
        this->sent++;
        this->last_tx = pack[this->sent];
        this->busy |= 2;
        locked = true;
        add_alarm_in_ms(1, unlock_callback, NULL, false);
        pio_sm_put(this->pio, this->sm, ps2phy_frame(this->last_tx));
      }
    }
  }
  
  if(!pio_sm_is_rx_fifo_empty(this->pio, this->sm)) {
    u32 fifo = pio_sm_get(this->pio, this->sm) >> 23;
    
    bool parity = 1;
    for(i = 0; i < 8; i++) {
      parity = parity ^ (fifo >> i & 1);
    }
    
    if(parity != fifo >> 8) {
      pio_sm_put(this->pio, this->sm, ps2phy_frame(0xfe));
      return;
    }
    
    if((fifo & 0xff) == 0xfe) {
      pio_sm_put(this->pio, this->sm, ps2phy_frame(this->last_tx));
      return;
    }
    
    while(queue_try_remove(&this->qbytes, &byte));
    while(queue_try_remove(&this->qpacks, &pack));
    this->sent = 0;
    
    (*this->rx)(fifo, this->last_rx);
    this->last_rx = fifo;
  }
}
