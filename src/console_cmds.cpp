/* 
 * File: console_cmds.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-13 18:03:04
 *
 * Implemented commands are:
 *  
 */

#include "console.h"
#include "wifi.h"
#include "config.h"
#include "update.h"
#include "globals.h"
#include "drivers.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_console.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "rom/uart.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

static const char * NAME = "Command";

/******************************************************************************
 * Some common utilities
 */

/* @brief   Parse command line arguments into argtable and catch any errors
 * @return
 *          - true  : arguments successfully parsed, no error
 *          - false : error occur
 * */
static bool arg_noerror(int argc, char **argv, void **argtable) {
    if (arg_parse(argc, argv, argtable) != 0) {
        struct arg_hdr **table = (struct arg_hdr **) argtable;
        int tabindex = 0;
        while (!(table[tabindex]->flag & ARG_TERMINATOR)) { tabindex++; }
        arg_print_errors(stdout, (struct arg_end *) table[tabindex], argv[0]);
        return false;
    }
    return true;
}

/******************************************************************************
 * WiFi commands
 */

esp_console_cmd_t cmd_wifi_connect = {
    .command = "connect",
    .help = "not implemented yet",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        return ESP_ERR_NOT_FOUND;
    },
    .argtable = NULL
};

esp_console_cmd_t cmd_wifi_disconnect = {
    .command = "disconnect",
    .help = "not implemented yet",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        return wifi_disconnect() ? 0 : -1;
    },
    .argtable = NULL
};

esp_console_cmd_t cmd_wifi_scan = {
    .command = "lsap",
    .help = "WiFi scan for Access Pointes",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { wifi_sta_list(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t cmd_wifi_clients = {
    .command = "lssta",
    .help = "List WiFi stations connected to current softAP",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { wifi_ap_list(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t cmd_wifi_softap = {
    .command = "wifi",
    .help = "Hide/Show/Refresh softAP configuration",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        return ESP_ERR_NOT_FOUND;
    },
    .argtable = NULL
};

/******************************************************************************
 * Power commands
 */

const char* const wakeup_reason_list[] = {
    "Undefined", "Undefined", "EXT0", "EXT1", 
    "Timer", "Touchpad", "ULP", "GPIO", "UART",
};

static struct {
    struct arg_int *wakeup_time;
    struct arg_int *wakeup_gpio_num;
    struct arg_int *wakeup_gpio_level;
    struct arg_str *sleep;
    struct arg_end *end;
} sleep_args = {
    .wakeup_time = arg_int0("t", "time", "<t>", "wakeup time, ms"),
    .wakeup_gpio_num = 
        arg_intn("p", "gpio", "<n>", 0, 8,
                 "If specified, wakeup using GPIO with given number"),
    .wakeup_gpio_level = 
        arg_intn("l", "level", "<0|1>", 0, 8,
                 "GPIO level to trigger wakeup"),
    .sleep = arg_str0(NULL, "method", "<light|deep>", "sleep mode"),
    .end = arg_end(4)
};

int enable_gpio_light_wakeup() {
    int gpio_count = sleep_args.wakeup_gpio_num->count;
    int level_count = sleep_args.wakeup_gpio_level->count;
    if (level_count && (gpio_count != level_count)) {
        ESP_LOGE(NAME, "GPIO and level mismatch!");
        return ESP_ERR_INVALID_ARG;
    }
    int gpio, level;
    gpio_int_type_t intr;
    const char *lvls;
    for (int i = 0; i < gpio_count; i++) {
        gpio = sleep_args.wakeup_gpio_num->ival[i];
        level = level_count ? sleep_args.wakeup_gpio_level->ival[i] : 0;
        lvls = level ? "HIGH" : "LOW";
        intr = level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
        fprintf(stderr, "Enable GPIO wakeup, num: %d, level: %s\n", gpio, lvls);
        ESP_ERROR_CHECK( gpio_wakeup_enable((gpio_num_t)gpio, intr) );
    }
    ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
    return ESP_OK;
}

int enable_gpio_deep_wakeup() {
    int gpio = sleep_args.wakeup_gpio_num->ival[0], level = 0;
    if (sleep_args.wakeup_gpio_level->count) {
        level = sleep_args.wakeup_gpio_level->ival[0];
        if (level != 0 && level != 1) {
            ESP_LOGE(NAME, "Invalid wakeup level: %d", level);
            return ESP_ERR_INVALID_ARG;
        }
    }
    const char *lvls = level ? "HIGH" : "LOW";
    esp_sleep_ext1_wakeup_mode_t mode;
    mode = level ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW;
    fprintf(stderr, "Enable GPIO wakeup, num: %d, level: %s\n", gpio, lvls);
    ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup(1ULL << gpio, mode) );
    return ESP_OK;
}

esp_console_cmd_t cmd_sys_sleep = {
    .command = "sleep",
    .help = "Turn ESP32 into light/deep sleep mode",
    .hint = NULL,
    .func = [](int argc, char **argv) {
        if (!arg_noerror(argc, argv, (void **) &sleep_args))
            return ESP_ERR_INVALID_ARG;
        if (sleep_args.wakeup_time->count) {
            uint64_t timeout = sleep_args.wakeup_time->ival[0];
            fprintf(stderr, "Enable timer wakeup, timeout: %llums\n", timeout);
            ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout * 1000) );
        }
        bool light = true;
        if (sleep_args.sleep->count) {
            const char *sleep = sleep_args.sleep->sval[0];
            if (!strcmp(sleep, "deep")) light = false;
            else if (strcmp(sleep, "light")) {
                ESP_LOGE(NAME, "Unsupported sleep mode: %s", sleep);
                return ESP_ERR_INVALID_ARG;
            }
        }
        esp_err_t err;
        if (light) {
            if (sleep_args.wakeup_gpio_num->count) {
                if (err = enable_gpio_light_wakeup()) return err;
            }
            fprintf(stderr, "Enable UART wakeup, num: %d\n", NUM_UART);
            ESP_ERROR_CHECK( uart_set_wakeup_threshold(NUM_UART, 3) );
            ESP_ERROR_CHECK( esp_sleep_enable_uart_wakeup(NUM_UART) );
        } else {
            if (sleep_args.wakeup_gpio_num->count) {
                if (err = enable_gpio_deep_wakeup()) return err;
            }
        }

        fprintf(stderr, "Turn to %s sleep mode\n", light ? "light" : "deep");
        fflush(stderr); uart_tx_wait_idle(NUM_UART);
        if (light) {
            esp_light_sleep_start();
        } else {
            esp_deep_sleep_start();
        }
        fprintf(stderr, "ESP32 is woken up from light sleep mode by %s\n",
                wakeup_reason_list[(int)esp_sleep_get_wakeup_cause()]);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        return ESP_OK;

    },
    .argtable = &sleep_args
};

esp_console_cmd_t cmd_sys_restart = {
    .command = "reboot",
    .help = "Software reset of ESP32",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { esp_restart(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t cmd_sys_otainfo = {
    .command = "lsota",
    .help = "List information of OTA partitons and ota_apps",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { ota_info(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t cmd_sys_otaupdate = {
    .command = "update",
    .help = "OTA Updation through BT/WiFi/HTTPServer",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return ESP_ERR_NOT_FOUND; },
    .argtable = NULL
};

/******************************************************************************
 * Utilities commands
 */

esp_console_cmd_t cmd_utils_version = {
    .command = "version",
    .help = "Get version of firmware and SDK",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { version_info(); return 0; },
    .argtable = NULL
};

static struct {
    struct arg_lit *verbose;
    struct arg_end *end;
} memory_args = {
    .verbose = arg_litn("v", NULL, 0, 2, "additive option for more output"),
    .end = arg_end(1)
};

esp_console_cmd_t cmd_utils_memory = {
    .command = "lsmem",
    .help = "List the avaiable memory with their status",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &memory_args))
            return ESP_ERR_INVALID_ARG;
        switch (memory_args.verbose->count) {
        case 0:
            memory_info(); break;
        case 2:
            // Too much infomation
            // heap_caps_dump_all(); break;
            heap_caps_print_heap_info(MALLOC_CAP_DMA);
            heap_caps_print_heap_info(MALLOC_CAP_EXEC);
        case 1:
            heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
            heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
            break;
        }
        return ESP_OK;
    },
    .argtable = &memory_args
};

esp_console_cmd_t cmd_utils_tasks = {
    .command = "lstask",
    .help = "List information of running RTOS tasks",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { task_info(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t cmd_utils_hardware = {
    .command = "lshw",
    .help = "Display hardware details",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { hardware_info(); return 0; }
};

esp_console_cmd_t cmd_utils_part = {
    .command = "lspart",
    .help = "List information of partitions in flash",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { partition_info(); return 0; },
    .argtable = NULL
};

static struct {
    struct arg_str *action;
    struct arg_str *filename;
    struct arg_end *end;
} hist_args = {
    .action = arg_str1(NULL, NULL, "<load/save>", ""),
    .filename = arg_str0("f", "file", "history.txt", "relative path to file"),
    .end = arg_end(2)
};

esp_console_cmd_t cmd_utils_hist = {
    .command = "hist",
    .help = "Load from or save console history to a FlashFS file",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &hist_args))
            return ESP_ERR_INVALID_ARG;
        static char *path, *fp, *tmp;
        const char *fn = hist_args.filename->count ? \
                         hist_args.filename->sval[0] : \
                         "history.txt";
        const char *action = hist_args.action->sval[0];
        bool save;
        if (!strcmp(action, "load")) save = false;
        else if (!strcmp(action, "save")) save = true;
        else {
            printf("Invalid command: `%s`\n", action);
            return ESP_ERR_INVALID_ARG;
        }

        uint8_t len = strlen(Config.web.DIR_DATA) + strlen(fn);
        tmp = (char *)realloc(path, len + 1);
        if (tmp) path = tmp; else return ESP_ERR_NO_MEM;
        tmp = (char *)realloc(fp, len + strlen(FFS_MP) + 1);
        if (tmp) fp = tmp; else return ESP_ERR_NO_MEM;
        strcpy(path, Config.web.DIR_DATA); strcat(path, fn);
        strcpy(fp, FFS_MP); strcat(fp, path);
        if (!FFS.exists(path) && !save) {
            printf("History file `%s` does not exists\n", fn);
            return ESP_ERR_NOT_FOUND;
        }

        int ret = save ? linenoiseHistorySave(fp) : linenoiseHistoryLoad(fp);
        printf("History file `%s` %s %s\n", fp, action, ret ? "fail" : "done");
        return ret;
    },
    .argtable = &hist_args
};

/******************************************************************************
 * Configuration commands
 */

esp_console_cmd_t cmd_config_load = {
    .command = "loadcfg",
    .help = "Load configuration from NVS data partition",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return config_load() ? 0 : -1; },
    .argtable = NULL
};

esp_console_cmd_t cmd_config_dump = {
    .command = "savecfg",
    .help = "Save configuration to NVS data partition",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return config_dump() ? 0 : -1; },
    .argtable = NULL
};

esp_console_cmd_t cmd_config_list = {
    .command = "lscfg",
    .help = "List configuration key:value details",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { config_nvs_list(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t cmd_config_stats = {
    .command = "lsnvs",
    .help = "Get NVS flash partition entries details",
    .hint = NULL,
    .func = [](int arg, char **argv) -> int { config_nvs_stats(); return 0; },
    .argtable = NULL
};

static struct {
    struct arg_str *key;
    struct arg_str *val;
    struct arg_end *end;
} config_args = {
    .key = arg_str1(NULL, NULL, NULL, "key"),
    .val = arg_str0(NULL, NULL, NULL, "value"),
    .end = arg_end(2)
};

esp_console_cmd_t cmd_config_io = {
    .command = "config",
    .help = "Set/get configuration value by key name",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &config_args))
            return ESP_ERR_INVALID_ARG;
        const char *key = config_args.key->sval[0];
        if (config_args.val->count) {
            const char *val = config_args.val->sval[0];
            printf("Set `%s` to `%s` %s\n", key, val,
                   config_set(key, val) ? "done" : "fail");
        } else {
            printf("Get `%s` value `%s`\n", config_get(key));
        }
        return ESP_OK;
    },
    .argtable = &config_args
};

/******************************************************************************
 * GPIO commands
 */

static struct {
    struct arg_int *index;
    struct arg_str *action;
    struct arg_str *color;
    struct arg_end *end;
} led_args = {
    .index = arg_int0("i", "index", "<0-20>", "specify index, default 0"),
    .action = arg_str0(NULL, NULL, "<on|off>", "enable/disable LED"),
    .color = arg_str0("c", "color", "<0xAABBCC>", "specify RGB color"),
    .end = arg_end(3)
};

esp_console_cmd_t cmd_gpio_ledc = {
    .command = "ledc",
    .help = "Set/get LED color/status",
    .hint = NULL,
    .func= [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &led_args))
            return ESP_ERR_INVALID_ARG;
        uint8_t idx = led_args.index->count ? led_args.index->ival[0] : 0;
        if (led_args.color->count) {
            uint32_t color = strtol(led_args.color->sval[0], NULL, 0);
            if (color > 0xffffff) {
                printf("Unsupported color: `%s`\n", led_args.color->sval[0]);
                return ESP_ERR_INVALID_ARG;
            } else led_color(idx, color);
        }
        if (led_args.action->count) {
            const char *action = led_args.action->sval[0];
            bool enable;
            if (!strcmp(action, "off")) enable = false;
            else if (!strcmp(action, "on")) enable = true;
            else {
                printf("Invalid LED command: `%s`\n", action);
                return ESP_ERR_INVALID_ARG;
            }
            printf("Setting LED %d to %s\n", idx, action);
            if (enable) led_on(idx); else led_off(idx);
        }
        printf("LED %d: color 0x%06X, %s\n",
               idx, led_color(idx), led_status(idx) ? "ON" : "OFF");
        return ESP_OK;
    },
    .argtable = &led_args
};

static struct {
    struct arg_int *pin;
    struct arg_int *level;
    struct arg_end *end;
} gpio_args = {
    .pin = arg_int0(NULL, NULL, "<0-39|100-123|200-215>", "pin number"),
    .level = arg_int0(NULL, NULL, "<0|1>", "set pin to LOW / HIGH"),
    .end = arg_end(2)
};

esp_console_cmd_t cmd_gpio_level = {
    .command = "gpio",
    .help = "Set/get GPIO pin level",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &gpio_args))
            return ESP_ERR_INVALID_ARG;
        esp_err_t err = ESP_OK;
        uint32_t pin_num = -1, level = -1;
        if (gpio_args.pin->count) pin_num = gpio_args.pin->ival[0];
        if (gpio_args.level->count) level = gpio_args.level->ival[0] ? 1 : 0;
        // printf("Input gpio %d, level %d\n", pin_num, level);
        if (pin_num == -1) {
            printf("TODO: List GPIO Table\n");
            return ESP_OK;
        } else if (pin_num < 40) {
            gpio_num_t pin = static_cast<gpio_num_t>(pin_num);
            if (level != -1) err = gpio_set_level(pin, level);
            else level = gpio_get_level(pin);
        } else if (PIN_I2C_MIN < pin_num && pin_num < PIN_I2C_MAX) {
            i2c_pin_num_t pin = static_cast<i2c_pin_num_t>(pin_num);
            if (level != -1) err = i2c_gpio_set_level(pin, level);
            else level = i2c_gpio_get_level(pin);
        } else if (PIN_SPI_MIN < pin_num && pin_num < PIN_SPI_MAX) {
            spi_pin_num_t pin = static_cast<spi_pin_num_t>(pin_num);
            if (level != -1) err = spi_gpio_set_level(pin, level);
            else level = spi_gpio_get_level(pin);
        } else return ESP_ERR_INVALID_ARG;
        if (!err) 
            printf("GPIO %d: %s\n", pin_num, level ? "HIGH" : "LOW");
        else 
            printf("%s GPIO %d level error: %s\n",
                   level != -1 ? "Set" : "Get", pin_num, esp_err_to_name(err));
        return ESP_OK;
    },
    .argtable = &gpio_args
};

/******************************************************************************
 * Export register commands
 */

esp_console_cmd_t * commands[] = {
    //&cmd_wifi_connect, &cmd_wifi_disconnect, &cmd_wifi_scan, &cmd_wifi_softap,
    &cmd_wifi_clients,

    &cmd_sys_sleep, &cmd_sys_restart, &cmd_sys_otainfo,
    // &cmd_sys_otaupdate,

    &cmd_utils_version, &cmd_utils_memory, &cmd_utils_tasks,
    &cmd_utils_hardware, &cmd_utils_part, &cmd_utils_hist,

    &cmd_config_load, &cmd_config_dump,
    &cmd_config_list, &cmd_config_stats, &cmd_config_io,

    &cmd_gpio_ledc, &cmd_gpio_level,
};

void console_register_commands() {
    esp_log_level_set(NAME, ESP_LOG_INFO);
    ESP_ERROR_CHECK( esp_console_register_help_command() );
    for (uint8_t idx = 0; idx < sizeof(commands)/sizeof(commands[0]); idx++) {
        ESP_ERROR_CHECK( esp_console_cmd_register(commands[idx]) );
    }
}
