/*
 * File: config.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 21:46:59
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

struct config_webserver_t {
    const char * USER_WS;
    const char * PASS_WS;
    const char * USER_WEB;
    const char * PASS_WEB;
    const char * VIEW_EDIT;
    const char * VIEW_FILE;
    const char * DIR_STATIC;
    const char * DIR_UPLOAD;
};

struct config_wifi_t {
    const char * AP_SSID;
    const char * AP_PASS;
          bool   STA_EN;
    const char * STA_SSID;
    const char * STA_PASS;
}

struct config_application_t {
          bool   DNS_EN;
    const char * DNS_HOST;
          bool   OTA_EN;
    const char * OTA_URL;
    const char * PROMPT;
};

struct config_information_t {
    const char * NAME;
    const char * UID;
    const char * VERSION;
};

typedef struct config_t config_t;

struct config_t {
    struct config_webserver_t   web;
    struct config_wifi_t        wifi;
    struct config_application_t app;
    const struct config_information_t info;
};

extern config_t Config;

extern const char * UUID;

bool config_init();
bool config_load(config_t *cfg = &Config);
bool config_save(config_t *cfg = &Config);

#endif // _CONFIG_H_
