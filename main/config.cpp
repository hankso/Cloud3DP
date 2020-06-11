/* 
 * File: config.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 21:49:38
 *
 */

#include "config.h"
#include "globals.h"

#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"

#if __has_include("esp_idf_version.h")
    #include "esp_idf_version.h"
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    #define _NVS_ITER_LIST
    #endif
#endif

#define NAMESPACE_INFO "info"
#define NAMESPACE_CFG "config"

static const char * TAG = "Config";

// default values
config_t Config = {
    .web = {
        .WS_NAME   = "",
        .WS_PASS   = "",
        .HTTP_NAME = "",
        .HTTP_PASS = "",
        .VIEW_EDIT = "/ap/editor.html",
        .VIEW_FILE = "/ap/fileman.html",
        .VIEW_OTA  = "/ap/update.html",
        .DIR_ASSET = "/src/",
        .DIR_STA   = "/sta/",
        .DIR_AP    = "/ap/",
        .DIR_ROOT  = "/root/",
        .DIR_DATA  = "/data/",
    },
    .net = {
        .AP_NAME   = "Cloud3DP",
        .AP_PASS   = "12345678",
        .AP_HOST   = "10.0.0.1",
        .AP_HIDE   = "0",
        .STA_NAME  = "",
        .STA_PASS  = "",
    },
    .app = {
        .DNS_RUN   = "",
        .DNS_HOST  = "",
        .OTA_RUN   = "",
        .OTA_URL   = "",
        .PROMPT    = "c3dp> ",
    },
    .info = {
#ifdef PROJECT_NAME
        .NAME  = PROJECT_NAME,
#else
        .NAME  = "",
#endif
#ifdef PROJECT_VER
        .VER   = PROJECT_VER,
#else
        .VER   = "",
#endif
#ifdef CHIP_UID
        .UID   = CHIP_UID,
#else
        .UID   = "",
#endif
    }
};

// Sometimes we point a const char pointer to char * (e.g. new json comming in),
// but const char * cannot be deallocated. If the pointer in staticlist, then
// it's a TRUE const char pointer, or it's actually char * (free it!)
/*
static const char * staticlist[] = {
    Config.web.WS_NAME,   Config.web.WS_PASS,
    Config.web.HTTP_NAME, Config.web.HTTP_PASS,
    Config.web.VIEW_EDIT, Config.web.VIEW_FILE, Config.web.VIEW_OTA
    Config.web.DIR_ASSET, Config.web.DIR_STA,   Config.web.DIR_AP,
    Config.web.DIR_ROOT,  Config.web.DIR_DATA,

    Config.net.AP_NAME,   Config.net.AP_PASS,
    Config.net.AP_HOST,   Config.net.AP_HIDE,
    Config.net.STA_NAME,  Config.net.STA_PASS,

    Config.app.DNS_RUN,   Config.app.DNS_HOST,
    Config.app.OTA_RUN,   Config.app.OTA_URL,
    Config.app.PROMPT,
};
*/

config_entry_t cfglist[] = {
    {"web.ws.name",     &Config.web.WS_NAME },
    {"web.ws.pass",     &Config.web.WS_PASS},
    {"web.http.name",   &Config.web.HTTP_NAME},
    {"web.http.pass",   &Config.web.HTTP_PASS},
    {"web.view.editor", &Config.web.VIEW_EDIT},
    {"web.view.manage", &Config.web.VIEW_FILE},
    {"web.view.update", &Config.web.VIEW_OTA},
    {"web.path.assets", &Config.web.DIR_ASSET},
    {"web.path.sta",    &Config.web.DIR_STA},
    {"web.path.ap",     &Config.web.DIR_AP},
    {"web.path.static", &Config.web.DIR_ROOT},
    {"web.path.data",   &Config.web.DIR_DATA},

    {"net.ap.ssid",     &Config.net.AP_NAME},
    {"net.ap.pass",     &Config.net.AP_PASS},
    {"net.ap.host",     &Config.net.AP_HOST},
    {"net.ap.hide",     &Config.net.AP_HIDE},
    {"net.sta.ssid",    &Config.net.STA_NAME},
    {"net.sta.pass",    &Config.net.STA_PASS},

    {"app.dns.run",     &Config.app.DNS_RUN},
    {"app.dns.host",    &Config.app.DNS_HOST},
    {"app.ota.run",     &Config.app.OTA_RUN},
    {"app.ota.url",     &Config.app.OTA_URL},
    {"app.cmd.prompt",  &Config.app.PROMPT},
};

static uint16_t numcfg = sizeof(cfglist) / sizeof(config_entry_t);

/******************************************************************************
 * Configuration I/O
 */

// nvs helper functions
bool _nvs_set_str(const char *, const char *, bool);
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
    if (strcmp(*cfglist[idx].value, value)) {
        *cfglist[idx].value = value ? value : "";
    }
    // if (*cfglist[idx].value != staticlist[idx]) {
    //     free((void *)*cfglist[idx].value);
    // }
    return true;
}

bool config_load() {
    if (config_nvs_open(NAMESPACE_CFG, true)) return false;
    for (uint16_t i = 0; i < numcfg; i++) {
        _nvs_load_str(cfglist[i].key, cfglist[i].value);
    }
    return config_nvs_close() == ESP_OK;
}

bool config_dump() {
    if (config_nvs_open(NAMESPACE_CFG)) return false;
    bool s = true;
    for (uint16_t i = 0; i < numcfg; i++) {
        s = s && _nvs_set_str(cfglist[i].key, *cfglist[i].value, false);
    }
    return (config_nvs_close() == ESP_OK) && s;
}

// save value to nvs flash
void _set_config_callback(const char *key, cJSON *item) {
    if (!cJSON_IsString(item)) {
        if (!cJSON_IsObject(item)) {
            ESP_LOGE(TAG, "Invalid type of `%s`", cJSON_Print(item));
        }
        return;
    }
    const char *val = strdup(item->valuestring);
    if (!val) {
        ESP_LOGE(TAG, "No memory for new config value");
    } else if (!config_set(key, val)) {
        ESP_LOGW(TAG, "JSON Config set `%s` to `%s` failed", key, val);
    }
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
        if (key == NULL) continue;
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
        if (config_nvs_open(NAMESPACE_CFG)) return false;
        json_parse_object_recurse(obj, &_set_config_callback);
        config_nvs_close();
        cJSON_Delete(obj);
        return true;
    }
    ESP_LOGE(TAG, "Cannot parse JSON: %s", cJSON_GetErrorPtr());
    return false;
}

char * config_dumps() {
    cJSON *obj = cJSON_CreateObject();
    for (uint16_t i = 0; i < numcfg; i++) {
        cJSON_AddStringToObject(obj, cfglist[i].key, *cfglist[i].value);
    }
    char *string = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return string;
}

/******************************************************************************
 * Configuration utilities
 */

static struct {
    bool init;                      // whether nvs flash has been initialized
    esp_err_t error;                // nvs flash init result
    nvs_handle handle;              // nvs handle obtained from nvs_open
    const esp_partition_t *part;    // nvs flash partition
} nvs_st = { false, ESP_OK, 0, NULL };

bool config_initialize() {
    config_nvs_init();
    esp_log_level_set(TAG, ESP_LOG_WARN);

    // load readonly values
    if (config_nvs_open(NAMESPACE_INFO, true) == ESP_OK) {
        _nvs_load_str("uid",  &Config.info.UID);
        config_nvs_close();
    }

    // startup times counter test
    if (config_nvs_open(NAMESPACE_INFO) == ESP_OK) {
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
    if (nvs_st.init) return nvs_st.error;
    nvs_st.part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    if (nvs_st.part == NULL) {
        ESP_LOGE(TAG, "Cannot found nvs partition. Skip");
        nvs_st.init = true;
        return nvs_st.error = ESP_ERR_NOT_FOUND;
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
    nvs_st.error = err;
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
    if (config_nvs_open(NAMESPACE_CFG)) return false;
    esp_err_t err = nvs_erase_key(nvs_st.handle, key);
    if (err) ESP_LOGE(TAG, "erase `%s` fail: %s", key, esp_err_to_name(err));
    return config_nvs_close() == ESP_OK;
}

bool config_nvs_clear() {
    if (config_nvs_open(NAMESPACE_CFG)) return false;
    esp_err_t err = nvs_erase_all(nvs_st.handle);
    if (err) ESP_LOGE(TAG, "erase nvs data fail: %s", esp_err_to_name(err));
    return config_nvs_close() == ESP_OK;
}

void config_nvs_list() {
    if (nvs_st.part == NULL) {
        ESP_LOGE(TAG, "Cannot found nvs partition. Skip");
        return;
    }
#ifdef _NVS_ITER_LIST
    nvs_iterator_t iter;
    nvs_entry_info_t info;
    iter = nvs_entry_find(nvs_st.part->label, NAMESPACE_CFG, NVS_TYPE_ANY);
    if (iter == NULL) {
        ESP_LOGE(TAG, "Cannot find entries under `" NAMESPACE_CFG "` in `%s`",
                 nvs_st.part->label);
        return;
    }
    printf("Namespace: " NAMESPACE_CFG "\n KEY\t\t\tVALUE\n");
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
    if (config_nvs_open(NAMESPACE_CFG, true)) {
        ESP_LOGE(TAG, "Cannot find entries under `" NAMESPACE_CFG "` in `%s`",
                 nvs_st.part->label);
        return;
    } else config_nvs_close();
    printf("Namespace: " NAMESPACE_CFG "\n KEY\t\t\tVALUE\n");
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
        "NVS Partition Size: %s\n"
        "  Namespaces: %d\n"
        "  Used: %d entries\n"
        "  Free: %d entries\n"
        "  Total: %d entries\n",
        format_size(nvs_st.part->size),
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
    } else if (!strcmp(*ptr, tmp)) {
        free(tmp);
    } else {
        // int16_t idx = config_index(key);
        // if (idx != -1 && staticlist[idx] != *ptr) {
        //     free(cast_away_const_force(*ptr));
        // }
        *ptr = tmp ? tmp : "";
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
