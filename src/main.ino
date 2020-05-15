#include <Arduino.h>

#include "globals.h"
#include "drivers.h"
#include "wifi.h"
#include "config.h"
#include "update.h"
#include "server.h"
#include "console.h"

#include "esp_task_wdt.h"

static const char *TAG = "main";

void twdt_initialize() {
#ifdef CONFIG_TASK_WDT
    ESP_ERROR_CHECK( esp_task_wdt_init(5, false) ); // update esp_int_wdt_init

    // Idle tasks are created on each core automatically by RTOS scheduler
    // with the lowest possible priority (0). Our tasks have higher priority,
    // thus leaving almost no time for idle tasks to run. Disable WDT on them.
#ifdef CONFIG_AUTOSTART_ARDUINO
    disableCore0WDT();
    #ifndef CONFIG_FREERTOS_UNICORE
    disableCore1WDT();
    #endif // CONFIG_FREERTOS_UNICORE
#else // CONFIG_AUTOSTART_ARDUINO
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
#endif // CONFIG_AUTOSTART_ARDUINO
#endif // CONFIG_TASK_WDT
}

/*
 * Task list:
 *  WiFi/WebServer Core 1
 *  Console (command dispatcher) Core 1
 *  Marlin GCode parser Core 0
 */

void init() {
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Init OTA Updation");	        ota_initialize();
    ESP_LOGI(TAG, "Init Configuration");	    config_initialize();
    ESP_LOGI(TAG, "Init Task Watchdog Timer");	twdt_initialize();
    ESP_LOGI(TAG, "Init GPIO Drivers");	        driver_initialize();
    ESP_LOGI(TAG, "Init Flash File Systems");   FFS.begin(false, FFS_MP);
    ESP_LOGI(TAG, "Init SD Card SPI FS");       SDFS.begin(false, SDFS_MP);
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
    delay(5000);
    
}

#ifndef CONFIG_AUTOSTART_ARDUINO
void loopTask(void *pvParameters) {
    while (true) {
        loop();
#ifdef CONFIG_TASK_WDT
        esp_task_wdt_reset();
#endif // CONFIG_TASK_WDT
    }
}

void app_main() {
    init(); setup();
    xTaskCreate(loopTask, "main-loop", 1024 * 4, NULL, 2, NULL);
}
#endif // CONFIG_AUTOSTART_ARDUINO
