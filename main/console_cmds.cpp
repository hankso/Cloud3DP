/* 
 * File: console_cmds.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-13 18:03:04
 *
 * Implemented commands are:
 *  
 */

#include "console.h"
#include "config.h"
#include "globals.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_console.h"
#include "esp_ota_ops.h"
#include "rom/uart.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

#include <SPIFFS.h>
#define FFS SPIFFS

/******************************************************************************
 * Common
 */

static const char * NAME = "Command";

/* The argtables of each sub-commands are defined as a struct, containing 
 * declaration of arguments. When an argtable is initialized by
 * `struct XXX_args_t xxx_args = { ... }`, arguments are defined, too.
 * In this way it's not necessary to make argument variables global because
 * they can be accessed by `xxx_args.yyy`
 */
static struct arg_str *action = arg_str0(
        NULL, NULL, "<on|off>", "action can also be <true|false>");
static struct arg_int *channel = arg_int0(
        "c", "channel", "<0-7>", "specify channel number, default all");


// static struct action_args_t {
//     struct arg_str *action;
//     struct arg_end *end;
// } bool_args = {
//     .action = action,
//     .end = arg_end(1)
// };

typedef struct {
    struct arg_str *action;
    struct arg_int *channel;
    struct arg_end *end;
} action_channel_args_t;

static action_channel_args_t
    action_args = {
        .action = action,
        .channel = arg_int0(NULL, NULL, "", NULL),
        .end = arg_end(1)
    },
    channel_args = {
        .action = action,
        .channel = channel,
        .end = arg_end(2),
    };

/* @brief   Get action value of commands, same as `bool(str) -> 0|1|2`
 * @return
 *          - 0     : Turn off (false)
 *          - 1     : Turn on (true)
 *          - other : Invalid action or argument not specified
 */
static int get_action(action_channel_args_t argtable) {
    if (argtable.action->count) {
        const char *action = argtable.action->sval[0];
        if (!strcmp(action, "true") ||
            !strcmp(action, "True") ||
            !strcmp(action, "on") ||
            !strcmp(action, "ON")) return 1;
        if (!strcmp(action, "false") ||
            !strcmp(action, "False") ||
            !strcmp(action, "off") ||
            !strcmp(action, "OFF")) return 0;
        ESP_LOGE(NAME, "Invalid action: %s", action);
    }
    return -1;
}

/* @brief   Get channel value of commands, valid channel number is [0-7]
 * @return
 *          - -1  : invalid channel
 *          - num : channel number
 */
// static int get_channel(void **argtable) {
    // struct arg_hdr **table = (struct arg_hdr **) argtable;
static int get_channel(action_channel_args_t argtable) {
    if (argtable.channel->count) {
        int channel = argtable.channel->ival[0];
        if (-1 < channel && channel < 8) {
            return channel;
        }
        ESP_LOGE(NAME, "Invalid channel: %d", channel);
    }
    return -1;
}

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

static struct {
    struct arg_int *source;
    struct arg_end *end;
} output_args = {
    .source = arg_int0(
            "d", "data", "<0|1|2|3>", "choose data source from one of "
            "[ADS1299 Raw, ADS1299 Notch, ESP Square, ESP Sine]"),
    .end = arg_end(1)
};

static struct {
    struct arg_int *freq;
    struct arg_end *end;
} sinfreq_args = {
    .freq = arg_int0(
            "f", "freq", "<int>", "center frequency of generated fake wave"),
    .end = arg_end(1)
};


/******************************************************************************
 * WiFi commands
 */

esp_console_cmd_t cmd_wifi_connect = {
    .command = "connect",
    .help = "not implemented yet",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return 1; },
    .argtable = NULL
};

esp_console_cmd_t cmd_wifi_disconnect = {
    .command = "disconnect",
    .help = "not implemented yet",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return 1; },
    .argtable = NULL
};

esp_console_cmd_t cmd_wifi_list = {
    .command = "lswifi",
    .help = "not implemented yet",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return 1; },
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
    .sleep = arg_str0(NULL, "method", "<light/deep>", "sleep mode"),
    .end = arg_end(4)
};

int enable_gpio_light_wakeup() {
    int gpio_count = sleep_args.wakeup_gpio_num->count;
    int level_count = sleep_args.wakeup_gpio_level->count;
    if (level_count && (gpio_count != level_count)) {
        ESP_LOGE(NAME, "GPIO and level mismatch!");
        return 1;
    }
    int gpio, level;
    gpio_int_type_t intr;
    const char *lvls;
    for (int i = 0; i < gpio_count; i++) {
        gpio = sleep_args.wakeup_gpio_num->ival[i];
        if (level_count != 0) {
            level = sleep_args.wakeup_gpio_level->ival[i];
        } else {
            level = 0;
        }
        lvls = level ? "HIGH" : "LOW";
        intr = level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
        printf("Enable GPIO wakeup, num: %d, level: %s\n", gpio, lvls);
        ESP_ERROR_CHECK( gpio_wakeup_enable((gpio_num_t)gpio, intr) );
    }
    ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
    return 0;
}

int enable_gpio_deep_wakeup() {
    int gpio = sleep_args.wakeup_gpio_num->ival[0], level = 0;
    if (sleep_args.wakeup_gpio_level->count) {
        level = sleep_args.wakeup_gpio_level->ival[0];
        if (level != 0 && level != 1) {
            ESP_LOGE(NAME, "Invalid wakeup level: %d", level);
            return 1;
        }
    }
    const char *lvls = level ? "HIGH" : "LOW";
    esp_sleep_ext1_wakeup_mode_t mode;
    mode = level ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW;
    printf("Enable GPIO wakeup, num: %d, level: %s\n", gpio, lvls);
    ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup(1ULL << gpio, mode) );
    return 0;
}

esp_console_cmd_t cmd_sys_sleep = {
    .command = "sleep",
    .help = "Turn ESP32 into light/deep sleep mode",
    .hint = NULL,
    .func = [](int argc, char **argv) {
        if (!arg_noerror(argc, argv, (void **) &sleep_args)) return 1;
        if (sleep_args.wakeup_time->count) {
            uint64_t timeout = sleep_args.wakeup_time->ival[0];
            printf("Enable timer wakeup, timeout: %llums\n", timeout);
            ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout * 1000) );
        }
        bool light = true;
        if (sleep_args.sleep->count) {
            const char *sleep = sleep_args.sleep->sval[0];
            if (strcmp(sleep, "deep") == 0) {
                light = false;
            } else if (strcmp(sleep, "light") != 0) {
                ESP_LOGE(NAME, "Invalid sleep mode: %s", sleep);
                return 1;
            }
        }
        if (light) {
            if (sleep_args.wakeup_gpio_num->count) {
                if (enable_gpio_light_wakeup() != 0) return 1;
            }
            printf("Enable UART wakeup, num: %d\n", UART_NUM_0);
            ESP_ERROR_CHECK( uart_set_wakeup_threshold(UART_NUM_0, 3) );
            ESP_ERROR_CHECK( esp_sleep_enable_uart_wakeup(UART_NUM_0) );
        } else {
            if (sleep_args.wakeup_gpio_num->count) {
                if (enable_gpio_deep_wakeup() != 0) return 1;
            }
        }

        printf("ESP32 will turn to %s sleep mode\n", light ? "light" : "deep");
        fflush(stdout); uart_tx_wait_idle(UART_NUM_0);
        if (light) {
            esp_light_sleep_start();
        } else {
            esp_deep_sleep_start();
        }
        printf("ESP32 is woken up from light sleep mode by %s\n", 
               wakeup_reason_list[(int)esp_sleep_get_wakeup_cause()]);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        return 0;

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

esp_console_cmd_t cmd_sys_update = {
    .command = "update",
    .help = "OTA Updation through BT/WiFi/HTTPServer",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { 
        return 0;
    },
    .argtable = NULL
};

bool ota_update_from_url(char *url) {
    esp_ota_handle_t handle;
    const esp_partition_t *current = esp_ota_get_running_partition();
    printf("Currently running partition %d-%d\n");
}

/******************************************************************************
 * Utilities commands
 */

esp_console_cmd_t cmd_utils_summary = {
    .command = "summary",
    .help = "Print summary of current status",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return memory_info(); },
    .argtable = NULL
};

esp_console_cmd_t cmd_utils_version = {
    .command = "version",
    .help = "Get version of chip and SDK",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return version_info(); },
    .argtable = NULL
};


esp_console_cmd_t cmd_utils_tasks = {
    .command = "lstask",
    .help = "Get information about running RTOS tasks",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return task_info(); },
    .argtable = NULL
};

static struct {
    struct arg_str *filename;
    struct arg_end *end;
} hist_args = {
    .filename = arg_str0("f", "file", "", "full path to file"),
    .end = arg_end(1)
};

esp_console_cmd_t cmd_utils_load = {
    .command = "loadhist",
    .help = "Load console history from a SPIFFS file",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &hist_args)) return 1;
        const char *fn = hist_args.filename->count ? \
                         hist_args.filename->sval[0] : \
                         "/history.txt";
        File file = FFS.open(fn);
        if (!file) {
            ESP_LOGE(NAME, "History file `%s` does not exists", fn);
            return 1;
        } else file.close();
        char *mountpoint = "/spiffs";
        uint8_t len = strlen(fn) + strlen(mountpoint);
        char *tmp = (char *)malloc(len + 1);
        if (tmp == NULL) {
            ESP_LOGE(NAME, "Cannot allocate memory for filename. Try later");
            return 1;
        } else {
            snprintf(tmp, len + 1, "%s%s", mountpoint, fn);
            ESP_LOGE(NAME, "Loading history file %s", tmp);
        }
        int ret = linenoiseHistoryLoad(tmp);
        free(tmp); return ret;
    },
    .argtable = &hist_args
};

esp_console_cmd_t cmd_utils_save = {
    .command = "savehist",
    .help = "Save console history to a SPIFFS file",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &hist_args)) return 1;
        const char *fn = hist_args.filename->count ? \
                         hist_args.filename->sval[0] : \
                         "/history.txt";
        File file = FFS.open(fn);
        if (file.isDirectory()) {
            ESP_LOGE(NAME, "Invalid filename: %s is directory", fn);
            file.close(); return 1;
        } else file.close();
        char *mountpoint = "/spiffs";
        uint8_t len = strlen(fn) + strlen(mountpoint);
        char *tmp = (char *)malloc(len + 1);
        if (tmp == NULL) {
            ESP_LOGE(NAME, "Cannot allocate memory for filename. Try later");
            return 1;
        } else {
            snprintf(tmp, len + 1, "%s%s", mountpoint, fn);
            ESP_LOGI(NAME, "Saving history file %s". tmp);
        }
        int ret = linenoiseHistorySave(tmp);
        free(tmp); return ret;
    },
    .argtable = &hist_args
};

esp_console_cmd_t cmd_utils_list = {
    .command = "lspart",
    .help = "List information of partitions in flash",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return partition_info(); },
    .argtable = NULL
};

/******************************************************************************
 * Configuration commands
 */

esp_console_cmd_t cmd_config_load = {
    .command = "loadcfg",
    .help = "Load configuration from NVS data partition",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return config_load() ? 0 : 1; },
    .argtable = NULL
};

esp_console_cmd_t cmd_config_dump = {
    .command = "savecfg",
    .help = "Save configuration to NVS data partition",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return config_dump() ? 0 : 1; },
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

esp_console_cmd_t cmd_config_get = {
    .command = "get",
    .help = "Get configuration value by key name",
    .hint = NULL,
    .func = [](int arg, char **argv) -> int {
        return 1;
        printf("%s\n", config_get("asdf"));
    },
    .argtable = NULL
};

esp_console_cmd_t cmd_config_set = {
    .command = "set",
    .help = "Set configuration key to value",
    .hint = NULL,
    .func = [](int arg, char **argv) -> int {
        return 1;
        printf("%s\n", config_get("asdf"));
    },
    .argtable = NULL
};

/******************************************************************************
 * Export register commands
 */

void console_register_wifi() {
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_wifi_connect) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_wifi_disconnect) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_wifi_list) );
}

void console_register_sys() {
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_sys_sleep) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_sys_restart) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_sys_update) );
}

void console_register_utils() {
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_utils_summary) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_utils_version) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_utils_tasks) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_utils_load) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_utils_save) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_utils_list) );
}

void console_register_config() {
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_config_load) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_config_dump) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_config_list) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_config_stats) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_config_get) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_config_set) );
}

void console_register_commands() {
    esp_log_level_set(NAME, ESP_LOG_INFO);
    ESP_ERROR_CHECK( esp_console_register_help_command() );
    console_register_wifi();
    console_register_sys();
    console_register_utils();
    console_register_config();
}
