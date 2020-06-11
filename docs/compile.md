> This page is for those who want to compile ESP32 firmware from source.

# Environment

Cloud3DP supports two ways of using [arduino-esp32](https://github.com/espressif/arduino-esp32), a wrapping layer based on ESP32 development framework [esp-idf](https://github.com/espressif/esp-idf).

1. Take arduino-esp32 as a component of esp-idf and compile with esp-idf's native `make`/`cmake` system.
2. The arduino-esp32 integrates a copy of pre-compiled esp-idf libraries (under `arduino-esp32/tools/sdk/lib/`), so you can also build firmware without esp-idf by `makeEspArduino`/`Arduino IDE`.

Both methods depend on some path configurations and environment variables. Checkout the author's development environment for your reference:

```
ESP_PATH = /path/to/Espressif (the root directory of esp-idf and/or arduino-esp32)
ESP_ROOT = ${ESP_PATH}/components/arduino-esp32 (necessary for method 2)
IDF_PATH = ${ESP_PATH}/esp-idf (necessary for method 1)

/path/to/Espressif ($ESP_PATH)
|-- components
|   |-- mkspiffs (https://github.com/igrr/mkspiffs)
|   |-- mkfatfs (https://github.com/jkearins/ESP32_mkfatfs)
|   |-- ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer)
|   |-- AsyncTCP (https://github.com/me-no-dev/AsyncTCP)
|   |-- ... (other arduino libraries if used)
|   `-- arduino-esp32 ($ESP_ROOT)
|       |-- ... (core files of arduino-esp32)
|       `-- tools
|           |-- mkspiffs -> ../../mkspiffs/
|           |-- mkfatfs -> ../../mkfatfs/
|           `-- xtensa-esp32-elf -> ../../../tools/xtensa-esp32-elf-5.2.0
|-- tools
|   |-- makeEspArduino (https://github.com/plerup/makeEspArduino)
|   |-- xtensa-esp32-elf-5.2.0
|   `-- xtensa-esp32-elf-8.2.0
|-- xtensa-esp32-elf -> tools/xtensa-esp32-elf-8.2.0
`-- esp-idf ($IDF_PATH)
```

# Installation

#### Arduino-ESP32 (necessary for both methods)
Read more instructions from https://github.com/espressif/arduino-esp32.

```bash
export ESP_PATH = /path/to/Espressif
cd $ESP_PATH && mkdir components && cd components
git clone --depth=1 --recursive https://github.com/espressif/arduino-esp32.git
export ESP_ROOT = ${ESP_PATH}/components/arduino-esp32
```

Running `arduino-esp32/tools/get.[py|exe]` will install tools (mkspiffs, toolchain etc.) into folder `arduino-esp32/tools`. Feel free to skip running this script if you want to configure the environment like above.

#### ESP-IDF (optional for method 2)
Read more instructions from https://docs.espressif.com/projects/esp-idf.

```bash
sudo apt-get install python flex bison gperf cmake ninja-build ccache libffi-dev libssl-dev dfu-util
cd $ESP_PATH
git clone --depth=1 --recursive https://github.com/espressif/esp-idf.git
export IDF_PATH = ${ESP_PATH}/esp-idf
```

The thing is, running `esp-idf/install.[sh|bat]` do install necessary tools, but it will also touch your system packages and environment variables **a lot**. All you need is just a toolchain (xtensa-esp32-elf), C libs (ESP-IDF) and a burning program (esptool.py). So unless you are ok with adding too much scripts into $PATH and install a lot of things under `~/.espressif`, we suggest to configure a clean and easy-to-manage files structure like above.

?> Native esp-idf make system depends on many Python scripts. Here we suggest using [pyenv](https://github.com/pyenv/pyenv) as a manager of multiple versions of Python (if you have).

#### Cloud3DP
```bash
git clone https://github.com/hankso/Cloud3DP.git
cd Cloud3DP
ln -sf ${ESP_PATH}/components ./components
```

# Build

Open and edit file `Cloud3DP/Makefile` to select a compiling method. Then run  `make`:

```bash
make all        # build app/partition/ota_data/spiffs etc.
make flash      # burn firmware through USB
make monitor    # UART communication
make help       # see all targets
```

?> You should change paths defined in `Cloud3DP/tools/arduino-esp32.mk` according to your position of esp-idf and arduino-esp32 for makeEspArduino.

# FAQs

#### Why use two different versions of toolchain?
Using two version of compilers is **NOT** necessary.

Because the development of arduino-esp32 is a little bit slow than esp-idf,  when the author write this document, esp-idf just released v4.2 and the newest arduino-esp32 is v1.0.4, which depends on esp-idf v3.2. Toolchain xtensa-esp32-elf-5.2.0 is for esp-idf v3.2 and 8.2.0 for esp-idf v4.2.

Some important features like `sdspi_*` and `esp_app_desc_t` are not committed in v3.2. We finally decide to use esp-idf v3.3, which is compitable with arduino-esp32 v1.0.4 and supports enough features.

#### Why use arduino-esp32 instead of native esp-idf?
In Cloud3DP, the library `ESPAsyncWebServer` is used to develop HTTP APIs and WebSocket server. And ESPAsyncWebServer depends on arduino-esp32.
