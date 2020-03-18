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
    setvbuf(stdin, NULL, _IONBF, 0);
    // setvbuf(stdout, NULL, _IONBF, 0);
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
            ESP_LOGW(TAG, "Using color prompt `%s`", prompt);
        } else ESP_LOGE(TAG, "Cannot allocate space for new prompt");
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
    
    console_pipe_init();
}

/*
 * Pipe STDOUT to a buffer, thus any thing printed to STDOUT will be
 * collected as a string. But keep in mind that you must free the buffer
 * after result is handled.
 */

static char *stdout_buf = NULL;
static int stdout_index = 0;
static FILE *stdout_bak, *stdout_pipe;

static int _pipe_stdout_to_buf(void *cookie, const char *data, int size) {
    int available = sizeof(stdout_buf) - stdout_index;
    if (available < size) {
        char *tmp = (char *)realloc(stdout_buf, stdout_index + size + 64);
        if (tmp == NULL) return 0;
        else stdout_buf = tmp;
    }
    stdout_index += sprintf(stdout_buf + stdout_index, data);
    return size;
}

void console_pipe_init() {
    stdout_pipe = funopen(
        NULL,                                   // cookie
        (int (*)(void*, char*, int))0,          // read function
        &_pipe_stdout_to_buf,                   // write function
        (fpos_t (*)(void*, fpos_t, int))0,      // seek function
        (int (*)(void*))0                       // close function
    );
    setvbuf(stdout_pipe, NULL, _IONBF, 0);      // disable stream buffer
}

// TODO: replace Semaphore with RTOS-task-notifications
static SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();

char * console_handle_command(char *cmd, bool history) {
    char *tmp = (char *)malloc(128);
    if (tmp == NULL) {
        ESP_LOGE(TAG, "Cannot allocate memory for command result. Try later");
    } else {
        console_handle_command(cmd, tmp, history);
    }
    return tmp;
}

void console_handle_command(char *cmd, char *ret, bool history) {
    if (xSemaphore == NULL || pdTRUE != xSemaphoreTake(
            xSemaphore, (TickType_t) 1000 / portTICK_PERIOD_MS))
    {
        snprintf(ret, sizeof(ret), "Console task is locking");
        return;
    }

    if (history) linenoiseHistoryAdd(cmd);
    stdout_buf = ret; stdout_index = 0;

    stdout_bak = stdout; stdout = stdout_pipe;

    // TODO: which one should we use to log error: printf/ESP_LOGx
    int code;
    esp_err_t err = esp_console_run(cmd, &code);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Unrecognized command: `%s`", cmd);
    } else if (err == ESP_OK && code != ESP_OK) {
        ESP_LOGE(TAG, "Command error: %d (%s)", code, esp_err_to_name(code));
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Command error: %d (%s)", err, esp_err_to_name(err));
    }

    stdout = stdout_bak; stdout_bak = NULL;

    xSemaphoreGive(xSemaphore);
}

void console_handle_one() {
    char *cmd = linenoise(prompt);
    if (cmd != NULL) {
        printf("Get %d: `%s`\n", strlen(cmd), cmd);
        char *ret = console_handle_command(cmd);
        if (ret != NULL) {
            printf("Result %d: `%s`\n", strlen(ret), ret);
            free(ret);
        }
    }
    linenoiseFree(cmd);
}

static bool console_loop_flag = true;

void console_loop_begin(int xCoreID) {
    TaskFunction_t pxTaskCode = console_handle_loop;
    const char * const pcName = "console";
    const uint32_t usStackDepth = 8192;
    void * const pvParameters = NULL;
    const UBaseType_t uxPriority = 2;
    TaskHandle_t *pxCreatedTask = NULL;
#ifndef CONFIG_FREERTOS_UNICORE
    if (0 <= xCoreID && xCoreID < 2) {
        xTaskCreatePinnedToCore(
            pxTaskCode, pcName, usStackDepth,
            pvParameters, uxPriority, pxCreatedTask, xCoreID);
    } else
#endif
    {
        xTaskCreate(
            pxTaskCode, pcName, usStackDepth,
            pvParameters, uxPriority, pxCreatedTask);
    }
}

void console_loop_end() {
    console_loop_flag = false;
}

void console_handle_loop(void *argv) {
    ESP_LOGI(TAG, "REPL Console started");
    while (console_loop_flag) {
        console_handle_one();
    }
    ESP_LOGI(TAG, "REPL Console stopped");
}

// THE END
