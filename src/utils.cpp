/* 
 * File: utils.c
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-20 21:37:14
 */

#include "globals.h"
#include "config.h"

#include <math.h>
#include <sys/param.h>
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

char * cast_away_const(const char *str) {
    char *buf = (char *)malloc(strlen(str) + 1);
    if (buf != NULL)
        snprintf(buf, strlen(str) + 1, str);
    return buf;
}

char * cast_away_const_force(const char *str) {
    return (char *)(void *)(const void *)str;
}

char hexdigits(uint8_t v) {
    return (v < 10) ? (v + '0') : (v - 10 + 'a');
}

const char * format_sha256(const uint8_t *src, size_t len) {
    static char buf[64 + 1];
    for (uint8_t i = 0; i < 32; i++) {
        buf[2 * i] = hexdigits(src[i] >> 4);
        buf[2 * i + 1] = hexdigits(src[i] & 0xf);
    }
    buf[MIN(len, 64)] = '\0';
    return buf;
}

const char * format_mac(const uint8_t *src, size_t len) {
    static char buf[17 + 1]; // XX:XX:XX:XX:XX:XX
    for (uint8_t i = 0; i < 6; i++) {
        buf[3 * i] = hexdigits(src[i] >> 4);
        buf[3 * i + 1] = hexdigits(src[i] & 0xf);
        buf[3 * i + 2] = (i == 5) ? '\0' : ':';
    }
    buf[MIN(len, 17)] = '\0';
    return buf;
}

static const char units[] = "BKMGT";

// Note: format_size format string into static buffer. Therefore you don't
// need to free the buffer after logging/strcpy etc. But you must save the
// result before calling format_size once again, because the buffer will be
// reused and overwriten.
const char * format_size(size_t size) {
    static char buf[7 + 1 + 1];  // xxxx.xxu\0
    static uint8_t maxlen = strlen(units) - 1;
    uint8_t exponent = log2(size) / 10;
    uint8_t idx = exponent > maxlen ? maxlen : exponent;
    snprintf(buf, sizeof(buf), "%.*f%c", idx > 2 ? 2 : idx,
             size/pow(1024, exponent), units[idx]);
    return buf;
}

// Running Ready Blocked Suspended Deleted
static const char task_states[] = "*RBSD";

void task_info() {
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    uint32_t ulTotalRunTime;
    uint16_t num = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasks = pvPortMalloc(num * sizeof(TaskStatus_t));
    if (tasks == NULL) {
        printf("Cannot allocate space for tasks list");
        return;
    }
    num = uxTaskGetSystemState(tasks, num, &ulTotalRunTime);
    if (!num || !ulTotalRunTime) {
        printf("TaskStatus_t array size too small. Skip");
        vPortFree(tasks);
        return;
    }
    printf("TID State" " Name\t\t" "Pri CPU%%" "Counter Stack\n");
    for (uint16_t i = 0; i < num; i++) {
        printf("%3d (%c)\t " " %s\t\t" "%3d %4.1f" "%7lu %5.5s\n",
               tasks[i].xTaskNumber, task_states[tasks[i].eCurrentState],
               tasks[i].pcTaskName, tasks[i].uxCurrentPriority,
               100.0 * tasks[i].ulRunTimeCounter / ulTotalRunTime,
               tasks[i].ulRunTimeCounter,
               format_size(tasks[i].usStackHighWaterMark));
    }
    vPortFree(tasks);
#else
    printf("Unsupported command! Enable `CONFIG_FREERTOS_USE_TRACE_FACILITY` "
           "in menuconfig/sdkconfig to run this command\n");
#endif
}

void version_info() {
    const esp_app_desc_t *desc = esp_ota_get_app_description();
    printf("IDF Version: %s based on FreeRTOS %s\n"
           "Firmware Version: %s %s\n"
           "Compile time: %s %s\n",
           esp_get_idf_version(), tskKERNEL_VERSION_NUMBER,
           // Config.info.NAME, Config.info_VER,
           desc->project_name, desc->version,
           __DATE__, __TIME__);
}

static uint16_t memory_types[] = {
    MALLOC_CAP_EXEC, MALLOC_CAP_DMA, MALLOC_CAP_INTERNAL, MALLOC_CAP_DEFAULT
};

static const char * const memory_names[] = {
    "Execute", "DMA", "Intern", "Default"
};

void memory_info() {
    uint8_t num = sizeof(memory_types)/sizeof(memory_types[0]);
    multi_heap_info_t info;
    printf(/*"Address"*/ "Type\tSize\tUsed\tAvail\tUsed%%\n");
    while (num--) {
        heap_caps_get_info(&info, memory_types[num]);
        size_t size = info.total_free_bytes + info.total_allocated_bytes;
        printf("%-8.7s%8.7s", memory_names[num], format_size(size));
        printf("%8.7s", format_size(info.total_allocated_bytes));
        printf("%8.7s%5.1f\n",
               format_size(info.total_free_bytes),
               100.0 * info.total_allocated_bytes / size);
    }
    printf("Total: free %dK used %dK %lu/%lu blocks\n",
           info.total_free_bytes, info.total_allocated_bytes,
           info.free_blocks, info.total_blocks);
}

void hardware_info() {
    esp_chip_info_t info;
    esp_chip_info(&info);
    printf(
        "Chip UID: %s-%s\n"
        " model: %s\n"
        " cores: %d\n"
        " revision number: %d\n"
        " feature: %s%s%s/%s-Flash: %s\n",
        Config.info.NAME, Config.info.UID,
        info.model == CHIP_ESP32 ? "ESP32" : "???",
        info.cores, info.revision,
        info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
        info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
        info.features & CHIP_FEATURE_BT ? "/BT" : "",
        info.features & CHIP_FEATURE_EMB_FLASH ? "Embedded" : "External",
        format_size(spi_flash_get_chip_size())
    );

    uint8_t mac[2][6];
    esp_read_mac(mac[0], ESP_MAC_WIFI_STA);
    esp_read_mac(mac[1], ESP_MAC_WIFI_SOFTAP);
    printf("STA MAC address: %s\n", format_mac(mac[0], 17));
    printf("AP  MAC address: %s\n", format_mac(mac[1], 17));
}

static esp_partition_type_t partition_types[] = {
    ESP_PARTITION_TYPE_DATA, ESP_PARTITION_TYPE_APP
};

void partition_info() {
    static uint8_t max = sizeof(partition_types)/sizeof(partition_types[0]);
    uint8_t idx, num = 0;
    const esp_partition_t * parts[16], *part, *tmp;
    esp_partition_iterator_t iter;
    for (uint8_t i = 0; i < max; i++) {
        iter = esp_partition_find(
            partition_types[i],
            ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (iter != NULL && num < 16) {
            part = esp_partition_get(iter);
            for (idx = 0; idx < num; idx++) {
                if (parts[idx]->address < part->address) {
                    tmp = parts[idx]; parts[idx] = part; part = tmp;
                }
            }
            parts[num++] = part;
            iter = esp_partition_next(iter);
        }
        esp_partition_iterator_release(iter);
    }
    if (!num) {
        printf("No partitons found in flash. Skip");
        return;
    }
    printf("LabelName\tType\tSubType\t" "Offset\t Size\t  " "Secure\n");
    while (num--) {
        part = parts[num];
        printf("%-16.15s" "%s\t0x%02x\t" "0x%06x 0x%06x " "%s\n",
               part->label, part->type ? "data" : "app", part->subtype,
               part->address, part->size,
               part->encrypted ? "true" : "false");
    }
}
