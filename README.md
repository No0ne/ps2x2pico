# ps2x2pico
USB keyboard/mouse to PS/2 interface converter using a Raspberry Pi Pico


|![hw1](https://raw.githubusercontent.com/No0ne/ps2x2pico/main/hw1.jpg) |![hw2](https://raw.githubusercontent.com/No0ne/ps2x2pico/main/hw2.jpg) |![hw3](https://raw.githubusercontent.com/No0ne/ps2x2pico/main/hw3.jpg) |![hw4](https://raw.githubusercontent.com/No0ne/ps2x2pico/main/hw4.jpg) |
|-|-|-|-|

Keyboard only variant: https://github.com/No0ne/ps2pico
PC-XT variant: https://github.com/No0ne/ps2pico/tree/xt-version

# Usage
* Download `ps2x2pico.uf2` from https://github.com/No0ne/ps2x2pico/releases
* Copy `ps2x2pico.uf2` to your Pi Pico by pressing BOOTSEL before pluggging in.
* 3.3V/5V conversion is done using a bi-directional level shifter: https://learn.sparkfun.com/tutorials/bi-directional-logic-level-converter-hookup-guide/
* Afterwards connect a USB keyboard and/or mouse using an OTG-adapter and optional USB hub.
* Also works with wireless keyboards and mice with a dedicated USB receiver.
```
                   _________________
                  |                 |
Pico GPIO11 ______| LV1         HV1 |______ PS/2 keyboard clock
Pico GPIO12 ______| LV2         HV2 |______ PS/2 keyboard data
Pico GPIO13 ______| LV          HV  |______ PS/2 5V + Pico VBUS
Pico    GND ______| GND         GND |______ PS/2 GND
Pico GPIO14 ______| LV3         HV3 |______ PS/2 mouse clock
Pico GPIO15 ______| LV4         HV4 |______ PS/2 mouse data
                  |_________________|
```

# Build
```
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build
cd build
cmake ..
make
```

# Resources
* https://github.com/No0ne/ps2pico
* https://wiki.osdev.org/PS/2_Keyboard
* https://wiki.osdev.org/PS/2_Mouse
* https://wiki.osdev.org/Mouse_Input
* https://wiki.osdev.org/%228042%22_PS/2_Controller
* https://isdaman.com/alsos/hardware/mouse/ps2interface.htm