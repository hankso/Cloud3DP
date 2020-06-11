#include <Arduino.h>

#include "globals.h"
#include "drivers.h"
#include "wifi.h"
#include "config.h"
#include "update.h"
#include "server.h"
#include "console.h"
#include "filesys.h"

#include "esp_task_wdt.h"

static const char *TAG = "main";

void twdt_initialize() {
#ifdef CONFIG_TASK_WDT
    ESP_ERROR_CHECK( esp_task_wdt_init(5, false) ); // update esp_int_wdt_init

    // Idle tasks are created on each core automatically by RTOS scheduler
    // with the lowest possible priority (0). Our tasks have higher priority,
    // thus leaving almost no time for idle tasks to run. Disable WDT on them.
    #ifndef CONFIG_FREERTOS_UNICORE
    uint8_t num = 2;
    #else
    uint8_t num = 1;
    #endif // CONFIG_FREERTOS_UNICORE
    while (num--) {
        TaskHandle_t idle = xTaskGetIdleTaskHandleForCPU(num);
        if (idle && !esp_task_wdt_status(idle) && !esp_task_wdt_delete(idle)) {
            ESP_LOGW(TAG, "Task IDLE%d @ CPU%d removed from WDT", num, num);
        }
    }
#endif // CONFIG_TASK_WDT
}

void fs_initialize() {
    if (FFS.begin(false, FFS_MP, 10)) FFS.printInfo();
    if (SDFS.begin(false, SDFS_MP, 5)) SDFS.printInfo();
    fflush(stdout);
}

/*
 * Task list:
 *  WiFi/AsyncTCP/WebServer Core 0
 *  Console (command dispatcher) Core 1
 *  Marlin GCode parser Core 1
 */

void init() {
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Init OTA Updation");	        ota_initialize();
    ESP_LOGI(TAG, "Init Configuration");	    config_initialize();
    ESP_LOGI(TAG, "Init Task Watchdog Timer");	twdt_initialize();
    ESP_LOGI(TAG, "Init File Systems");         fs_initialize();
    ESP_LOGI(TAG, "Init GPIO Drivers");	        driver_initialize();
    ESP_LOGI(TAG, "Init WiFi Connection");	    wifi_initialize();
    ESP_LOGI(TAG, "Init Command Line Console"); console_initialize();
    fflush(stdout);
}

void setup() {
    wifi_loop_begin();
    server_loop_begin();
    console_loop_begin();
}

void loop() {
    led_blink(0, 1000);
    vTaskDelay(pdMS_TO_TICKS(5000));
#ifdef CONFIG_TASK_WDT
    esp_task_wdt_reset();
#endif // CONFIG_TASK_WDT
}

#if !CONFIG_AUTOSTART_ARDUINO
void loopTask(void *pvParameters) {
    for (;;) {
        loop();
    }
}

extern "C" void app_main() {
    init(); setup();
    //                      function   task name  stacksize param prio hdlr cpu
    xTaskCreatePinnedToCore(loopTask, "main-loop", 1024 * 4, NULL, 1, NULL, 1);
}
#endif // CONFIG_AUTOSTART_ARDUINO
