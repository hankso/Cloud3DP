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
    printf("You can connect to WiFi hotspot %s\n", SSID);
    printf("Visit http://%s.\n", WiFi.softAPIP().toString().c_str());
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
#if defined(CONFIG_AUTOSTART_ARDUINO)
    enableCore0WDT();
    #ifndef CONFIG_FREERTOS_UNICORE
    disableCore1WDT();
    #endif // CONFIG_FREERTOS_UNICORE
#elif defined(CONFIG_TASK_WDT)
    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCPU(0);
    if (idle0 && \
        esp_task_wdt_add(idle0) == ESP_OK && \
        esp_task_wdt_status(idle0) == ESP_OK
    ) {
        ESP_LOGD(NAME, "Task IDLE0 @ CPU0 subscribed to WDT");
    }
    #ifndef CONFIG_FREERTOS_UNICORE
    TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCPU(1);
    if (idle1 && esp_task_wdt_delete(idle1) == ESP_OK) {
        ESP_LOGD(NAME, "Task IDLE1 @ CPU1 unsubscribed from WDT");
    }
    #endif // CONFIG_FREERTOS_UNICORE
#endif // CONFIG_AUTOSTART_ARDUINO & CONFIG_TASK_WDT
}

void init() {
    uart_initialize();    ESP_LOGI(TAG, "Initializing UART Serial Connection");
    config_initialize();  ESP_LOGI(TAG, "Initializing Configuration");
    twdt_initialize();    ESP_LOGI(TAG, "Initializing Task Watchdog Timer");
    gpio_initialize();    ESP_LOGI(TAG, "Initializing GPIO functions");
    wifi_initialize();    ESP_LOGI(TAG, "Initializing WiFi Connection");
    server_initialize();  ESP_LOGI(TAG, "Initializing WebServer with HTTP/WS");
    console_initialize(); ESP_LOGI(TAG, "Initializing Command Line Console");
    fflush(stdout);
}

void initVariant() {  // testing features. maybe removed in the future
    max6675_init();
}

void setup() {}

void loop() {
    LIGHTBLK(100);
    /* max6675_read(); */
    delay(1000);
    console_handle_one();
}

#ifndef CONFIG_AUTOSTART_ARDUINO
void loopTask(void *pvParameters) {
    while (true) {
#ifdef CONFIG_TASK_WDT
        esp_task_wdt_reset();
#endif
        loop();
    }
}

void app_main() {
    init(); initVariant(); setup();
#ifdef CONFIG_FREERTOS_UNICORE
    xTaskCreate(loopTask, "loopTask", 8192, NULL, 1, NULL);
#else
    xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, 0);
#endif // CONFIG_FREERTOS_UNICORE
}
#endif // CONFIG_AUTOSTART_ARDUINO
