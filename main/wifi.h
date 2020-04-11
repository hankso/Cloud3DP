/* 
 * File: wifi.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-21 13:33:32
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
