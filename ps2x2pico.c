//#include <stdio.h>
//#include "pico/stdlib.h"
//#include "hardware/uart.h"
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "bsp/board.h"
#include "tusb.h"

/*#define UART_ID uart1
#define BAUD_RATE 9600
#define UART_TX_PIN 4
#define UART_RX_PIN 5*/

void main() {
  board_init();
  printf("TinyUSB Host CDC MSC HID Example\r\n");
  
  /*stdio_init_all();
  uart_init(UART_ID, BAUD_RATE);
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  puts("Hello, world!");*/
  
  tusb_init();
  
  while(true) {
    tuh_task();
    
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {

}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {

}
