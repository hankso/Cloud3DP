# Makefile for arduino-esp32
# See more at https://github.com/plerup/makeEspArduino

# Variables defined for makeEspArduino
SKETCH      = main/main.ino
CHIP        = esp32

BUILD_DIR   = build
BUILD_EXTRA_FLAGS := \
	-DPROJECT_VER=\"${PROJECT_VER}\" \
	-DPROJECT_NAME=\"${PROJECT_NAME}\"

FS_DIR      = webdev/dist
PART_FILE   = partitions.csv
USE_CCACHE  = 1
CUSTOM_LIBS ?= ${ESP_PATH}/components

ESP_ROOT    ?= ${ESP_PATH}/components/arduino-esp32
include ${ESP_PATH}/tools/makeEspArduino/makeEspArduino.mk

# Variables and targets defined to extend makeEspArduino
ESP_MONITOR_PY = ${IDF_PATH}/tools/idf_monitor.py
ESP_NVS_PY = ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py
TMPFN = tmp.nvs_flash.csv
NULL :=
SPACE := ${NULL} ${NULL}
define NEWLINE


endef

fatfs:
	${ESP_PATH}/components/mkfatfs/mkfatfs -d 2 -s ${SPIFFS_SIZE} -c ${FS_DIR} ${FS_IMAGE}

spiffs:
	${ESP_PATH}/components/mkspiffs/mkspiffs -d 2 -s ${SPIFFS_SIZE} -b ${SPIFFS_BLOCK_SIZE} -c ${FS_DIR} ${FS_IMAGE}

monitor:
	python3 ${ESP_MONITOR_PY} --toolchain-prefix ${ESP_ROOT}/tools/xtensa-esp32-elf/bin/xtensa-esp32-elf- ${BUILD_DIR}/${MAIN_NAME}.elf

nvs_data:
	@echo Prepare NVS Flash data from nvs_flash.csv
	$(shell python3 tools/manager.py genid -o ${TMPFN})
	
nvs_flash: nvs_data
	@echo Compile to NVS binary ${BUILD_DIR}/nvs.bin
	python3 ${ESP_NVS_PY} generate ${TMPFN} ${BUILD_DIR}/nvs.bin 0x4000
	@rm ${TMPFN}
