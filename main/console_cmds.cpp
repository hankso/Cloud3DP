/* 
 * File: console_cmds.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-13 18:03:04
 *
 * Implemented commands are:
 *  
 */

#include "globals.h"
#include "console_cmds.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_console.h"
#include "esp_ota_ops.h"
#include "rom/uart.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

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
        String action = String(argtable.action->sval[0]);
        if (action.equalsIgnoreCase("true") || 
            action.equalsIgnoreCase("on")) {
            return 1;
        } 
        if (action.equalsIgnoreCase("false") ||
            action.equalsIgnoreCase("off")) {
            return 0;
        }
        ESP_LOGE(NAME, "Invalid action: %s", action.c_str());
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

esp_console_cmd_t wifi_connect = {
    .command = "connect",
    .help = "not implemented yet",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return 1; },
    .argtable = NULL
};

esp_console_cmd_t wifi_disconnect = {
    .command = "disconnect",
    .help = "not implemented yet",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { return 1; },
    .argtable = NULL
};

esp_console_cmd_t wifi_direction = {
    .command = "echoing",
    .help = "Turn on/off serial-to-Wifi direction",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        if (!arg_noerror(argc, argv, (void **) &action_args)) return 1;
        int act = get_action(action_args);
        bool wifi_echo = act == 0 ? false : (act == 1 ? true : wifi_echo);
        ESP_LOGI(NAME, "Serial-to-wifi echo: %s", wifi_echo ? "ON" : "OFF");
        return 0;
    },
    .argtable = &action_args,
};

/******************************************************************************
 * Power commands
 */

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
    for (int i = 0; i < gpio_count; i++) {
        gpio = sleep_args.wakeup_gpio_num->ival[i];
        if (level_count != 0) {
            level = sleep_args.wakeup_gpio_level->ival[i];
        } else {
            level = 0;
        }
        ESP_LOGI(NAME, "Enable GPIO wakeup, num: %d, level: %s",
                 gpio, level ? "HIGH" : "LOW");
        intr = level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
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
    ESP_LOGI(NAME, "Enable GPIO wakeup, num: %d, level: %s",
             gpio, level ? "HIGH" : "LOW");
    esp_sleep_ext1_wakeup_mode_t mode;
    mode = level ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW;
    ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup(1ULL << gpio, mode) );
    return 0;
}

esp_console_cmd_t sys_sleep = {
    .command = "sleep",
    .help = "Turn ESP32 into light/deep sleep mode",
    .hint = NULL,
    .func = [](int argc, char **argv) {
        if (!arg_noerror(argc, argv, (void **) &sleep_args)) return 1;
        if (sleep_args.wakeup_time->count) {
            uint64_t timeout = sleep_args.wakeup_time->ival[0];
            ESP_LOGI(NAME, "Enable timer wakeup, timeout: %llums", timeout);
            ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout * 1000) );
        }
        bool light_sleep = true;
        if (sleep_args.sleep->count) {
            const char *sleep = sleep_args.sleep->sval[0];
            if (strcmp(sleep, "deep") == 0) {
                light_sleep = false;
            } else if (strcmp(sleep, "light") != 0) {
                ESP_LOGE(NAME, "Invalid sleep mode: %s", sleep);
                return 1;
            }
        }
        if (light_sleep) {
            if (sleep_args.wakeup_gpio_num->count) {
                if (enable_gpio_light_wakeup() != 0) return 1;
            }
            ESP_LOGI(NAME, "Enable UART wakeup, num: %d", UART_NUM_0);
            ESP_ERROR_CHECK( uart_set_wakeup_threshold(UART_NUM_0, 3) );
            ESP_ERROR_CHECK( esp_sleep_enable_uart_wakeup(UART_NUM_0) );
        } else {
            if (sleep_args.wakeup_gpio_num->count) {
                if (enable_gpio_deep_wakeup() != 0) return 1;
            }
        }

        ESP_LOGW(NAME, "ESP32 will turn into %s sleep mode",
                 light_sleep ? "light" : "deep");
        fflush(stdout); uart_tx_wait_idle(UART_NUM_0);
        if (light_sleep) {
            esp_light_sleep_start();
        } else {
            esp_deep_sleep_start();
        }
        ESP_LOGW(NAME, "ESP32 is woken up from light sleep mode by %s", 
                 wakeup_reason_list[(int)esp_sleep_get_wakeup_cause()]);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        return 0;

    },
    .argtable = &sleep_args
};

esp_console_cmd_t sys_restart = {
    .command = "reboot",
    .help = "Software reset of ESP32",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { esp_restart(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t sys_update = {
    .command = "battery",
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
    ESP_LOGI(NAME, "Currently running partition %d-%d");
}

/******************************************************************************
 * Utilities commands
 */

void version_info() {
    esp_chip_info_t info;
    esp_chip_info(&info);
    ESP_LOGE(NAME, "IDF Version: %s", esp_get_idf_version());
    ESP_LOGE(NAME, "Chip info:");
    ESP_LOGE(NAME, "\tmodel: %s", info.model == CHIP_ESP32 ? "ESP32" : "???");
    ESP_LOGE(NAME, "\tcores: %d", info.cores);
    ESP_LOGE(NAME, "\tfeature: %s%s%s/%s-Flash: %.1f MB", 
        info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
        info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
        info.features & CHIP_FEATURE_BT ? "/BT" : "",
        info.features & CHIP_FEATURE_EMB_FLASH ? "Embedded" : "External",
        spi_flash_get_chip_size() / (1024.0 * 1024));
    ESP_LOGE(NAME, "\trevision number: %d", info.revision);

    ESP_LOGE(NAME, "Firmware Version: TempCtrl-" __DATE__);
}

void summary() {
    static multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    static float
        mfree = info.total_free_bytes,
        mallo = info.total_allocated_bytes,
        mrate = mfree / (mfree + mallo) * 100;

    ESP_LOGW(NAME, "ESP free memory:  %.2f KB, %d%%", mfree / 1024.0, mrate);
    ESP_LOGW(NAME, "WiFi status:      %s", "TODO");
}

esp_console_cmd_t utils_summary = {
    .command = "summary",
    .help = "Print summary of current status",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { summary(); return 0; },
    .argtable = NULL
};

esp_console_cmd_t utils_version = {
    .command = "version",
    .help = "Get version of chip and SDK",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int { version_info(); return 0; },
    .argtable = NULL
};


esp_console_cmd_t utils_tasks = {
    .command = "tasks",
    .help = "Get information about running RTOS tasks",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
        const size_t task_bytes = 40;
        char *taskstr = (char *)malloc(uxTaskGetNumberOfTasks() * task_bytes);
        if (taskstr == NULL) {
            ESP_LOGE(NAME, "allocation for tasks list failed");
            return 1;
        }
        vTaskList(taskstr);
        ESP_LOGE(NAME, "\nTask Name\tStatus\tPrio\tHWM\tTask#"
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
                 "\tAffinity"
#endif
                 "\n%s", taskstr);
        free(taskstr);
#else
        ESP_LOGE(NAME, "Unsupported command! Enable `%s` in sdkconfig.",
                 "CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS")
#endif
        return 0;
    },
    .argtable = NULL
};

esp_console_cmd_t utils_load = {
    .command = "load",
    .help = "Not implemented yet! Load console history from SPI Flash",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        return 0;
        char *path = "/history.txt";
        linenoiseHistoryLoad(path);
    }
};

esp_console_cmd_t utils_save = {
    .command = "save",
    .help = "Not implemented yet! Save console history to SPI Flash",
    .hint = NULL,
    .func = [](int argc, char **argv) -> int {
        return 0;
        char *path = "/history.txt";
        linenoiseHistorySave(path);
    }
};

/******************************************************************************
 * Export register commands
 */

void console_register_wifi() {
    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_connect) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_disconnect) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_direction) );
}

void console_register_sys() {
    ESP_ERROR_CHECK( esp_console_cmd_register(&sys_sleep) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&sys_restart) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&sys_update) );
}

void console_register_utils() {
    ESP_ERROR_CHECK( esp_console_cmd_register(&utils_summary) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&utils_version) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&utils_tasks) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&utils_load) );
    ESP_ERROR_CHECK( esp_console_cmd_register(&utils_save) );
}

void console_register_commands() {
    console_register_wifi();
    console_register_sys();
    console_register_utils();
    ESP_ERROR_CHECK( esp_console_register_help_command() );
}
