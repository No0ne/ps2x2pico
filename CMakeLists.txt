# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/Users/noone/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ps2x2pico C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ps2x2pico ps2x2pico.c )

pico_set_program_name(ps2x2pico "ps2x2pico")
pico_set_program_version(ps2x2pico "0.1")

pico_enable_stdio_uart(ps2x2pico 1)
pico_enable_stdio_usb(ps2x2pico 0)

# Add the standard library to the build
target_link_libraries(ps2x2pico pico_stdlib)

# Add any user requested libraries
target_link_libraries(ps2x2pico
        hardware_pio
        )

pico_add_extra_outputs(ps2x2pico)

