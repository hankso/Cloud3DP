/*
 * File: config.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 21:46:59
 *
 * See more in comment strings
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "esp_err.h"

typedef struct config_webserver_t {
    const char * WS_NAME;   // Username to access websocket connection
    const char * WS_PASS;   // Password. Leave it empty("") to disable
    const char * HTTP_NAME; // Username to access webpage (HTML/JS/CSS)
    const char * HTTP_PASS; // Password. Leave it empty("") to disable
    const char * VIEW_EDIT; // Template filename for Online Editor
    const char * VIEW_FILE; // Template filename for Files Manager
    const char * VIEW_OTA;  // Template filename for OTA Updation
    const char * DIR_ASSET; // Path to public CSS/JS files
    const char * DIR_STA;   // Path to static files hosted on STA interface
    const char * DIR_AP;    // Path to static files hosted on AP interface
    const char * DIR_ROOT;  // Path to static files like sitemap, favicon etc.
    const char * DIR_DATA;  // Directory to storage data (gcode etc)
} config_web_t;

typedef struct config_network_t {
    const char * AP_NAME;   // Hotspot SSID
    const char * AP_PASS;   // Hotspot password
    const char * AP_HOST;   // Hotspot IP address
    const char * AP_HIDE;   // Whether to hide AP SSID
    const char * STA_NAME;  // BSSID of access point to connect after startup
    const char * STA_PASS;  // Password to connect to access point
} config_net_t;

// For easier managing, Boolean values are stored as "0" or "1" (string)
typedef struct config_application_t {
    const char * DNS_RUN;   // Enable mDNS service
    const char * DNS_HOST;  // redirect http://{DNS_HOST} to http://{AP_HOST}
    const char * OTA_RUN;   // Enable auto updation checking
    const char * OTA_URL;   // URL to fetch firmware from
    const char * PROMPT;    // Console promption string
} config_app_t;

// information are readonly values (after initialization)
typedef struct config_information_t {
    const char * NAME;      // Program name (PROJECT_NAME if defined)
    const char * VER;       // Program version (PROJECT_VER if defined)
    const char * UID;       // Unique serial number
} config_info_t;

// Global instance to access configuration
typedef struct config_t {
    config_web_t web;
    config_net_t net;
    config_app_t app;
    config_info_t info;
} config_t;

extern config_t Config;

/* The cfglist is a mapping to flattened Configuration attributes.
 * Low level config_get/config_set are actually manipulation on this array.
 * Therefore, global variable `Config` is just a reference to the cfglist
 * memory.
 */
typedef struct config_entry_t {
    const char *key;
    const char * *value;
} config_entry_t;

extern config_entry_t cfglist[];

bool config_initialize();
bool config_load();                     // load from nvs flash to Config
bool config_dump();                     // save Config to nvs flash
bool config_loads(const char *);        // load Config from json
char * config_dumps();                  // dump Config into json

/* Get one config value by key or set one by key & value. When you update an
 * entry, the result is applied on both Config and NVS flash. You don't need
 * to call config_load/config_dump to sync between.
 */
const char * config_get(const char *);
bool config_set(const char *, const char *);

/* NVS helper functions.
 * These are similar as Arduino-ESP32 library `Preference`.
 * But more lightweight and without dependency on Arduino.
 */
esp_err_t config_nvs_init();
esp_err_t config_nvs_open(const char *, bool ro = false); // open namespace
esp_err_t config_nvs_commit();          // must be called after config_nvs_open
esp_err_t config_nvs_close();           // close with auto commit
bool config_nvs_remove(const char *);   // remove one entry
bool config_nvs_clear();                // remove all entries
void config_nvs_stats();                // get nvs flash detail
void config_nvs_list();                 // list all entries

#endif // _CONFIG_H_
