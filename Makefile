# Makefile for arduino-core
# See more information at https://github.com/plerup/makeEspArduino

# sketch name is optional
SKETCH := main/Temp.ino
CHIP   := esp32

CUSTOM_LIBS = ${ESP_PATH}/arduino

BUILD_ROOT = ./build

FS_DIR = ./data

USE_CCACHE = 1

ESP_ROOT = ${ESP_PATH}/arduino/arduino-esp32

include ${ESP_PATH}/arduino/makeEspArduino/makeEspArduino.mk
