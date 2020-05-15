/* 
 * File: SDSPIFS.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-05-15 19:42:04
 */

#include "SDSPIFS.h"
#include "drivers.h"

#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"

using namespace fs;

bool SDSPIFSFS::begin(bool format, const char *base, uint8_t max) {
    esp_vfs_fat_sdmmc_mount_config_t mount_conf = {
        .format_if_mount_failed = format,
        .max_files = max,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host_conf = SDSPI_HOST_DEFAULT();
    host_conf.slot = HSPI_HOST;

    /* SDSPI_SLOT_CONFIG_DEFAULT() */
    sdspi_slot_config_t slot_conf = {
        .gpio_miso = PIN_HMISO,
        .gpio_mosi = PIN_HMOSI,
        .gpio_sck  = PIN_HSCLK,
        .gpio_cs   = PIN_HCS1,
        .gpio_cd   = PIN_HQDHD,
        .gpio_wp   = SDSPI_SLOT_NO_WP,
        .dma_channel = 1,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(
        base, &host_conf, &slot_conf, &mount_conf, &card);
    if (err) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(err));
        return false;
    } else {
        _impl->mountpoint(base);
        return true;
    }
}

void SDSPIFSFS::info() { if (card) sdmmc_card_print_info(stdout, card); }

void SDSPIFSFS::end() {
    esp_err_t err = esp_vfs_fat_sdmmc_unmount();
    if (!err) _impl->mountpoint(NULL);
}

SDSPIFSFS SDSPIFS;
