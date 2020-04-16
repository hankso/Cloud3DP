Cloud3DP
--------

3D printer board based on ESP32. This repo contains open source PCB design files. Free of usage but at your own risk!

Pages
=====

- [Marlin Configuration](http://marlinfw.org/docs/configuration/configuration.html)
- About [Marlin for ESP32](https://github.com/MarlinFirmware/Marlin/issues/14345)
- Getting started by [Smoothieware](http://smoothieware.org/3D-printer-guide)

1. use ``../assets/fs-config.js`` with fontawesome.min.js
2. SVG JS will insert SVGs as child elements BUT with parent's attribute, so if you have events like onclick on parent element, these will also be copied to child, thus calling event listener function twice. In this ocation, explictly reference icon on child element to prevent coping.
3. If `fas`, `fab` or `far` are in the class list while icon name `fa-*` is to be generated or rendered by Vue, you should 


Development
===========

Environment
+++++++++++
This part is for those who want to compile ESP32's firmware from source.

``Makefile`` dependent on environment variable ``ESP_PATH`` - root directory of ESP-IDF and/or arduino-esp32. Here's my 
# My ESP_PATH = /path/to/Espressif
#
# /path/to/Espressif
|-- arduino
|   |-- makeEspArduino (https://github.com/plerup/makeEspArduino)
|   |-- xtensa-esp32-elf-5.2.0 (for arduino-esp32 1.0.4 and esp-idf v3.2)
|   `-- arduino-esp32 (https://github.com/espressif/arduino-esp32)
|       |-- ...
|       `-- tools
|           |-- ...
|           `-- xtensa-esp32-elf -> ../../xtensa-esp32-elf-5.2.0
|-- esp-idf (https://github.com/espressif/esp-idf)
`-- xtensa-esp32-elf (8.2.0 for esp-idf v4.0)

# Change paths according to your installation of arduino-esp32 & makeEspArduino

Installation
++++++++++++

`cnpm install` costs about 8 seconds

FAQs
====

Why USB Type-C?
+++++++++++++++
It's **small** and bidirectionally plugable. Bidirection also means up to two working mode(Debug/Upload) corresponding to two plugging direction.

Why IO expander?
++++++++++++++++
ESP32 has huge calculation resources yet much less GPIO (30 pins) comparing to AVR Atmega128 (53 pins).

In this project, ``74*595`` is used to control 6 stepper motors with only 3 ESP32 GPIO. ``74*595``'s high speed clock ensures that the maxmuim pulse frequency (about 25MHz/8bit=3MHz, Vcc=4.5V) on each output pin satisfy the need of controlling a stepper motor.

``PCF8574`` is used to handle 8 endstops with only 3 ESP32 GPIO (1 INT + 2 I2C).

How many ways can I transmit GCode?
+++++++++++++++++++++++++++++++++++
- WebConsole using WebSocket on WiFi interface
- Bluetooth interface (you may need a BT client software)
- MicroSD Card
- USB Connection + Cura/Printrun/SerialCommander

What stepper drivers can I use?
+++++++++++++++++++++++++++++++
The drivers carrier on this board supports AT2100/A4988/DRV8825/TCM2209 layout.

How to change step size?
++++++++++++++++++++++++
On most 3D printer board like Ramps and SmoothieBoard, jumpers are used on MSx pins to configure the step size. But considering the fact that most of users will **never** change the step size after initial configuration, it's an **unbearable waste of space** to have so many 0.1"x0.2"x0.4" jumper cap on board. And that's why I recommend using surface mounted 0.1"x0.1" jumper pad. MSx pins have internal 50kΩ pull-down resisters and the jumpers are all normally closed. So you need a knife to cut traces between two pads to open the connection. Reapply some solder or a 0R resister to reclose jumpers.

Biomaterial-bump
================

- 74HC595 + ULN2803, Vcc=24V

Target
======

- ESP32-WROOM module vs. ESP32-Pico chip
    - ESP32-WROOM module has well tested layout and WiFi antenna
    - ESP32-Pico can use larger SPI Flash to storage local files

- Actuators (22 pins)
    - Stepper (driver >= 6*3 = 18 pins)
        - XYZ[Z]
        - E*3 for dual resin & one fiber
    - MOSFETs (for laser/valves/spindles etc. >= 4 pins)

- Connectors (10+ pins)
    - Serial (TTL-UART 2 pins)
    - SD Card slot (SPI-Master 4 pins)
    - Axis endstops (XYZL **interrupt** >= 4 pins)

- Power
    - Input
        - 12V Power
        - 5V USB
        - 3.7V Li battery module
    - Output
        - 12V hot bed / circumstense
        - 12V print head
        - 12V steppers
        - 12V fan
        - 5V servo
        - 5V valve/relay
        - 5V BAT
        - 3V3 ESP32/PCF8574/74HC595

Features
========

- Stepper driver
    - AT2100
    - A4983/A4988 (super cheap, <10 CNY)
    - DRV8823/DRV8825 (better than A49XX, 10-20 CNY)
    - TMC2100/TMC2130/TMC2208/TMC2209 (super silent & stable, 30-40 CNY)

- Marlin
    - OLED/LCD Screen (optional since we already have WebUI)
    - Configure file
    - SD/UART/BLE/WebSocket input

- Bluetooth
    - GCode file uploading
    - Command line interface

- WiFi connection
    - STA / AP / Mesh?
    - Web graphic user interface: moving, extruding, IO, fan, temp and etc.
    - GCode file uploading
    - WebSocket realtime camera streaming
    - Error detection: Is printer table clean? Is the first floor firm?
    - Printer firmware updating
