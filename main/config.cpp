/* 
 * File: config.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 21:49:38
 *
 */

#include "config.h"
#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#endif

static const char * TAG = "Config";

// default values
static config_web_t WEB = {
    .WS_NAME   = "",
    .WS_PASS   = "",
    .HTTP_NAME = "temp",
    .HTTP_PASS = "temp",
    .VIEW_EDIT = "/ap/editor.html",
    .VIEW_FILE = "/ap/files.html",
    .DIR_SRC   = "/src/",
    .DIR_STA   = "/sta/",
    .DIR_AP    = "/ap/",
    .DIR_DATA  = "/data/",
};
static config_net_t NET = {
    .AP_NAME   = "TempCtrlAP",
    .AP_PASS   = "12345678",
    .AP_HOST   = "10.0.0.1",
    .AP_HIDE   = "",
    .STA_NAME  = "",
    .STA_PASS  = "",
};
static config_app_t APP = {
    .DNS_RUN   = "y",
    .DNS_HOST  = "temp.esp32.local",
    .OTA_RUN   = "",
    .OTA_URL   = "",
    .PROMPT    = "temp> ",
};

config_t Config = {
    .web = WEB, .net = NET, .app = APP,
    .info = {
#ifdef PROJECT_NAME
        .NAME  = PROJECT_NAME,
#else
        .NAME  = "",
#endif
        .VER   = "",
        .UID   = "",
    }
};

// Sometimes we point a const char pointer to char * (e.g. new json comming in),
// but const char * cannot be deallocated. If the pointer in staticlist, then 
// it's a TRUE const char pointer, or it's actually char * (free it!)
static const char * staticlist[] = {
    WEB.WS_NAME,   WEB.WS_PASS,
    WEB.HTTP_NAME, WEB.HTTP_PASS, WEB.VIEW_EDIT, WEB.VIEW_FILE,
    WEB.DIR_SRC,   WEB.DIR_STA,   WEB.DIR_AP,    WEB.DIR_DATA,
    NET.AP_NAME,   NET.AP_PASS,   NET.AP_HOST,   NET.AP_HIDE,
    NET.STA_NAME,  NET.STA_PASS,
    APP.DNS_RUN,   APP.DNS_HOST,  APP.OTA_RUN,   APP.OTA_URL,   APP.PROMPT,
};

// key values stored in NVS partition & JSON
static const char
    *WEB_WS_NAME   = "web.ws.name",
    *WEB_WS_PASS   = "web.ws.pass",
    *WEB_HTTP_NAME = "web.http.name",
    *WEB_HTTP_PASS = "web.http.pass",
    *WEB_VIEW_EDIT = "web.view.editor",
    *WEB_VIEW_FILE = "web.view.manage",
    *WEB_DIR_SRC   = "web.path.src",
    *WEB_DIR_STA   = "web.path.sta",
    *WEB_DIR_AP    = "web.path.ap",
    *WEB_DIR_DATA  = "web.path.upload",

    *NET_AP_NAME   = "net.ap.ssid",
    *NET_AP_PASS   = "net.ap.pass",
    *NET_AP_HOST   = "net.ap.host",
    *NET_AP_HIDE   = "net.ap.hide",
    *NET_STA_NAME  = "net.sta.ssid",
    *NET_STA_PASS  = "net.sta.pass",

    *APP_DNS_RUN   = "app.dns.enable",
    *APP_DNS_HOST  = "app.dns.host",
    *APP_OTA_RUN   = "app.ota.enable",
    *APP_OTA_URL   = "app.ota.url",
    *APP_PROMPT    = "app.prompt";

config_entry_t cfglist[] = {
    {WEB_WS_NAME,   &WEB.WS_NAME},   {WEB_WS_PASS,   &WEB.WS_PASS},
    {WEB_HTTP_NAME, &WEB.HTTP_NAME}, {WEB_HTTP_PASS, &WEB.HTTP_PASS},
    {WEB_VIEW_EDIT, &WEB.VIEW_EDIT}, {WEB_VIEW_FILE, &WEB.VIEW_FILE},
    {WEB_DIR_SRC,   &WEB.DIR_SRC},   {WEB_DIR_STA,   &WEB.DIR_STA},
    {WEB_DIR_AP,    &WEB.DIR_AP},    {WEB_DIR_DATA,  &WEB.DIR_DATA},

    {NET_AP_NAME,   &NET.AP_NAME},   {NET_AP_PASS,   &NET.AP_PASS},
    {NET_AP_HOST,   &NET.AP_HOST},   {NET_AP_HIDE,   &NET.AP_HIDE},
    {NET_STA_NAME,  &NET.STA_NAME},  {NET_STA_PASS,  &NET.STA_PASS},

    {APP_DNS_RUN,   &APP.DNS_RUN},   {APP_DNS_HOST,  &APP.DNS_HOST},
    {APP_OTA_RUN,   &APP.OTA_RUN},   {APP_OTA_URL,   &APP.OTA_URL},
    {APP_PROMPT,    &APP.PROMPT},
};

static uint16_t numcfg = sizeof(cfglist) / sizeof(config_entry_t);

/******************************************************************************
 * Configuration I/O
 */

// nvs helper functions
bool _nvs_set_str(const char *, const char *, bool commit = true);
bool _nvs_get_str(const char *, char *, size_t);
bool _nvs_load_str(const char *, const char **);

int16_t config_index(const char *key) {
    for (int16_t idx = 0; idx < numcfg; idx++) {
        if (!strcmp(key, cfglist[idx].key)) return idx;
    }
    return -1;
}

const char * config_get(const char *key) {
    int16_t idx = config_index(key);
    return idx == -1 ? "Unknown" : *cfglist[idx].value;
}

bool config_set(const char *key, const char *value) {
    int16_t idx = config_index(key);
    if (idx == -1) return false;
    if (*cfglist[idx].value != staticlist[idx]) {
        free((void *)*cfglist[idx].value);
    }
    *cfglist[idx].value = value;
    _nvs_set_str(key, value);
    return true;
}

bool config_load() {
    if (config_nvs_open("config")) return false;
    for (uint16_t i = 0; i < numcfg; i++) {
        _nvs_load_str(cfglist[i].key, cfglist[i].value);
    }
    return config_nvs_close() == ESP_OK;
}

bool config_dump() {
    if (config_nvs_open("config")) return false;
    bool s = true;
    for (uint16_t i = 0; i < numcfg; i++) {
        s = s && _nvs_set_str(cfglist[i].key, *cfglist[i].value, false);
    }
    if (!s) ESP_LOGE(TAG, "config save fail");
    esp_err_t err = config_nvs_close();
    return s && (err == ESP_OK);
}

// save value to nvs flash
void _set_config_callback(const char *key, cJSON *item) {
    if (!cJSON_IsString(item)) {
        if (!cJSON_IsObject(item)) {
            ESP_LOGW(TAG, "Invalid type of `%s`", cJSON_Print(item));
        }
        return;
    }
    if (config_set(key, item->valuestring)) {
        // make reference on the string so that cJSON_Delete won't free it
        cJSON_CreateStringReference(item->valuestring);
        ESP_LOGD(TAG, "Get setting `%s`: `%s`", key, item->valuestring);
    } else ESP_LOGW(TAG, "Invalid configuration key: `%s`", key);
}

void json_parse_object_recurse(
    cJSON *item, void (*cb)(const char *, cJSON *), const char *prefix = "")
{
    while (item) {
        if (item->string == NULL) {  // may be the root object
            if (item->child) json_parse_object_recurse(item->child, cb);
            item = item->next;
            continue;
        }

        // resolve key string with parent's name
        uint8_t plen = strlen(prefix);
        uint8_t slen = strlen(item->string) + (plen ? plen + 1 : 0) + 1;
        char *key = (char *)malloc(slen);
        if (key == NULL) return;
        if (plen) snprintf(key, slen, "%s.%s", prefix, item->string);
        else      snprintf(key, slen, item->string);

        (*cb)(key, item);

        // recurse to child and iterate to next sibling
        if (item->child) json_parse_object_recurse(item->child, cb, key);
        item = item->next;
        free(key);
    }
}

bool config_loads(const char *json) {
    cJSON *obj = cJSON_Parse(json);
    if (obj) {
        if (config_nvs_open("config")) return false;
        json_parse_object_recurse(obj, &_set_config_callback);
        config_nvs_close();
        cJSON_Delete(obj);
        return true;
    } else {
        ESP_LOGE(TAG, "Cannot parse JSON: %s", cJSON_GetErrorPtr());
        // ESP_LOGD(TAG, json);
        return false;
    }
}

bool config_dumps(char *buffer) {
    cJSON *obj = cJSON_CreateObject();
    for (uint16_t i = 0; i < numcfg; i++) {
        cJSON_AddStringToObject(obj, cfglist[i].key, *cfglist[i].value);
    }
    buffer = cJSON_Print(obj);
    cJSON_Delete(obj);
    return buffer != NULL;
}

/******************************************************************************
 * Configuration utilities
 */

static struct {
    bool init;                      // whether nvs flash has been initialized
    esp_err_t result;               // nvs flash init result
    nvs_handle handle;              // nvs handle obtained from nvs_open
    const esp_partition_t *part;    // nvs flash partition
} nvs_st = {
    .init = false,
    .result = ESP_OK,
    .handle = 0,
    .part = NULL
};

bool config_initialize() {
    config_nvs_init();
    esp_log_level_set(TAG, ESP_LOG_WARN);

    // load readonly values
    if (config_nvs_open("data", true) == ESP_OK) {
        _nvs_load_str("name",  &Config.info.NAME);
        _nvs_load_str("ver",  &Config.info.VER);
        _nvs_load_str("uid",  &Config.info.UID);
        config_nvs_close();
    }

    // startup times counter test
    if (config_nvs_open("data", false) == ESP_OK) {
        uint32_t counter = 0;
        esp_err_t err = nvs_get_u32(nvs_st.handle, "counter", &counter);
        if (err) {
            ESP_LOGE(TAG, "get u32 `counter` fail: %s", esp_err_to_name(err));
        }
        printf("Current run times: %u\n", ++counter);
        err = nvs_set_u32(nvs_st.handle, "counter", counter);
        if (err) {
            ESP_LOGE(TAG, "set u32 `counter` fail: %s", esp_err_to_name(err));
        }
        config_nvs_close();
    }
    
    return config_load();
}

esp_err_t config_nvs_init() {
    if (nvs_st.init) return nvs_st.result;
    nvs_st.part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    if (nvs_st.part == NULL) {
        ESP_LOGE(TAG, "Cannot found nvs partition. Skip");
        nvs_st.init = true;
        nvs_st.result = ESP_ERR_NOT_FOUND;
        return nvs_st.result;
    }
#ifdef CONFIG_AUTOSTART_ARDUINO
    nvs_flash_deinit();
#endif
    esp_err_t err;
    bool enc = false;
#ifdef CONFIG_NVS_ENCRYPT
    enc = true;
    const esp_partition_t *keys = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS_KEY, NULL);
    if (keys != NULL) {
        nvs_sec_cfg_t *cfg;
        err = nvs_flash_read_security_cfg(keys, cfg);
        if (err == ESP_ERR_NVS_KEYS_NOT_INITIALIZED) {
            err = nvs_flash_generate_keys(keys, cfg);
        }
        if (!err) {
            err = nvs_flash_secure_init_partition(nvs_st.part->label, cfg);
        } else {
            ESP_LOGE(TAG, "Cannot initialize nvs with encryption: %s",
                     esp_err_to_name(err));
            enc = false;
        }
    } else { enc = false; }
#endif
    if (!enc) err = nvs_flash_init_partition(nvs_st.part->label);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        err = nvs_flash_erase_partition(nvs_st.part->label);
        if (!err) { // partition cleared. we try again
#ifdef CONFIG_NVS_ENCRYPT
            if (enc) {
                err = nvs_flash_secure_init_partition(nvs_st.part->label, cfg);
            } else
#endif
            {
                err = nvs_flash_init_partition(nvs_st.part->label);
            }
        }
    }
    if (err) {
        ESP_LOGE(TAG, "Cannot initialize nvs flash: %s", esp_err_to_name(err));
    }
    nvs_st.init = true;
    nvs_st.result = err;
    return err;
}

esp_err_t config_nvs_open(const char *ns, bool ro) {
    if (nvs_st.handle) return ESP_FAIL;
    esp_err_t err = ESP_OK;
    if (!nvs_st.init) err = config_nvs_init();
    if (!err) {
        err = nvs_open(ns, ro ? NVS_READONLY : NVS_READWRITE, &nvs_st.handle);
    }
    if (err) {
        ESP_LOGE(TAG, "Cannot open nvs namespace `%s:` %s",
                 ns, esp_err_to_name(err));
    }
    return err;
}

esp_err_t config_nvs_commit() {
    if (!nvs_st.handle) return ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t err = nvs_commit(nvs_st.handle);
    if (err) ESP_LOGE(TAG, "commit fail: %s", esp_err_to_name(err));
    return err;
}

esp_err_t config_nvs_close() {
    if (!nvs_st.handle) return ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t err = config_nvs_commit();
    nvs_close(nvs_st.handle); nvs_st.handle = 0;
    return err;
}

bool config_nvs_remove(const char *key) {
    if (config_nvs_open("config")) return false;
    esp_err_t err = nvs_erase_key(nvs_st.handle, key);
    if (err) ESP_LOGE(TAG, "erase `%s` fail: %s", key, esp_err_to_name(err));
    return config_nvs_close() == ESP_OK;
}

bool config_nvs_clear() {
    if (config_nvs_open("config")) return false;
    esp_err_t err = nvs_erase_all(nvs_st.handle);
    if (err) ESP_LOGE(TAG, "erase nvs data fail: %s", esp_err_to_name(err));
    return config_nvs_close() == ESP_OK;
}

#ifdef ESP_IDF_VERSION
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#define _NVS_ITER_LIST
#endif
#endif
void config_nvs_list() {
    if (nvs_st.part == NULL) {
        ESP_LOGE(TAG, "Cannot found nvs partition. Skip");
        return;
    }
#ifdef _NVS_ITER_LIST
    nvs_iterator_t iter;
    nvs_entry_info_t info;
    iter = nvs_entry_find(nvs_st.part->label, "config", NVS_TYPE_ANY);
    if (iter == NULL) {
        ESP_LOGE(TAG, "Cannot find entries under `config` namespace in `%s`",
                 nvs_st.part->label);
        return;
    }
    printf("Namespace: config\nKEY\t\t\tVALUE\n");
    while (iter != NULL) {
        nvs_entry_info(iter, &info);
        iter = nvs_entry_next(iter);
        const char *key = info.key, *value = config_get(key);
        printf(" %-15.15s\t", key);
        if (!strcmp(key + strlen(key) - 4, "pass")) {
            printf("`%.*s`\n", strlen(value), "********");
        } else {
            printf("`%s`\n", value);
        }
    }
    nvs_release_iterator(iter);
#else
    if (config_nvs_open("config", true)) return;
    else config_nvs_close();
    printf("Namespace: config\n KEY\t\t\tVALUE\n");
    for (uint16_t i = 0; i < numcfg; i++) {
        const char *key = cfglist[i].key, *value = *cfglist[i].value;
        printf(" %-15.15s\t", key);
        if (!strcmp(key + strlen(key) - 4, "pass")) {
            printf("`%.*s`\n", strlen(value), "********");
        } else {
            printf("`%s`\n", value);
        }
    }
#endif
}

void config_nvs_stats() {
    if (nvs_st.part == NULL) {
        ESP_LOGE(TAG, "Cannot found nvs partition. Skip");
        return;
    }
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(nvs_st.part->label, &nvs_stats);
    if (err) {
        ESP_LOGE(TAG, "Cannot get nvs data status: %s", esp_err_to_name(err));
        return;
    }
    printf(
        "NVS Partition Status:\n"
        " Namespaces: %d\n"
        " Used: %d entries\n"
        " Free: %d entries\n"
        " Total: %d entries\n",
        nvs_stats.namespace_count, nvs_stats.used_entries,
        nvs_stats.free_entries, nvs_stats.total_entries
    );
}

bool _nvs_load_str(const char *key, const char * *ptr) {
    size_t len = 0;
    esp_err_t err = nvs_get_str(nvs_st.handle, key, NULL, &len);
    if (err) {
        ESP_LOGE(TAG, "get `%s` length fail: %s", key, esp_err_to_name(err));
        return false;
    }
    char *tmp = (char *)malloc(len);
    if (tmp == NULL) {
        ESP_LOGE(TAG, "not enough space for str: `%s`", key);
        return false;
    }
    err = nvs_get_str(nvs_st.handle, key, tmp, &len);
    if (err) {
        ESP_LOGE(TAG, "get str `%s` fail: %s", key, esp_err_to_name(err));
        free(tmp);
    } else {
        int16_t idx = config_index(key);
        if (idx != -1 && *ptr != staticlist[idx]) {
            free(cast_away_const_force(*ptr));
        }
        *ptr = tmp;
    }
    return err == ESP_OK;
}

bool _nvs_get_str(const char *key, char *val, size_t maxlen) {
    size_t len = 0;
    esp_err_t err = nvs_get_str(nvs_st.handle, key, NULL, &len);
    if (err) {
        ESP_LOGE(TAG, "get `%s` length fail: %s", key, esp_err_to_name(err));
        return false;
    }
    if (len > maxlen) {
        char *tmp = (char *)realloc(val, len);
        if (tmp == NULL) {
            ESP_LOGE(TAG, "not enough space in value: %u < %u", maxlen, len);
            return false;
        } else val = tmp;
    }
    err = nvs_get_str(nvs_st.handle, key, val, &len);
    if (err) {
        ESP_LOGE(TAG, "get str `%s` fail: %s", key, esp_err_to_name(err));
    }
    return err == ESP_OK;
}

bool _nvs_set_str(const char *key, const char *val, bool commit) {
    esp_err_t err = nvs_set_str(nvs_st.handle, key, val);
    if (err) {
        ESP_LOGE(TAG, "set str `%s` to `%s` fail: %s",
                 key, val, esp_err_to_name(err));
    } else if (commit) {
        err = config_nvs_commit();
        if (!err) ESP_LOGD(TAG, "set str `%s` to `%s`", key, val);
    }
    return err == ESP_OK;
}

/*
bool _nvs_get_u8(const char *key, uint8_t *val) {
    esp_err_t err = nvs_get_u8(nvs_st.handle, key, val);
    if (err) {
        ESP_LOGE(TAG, "get u8 `%s` fail: %s", key, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool _nvs_set_u8(const char *key, uint8_t val, bool commit = true) {
    esp_err_t err = nvs_set_u8(nvs_st.handle, key, val);
    if (err) {
        ESP_LOGE(TAG, "set u8 `%s` to `%d` fail: %s",
                 key, val, esp_err_to_name(err));
    } else if (commit) {
        err = config_nvs_commit();
        if (!err) ESP_LOGD(TAG, "set u8 `%s` to `%d`", key, val);
    }
    return err == ESP_OK;
}
*/
