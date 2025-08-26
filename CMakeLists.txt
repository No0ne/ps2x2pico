cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico CACHE STRING "Board type")
#set(PICO_PLATFORM rp2350 CACHE STRING "rp2350")
#set(PICO_BOARD pico2 CACHE STRING "Board type")

set(MS_RATE_DEFAULT 100 CACHE STRING "Default mouse sample rate")
set(MS_RATE_HOST_CONTROL ON CACHE BOOL "Allow the host to configure the mouse sample rate")

# Pull in Raspberry Pi Pico SDK
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.1")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.1.1 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

set(target_name ps2x2pico)
set(target_version 3.0)
project(${target_name} C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(ps2x2pico src/ps2x2pico.c src/usbin.c src/ps2kb.c src/ps2ms.c src/ps2out.c) # src/scancodes.c src/ps2in.c)

pico_generate_pio_header(ps2x2pico ${CMAKE_CURRENT_LIST_DIR}/src/ps2out.pio)
#pico_generate_pio_header(ps2x2pico ${CMAKE_CURRENT_LIST_DIR}/src/ps2in.pio)

#target_sources(${target_name} PRIVATE
# # can use 'tinyusb_pico_pio_usb' library later when pico-sdk is updated
# ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
#)

# use tinyusb implementation
#target_compile_definitions(${target_name} PRIVATE PIO_USB_USE_TINYUSB)

# print memory usage, enable all warnings
target_link_options(${target_name} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_name} PRIVATE -Wall -Wextra)

add_compile_definitions(PICO_PANIC_FUNCTION=reset)
add_compile_definitions(PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

### PS/2 output
add_compile_definitions(LVOUT=13) # Level shifter pull-up voltage
add_compile_definitions(KBOUT=11) # Keyboard data GPIO11, clock is GPIO12
add_compile_definitions(MSOUT=14) # Mouse data GPIO14, clock is GPIO15

### PS/2 input
#add_compile_definitions(LVIN=5) # Level shifter pull-up voltage
#add_compile_definitions(KBIN=3) # Keyboard data GPIO3, clock pin is GPIO4
#add_compile_definitions(MSIN=6) # Mouse data GPIO6, clock pin is GPIO7

add_compile_definitions(MS_RATE_DEFAULT=${MS_RATE_DEFAULT})
if (MS_RATE_HOST_CONTROL)
    add_compile_definitions(MS_RATE_HOST_CONTROL)
endif()

pico_set_program_name(${target_name} ${target_name})
pico_set_program_version(${target_name} ${target_version})

pico_enable_stdio_uart(${target_name} 1)
pico_enable_stdio_usb(${target_name} 0)

#add_subdirectory("./Pico-PIO-USB" pico_pio_usb)

target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(${target_name} pico_stdlib hardware_pio tinyusb_host tinyusb_board) # pico_pio_usb)

pico_add_extra_outputs(${target_name})
