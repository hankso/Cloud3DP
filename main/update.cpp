/* 
 * File: update.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-23 11:42:48
 */

#include "update.h"
#include "config.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "sys/param.h"

static const char *TAG = "ota";

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_ca_cert_pem_end");

static struct {
    const esp_partition_t *target, *running;
    esp_app_desc_t info;
    esp_ota_handle_t handle;
    esp_err_t error;
    size_t saved, total;
} ota_updation_st;

static const char * const ota_img_states[] = {
    "New", "Pending", "Valid", "Invalid", "Aborted", "Unknown",
    "Running", "OTANext"
};

bool check_valid() { return true; }

void ota_initialize() {
    esp_log_level_set(TAG, ESP_LOG_WARN);
    const esp_partition_t
        *running = esp_ota_get_running_partition(),
        *target = esp_ota_get_last_invalid_partition();
    if (!target) target = esp_ota_get_next_update_partition(running);
    if (target) {
        ota_updation_st.target = target;
        esp_ota_get_partition_description(target, &ota_updation_st.info);
        ota_updation_reset();
    } else {
        ota_updation_st.error = ESP_ERR_NOT_FOUND;
    }
    ota_updation_st.running = running;
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

void ota_updation_reset() {
    ota_updation_st.handle = 0;
    ota_updation_st.error = ESP_OK;
    ota_updation_st.saved = 0;
    ota_updation_st.total = ota_updation_st.target ? \
                            ota_updation_st.target->size : OTA_SIZE_UNKNOWN;
}

bool ota_updation_partition(const char *label) {
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
    esp_err_t err = ESP_OK;
    if (!part) err = ESP_ERR_NOT_FOUND;
    else err = esp_ota_set_boot_partition(part);
    ota_updation_st.error = err;
    return err == ESP_OK;
}

bool ota_updation_begin(size_t size) {
    if (!ota_updation_st.target) return false;
    if (ota_updation_st.handle) return true;
    ota_updation_reset();
    size_t part_size = ota_updation_st.target->size;
    if (size && size != OTA_SIZE_UNKNOWN) {
        if (size > part_size) {
            ota_updation_st.error = ESP_ERR_INVALID_SIZE;
            return false;
        } else if (size == part_size) {
            // esp_ota_begin will erase (size/sector + 1) * sector
            // So we either make size one byte smaller than partition size
            // or set size to OTA_SIZE_UNKNOWN which means whole partition
            size--;
        } else {
            ota_updation_st.total = size;
        }
    }
    ota_updation_st.error = esp_ota_begin(
        ota_updation_st.target, size, &ota_updation_st.handle);
    if (ota_updation_st.error) {
        ota_updation_st.handle = 0;
        ESP_LOGE(TAG, "OTA init error: %s", ota_updation_error());
        return false;
    }
    return true;
}

bool ota_updation_write(uint8_t *data, size_t size) {
    if (!ota_updation_st.handle || ota_updation_st.error) return false;
    ota_updation_st.error = esp_ota_write(ota_updation_st.handle, data, size);
    if (ota_updation_st.error) {
        ESP_LOGE(TAG, "OTA write error: %s", ota_updation_error());
        return false;
    }
    ota_updation_st.saved += size;
    printf("\rProgress: %.2f%% %d/%d KB",
           (float)ota_updation_st.saved / ota_updation_st.total * 100,
           ota_updation_st.saved / 1024, ota_updation_st.total / 1024);
    return true;
}

bool ota_updation_end() {
    if (!ota_updation_st.handle) return false;
    printf("\n"); // enter newline after ota_updation_write progress
    ota_updation_st.error = esp_ota_end(ota_updation_st.handle);
    if (ota_updation_st.error) {
        ESP_LOGE(TAG, "OTA end error: %s", ota_updation_error());
        return false;
    }
    esp_err_t err = esp_ota_set_boot_partition(ota_updation_st.target);
    ESP_LOGI(TAG, "Set boot partition to %s %s",
             ota_updation_st.target->label, esp_err_to_name(err));
    ota_updation_st.handle = 0;
    return true;
}

const char * ota_updation_error() {
    return ota_updation_st.error != ESP_OK ? \
            esp_err_to_name(ota_updation_st.error) : NULL;
}

esp_err_t ota_updation_fetch_url(const char *url) {
    if (!ota_updation_st.target) return ota_updation_st.error;
    esp_http_client_config_t config;
    config.url = url;
    // {
    //     .url = url
    //     .cert_pem = (char *)server_cert_pem_start,
    // };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;
    size_t
        hsize = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t),
        asize = sizeof(esp_app_desc_t);
    uint8_t *ota_buf = (uint8_t *)calloc(1024 + 1, sizeof(uint8_t));
    if (!ota_buf) return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_OK;
    if ( (err = esp_http_client_open(client, 0)) ) goto http_clean;
    esp_http_client_fetch_headers(client);
    int rsize;
    while ( (rsize = esp_http_client_read(client, (char *)ota_buf, 1024)) ) {
        if (rsize < 0) {
            err = ESP_FAIL;
            goto http_clean;
        } else if (!ota_updation_st.handle) {
            if (rsize < (hsize + asize)) {
                err = ESP_FAIL;
                goto http_clean;
            }
            esp_app_desc_t new_info, run_info, ota_info = ota_updation_st.info;
            const esp_partition_t *running = ota_updation_st.running;
            memcpy(&new_info, ota_buf + hsize, asize);
            printf("New app firmware version: %s\n", new_info.version);
            esp_ota_get_partition_description(running, &run_info);
            printf("Running firmware version: %s\n", run_info.version);
            size_t len = sizeof(new_info.version);
            if (!memcmp(new_info.version, ota_info.version, len) ||
                !memcmp(new_info.version, run_info.version, len)) {
                printf("Running version is the same as new app. Skip\n");
                err = ESP_ERR_INVALID_STATE;
                goto http_clean;
            }
            if (!ota_updation_begin()) goto ota_err;
        }
        if (!ota_updation_write(ota_buf, rsize)) goto ota_err;
    }
    if (!ota_updation_end()) goto ota_err;
    goto http_clean;

ota_err:
    err = ota_updation_st.error;
http_clean:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (ota_buf) free(ota_buf);
    return err;
}

bool ota_updation_url(const char *url) {
    if (!url || !strlen(url)) url = Config.app.OTA_URL;
    printf("OTA Updation from URL `%s`\n", url);
    if (!strlen(url)) {
        ota_updation_st.error = ESP_ERR_INVALID_ARG;
        return false;
    }
    if (!strbool(Config.app.OTA_RUN)) {
        printf("OTA Updation not enabled: `%s`\n", Config.app.OTA_RUN);
        return false;
    }
    return (ota_updation_st.error = ota_updation_fetch_url(url)) == ESP_OK;
}

static const char * ota_get_img_state(const esp_partition_t *part) {
    static const esp_partition_t
        *boot = esp_ota_get_boot_partition(),
        *next = ota_updation_st.target,
        *error = esp_ota_get_last_invalid_partition(),
        *running = ota_updation_st.running;
    if (!boot) boot = running;
    if (part->address == boot->address) {
        return boot->address == running->address ? "Boot *" : "Boot";
    }
    else if (part->address == running->address) return ota_img_states[6];
    else if (next && part->address == next->address) return ota_img_states[7];
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

void ota_partition_info() {
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
        if ( (err = esp_ota_get_partition_description(part, &desc)) ) {
            printf("%-8.7s" "%-8.7s" "%s\n",
                   ota_img_states[3], part->label, esp_err_to_name(err));
            continue;
        }
        printf("%-8.7s" "%-8.7s" "0x%06X 0x%06X " "%-6.6s %7.7s "
               "%-8.8s.. %s %s\n",
               ota_get_img_state(part),
               part->label, part->address, part->size,
               desc.idf_ver, desc.version,
               format_sha256(desc.app_elf_sha256, 8),
               desc.date, desc.time);
    }
    esp_partition_iterator_release(iter);
}
