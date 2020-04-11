/*************************************************************************
File: ${ESP_BASE}/projects/Temprature/main/Temp.ino
Author: Hankso
Time: Tue 21 May 2019 09:46:44 CST
************************************************************************/

#include <Arduino.h>

#include "globals.h"
#include "gpio.h"
#include "wifi.h"
#include "config.h"
#include "update.h"
#include "server.h"
#include "console.h"

#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "esp_task_wdt.h"

static const char *TAG = "main";

void uart_initialize() {
    // UART driver configuration
    uart_port_t num = static_cast<uart_port_t>(CONFIG_CONSOLE_UART_NUM);
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .use_ref_tick = true
    };
    ESP_ERROR_CHECK( uart_param_config(num, &uart_config) );
    ESP_ERROR_CHECK( uart_driver_install(num, 256, 0, 0, NULL, 0) );
    // Register UART to VFS and configure
    /* esp_vfs_dev_uart_register(); */
    esp_vfs_dev_uart_use_driver(num);
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
}

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

void init() {
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Initializing OTA Updation");	        ota_initialize();
    ESP_LOGI(TAG, "Initializing UART Serial");          uart_initialize();
    ESP_LOGI(TAG, "Initializing Configuration");	    config_initialize();
    ESP_LOGI(TAG, "Initializing Task Watchdog Timer");	twdt_initialize();
    ESP_LOGI(TAG, "Initializing GPIO functions");	    gpio_initialize();
    ESP_LOGI(TAG, "Initializing WiFi Connection");	    wifi_initialize();
    ESP_LOGI(TAG, "Initializing Command Line Console"); console_initialize();
    fflush(stdout);
}

void setup() {
    wifi_loop_begin();
    server_loop_begin();
    console_loop_begin();
}

/*
 * Task list:
 *  WiFi/WebServer Core 1
 *  Console (command dispatcher) Core 1
 *  GCode parser Core 0
 */

void loop() {
    LIGHTBLK(100);
    delay(2500);
    
}

#ifndef CONFIG_AUTOSTART_ARDUINO
void loopTask(void *pvParameters) {
    while (true) {
        loop();
#ifdef CONFIG_TASK_WDT
        esp_task_wdt_reset();
#endif
    }
}

void app_main() {
    init(); setup();
    xTaskCreate(loopTask, "main-loop", 8192, NULL, 2, NULL);
}
#endif // CONFIG_AUTOSTART_ARDUINO
