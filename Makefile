# Makefile for arduino-core
# See more information at https://github.com/plerup/makeEspArduino

# sketch name is optional
SKETCH  = src/main.ino
CHIP    = esp32
VERSION = 0.1.2

BUILD_ROOT = ./build
BUILD_EXTRA_FLAGS := -DPROJECT_VER=\"${VERSION}\" -DPROJECT_NAME=\"Cloud3DP\"

FS_DIR = ./dist
PART_FILE = ./partitions.csv

USE_CCACHE = 1

CUSTOM_LIBS ?= ${ESP_PATH}/arduino
ESP_ROOT ?= ${ESP_PATH}/arduino/arduino-esp32
IDF_PATH ?= ${ESP_PATH}/esp-idf
include ${ESP_PATH}/arduino/makeEspArduino/makeEspArduino.mk

bootloader:
	cp -v ${ESP_ROOT}/tools/sdk/bin/bootloader_dio_40m.bin bin/bootloader.bin

otadata:
	cp -v ${ESP_ROOT}/tools/partitions/boot_app0.bin bin/otadata.bin

spiffs: ${FS_IMAGE}

monitor:
	python3 ${IDF_PATH}/tools/idf_monitor.py --toolchain-prefix ${ESP_PATH}/arduino/xtensa-esp32-elf-5.2.0/bin/xtensa-esp32-elf- bin/app.elf

null :=
SPACE := ${null} ${null}
define NEWLINE


endef

nvs_data:
	@echo Prepare NVS Flash data from nvs_flash.csv
	$(file > tmp.nvs_flash.csv,$(subst ${SPACE},${NEWLINE},$(subst {UID},$(shell python3 manager.py genid),$(shell cat nvs_flash.csv))))
	
nvs_flash: nvs_data
	@echo Compile to NVS binary to bin/nvs.bin
	python3 ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate tmp.nvs_flash.csv bin/nvs.bin 0x4000
	@rm tmp.nvs_flash.csv
