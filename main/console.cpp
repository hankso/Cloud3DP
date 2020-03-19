/*
 * File: console.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 15:58:29
*/

#include "config.h"
#include "console.h"
#include "console_cmds.h"

#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "linenoise/linenoise.h"

static const char *TAG = "Console";

static const char *prompt = "$ ";

void console_pipe_init();

void console_initialize() {
    esp_log_level_set(TAG, ESP_LOG_WARN);
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);
    prompt = Config.app.PROMPT;
    if (linenoiseProbe() != 0) {
        linenoiseSetDumbMode(1);
        ESP_LOGW(TAG, "Your terminal does not support escape sequences.\n"
                 "Line editing and history features are disabled, as well as "
                 "console color. Try use PuTTY/Miniterm.py/idf_monitor.py\n");
    } else {
#if CONFIG_LOG_COLORS
        int l = strlen(prompt) + strlen(LOG_COLOR_W) + strlen(LOG_RESET_COLOR);
        char *tmp = (char *)malloc(l + 1);
        if (tmp != NULL) {
            sprintf(tmp, "%s%s%s", LOG_COLOR_W, prompt, LOG_RESET_COLOR);
            prompt = tmp;
            ESP_LOGI(TAG, "Using color prompt `%s`", prompt);
        }
#endif
    }
    esp_console_config_t console_config = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 8,
#if CONFIG_LOG_COLORS
        .hint_color = atoi(LOG_COLOR_CYAN),
        .hint_bold = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );
    console_register_commands();
}

/*
 * Pipe STDOUT to a buffer, thus any thing printed to STDOUT will be
 * collected as a string. But keep in mind that you must free the buffer
 * after result is handled.
 *
 * If pipe STDOUT to a disk file on flash or memory block using VFS, thing
 * becomes much easier. e.g.:
 *      FILE stdout_bak = stdout; stdout = fopen("/spiffs/runtime.log", "w");
 * or register memory block:
 *      esp_vfs_t vfs_buffer = {
 *          .open = &my_malloc,
 *          .close = &my_free,
 *          .read = &my_strcpy,
 *          .write = &my_snprintf,
 *          .fstat = NULL,
 *          .flags = ESP_VFS_FLAG_DEFAULT,
 *      }
 *      esp_vfs_register("/dev/ram", &vfs_buffer, NULL);
 *      stdout = fopen("/dev/ram", "w");
 * Implement this if necessary in the future.
 */

static struct {
    FILE *obj;
    FILE *bak;
    char *buf;
    size_t idx;
    size_t len;
} stdout_pipe {
    .obj = NULL,
    .bak = NULL,
    .buf = NULL,
    .idx = 0,
    .len = 0,
};

static TaskHandle_t console_task = NULL;
static bool console_loop_flag = false;

static int _pipe_writefn(void *cookie, const char *data, int size) {
    fprintf(stderr, "%d/%d data: %d, %s\n",
            stdout_pipe.len, stdout_pipe.idx, size, data);
    size_t available = stdout_pipe.len - stdout_pipe.idx;
    if (available <= size) {
        size_t newsize = stdout_pipe.idx + size + 64;
        char *tmp = (char *)realloc(stdout_pipe.buf, newsize);
        if (tmp == NULL) return 0;
        stdout_pipe.buf = tmp;
        stdout_pipe.len = newsize;
        fprintf(stderr, "new size: %d\n", newsize);
    }
    fprintf(stderr, "%d - %d",
        stdout_pipe.buf + stdout_pipe.idx,
        stdout_pipe.len - stdout_pipe.idx,
    );
    stdout_pipe.idx += size;
    // stdout_pipe.idx += snprintf(
    //     stdout_pipe.buf + stdout_pipe.idx,
    //     stdout_pipe.len - stdout_pipe.idx,
    //     data);
    return size;
}

void console_pipe_init() {
    if (stdout_pipe.obj) return;
    stdout_pipe.obj = funopen(
        NULL,                                   // cookie
        (int (*)(void*, char*, int))0,          // read function
        &_pipe_writefn,                         // write function
        (fpos_t (*)(void*, fpos_t, int))0,      // seek function
        (int (*)(void*))0                       // close function
    );
    setvbuf(stdout_pipe.obj, NULL, _IONBF, 0);  // disable stream buffer
}

char * console_handle_command(char *cmd, bool history) {
    char *buf = (char *)malloc(64);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Cannot allocate memory for command result. Try later");
    } else {
        console_handle_command(cmd, buf, 64, history);
    }
    return buf;
}

void console_handle_command(char *cmd, char *ret, size_t len, bool history) {
    if (console_task == NULL) {
        snprintf(ret, len, "Console loop task is not created");
        return;
    } else if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)) != 1) {
        snprintf(ret, len, "Console task is executing command");
        return;
    } else {
        ESP_LOGE(TAG, "Console get %d: `%s`", strlen(cmd), cmd);
        if (history) linenoiseHistoryAdd(cmd);
    }

    stdout_pipe.buf = ret; stdout_pipe.idx = 0; stdout_pipe.len = len;
    stdout_pipe.bak = stdout; stdout = stdout_pipe.obj;

    int code;
    esp_err_t err = esp_console_run(cmd, &code);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Unrecognized command: `%s`", cmd);
    } else if (err == ESP_OK && code != ESP_OK) {
        ESP_LOGE(TAG, "Command error: %d (%s)", code, esp_err_to_name(code));
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Command error: %d (%s)", err, esp_err_to_name(err));
    }

    stdout = stdout_pipe.bak; stdout_pipe.buf = NULL;
    xTaskNotifyGive(console_task);
}

void console_handle_one() {
    char *cmd = linenoise(prompt);
    if (cmd != NULL) {
        char *ret = console_handle_command(cmd);
        if (ret != NULL) {
            printf("Result %d: %s\n", strlen(ret), ret);
            free(ret);
        }
    }
    linenoiseFree(cmd);
}

void console_handle_loop(void *argv) {
    // executed inside task
    if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10))) {
        xTaskNotifyGive(console_task);
    }
    ESP_LOGI(TAG, "REPL Console started");
    while (console_loop_flag) { console_handle_one(); }
    ESP_LOGI(TAG, "REPL Console stopped");
}

void console_loop_begin(int xCoreID) {
    if (console_loop_flag) return;
    console_loop_flag = true;
    console_pipe_init();
    const char * const pcName = "console";
    const uint32_t usStackDepth = 8192;
    void * const pvParameters = NULL;
    const UBaseType_t uxPriority = 2;
#ifndef CONFIG_FREERTOS_UNICORE
    if (0 <= xCoreID && xCoreID < 2) {
        xTaskCreatePinnedToCore(
            console_handle_loop, pcName, usStackDepth,
            pvParameters, uxPriority, &console_task, xCoreID);
    } else
#endif
    {
        xTaskCreate(
            console_handle_loop, pcName, usStackDepth,
            pvParameters, uxPriority, &console_task);
    }
}

void console_loop_end() {
    console_loop_flag = false;
}

// THE END
