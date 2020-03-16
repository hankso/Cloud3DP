/* 
 * File: config.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 21:49:38
 */

#include "config.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"

static const char * TAG = "Config";

config_t Config;  // default initialized with 0 & nullpointer

void config_reset_default(config_t *cfg) {
    cfg->web.USER_WS    = "";
    cfg->web.PASS_WS    = "";
    cfg->web.USER_WEB   = "";
    cfg->web.PASS_WEB   = "";
    cfg->web.VIEW_EDIT  = "/src/editor.html";
    cfg->web.VIEW_FILE  = "/src/files.html";
    cfg->web.DIR_STATIC = "/src/";
    cfg->web.DIR_UPLOAD = "/dat/";

    cfg->wifi.AP_SSID   = "TempCtrlAP";
    cfg->wifi.AP_PASS   = "tempctrl";
    cfg->wifi.STA_EN    = false;
    cfg->wifi.STA_SSID  = "";
    cfg->wifi.STA_PASS  = "";

    cfg->app.DNS_EN     = true;
    cfg->app.DNS_HOST   = "temp.esp32.local";
    cfg->app.OTA_EN     = false;
    cfg->app.OTA_URL    = "";
    cfg->app.PROMPT     = "temp> ";

    // readonly entries
    cfg->info.NAME      = "TempCtrl";
    cfg->info.UID       = "";
    cfg->info.VERSION   = __DATE__ " " __TIME__;
}

/******************************************************************************
 * NVS helper functions
 */

static struct {
    bool init;           // whether nvs flash initialized
    esp_err_t result;    // nvs flash init result
    nvs_handle_t handle; // nvs handle obtained from nvs_open function
} nvs_st  = {
    .init = false,
    .result = ESP_OK,
    .handle = 0
};

esp_err_t config_nvs_init() {
    if (nvs_st.init) return nvs_st.result;
    const esp_partition_t *nvs = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTUPE_DATA_NVS, NULL);
    if (nvs == NULL) {
        ESP_LOGE(TAG, "Cannot found nvs partition. Skip");
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t err;
#ifndef CONFIG_NVS_ENCRYPT
    bool enc = false;
#else
    bool enc = true;
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
            err = nvs_flash_secure_init(cfg);
        } else {
            ESP_LOGE(TAG, "Cannot initialize nvs with encryption: %s",         
                     esp_err_to_name(err));
            enc = false;
        }
    } else { enc = false; }
#endif
    if (!enc) err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        err = esp_partition_erase_range(nvs, 0, nvs->size);
        if (!err) err = enc ? nvs_flash_secure_init(cfg) : nvs_flash_init();
    }
    if (err) {
        ESP_LOGE(TAG, "Cannot initialize nvs flash: %s", esp_err_to_name(err));
    }
    nvs_st.init = true;
    nvs_st.result = err;
    return err;
}

bool config_nvs_open(const char *ns, bool ro = false) {
    if (nvs_st.handle) return false;
    esp_err_t err = ESP_OK;
    if (!nvs_st.init) err = config_nvs_init();
    if (!err) {
        err = nvs_open(ns, ro ? NVS_READONLY : NVS_READWRITE, &nvs_st.handle);
    }
    if (err) {
        ESP_LOGW(TAG, "Cannot open nvs namespace `%d`: %s",
                 ns, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool config_nvs_close() {
    if (!nvs_st.init || !nvs_st.handle) return false;
    esp_err_t err = nvs_commit(nvs_st.handle);
    if (err) ESP_LOGE(TAG, "commit fail: %s", esp_err_to_name(err));
    nvs_close(nvs_st.handle);
    nvs_st.handle = 0;
    return err == ESP_OK;
}

inline const char * _get_str(const char * key, const char * value) {
    size_t len = 0;
    esp_err_t err = nvs_get_str(nvs_st.handle, key, NULL, &len);
    if (err) {
        ESP_LOGE(TAG, "get `%s` length fail: %s", key, esp_err_to_name(err));
        return value;
    }
    size_t maxlen = sizeof(value);
    if (len > maxlen) {
        char *tmp = (char *)realloc(value, len);
        if (tmp == NULL) {
            ESP_LOGE(TAG, "not enough spave in value: %u < %u", maxlen, len);
            return value;
        } else value = tmp;
    }
    err = nvs_get_str(nvs_st.handle, key, value, &len);
    if (err) {
        ESP_LOGE(TAG, "get str `%s` fail: %s", key, esp_err_to_name(err));
    }
    return value;
}

inline esp_err_t _set_str(const char *key, const char *value, bool commit=true)
{
    esp_err_t err = nvs_set_str(nvs_st.handle, key, value);
    if (err) {
        ESP_LOGE(TAG, "set str `%s` to `%s` fail: %s",
                 key, value, esp_err_to_name(err));
    } else if (commit) {
        err = nvs_commit(nvs_st.handle);
        if (err) ESP_LOGE(TAG, "commit fail: %s", esp_err_to_name(err));
    }
    return err;
}

inline uint8_t _get_u8(const char *key, uint8_t value) {
    esp_err_t err = nvs_get_u8(nvs_st.handle, key, &value);
    if (err) {
        ESP_LOGE(TAG, "get u8 `%s` fail: %s", key, esp_err_to_name(err));
    }
    return value;
}

inline esp_err_t _set_u8(const char *key, uint8_t value, bool commit=true) {
    esp_err_t err = nvs_set_u8(nvs_st.handle, key, value);
    if (err) {
        ESP_LOGE(TAG, "set u8 `%s` to `%d` fail: %s",
                 key, value, esp_err_to_name(err));
    } else if (commit) {
        err = nvs_commit(nvs_st.handle);
        if (err) ESP_LOGE(TAG, "commit fail: %s", esp_err_to_name(err));
    }
    return err;
}

/******************************************************************************
 * Configuration utilities
 */

bool config_initialize() {
    config_reset_default(&Config);

    // load readonly values
    if (config_nvs_open("data", true)) {
        _get_str("name", Config.info.NAME);
        _get_str("uid",  Config.info.UID);
        _get_str("ver",  Config.info.VERSION);
        config_nvs_close();
    }

    // startup times counter test
    if (config_nvs_open("data", false)) {
        uint32_t counter = 0;
        err = nvs_get_u32(nvs_st.handle, "counter", &counter);
        if (err) {
            ESP_LOGE(TAG, "get u32 `counter` fail: %s", esp_err_to_name(err));
        }
        ESP_LOGI(TAG, "Current run times: %u", ++counter);
        err = nvs_set_u32(nvs_st.handle, "counter", counter);
        if (err) {
            ESP_LOGE(TAG, "set u32 `counter` fail: %s", esp_err_to_name(err));
        }
        config_nvs_close();
    }
    
    return config_load(&Config);
}

bool config_load(config_t *cfg) {
    if (!config_nvs_open("config")) return false;
    _get_str("web.user.ws",     Config.web.USER_WS);
    _get_str("web.pass.ws",     Config.web.PASS_WS);
    _get_str("web.user.web",    Config.web.USER_WEB);
    _get_str("web.pass.web",    Config.web.PASS_WEB);
    _get_str("web.view.edit",   Config.web.VIEW_EDIT);
    _get_str("web.view.file",   Config.web.VIEW_FILE);
    _get_str("web.dir.static",  Config.web.DIR_STATIC);
    _get_str("web.dir.upload",  Config.web.DIR_UPLOAD);

    _get_str("wifi.ap.ssid",    Config.wifi.AP_SSID);
    _get_str("wifi.ap.pass",    Config.wifi.AP_PASS);
    _get_u8( "wifi.sta.run",    (uint8_t)Config.wifi.STA_EN);
    _get_str("wifi.sta.ssid",   Config.wifi.STA_SSID);
    _get_str("wifi.sta.pass",   Config.wifi.STA_PASS);

    _get_u8( "app.dns.run",     (uint8_t)Config.app.DNS_EN);
    _get_str("app.dns.host",    Config.app.DNS_HOST);
    _get_u8( "app.ota.run",     (uint8_t)Config.app.OTA_EN);
    _get_str("app.ota.url",     Config.app.OTA_URL);
    _get_str("app.prompt",      Config.app.PROMPT);
    return config_nvs_close();
}

bool config_save(config_t *cfg) {
    if (!config_nvs_open("config")) return false;
    esp_err_t err;

    err = nvs_set_str("web.user.ws",    Config.web.USER_WS,     false);
    err = nvs_set_str("web.pass.ws",    Config.web.PASS_WS,     false);
    err = nvs_set_str("web.user.web",   Config.web.USER_WEB,    false);
    err = nvs_set_str("web.pass.web",   Config.web.PASS_WEB,    false);
    err = nvs_set_str("web.view.edit",  Config.web.VIEW_EDIT,   false);
    err = nvs_set_str("web.view.file",  Config.web.VIEW_FILE,   false);
    err = nvs_set_str("web.dir.static", Config.web.DIR_STATIC,  false);
    err = nvs_set_str("web.dir.upload", Config.web.DIR_UPLOAD,  false);

    err = nvs_set_str("wifi.ap.ssid",   Config.wifi.AP_SSID,    false);
    err = nvs_set_str("wifi.ap.pass",   Config.wifi.AP_PASS,    false);
    err = nvs_set_u8( "wifi.sta.run",   (uint8_t)Config.wifi.STA_EN, false);
    err = nvs_set_str("wifi.sta.ssid",  Config.wifi.STA_SSID,   false);
    err = nvs_set_str("wifi.sta.pass",  Config.wifi.STA_PASS,   false);

    err = nvs_set_u8( "app.dns.run",    (uint8_t)Config.app.DNS_EN, false);
    err = nvs_set_str("app.dns.host",   Config.app.DNS_HOST,    false);
    err = nvs_set_u8( "app.ota.run",    (uint8_t)Config.app.OTA_EN, false);
    err = nvs_set_str("app.ota.url",    Config.app.OTA_URL,     false);
    err = nvs_set_str("app.prompt",     Config.app.PROMPT,      false);

    if (err) ESP_LOGE(TAG, "config save fail: %s", esp_err_to_name(err));
    else {
        err = nvs_commit(nvs_st.handle);
        if (err) ESP_LOGE(TAG, "commit fail: %s", esp_err_to_name(err));
    }
    return err == ESP_OK;
}
