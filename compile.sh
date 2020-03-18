python ../../arduino/arduino-esp32/tools/gen_esp32part.py partitions.csv bin/partitions.bin

../../arduino/arduino-esp32/tools/mkspiffs/mkspiffs -b 4096 -s 0x210000 -c ./data bin/spiffs.bin

python ../../arduino/arduino-esp32/tools/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0xe000 /home/hank/programs/Espressif/arduino/arduino-esp32/tools/partitions/boot_app0.bin 0x1000 /home/hank/programs/Espressif/arduino/arduino-esp32/tools/sdk/bin/bootloader_dio_40m.bin

python ../../arduino/arduino-esp32/tools/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x10000 build/Temp_esp32/Temp.bin 0x8000 bin/partitions.bin 0x1F0000 bin/spiffs.bin

python -m serial.tools.miniterm --rts=0 --dtr=0 /dev/ttyUSB0 115200

python ../../esp-idf/tools/idf_monitor.py ./build/Temp_esp32/Temp.elf --port /dev/ttyUSB0 --baud 115200 --toolchain-prefix ../../arduino/xtensa-esp32-elf-5.2.0/bin/xtensa-esp32-elf-
