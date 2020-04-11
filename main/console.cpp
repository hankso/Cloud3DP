/*
 * File: console.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 15:58:29
*/

#include "console.h"
#include "config.h"
#include "globals.h"

#include "esp_log.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "linenoise/linenoise.h"

static const char *TAG = "Console";

static const char *prompt = "$ ";

// static TaskHandle_t console_task = NULL;
static SemaphoreHandle_t xSemaphore = NULL;


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
            ESP_LOGI(TAG, "Using colorful prompt `%s`", prompt);
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

    xSemaphore = xSemaphoreCreateBinary();
    assert(xSemaphore != NULL && "Cannot create binary semaphore for console");
    xSemaphoreGive(xSemaphore);
    console_pipe_init();
}

char * console_handle_command(char *cmd, bool history) {
    // Semaphore is better than task notification here
    // if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)) != 1) {
    if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(200)) == pdFALSE) {
        return cast_away_const("Console task is executing command");
    } else {
        ESP_LOGD(TAG, "Console get %d: `%s`", strlen(cmd), cmd);
        if (history) linenoiseHistoryAdd(cmd);
    }
    console_pipe_enter();
    char *buf; size_t size;
    stdout = open_memstream(&buf, &size);

    int code;
    esp_err_t err = esp_console_run(cmd, &code);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Unrecognized command: `%s`", cmd);
    } else if (err == ESP_OK && code != ESP_OK) {
        ESP_LOGE(TAG, "Command error: %d (%s)", code, esp_err_to_name(code));
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Command error: %d (%s)", err, esp_err_to_name(err));
    }

    console_pipe_exit();
    if (buf != NULL) {
        if (!size) {
            free(buf);
            buf = NULL;
        } else {
            buf[size] = '\0';
        }
    }
    // xTaskNotifyGive(console_task);
    xSemaphoreGive(xSemaphore);
    return buf;
}

void console_handle_one() {
    char *ret, *cmd = linenoise(prompt);
    if (cmd != NULL && (ret = console_handle_command(cmd))) {
        printf("%s\n", ret);
        free(ret);
    }
    linenoiseFree(cmd);
}

void console_handle_loop(void *argv) {  // executed inside task
    // xTaskNotifyGive(console_task);
    for (;;) { console_handle_one(); }
}

void console_loop_begin(int xCoreID) {
    const char * const pcName = "console";
    const uint32_t usStackDepth = 8192;
    void * const pvParameters = NULL;
    const UBaseType_t uxPriority = 1;
    TaskHandle_t *pvCreatedTask = NULL;
#ifndef CONFIG_FREERTOS_UNICORE
    if (0 <= xCoreID && xCoreID < 2) {
        xTaskCreatePinnedToCore(
            console_handle_loop, pcName, usStackDepth,
            pvParameters, uxPriority, pvCreatedTask, xCoreID);
    } else
#endif
    {
        xTaskCreate(
            console_handle_loop, pcName, usStackDepth,
            pvParameters, uxPriority, pvCreatedTask); // default tskNO_AFFINITY
    }
}

// THE END
