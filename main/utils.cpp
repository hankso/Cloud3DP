/* 
 * File: utils.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-20 21:37:14
 */

#include "globals.h"
#include "config.h"

#include "esp_system.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

char * cast_away_const(const char *str) {
    char *buf = (char *)malloc(strlen(str) + 1);
    if (buf != NULL) snprintf(buf, strlen(str) + 1, str);
    return buf;
}

char * cast_away_const_force(const char *str) {
    return (char *)(void *)(const void *)str;
}

int task_info() {
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    uint32_t ulTotalRunTime;
    uint16_t num = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasks = pvPortMalloc(num * sizeof(TaskStatus_t));
    if (tasks == NULL) {
        printf("Cannot allocate space for tasks list");
        return 1;
    }
    num = uxTaskGetSystemState(tasks, num, &ulTotalRunTime);
    if (!num || !ulTotalRunTime) {
        printf("TaskStatus_t array size too small. Skip");
        vPortFree(tasks);
        return 1;
    }
    const char *states = "*RBSD"; // Running Ready Blocked Suspended Deleted
    printf("TID State" " Name\t\t" "Pri CPU%%" "Counter Stack\n");
    for (uint16_t i = 0; i < num; i++) {
        printf("%3d (%c)\t " " %s\t\t" "%3d %4.1f" "%7lu %4.1fK\n",
               tasks[i].xTaskNumber, states[tasks[i].eCurrentState],
               tasks[i].pcTaskName, tasks[i].uxCurrentPriority,
               100.0 * tasks[i].ulRunTimeCounter / ulTotalRunTime,
               tasks[i].ulRunTimeCounter,
               tasks[i].usStackHighWaterMark / 1024.0);
    }
    vPortFree(tasks);
#else
    printf("Unsupported command! Enable `CONFIG_FREERTOS_USE_TRACE_FACILITY` "
           "in menuconfig/sdkconfig to run thir command\n");
#endif
    return 0;
}

int version_info() {
    esp_chip_info_t info;
    esp_chip_info(&info);
    printf(
        "IDF Version: %s based on FreeRTOS %s\n"
        "Chip info:\n"
        " model: %s\n"
        " cores: %d\n"
        " revision number: %d\n"
        " feature: %s%s%s/%s-Flash: %.1f MB\n",
        esp_get_idf_version(), tskKERNEL_VERSION_NUMBER,
        info.model == CHIP_ESP32 ? "ESP32" : "???",
        info.cores, info.revision,
        info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
        info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
        info.features & CHIP_FEATURE_BT ? "/BT" : "",
        info.features & CHIP_FEATURE_EMB_FLASH ? "Embedded" : "External",
        spi_flash_get_chip_size() / (1024.0 * 1024)
    );
    printf("Chip ID: %s %s\n", "TODO", Config.info.UID);
    printf("Firmware Version: %s %s\n", Config.info.NAME, Config.info.VER);
    return 0;
}

int memory_info() {
    return 1;
    multi_heap_info_t info; heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    uint32_t mfree = info.total_free_bytes, mallo = info.total_allocated_bytes;
    double mrate = 100.0 * mfree / (mfree + mallo);
    fprintf(stderr, "Free mem: %d - %d", mfree, mallo);
    uint8_t mac1[6], mac2[6];
    esp_read_mac(mac1, ESP_MAC_WIFI_STA);
    esp_read_mac(mac2, ESP_MAC_WIFI_SOFTAP);
    char smac1[18], smac2[18];
    for (uint8_t i = 0; i < 6; i++) {
        sprintf(smac1 + strlen(smac1), "%s%02x", i ? ":" : "", mac1[i]);
        sprintf(smac2 + strlen(smac2), "%s%02x", i ? ":" : "", mac2[i]);
    }

    printf("ESP free memory:  %.2f KB, %d%%\n", mfree / 1024.0, mrate);
    printf("ESP total memory: %.2f KB, %d%%\n", mallo / 1024.0, mrate);
    printf("WiFi status:      %s\n", "TODO");
    printf("STA MAC address:  %s\n", smac1);
    printf("AP  MAC address:  %s\n", smac2);
}

int partition_info() {
    uint8_t idx, num = 0;
    const esp_partition_t * parts[16], *part, *tmp;
    esp_partition_type_t type[2] = {
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_TYPE_APP
    };
    esp_partition_iterator_t iter;
    for (uint8_t i = 0; i < 2; i++) {
        iter = esp_partition_find(type[i], ESP_PARTITION_SUBTYPE_ANY, NULL);
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
        return 1;
    }
    printf("LabelName\tType\tSubType\t" "Offset\t Size\t " " Encryption\n");
    while (num-- > 0) {
        part = parts[num];
        printf("%-16.15s" "%s\t0x%02x\t" "0x%06x 0x%06x" " %s\n",
               part->label, part->type ? "data" : "app", part->subtype,
               part->address, part->size,
               part->encrypted ? "true" : "false");
    }
    return 0;
}
