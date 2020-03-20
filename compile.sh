python ${IDF_PATH}/components/partition_table/gen_esp32part.py partitions.csv bin/partitions.bin

python -c "import random, string; print(''.join([random.choice(string.hexdigits) for i in range(8)]))" > nvs_id
python ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs_flash.csv bin/nvs.bin 0x4000

${ESP_PATH}/components/mkspiffs/mkspiffs -b 4096 -s 0x210000 -c ./data bin/spiffs.bin

python ${IDF_PATH}/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
    0x1000 bin/bootloader.bin \
    0x8000 bin/partitions.bin \
    0x9000 bin/nvs.bin \
    0xe000 bin/otadata.bin \
    0x10000 bin/app.bin \
    0x1F0000 bin/spiffs.bin

python ${IDF_PATH}/tools/idf_monitor.py bin/app.elf --port /dev/ttyUSB0 --baud 115200 --toolchain-prefix ${ESP_PATH}/arduino/xtensa-esp32-elf-5.2.0/bin/xtensa-esp32-elf-
