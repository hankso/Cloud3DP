/* 
 * File: wifi.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-21 13:33:54
 */

#include <WiFi.h>

#include "wifi.h"
#include "globals.h"
#include "config.h"
#include "drivers.h"

#include "esp_wifi.h"
#include "tcpip_adapter.h"

static const char *TAG = "wifi";

static wifi_config_t conf;

void wifi_initialize() {
    // WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
}

void wifi_loop_begin() {
    if (strlen(Config.net.STA_NAME)) {
        WiFi.begin(Config.net.STA_NAME, Config.net.STA_PASS);
        while (WiFi.status() != WL_CONNECTED) {
            delay(250); printf("."); led_blink(0, 125, 1);
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
        led_blink(0, 50, 5);
        printf("You can connect to WiFi hotspot: `%s`\n", conf.ap.ssid);
        tcpip_adapter_ip_info_t ip;
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip);
        const char *addr = ip4addr_ntoa(&ip.ip);
        printf("Visit http://%s/ap/index.html for homepage\n", addr);
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

    uint8_t maxlen = sizeof(conf.ap.ssid);
    if (strlen(Config.info.UID)) {
        snprintf((char *)conf.ap.ssid, maxlen, "%s-%s", ssid, Config.info.UID);
    } else {
        strncpy((char *)conf.ap.ssid, ssid, maxlen - 1);
        conf.ap.ssid[maxlen - 1] = '\0';
    }
    conf.ap.ssid_len = strlen((char *)conf.ap.ssid);
    conf.ap.channel = 1;
    if (WiFi.status() == WL_CONNECTED && strbool(Config.net.AP_HIDE)) {
        conf.ap.ssid_hidden = 1;
    } else {
        conf.ap.ssid_hidden = 0;
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
    }
    return true;
}

bool wifi_close_ap() {
    *conf.ap.ssid = 0;
    *conf.ap.password = 0;
    conf.ap.authmode = WIFI_AUTH_OPEN;
    if (esp_wifi_set_config(WIFI_IF_AP, &conf)) {
        ESP_LOGE(TAG, "Cannot close soft AP");
        return false;
    }
    return true;
}

void wifi_ap_list() {

}

void wifi_sta_list() {
    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t tcpip_sta_list;
    esp_err_t err = esp_wifi_ap_get_sta_list(&wifi_sta_list);
    if (!err) err = tcpip_adapter_get_sta_list(&wifi_sta_list, &tcpip_sta_list);
    if (err) {
        printf("Cannot get sta list: %s\n", esp_err_to_name(err));
        return;
    } else if (!tcpip_sta_list.num) {
        printf("No connected stations\n");
        return;
    }
    printf("IP address\tMAC address\t  RSSI Mode\n");
    for (uint8_t i = 0; i < tcpip_sta_list.num; i++) {
        tcpip_adapter_sta_info_t sw = tcpip_sta_list.sta[i];
        wifi_sta_info_t hw = wifi_sta_list.sta[i];
        printf("%-16s%17s %4d %c%c%c%c\n",
               ip4addr_ntoa(&sw.ip),
               format_mac(hw.mac, 17), hw.rssi,
               hw.phy_11b ? 'b' : ' ',
               hw.phy_11g ? 'g' : ' ',
               hw.phy_11n ? 'n' : ' ',
               hw.phy_lr ? 'L' : 'H');
    }
}
