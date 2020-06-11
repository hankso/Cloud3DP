/*
 * File: console.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 15:58:29
*/

#include "console.h"
#include "config.h"
#include "globals.h"

#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "linenoise/linenoise.h"

static const char *TAG = "Console";

static const char *prompt = "$ ";

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
}

/* Redirect STDOUT to a buffer, thus any thing printed to STDOUT will be
 * collected as a string. But don't forget to free the buffer after result
 * is handled.
 *
 * 1. <stdio.h> setvbuf - store string in buffer (temporarily)
 *  pos: easy to set & unset, safe printing with buflen
 *  neg: cannot stop flushing to stdout
 *  e.g.:
 *      char *buf = (char *)calloc(1024, sizeof(char));
 *      setvbuf(stdout, buf, _IOFBF, 1024);
 *      printf("test string\n");
 *      setvbuf(stdout, NULL, _IONBF, 1024);
 *      printf("Get string from STDOUT %lu: `%s`", strlen(buf), buf);
 *
 * 2. <stdio.h> memstream - open memory as stream
 *  2.1 fmemopen(void *buf, size_t size, const char *mode)
 *  2.2 open_memstream(char * *ptr, size_t *sizeloc)
 *  pos: The open_memstream will dynamically allocate buffer and automatically
 *       grow. The fmemopen function support 'a'(append) mode.
 *  neg: Caller need to free the buffer after stream is closed. Stream opened
 *       by fmemopen function need buf config (i.e. setvbuf or fflush) and is
 *       limited by buffer size.
 *  e.g.:
 *      char *buf; size_t size;
 *      FILE *stdout_bak = stdout;
 *      stdout = open_memstream(&buf, &size);
 *      printf("hello\n");
 *      fclose(stdout); stdout = stdout_bak;
 *      printf("In buffer: size %d, msg %s\n", size, buf);
 *      free(buf);
 *
 * 3. <unistd.h> pipe & dup: this is not what we want
 *
 * 4. If pipe STDOUT to a disk file on flash or memory block using VFS, thing
 *    becomes much easier.
 *  e.g.:
 *      stdout = fopen("/spiffs/runtime.log", "w");
 *    But writing & reading from a file is slow and consume too much resources.
 *    A better way is memory mapping.
 *  e.g.:
 *      esp_vfs_t vfs_buffer = {
 *          .open = &my_malloc,
 *          .close = &my_free,
 *          .read = &my_strcpy,
 *          .write = &my_snprintf,
 *          .fstat = NULL,
 *          .flags = ESP_VFS_FLAG_DEFAULT,
 *      }
 *      esp_vfs_register("/dev/ram", &vfs_buffer, NULL);
 *      stdout = fopen("/dev/ram/blk0", "w");
 *
 * Currently implemented is method 2. Try method 4 if necessary in the future.
 */

char * console_handle_command(const char *cmd, bool history) {
    // Semaphore is better than task notification here
    // if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)) != 1) {
    if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(200)) == pdFALSE) {
        return cast_away_const("Console task is executing command");
    } else if (!cmd || !strlen(cmd)) {
        return cast_away_const("Invalid command to execute");
    }
    if (history) linenoiseHistoryAdd(cmd);

    FILE *bak = stdout; char *buf; size_t size = 0;
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

    fclose(stdout); stdout = bak;
    if (buf != NULL) {
        if (!size) {                        // empty string means no log output
            free(buf); buf = NULL;
        } else {                            // rstrip buffer string
            while (size--) {
                if (buf[size] != '\n' && buf[size] != '\r') break;
                buf[size] = '\0';
            }
            buf[size + 1] = '\0';
        }
    }
    xSemaphoreGive(xSemaphore);
    return buf;
}

void console_handle_one() {
    char *ret, *cmd = linenoise(prompt);
    if (cmd != NULL && (ret = console_handle_command(cmd))) {
        printf("%s\n\n", ret);
        free(ret);
    }
    linenoiseFree(cmd);
}

void console_handle_loop(void *argv) {
    for (;;) {
        console_handle_one();
#ifdef CONFIG_TASK_WDT
        esp_task_wdt_reset();
#endif
    }
}

void console_loop_begin(int xCoreID) {
    const char * const pcName = "console";
    const uint32_t usStackDepth = 8192;
    void * const pvParameters = NULL;
    const UBaseType_t uxPriority = 1;
    TaskHandle_t *pvCreatedTask = NULL;
#ifndef CONFIG_FREERTOS_UNICORE
    if (xCoreID == 0 || xCoreID == 1) {
        xTaskCreatePinnedToCore(
            console_handle_loop, pcName, usStackDepth,
            pvParameters, uxPriority, pvCreatedTask, xCoreID);
    } else
#endif
    {
        // default tskNO_AFFINITY
        xTaskCreate(
            console_handle_loop, pcName, usStackDepth,
            pvParameters, uxPriority, pvCreatedTask);
    }
}

static char * rpc_error(double code, const char *msg) {
    cJSON *rep = cJSON_CreateObject(), *error;
    cJSON_AddNullToObject(rep, "id");
    cJSON_AddItemToObject(rep, "error", error = cJSON_CreateObject());
    cJSON_AddStringToObject(rep, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", msg);
    return cJSON_PrintUnformatted(rep);
}

static char * rpc_response(cJSON *id, const char *result) {
    cJSON *rep = cJSON_CreateObject();
    if (id != NULL) {
        cJSON_AddItemToObject(rep, "id", id);
    } else {
        cJSON_AddNullToObject(rep, "id");
    }
    cJSON_AddStringToObject(rep, "jsonrpc", "2.0");
    cJSON_AddStringToObject(rep, "result", result);
    return cJSON_PrintUnformatted(rep);
}

char * console_handle_rpc(const char *json) {
    cJSON *obj = cJSON_Parse(json);
    if (!obj)
        return rpc_error(-32700, "Parse Error");
    if (!cJSON_HasObjectItem(obj, "method"))
        return rpc_error(-32600, "Invalid JSON");
    const char *method = cJSON_GetObjectItem(obj, "method")->valuestring;
    char *cmd;
    if (!cJSON_HasObjectItem(obj, "params")) {
        cmd = strdup(method);
    } else {
        cJSON *params = cJSON_GetObjectItem(obj, "params");
        if (!cJSON_IsArray(params)) return rpc_error(-32600, "Invalid Request");
        size_t size = 0; FILE *buf = open_memstream(&cmd, &size);
        fprintf(buf, "%s", method);
        for (uint8_t i = 0; i < cJSON_GetArraySize(params); i++) {
            fprintf(buf, " %s", cJSON_GetArrayItem(params, i)->valuestring);
        }
        fclose(buf);
    }
    if (!cmd) return rpc_error(-32400, "System Error");
    ESP_LOGI(TAG, "Get RPC command: `%s`", cmd);
    char *response = NULL, *ret = console_handle_command(cmd);
    if (cJSON_HasObjectItem(obj, "id")) { // not notification
        response = rpc_response(cJSON_GetObjectItem(obj ,"id"), ret ? ret : "");
    }
    if (ret) free(ret);
    free(cmd);
    return response;
}

// THE END
