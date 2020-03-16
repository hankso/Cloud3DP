/*************************************************************************
File: ${ESP_BASE}/projects/Temprature/main/Temp.ino
Author: Hankso
Time: Tue 21 May 2019 09:46:44 CST
************************************************************************/

#include <Arduino.h>
#include <WiFi.h>

#include "globals.h"
#include "config.h"
#include "console.h"
#include "server.h"
#include "max6675.h"

void wifi_initialize() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(
        IPAddress(10, 0, 1, 1),
        IPAddress(10, 0, 1, 1),
        IPAddress(255, 255, 255, 0)
    ); delay(100);
    const char SSID[strlen(Config.wifi.AP_SSID) + strlen(Config.info.UID) + 2];
    sprintf(SSID, "%s-%s", Config.wifi.AP_SSID, Config.info.UID);
    WiFi.softAP(SSID, Config.wifi.AP_PASS);
    String addr = WiFi.softAPIP().toString();
    printf("You can connect to WiFi hotspot %s\n", SSID);
    printf("Visit http://%s for homepage.\n", addr.c_str());
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
    printf("Initializing Configuration");
    config_initialize();
    printf("Initializing WiFi Connection");
    wifi_initialize();
    printf("Initializing Task Watchdog Timer");
    twdt_initialize();
    printf("Initializing WebServer with HTTP/WS");
    server_initialize();
}

void initVariant() {  // testing features. maybe removed in the future
    max6675_init();
}

void setup() {
    pinMode(PIN_LED, OUTPUT);
    Serial.begin(115200);
    Serial.println("Done");
}

void loop() {
    LIGHTBLK(100);
    max6675_read();
    delay(1000);
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
