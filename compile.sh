# Partition table
python ${IDF_PATH}/components/partition_table/gen_esp32part.py files/partitions.csv bin/partitions.bin

# NVS flash
csvfile=$(mktemp)
echo "$(cat files/nvs_flash.csv)$(python manager.py generateid)" >> $csvfile
python ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate $csvfile bin/nvs.bin 0x4000
rm $csvfile 2>/dev/null

# Bootloader
cp ${ESP_PATH}/arduino/arduino-esp32/tools/sdk/bin/bootloader_dio_40m.bin bin/bootloader.bin

# OTA Data
cp ${ESP_PATH}/arduino/arduino-esp32/tools/partitions/boot_app0.bin bin/otadata.bin

# SPI Flash File System
${ESP_PATH}/components/mkspiffs/mkspiffs -b 4096 -s 0x1F0000 -c ./dist bin/spiffs.bin

# Uploading
python ${IDF_PATH}/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
    0x1000 bin/bootloader.bin \
    0x8000 bin/partitions.bin \
    0x9000 bin/nvs.bin \
    0xe000 bin/otadata.bin \
    0x10000 bin/app.bin \
    0x210000 bin/spiffs.bin

# Monitor
python ${IDF_PATH}/tools/idf_monitor.py bin/app.elf --port /dev/ttyUSB0 --baud 115200 --toolchain-prefix ${ESP_PATH}/arduino/xtensa-esp32-elf-5.2.0/bin/xtensa-esp32-elf-
