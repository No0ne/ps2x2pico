#include <stdio.h>
#include "pico/stdlib.h"
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>

//#include "hardware/gpio.h"
//#include "hardware/pio.h"
//#include "bsp/board.h"
//#include "tusb.h"

//#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "blink.pio.h"

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq);

int main() {

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);

    blink_pin_forever(pio, 0, offset, 25, 1);
}

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (clock_get_hz(clk_sys) / (2 * freq)) - 3;
}

void mains() {
  //board_init();
  stdio_init_all();
  
  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  
  printf("TinyUSB Host CDC MSC HID Example\r\n");
  
  //tusb_init();
  
  while(true) {
    //tuh_task();
    gpio_put(LED_PIN, 1);
    sleep_ms(250);
    gpio_put(LED_PIN, 0);
    sleep_ms(250);
    printf("Hello, world!\n");
  }
}

/*void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {

}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {

}*/
