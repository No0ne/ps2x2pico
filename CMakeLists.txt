cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Raspberry Pi Pico SDK
include(pico_sdk_import.cmake)

project(ps2x2pico C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(ps2x2pico ps2x2pico.c)

pico_set_program_name(ps2x2pico "ps2x2pico")
pico_set_program_version(ps2x2pico "0.5")

pico_enable_stdio_uart(ps2x2pico 1)
pico_enable_stdio_usb(ps2x2pico 0)

target_include_directories(ps2x2pico PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ps2x2pico pico_stdlib tinyusb_host tinyusb_board)

pico_add_extra_outputs(ps2x2pico)