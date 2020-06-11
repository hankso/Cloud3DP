# Makefile for Cloud3DP

PROJECT_NAME := cloud3dp
PROJECT_VER  := 0.1.2

# Method 1: native esp-idf make system
# Depends: arduino-esp32 & xtensa-esp32-elf & esp-idf
# Uncomment next line if you choose to use method 1

include $(IDF_PATH)/make/project.mk

# Method 2: makeEspArduino
# Depends: arduino-esp32 & xtensa-esp32-elf
# Uncomment next line if you choose to use method 2

# include tools/arduino-esp32.mk
