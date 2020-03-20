/*************************************************************************
File: ${ESP_BASE}/projects/Temprature/main/Temp.ino
Author: Hankso
Time: Tue 21 May 2019 09:46:44 CST
************************************************************************/

#include <Arduino.h>
#include <WiFi.h>

#include "gpio.h"
#include "config.h"
#include "server.h"
#include "globals.h"
#include "console.h"
#include "max6675.h"

#include "driver/uart.h"
#include "esp_vfs_dev.h"

static const char *TAG = Config.info.NAME;

void wifi_initialize() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    if (strlen(Config.net.STA_NAME)) {
        WiFi.begin(Config.net.STA_NAME, Config.net.STA_PASS);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500); printf(".");
        }
        printf("\nWiFi connected to `%s`\n", Config.net.STA_NAME);
        printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    }
    IPAddress addr;
    if (!addr.fromString(Config.net.AP_HOST)) {
        addr = IPAddress(10, 0, 1, 1);
    }
    // TODO: set gateway
    WiFi.softAPConfig(addr, addr, IPAddress(255, 255, 255, 0)); delay(100);
    char SSID[strlen(Config.net.AP_NAME) + strlen(Config.info.UID) + 2];
    sprintf(SSID, "%s-%s", Config.net.AP_NAME, Config.info.UID);
    WiFi.softAP(SSID, Config.net.AP_PASS, 0, 2);
    printf("You can connect to WiFi hotspot %d: `%s`\n", strlen(SSID), SSID);
    printf("Visit http://%s/index.html\n", WiFi.softAPIP().toString().c_str());
    LIGHTBLK(10, 10);
}

void uart_initialize() {
    // UART driver configuration
    uart_port_t num = static_cast<uart_port_t>(Config.info.UART);
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
    // Idle tasks are created on each core automatically by RTOS scheduler
    // with the lowest possible priority (0). Our tasks have higher priority,
    // thus leaving almost no time for idle tasks to run. Disable WDT on them.
#if defined(CONFIG_AUTOSTART_ARDUINO)
    disableCore0WDT();
    #ifndef CONFIG_FREERTOS_UNICORE
    disableCore1WDT();
    #endif // CONFIG_FREERTOS_UNICORE
#elif defined(CONFIG_TASK_WDT)
    #ifndef CONFIG_FREERTOS_UNICORE
    uint8_t num = 2;
    #else
    uint8_t num = 1;
    #endif // CONFIG_FREERTOS_UNICORE
    while (num--) {
        TaskHandle_t idle = xTaskGetIdleTaskHandleForCPU(num);
        if (idle && esp_task_wdt_delete(idle) == ESP_OK) {
            ESP_LOGI(TAG, "Task IDLE%d @ CPU%d removed from WDT", num, num);
        }
    }
#endif // CONFIG_AUTOSTART_ARDUINO & CONFIG_TASK_WDT
}

void init() {
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    uart_initialize();    ESP_LOGI(TAG, "Initializing UART Serial Connection");
    config_initialize();  ESP_LOGI(TAG, "Initializing Configuration");
    twdt_initialize();    ESP_LOGI(TAG, "Initializing Task Watchdog Timer");
    gpio_initialize();    ESP_LOGI(TAG, "Initializing GPIO functions");
    wifi_initialize();    ESP_LOGI(TAG, "Initializing WiFi Connection");
    console_initialize(); ESP_LOGI(TAG, "Initializing Command Line Console");
    fflush(stdout);
}

void initVariant() {  // testing features. maybe removed in the future
    max6675_init();
}

void setup() {
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
    /* max6675_read(); */
    delay(5000);
    
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
    init(); initVariant(); setup();
    xTaskCreate(loopTask, "main-loop", 8192, NULL, 2, NULL);
}
#endif // CONFIG_AUTOSTART_ARDUINO
