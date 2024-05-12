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

#include "tusb.h"
#include "ps2phy.h"
#include "hardware/watchdog.h"

ps2phy kb_phy;

#define KBHOSTCMD_RESET_FF 0xff
#define KBHOSTCMD_RESEND_FE 0xfe
#define KBHOSTCMD_SSC3_SET_KEY_MAKE_FD 0xfd
#define KBHOSTCMD_SSC3_SET_KEY_MAKE_BREAK_FC 0xfc
#define KBHOSTCMD_SSC3_SET_KEY_MAKE_TYPEMATIC_FB 0xfb
#define KBHOSTCMD_SSC3_SET_ALL_MAKE_BREAK_TYPEMATIC_FA 0xfa
#define KBHOSTCMD_SSC3_SET_ALL_MAKE_F9 0xf9
#define KBHOSTCMD_SSC3_SET_ALL_MAKE_BREAK_F8 0xf8
#define KBHOSTCMD_SSC3_SET_ALL_MAKE_TYPEMATIC_F7 0xf7
#define KBHOSTCMD_SET_DEFAULT_F6 0xf6
#define KBHOSTCMD_DISABLE_F5 0xf5
#define KBHOSTCMD_ENABLE_F4 0xf4
#define KBHOSTCMD_SET_TYPEMATIC_PARAMS_F3 0xf3
#define KBHOSTCMD_READ_ID_F2 0xf2
#define KBHOSTCMD_SET_SCAN_CODE_SET_F0 0xf0
#define KBHOSTCMD_ECHO_EE 0xee
#define KBHOSTCMD_SET_LEDS_ED 0xed

typedef enum {
  KBH_STATE_IDLE,
  KBH_STATE_SET_LEDS_ED,
  KBH_STATE_SET_SCAN_CODE_SET_F0,
  KBH_STATE_READ_ID_F2,
  KBH_STATE_SET_TYPEMATIC_PARAMS_F3,
  KBH_STATE_SET_KEY_MAKE_FD,
  KBH_STATE_SET_KEY_MAKE_BREAK_FC,
  KBH_STATE_SET_KEY_MAKE_TYPEMATIC_FB
} kbhost_state_enum_t;

kbhost_state_enum_t kbhost_state = KBH_STATE_IDLE;

typedef enum {
  SSC3_MODE_MAKE,
  SSC3_MODE_MAKE_BREAK,
  SSC3_MODE_MAKE_TYPEMATIC,
  SSC3_MODE_MAKE_BREAK_TYPEMATIC,
} ssc3_mode_enum_t;

ssc3_mode_enum_t ssc3_mode = SSC3_MODE_MAKE_BREAK_TYPEMATIC;

#define SCAN_CODE_SET_1 1
#define SCAN_CODE_SET_2 2
#define SCAN_CODE_SET_3 3

u8 scancodeset = SCAN_CODE_SET_2;

u32 const repeats[] = {
  33333, 37453, 41667, 45872, 48309, 54054, 58480, 62500,
  66667, 75188, 83333, 91743, 100000, 108696, 116279, 125000,
  133333, 149254, 166667, 181818, 200000, 217391, 232558, 250000,
  270270, 303030, 333333, 370370, 400000, 434783, 476190, 500000
};
u16 const delays[] = { 250, 500, 750, 1000 };

u8 const *mod2ps2;
u8 const *hid2ps2;

bool kb_enabled = true;
bool blinking = false;
u32 repeat_us;
u16 delay_ms;
alarm_id_t repeater;

u8 prev_rpt[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
u8 repeat = 0;

u8 last_byte_send = 0;

void kb_send(u8 byte) {
  printf("kb_send(byte = 0x%x)\n", byte);
  queue_try_add(&kb_phy.qbytes, &byte);
}

void kb_maybe_send_e0(u8 key) {
  // Extended keycodes that require this prefix exist only for SCS_1 and SCS_2.
  // For both of these the keys that require the prefix are the same.
  if (scancodeset == SCAN_CODE_SET_1 || scancodeset == SCAN_CODE_SET_2) {
    if(key == HID_KEY_PRINT_SCREEN ||
      (key >= HID_KEY_INSERT && key <= HID_KEY_ARROW_UP) ||
      key == HID_KEY_KEYPAD_DIVIDE ||
      key == HID_KEY_KEYPAD_ENTER ||
      key == HID_KEY_APPLICATION ||
      key == HID_KEY_POWER ||
      (key >= HID_KEY_GUI_LEFT && key != HID_KEY_SHIFT_RIGHT)) {
      kb_send(0xe0);
    }
  }
}

void kb_set_leds(u8 byte) {
  if(byte > 7) byte = 0;
  tuh_kb_set_leds(led2ps2[byte]);
}

s64 blink_callback() {
  if(blinking) {
    printf("Blinking keyboard LEDs\n");
    kb_set_leds(KEYBOARD_LED_NUMLOCK | KEYBOARD_LED_CAPSLOCK | KEYBOARD_LED_SCROLLLOCK);
    blinking = false;
    return 500000;
  }
  kb_set_leds(0);
  return 0;
}

void set_scancodeset(u8 ssc) {
  scancodeset = ssc;
  switch (scancodeset)
  {
  case SCAN_CODE_SET_1:
    mod2ps2 = mod2ps2_1;
    hid2ps2 = hid2ps2_1;
    break;
  case SCAN_CODE_SET_2:
    mod2ps2 = mod2ps2_2;
    hid2ps2 = hid2ps2_2;
    break;
  case SCAN_CODE_SET_3:
    mod2ps2 = mod2ps2_3;
    hid2ps2 = hid2ps2_3;
    break;
  }
  printf("scancodeset set to %u\n", scancodeset);
}

void kb_set_defaults() {
  printf("Setting defaults for keyboard\n");
  kbhost_state = KBH_STATE_IDLE;
  ssc3_mode = SSC3_MODE_MAKE_BREAK_TYPEMATIC;
  set_scancodeset(2);
  kb_enabled = true;
  repeat_us = 91743;
  delay_ms = 500;
  repeat = 0;
  blinking = true;
  add_alarm_in_ms(100, blink_callback, NULL, false);
}

s64 repeat_callback() {
  if(repeat) {
    kb_maybe_send_e0(repeat);
    
    if(repeat >= HID_KEY_CONTROL_LEFT && repeat <= HID_KEY_GUI_RIGHT) {
      kb_send(mod2ps2[repeat - HID_KEY_CONTROL_LEFT]);
    } else {
      kb_send(hid2ps2[repeat]);
    }
    
    return repeat_us;
  }
  
  repeater = 0;
  return 0;
}

// Sends a key state change to the host
// u8 keycode          - from hid.h HID_KEY_ definition
// bool is_key_pressed - state of key: true=pressed, false=released
// u8 modifiers        - state of modifier keys as in report[0]
void kb_send_key(u8 key, bool is_key_pressed, u8 modifiers) {
  if (!kb_enabled) {
    printf("Keyboard disabled, ignoring key press %u\n", key);
    return;
  }

  if ((key > HID_KEY_F24 &&
       key < HID_KEY_CONTROL_LEFT) ||
      key > HID_KEY_GUI_RIGHT) {
    printf("Ignoring key %u\n", key);
    return;
  }

  if(key == HID_KEY_PAUSE) {
    repeat = 0;
    
    if(is_key_pressed && scancodeset != SCAN_CODE_SET_3) {
      if(modifiers & KEYBOARD_MODIFIER_LEFTCTRL ||
         modifiers & KEYBOARD_MODIFIER_RIGHTCTRL) {
        kb_send(0xe0); kb_send(0x7e); kb_send(0xe0); kb_send(0xf0); kb_send(0x7e);
      } else {
        kb_send(0xe1); kb_send(0x14); kb_send(0x77); kb_send(0xe1);
        kb_send(0xf0); kb_send(0x14); kb_send(0xf0); kb_send(0x77);
      }
    }
    
    return;
  }
  
  // In scan code set 1 and 2 some keys need a 0xe0 prefixed before the actual scan code.
  kb_maybe_send_e0(key);
  
  // Take care of typematic repeat
  if (is_key_pressed) {
    if (scancodeset != SCAN_CODE_SET_3 || 
    (scancodeset == SCAN_CODE_SET_3 
      && (ssc3_mode == SSC3_MODE_MAKE_BREAK_TYPEMATIC || ssc3_mode == SSC3_MODE_MAKE_TYPEMATIC)))
    {
      repeat = key;
      if (repeater)
        cancel_alarm(repeater);
      repeater = add_alarm_in_ms(delay_ms, repeat_callback, NULL, false);
    }
  }
  else {
    if (key == repeat)
      repeat = 0;
    kb_send(0xf0);
  }

  if(key >= HID_KEY_CONTROL_LEFT && key <= HID_KEY_GUI_RIGHT) {
    kb_send(mod2ps2[key - HID_KEY_CONTROL_LEFT]);
  } else {
    kb_send(hid2ps2[key]);
  }
}

void kb_usb_receive(u8 const* report) {
  if(report[1] == 0) {
  
    if(report[0] != prev_rpt[0]) {
      // modifiers have changed
      u8 rbits = report[0];
      u8 pbits = prev_rpt[0];
      
      for(u8 j = 0; j < 8; j++) {
        if((rbits & 1) != (pbits & 1)) {
          // send make or break depending on modifier key state change
          kb_send_key(HID_KEY_CONTROL_LEFT + j, rbits & 1, report[0]);
        }
        
        rbits = rbits >> 1;
        pbits = pbits >> 1;
      }
    }
    
    // go over pressed keys in report
    for(u8 i = 2; i < 8; i++) {
      if(prev_rpt[i]) {
        bool brk = true;
        
        // check if the key in previous report is still in the current report
        for(u8 j = 2; j < 8; j++) {
          if(prev_rpt[i] == report[j]) {
            brk = false;
            break;
          }
        }
        
        if(brk) {
          // send break if key not pressed anymore
          kb_send_key(prev_rpt[i], false, report[0]);
        }
      }
      
      if(report[i]) {
        bool make = true;
        
        // check if the key in previous report is still in the current report
        for(u8 j = 2; j < 8; j++) {
          if(report[i] == prev_rpt[j]) {
            make = false;
            break;
          }
        }
        
        // send make if key was in the current report the first time
        if(make) {
          kb_send_key(report[i], true, report[0]);
        }
      }
    }
    
    memcpy(prev_rpt, report, sizeof(prev_rpt));
  }
}

void kb_receive(u8 byte, u8 prev_byte) {
  if (last_byte_send != KBHOSTCMD_RESEND_FE)
    last_byte_send = byte; // need to remember this in case the host requests it with cmd 0xfe
  
  last_byte_send = byte;
  printf("kb_receive(byte = 0x%x, prev_byte = 0x%x)\n", byte, prev_byte);
  switch (kbhost_state) {
    case KBH_STATE_SET_KEY_MAKE_FD:
      // Scan code set 3 only
      printf("WARNING: Host command 0xfd not implemented yet!\n");
      kbhost_state = KBH_STATE_IDLE; // TODO
    break;
    
    case KBH_STATE_SET_KEY_MAKE_BREAK_FC:
      // Scan code set 3 only
      printf("WARNING: Host command 0xfc not implemented yet!\n");
      kbhost_state = KBH_STATE_IDLE; // TODO
    break;

    case KBH_STATE_SET_KEY_MAKE_TYPEMATIC_FB:
      // Scan code set 3 only
      printf("WARNING: Host command 0xfb not implemented yet!\n");
      kbhost_state = KBH_STATE_IDLE; // TODO
    break;

    case KBH_STATE_SET_LEDS_ED:
      kb_set_leds(byte);
      kbhost_state = KBH_STATE_IDLE;
    break;
    
    case KBH_STATE_SET_TYPEMATIC_PARAMS_F3:
      repeat_us = repeats[byte & 0x1f];
      delay_ms = delays[(byte & 0x60) >> 5];
      kbhost_state = KBH_STATE_IDLE;
    break;

    case KBH_STATE_READ_ID_F2:
      kb_send(0xab);
      kb_send(0x83);
      kbhost_state = KBH_STATE_IDLE;
    return; // don't send an ACK(0xfa)

    case KBH_STATE_SET_SCAN_CODE_SET_F0:
      switch((u8)byte) {
        case 0:
          kb_send(scancodeset);
          break;
        case SCAN_CODE_SET_1:
        case SCAN_CODE_SET_2:
        case SCAN_CODE_SET_3:
          set_scancodeset(byte);
          break;
        default:
          printf("WARNING: scancodeset requested to set to unknown value %u by host, defaulting to 2\n",byte);
          set_scancodeset(2);
        break;
      }
      kbhost_state = KBH_STATE_IDLE;
    break;

    case KBH_STATE_IDLE:
    default:
      switch ((u8)byte) {
        case KBHOSTCMD_RESET_FF:
          printf("Resetting keyboard requested (not really doing it, just setting defaults)");
          kb_set_defaults();
          kb_send(0xaa);
        break;

        case KBHOSTCMD_RESEND_FE:
          // TODO: Debug with actual keyboard
          printf("WARNING: Host command 0xfe unclear, sending last received byte 0x%x back!\n",last_byte_send);
          kb_send(last_byte_send);
          kbhost_state = KBH_STATE_IDLE;
        break;

        case KBHOSTCMD_SSC3_SET_KEY_MAKE_FD:
          if (scancodeset == SCAN_CODE_SET_3) {
            printf("Setting specifc keys to make only, no break, no typematic\n");
            kbhost_state = KBH_STATE_SET_KEY_MAKE_FD;
          }
          else {
            printf("Scan code set 3 not set. Ignoring command 0x%x\n",byte);
            kbhost_state = KBH_STATE_IDLE;
          }
        break;

        case KBHOSTCMD_SSC3_SET_KEY_MAKE_BREAK_FC:
          if (scancodeset == SCAN_CODE_SET_3) {
            printf("Setting specifc keys to make, break, no typematic\n");
            kbhost_state = KBH_STATE_SET_KEY_MAKE_BREAK_FC;
          }
          else {
            printf("Scan code set 3 not set. Ignoring command 0x%x\n",byte);
            kbhost_state = KBH_STATE_IDLE;
          }
        break;

        case KBHOSTCMD_SSC3_SET_KEY_MAKE_TYPEMATIC_FB:
          if (scancodeset == SCAN_CODE_SET_3) {
            printf("Setting specifc keys to make, typematic, no break\n");
            kbhost_state = KBH_STATE_SET_KEY_MAKE_TYPEMATIC_FB;
          }
          else {
            printf("Scan code set 3 not set. Ignoring command 0x%x\n",byte);
            kbhost_state = KBH_STATE_IDLE;
          }
        break;


        case KBHOSTCMD_SSC3_SET_ALL_MAKE_BREAK_TYPEMATIC_FA: 
          if (scancodeset == SCAN_CODE_SET_3) {
            printf("Setting all keys to make, break, typematic\n");
            ssc3_mode = SSC3_MODE_MAKE_BREAK_TYPEMATIC;
          }
          else {
            printf("Scan code set 3 not set. Ignoring command 0x%x\n",byte);
          }
          kbhost_state = KBH_STATE_IDLE;
        break;

        case KBHOSTCMD_SSC3_SET_ALL_MAKE_F9: 
          // utilized by SGI O2
          if (scancodeset == SCAN_CODE_SET_3) {
            printf("Setting all keys to make only, no typematic\n");
            ssc3_mode = SSC3_MODE_MAKE;
          }
          else {
            printf("Scan code set 3 not set. Ignoring command 0x%x\n",byte);
          }
          kbhost_state = KBH_STATE_IDLE;
        break;

        case KBHOSTCMD_SSC3_SET_ALL_MAKE_BREAK_F8: 
          if (scancodeset == SCAN_CODE_SET_3) {
            printf("Setting all keys to make, break, no typematic\n");
            ssc3_mode = SSC3_MODE_MAKE_BREAK;
          }
          else {
            printf("Scan code set 3 not set. Ignoring command 0x%x\n",byte);
          }
          kbhost_state = KBH_STATE_IDLE;
        break;

        case KBHOSTCMD_SSC3_SET_ALL_MAKE_TYPEMATIC_F7:
          if (scancodeset == SCAN_CODE_SET_3) {
            printf("Setting all keys to make, break, typematic\n");
            ssc3_mode = SSC3_MODE_MAKE_TYPEMATIC;
          }
          else {
            printf("Scan code set 3 not set. Ignoring command 0x%x\n",byte);
          }
          kbhost_state = KBH_STATE_IDLE;
        break;

        case KBHOSTCMD_SET_DEFAULT_F6:
          printf("Resetting keyboard requested by host\n");
          kb_set_defaults();
        break;
        
        case KBHOSTCMD_DISABLE_F5:
          printf("Disabling keyboard requested by host\n");
          // Documentation says this command might also set defaults.
          // In the case of a SGI O2 and generic PS/2 Cherry KB this not true
          // and would prevent the O2 from working.
          // The O2 set scan code set 3 and then disables the keyboard with F5.
          // It still expects the KB to be in scan code set 3 mode though
          // when it enables it afterwards with F4.
          //
          // kb_set_defaults();
          //
          kb_enabled = false;
        break;
        
        case KBHOSTCMD_ENABLE_F4:
          printf("Enabling keyboard requested by host\n");
          kb_enabled = true;
          kbhost_state = KBH_STATE_IDLE;
        break;
    
        case KBHOSTCMD_SET_TYPEMATIC_PARAMS_F3:
          printf("Typematic delay and repeat frequency set by host\n");
          kbhost_state = KBH_STATE_SET_TYPEMATIC_PARAMS_F3;
        break;
        
        case KBHOSTCMD_READ_ID_F2:
          printf("Keyboard ID requested by host\n");
          kbhost_state = KBH_STATE_READ_ID_F2;
        break;

        case KBHOSTCMD_SET_SCAN_CODE_SET_F0:
          printf("Scan code set change requested by host\n");
          kbhost_state = KBH_STATE_SET_SCAN_CODE_SET_F0;
        break;
        
        case KBHOSTCMD_ECHO_EE:
          printf("Echo 0xee requested by host\n");
          kb_send(0xee);
          kbhost_state = KBH_STATE_IDLE;
        return; // don't send an ACK(0xfa)

        case KBHOSTCMD_SET_LEDS_ED:
          printf("Keyboard LEDs set by host\n");
          kbhost_state = KBH_STATE_SET_LEDS_ED;
        break;

        default:
          printf("WARNING: Unknown command received byte=0x%x, ignoring it!\n",byte);
        break;
      }
    break;
  }

  kb_send(0xfa);
}

bool kb_task() {
  ps2phy_task(&kb_phy);
  return kb_enabled && !kb_phy.busy;
}

void kb_init(u8 gpio) {
  ps2phy_init(&kb_phy, pio0, gpio, &kb_receive);
  kb_set_defaults();
  kb_send(0xaa);
}
