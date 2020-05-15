/* 
 * File: update.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-23 11:42:48
 */

#include "update.h"
#include "globals.h"

#include <Update.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const esp_partition_t *running = NULL;

static const char * const ota_img_states[] = {
    "New", "Pending", "Valid", "Invalid", "Aborted", "Unknown"
};

void ota_initialize() {
    running = esp_ota_get_running_partition();
#if defined(CONFIG_APP_ROLLBACK_ENABLE) && !defined(CONFIG_AUTOSTART_ARDUINO)
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state)) return;
    else if (state != ESP_OTA_IMG_PENDING_VERIFY) return;
    if (check_valid()) {
        ESP_LOGW(TAG, "App validation success!");
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (!err) err = esp_ota_erase_last_boot_app_partition();
        if (err) {
            ESP_LOGE(TAG, "Anti-rollback error: %s", esp_err_to_name(err));
        } else {
            ESP_LOGE(TAG, "Last boot partition erased!");
        }
    } else if (esp_ota_check_rollback_is_possible()) {
        esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
        if (err) {
            ESP_LOGE(TAG, "Cannot rollback: %s", esp_err_to_name(err));
            // esp_restart();  // still running with error
        }
    } else ESP_LOGE(TAG, "App validation fail!");
#endif // CONFIG_APP_ROLLBACK_ENABLE
}

bool ota_updation_begin() {
    // Update.runAsync(true);
    if (!Update.begin()) {
        ESP_LOGE(TAG, "Update error: %s", Update.errorString());
        return false;
    }
    Update.onProgress([](uint32_t progress, size_t size){
        float rate = (float)progress / size * 100;
        progress /= 1024; size /= 1024;
        ESP_LOGE(TAG, "\rProgress: %.2f%% %d/%d KB", rate, progress, size);
    });
    return true;
}

bool ota_updation_write(uint8_t *data, size_t len) {
    if (!Update.hasError() && Update.write(data, len) != len) {
        ESP_LOGE(TAG, "Update error: %s", Update.errorString());
        return false;
    }
    return true;
}

const char * ota_updation_error() {
    return Update.hasError() ? Update.errorString() : NULL;
}

bool ota_updation_end() {
    ESP_LOGE(TAG, "");
    return Update.end(true);
}

bool ota_update_from_url(char *url) {
    esp_ota_handle_t handle;
    printf("Currently running partition: `%s`\n", running->label);
}

static const char * ota_get_img_state(const esp_partition_t *part) {
    if (!running) return ota_img_states[5];
    static const esp_partition_t
        *boot = esp_ota_get_boot_partition(),
        *next = esp_ota_get_next_update_partition(running),
        *error = esp_ota_get_last_invalid_partition();
    if (!boot) boot = running;
    if (part->address == boot->address) {
        return boot->address == running->address ? "Boot *" : "Boot";
    }
    else if (part->address == running->address) return "Running";
    else if (next && part->address == next->address) return "OTANext";
    else if (error && part->address == error->address) return ota_img_states[3];
    else {
        esp_ota_img_states_t state;
        if (esp_ota_get_state_partition(part, &state) || state > 5) {
            return ota_img_states[5];
        } else {
            return ota_img_states[state];
        }
    }
}

void ota_info() {
    esp_partition_iterator_t iter = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (iter == NULL) {
        printf("No OTA partition found. Skip");
        return;
    }
    printf("State\tLabel\t" "Offset\t Size\t  " "IDF    Version "
           "SHA256[:8] Time\n");
    const esp_partition_t *part;
    esp_app_desc_t desc;
    esp_err_t err;
    while (iter!= NULL) {
        part = esp_partition_get(iter);
        iter = esp_partition_next(iter);
        if (err = esp_ota_get_partition_description(part, &desc)) {
            printf("Invalid %-8.7s%s\n", part->label, esp_err_to_name(err));
            continue;
        }
        printf("%-8.7s" "%-8.7s" "0x%06x 0x%06x " "%-6.6s %7.7s "
               "%-10.8s %s %s\n",
               ota_get_img_state(part),
               part->label, part->address, part->size,
               desc.idf_ver, desc.version, 
               format_sha256(desc.app_elf_sha256, 8),
               desc.date, desc.time);
    }
    esp_partition_iterator_release(iter);
}
