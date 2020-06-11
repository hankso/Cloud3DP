/* 
 * File: wifi.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-21 13:33:32
 *
 * WiFi drivers and STA/AP helper functions occupy about 217KB in firmware.
 */

/* After startup, esp32 will first try to connect to an access point. On
 * connection fail (no available access point or password mismatch), esp32
 * will turn into STA_AP mode and establish a hotspot with AP_NAME & AP_PASS.
 * One can connect to this hotspot and visit http://{AP_HOST}/ap/index.html
 * to list all access points, select one and connect to it by setting config
 * value STA_NAME & STA_PASS.
 */

#ifndef _WIFI_H_
#define _WIFI_H_

void wifi_initialize();
void wifi_loop_begin();

bool wifi_connect(const char *ssid, const char *pass);
bool wifi_disconnect();

bool wifi_start_ap();
bool wifi_close_ap();

void wifi_sta_list();
void wifi_ap_list();

#endif // _WIFI_H_
