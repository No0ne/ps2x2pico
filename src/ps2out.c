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
#include "ps2out.pio.h"

s8 ps2out_txprg = -1;
s8 ps2out_rxprg = -1;
u8 ps2out_lock = 0;

u32 ps2_frame(u8 byte) {
  bool parity = 1;
  for(u8 i = 0; i < 8; i++) {
    parity = parity ^ (byte >> i & 1);
  }
  return ((1 << 10) | (parity << 9) | (byte << 1)) ^ 0x7ff;
}

void ps2out_send(ps2out* this, u8 len) {
  this->packet[0] = len;
  board_led_write(1);
  queue_try_add(&this->packets, &this->packet);
}

void ps2out_init(ps2out* this, bool sm, u8 data_pin, rx_callback rx_function) {
  this->sm = sm;
  this->data_pin = data_pin;
  this->rx_function = rx_function;
  this->last_rx = 0;
  this->last_tx = 0;
  this->sent = 0;

  queue_init(&this->packets, 9, 32);

  if(ps2out_txprg == -1) ps2out_txprg = pio_add_program(pio1, &ps2send_program);
  if(ps2out_rxprg == -1) ps2out_rxprg = pio_add_program(pio1, &ps2receive_program);

  ps2send_program_init(pio1, this->sm ? 0 : 2, ps2out_txprg, data_pin);
  ps2receive_program_init(pio1, this->sm ? 1 : 3, ps2out_rxprg, data_pin);
}

void ps2out_task(ps2out* this) {
  u8 packet[9];

  if(ps2out_lock) {
    ps2out_lock--;

    if(pio_interrupt_get(pio1, 7)) {
      ps2out_lock = 0;
    }
  }

  if(!ps2out_lock && !pio_interrupt_get(pio1, 7) && gpio_get(this->data_pin) && gpio_get(this->data_pin+1) && queue_try_peek(&this->packets, &packet)) {
    if(this->sent == packet[0]) {
      queue_try_remove(&this->packets, &packet);
      this->sent = 0;
      board_led_write(0);
    } else {
      this->sent++;
      this->last_tx = packet[this->sent];
      ps2out_lock = 100;
      //if(this->sm) printf(" %02x ", this->last_tx);
      //if(!this->sm) printf("ms > host  0x%02x\n", this->last_tx);
      pio_sm_put(pio1, this->sm ? 0 : 2, ps2_frame(this->last_tx));
    }
  }

  if(pio_interrupt_get(pio1, this->sm ? 0 : 2)) {
    if(this->sent > 0) this->sent--;
    pio_interrupt_clear(pio1, this->sm ? 0 : 2);
    pio_interrupt_clear(pio1, 7);
  }

  if(!pio_sm_is_rx_fifo_empty(pio1, this->sm ? 1 : 3)) {
    u32 fifo = pio_sm_get(pio1, this->sm ? 1 : 3) >> 23;

    bool parity = 1;
    for(u8 i = 0; i < 8; i++) {
      parity = parity ^ (fifo >> i & 1);
    }

    if(parity != fifo >> 8) {
      pio_sm_put(pio1, this->sm ? 0 : 2, ps2_frame(0xfe));
      return;
    }

    if((fifo & 0xff) == 0xfe) {
      pio_sm_put(pio1, this->sm ? 0 : 2, ps2_frame(this->last_tx));
      return;
    }

    while(queue_try_remove(&this->packets, &packet));
    this->sent = 0;

    (*this->rx_function)(fifo, this->last_rx);
    this->last_rx = fifo;
    //if(this->sm) printf("host > kb  0x%02x\n", this->last_rx);
    //if(!this->sm) printf("host > ms  0x%02x\n", this->last_rx);
  }
}
