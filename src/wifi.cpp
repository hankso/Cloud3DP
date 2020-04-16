/* 
 * File: wifi.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-21 13:33:54
 */

#include "wifi.h"
#include "gpio.h"
#include "config.h"
#include "globals.h"

#include "esp_wifi.h"

#include <WiFi.h>

static const char *TAG = "wifi";

void wifi_initialize() {
    // WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
}

void wifi_loop_begin() {
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
    // TODO: embed softAPConfig into wifi_start_ap and set gateway
    WiFi.softAPConfig(addr, addr, IPAddress(255, 255, 255, 0)); delay(100);
    if (wifi_start_ap()) {
        LIGHTBLK(10, 10);
    }
}

bool wifi_connect() {
    return false;
}

bool wifi_disconnect() {
    return false;
}

bool wifi_start_ap() {
    const char *ssid = Config.net.AP_NAME, *pass = Config.net.AP_PASS;
    if (!strlen(ssid)) {
        ESP_LOGE(TAG, "No soft AP SSID configured. Skip");
        return false;
    } else if ((strlen(pass) && strlen(pass) < 8) || strlen(pass) > 63) {
        ESP_LOGE(TAG, "Invalid AP password `%s`. Skip", pass);
        return false;
    }
    esp_wifi_start();
    wifi_config_t conf;

    uint8_t maxlen = sizeof(conf.ap.ssid);
    if (strlen(Config.info.UID)) {
        snprintf((char *)conf.ap.ssid, maxlen, "%s-%s", ssid, Config.info.UID);
    } else {
        strncpy((char *)conf.ap.ssid, ssid, maxlen - 1);
        conf.ap.ssid[maxlen - 1] = '\0';
    }
    conf.ap.ssid_len = strlen((char *)conf.ap.ssid);
    conf.ap.channel = 1;
    conf.ap.ssid_hidden = 0;
    if (WiFi.status() == WL_CONNECTED && !strcmp(Config.net.AP_HIDE, "y")) {
        conf.ap.ssid_hidden = 1;
    }
    conf.ap.max_connection = 4;
    conf.ap.beacon_interval = 100;
    if (strlen(pass)) {
        conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char *)conf.ap.password, pass, sizeof(conf.ap.password));
    } else {
        conf.ap.authmode = WIFI_AUTH_OPEN;
        *conf.ap.password = 0;
    }

    if (esp_wifi_set_config(WIFI_IF_AP, &conf)) {
        ESP_LOGE(TAG, "Cannot set configuration of AP");
        return false;
    } else {
        printf("You can connect to WiFi hotspot: `%s`\n", conf.ap.ssid);
        tcpip_adapter_ip_info_t ip;
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip);
        printf("Visit http://%d.%d.%d.%d/ap/index.html for homepage\n",
               (ip.ip.addr & 0xff),
               (ip.ip.addr >> 8) & 0xff,
               (ip.ip.addr >> 16) & 0xff,
               (ip.ip.addr >> 24));
        return true;
    }
}

bool wifi_close_ap() {
    wifi_config_t conf;
    *conf.ap.ssid = 0;
    *conf.ap.password = 0;
    conf.ap.authmode = WIFI_AUTH_OPEN;
    if (esp_wifi_set_config(WIFI_IF_AP, &conf)) {
        ESP_LOGE(TAG, "Cannot close soft AP");
        return false;
    }
    return true;
}

void wifi_sta_list() {

}

void wifi_ap_list() {
    wifi_sta_list_t clients;
    esp_err_t err = esp_wifi_ap_get_sta_list(&clients);
    if (err) {
        ESP_LOGE(TAG, "Cannot get sta list: %s", esp_err_to_name(err));
        return;
    } else if (!clients.num) {
        ESP_LOGE(TAG, "No connected stations");
        return;
    }
    printf("MAC address\t RSSI Mode\n");
    wifi_sta_info_t sta;
    for (uint8_t i = 0; i < clients.num; i++) {
        sta = clients.sta[i];
        printf("%17s %3d  %c%c%c\n",
               format_mac(sta.mac), sta.rssi,
               sta.phy_11b ? 'b' : ' ',
               sta.phy_11g ? 'g' : ' ',
               sta.phy_11n ? 'n' : ' ');
    }
}
