/* 
 * File: server.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2019-05-27 15:29:05
 */

#include "server.h"
#include "gpio.h"
#include "config.h"
#include "update.h"
#include "globals.h"

#include <FS.h>
// #include <FFat.h>
// #define FFS FFat
#include <SPIFFS.h>  // TODO: replace with general vfs
#define FFS SPIFFS

#include "cJSON.h"
#include "esp_log.h"
#include "esp_system.h"

static const char
*TAG = "Server",
*WS = "WebSocket",
*ERROR_HTML =
"<!DOCTYPE html>"
"<html>"
"<head>"
    "<title>Page not found</title>"
"</head>"
"<body>"
    "<h1>404: Page not found.</h1>"
    "<hr>"
    "<p>We're sorry. The page you requested could not be found.</p>"
    "<a href='/index.html'>Go back to homepage.</a>"
"</body>"
"</html>",
*UPDATE_HTML =
"<!DOCTYPE html>"
"<html>"
"<head>"
    "<title>OTA Updation</title>"
"</head>"
"<body>"
    "<form action='/update' method='post' enctype='multipart/form-data'>"
        "<input type='file' name='update'>"
        "<input type='submit' value='Upload' onclick='this.value+=\`ing...\`'>"
    "</form>"
"<body>"
"</html>";

void server_loop_begin() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    WebServer.begin();
}

void server_loop_end() { WebServer.end(); }

char * jsonify_files(File dir) {
    static char *ret = NULL;
    cJSON *lst = cJSON_CreateArray();
    File file;
    while (file = dir.openNextFile()) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "name", file.name());
        cJSON_AddNumberToObject(obj, "size", file.size());
        cJSON_AddNumberToObject(obj, "date", file.getLastWrite());
        cJSON_AddStringToObject(
            obj, "type", file.isDirectory() ? "folder" : "file");
        cJSON_AddItemToArray(lst, obj);
    }
    file.close();
    if (ret != NULL) {
        free(ret);
        ret = NULL;
    }
    ret = cJSON_Print(lst);
    cJSON_Delete(lst);
    return ret;
}

static bool log_request = true;

void log_msg(AsyncWebServerRequest *req, const char *msg = "") {
    if (!log_request) return;
    ESP_LOGD(TAG, "%4s %s%s", req->methodToString(), req->url().c_str(), msg);
}

/******************************************************************************
 * HTTP & static files API
 */

void onCommand(AsyncWebServerRequest *request) {
    log_msg(request);
    if (request->hasParam("exec")) {
        const char *cmd = request->getParam("exec")->value().c_str();
        printf("Command: %s\n", cmd);
    } else if (request->hasParam("gcode")) {
        printf("GCode parser: `%s`\n", request->getParam("gcode")->value().c_str());
    } else {
        request->send(400, "text/text", "Invalid parameter");
    }
}

void onUpdate(AsyncWebServerRequest *request) {
    log_msg(request);
    request->send(200, "text/html", UPDATE_HTML);
}

void onUpdatePost(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static bool updating = false;
    if (!index) {
        log_msg(request);
        if (updating) return request->send(400, "text/plain", "Busy updating");
        if (!ota_updation_begin()) {
            return request->send(400, "text/plain", ota_updation_error());
        };
        ESP_LOGW(TAG, "Updating file: %s\n", filename.c_str());
        updating = true;
        LIGHTBLK(50, 3);
    }
    if (updating && !ota_updation_error()) {
        LIGHTON();
        ota_updation_write(data, len);
        LIGHTOFF();
    }
    if (final && updating) {
        updating = false;
        if (ota_updation_end()) {
            ESP_LOGW(TAG, "Update success: %s\n", format_size(index + len));
        }
    }
}

void onConfig(AsyncWebServerRequest *request) {
    log_msg(request);
    char *json;
    if (config_dumps(json)) {
        request->send(200, "application/json", json);
        free(json);
    } else
        request->send(404, "text/plain", "Cannot dump Configuration into JSON");
}

void onConfigPost(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    printf("index: %lu, len: %lu, final: %d, data: %s\n", index, len, final, (char *)data);
}

void onEdit(AsyncWebServerRequest *request) {
    log_msg(request);
    if (request->hasParam("list")) { // listdir
        String path = request->getParam("list")->value();
        if (!path.startsWith("/")) path = "/" + path;
        File root = FFS.open(path);
        if (!root) {
            request->send(404, "text/plain", "Dir does not exists.");
        } else if (!root.isDirectory()) {
            request->send(400, "text/plain", "No more files under a file.");
            // request->redirect(path);
        } else {
            request->send(200, "application/json", jsonify_files(root));
        }
        if (root) root.close();
    } else if (request->hasParam("path")) { // serve static files
        String path = request->getParam("path")->value();
        if (!path.startsWith("/")) path = "/" + path;
        File file = FFS.open(path);
        if (!file) {
            request->send(404, "text/plain", "File does not exists.");
        } else if (file.isDirectory()) {
            request->send(400, "text/plain", "Cannot download directory.");
        } else {
            request->send(file, path, String(), request->hasParam("download"));
        }
        if (file) file.close();
    } else { // Online Editor page
        const char * buildTime = __DATE__ " " __TIME__ " GMT";
        if (request->header("If-Modified-Since").equals(buildTime)) {
            request->send(304);
        } else {
            AsyncWebServerResponse *response = request->beginResponse(
                FFS, Config.web.VIEW_EDIT);
            response->addHeader("Content-Encoding", "gzip");
            response->addHeader("Last-Modified", buildTime);
            request->send(response);
        }
    }
}

void onCreate(AsyncWebServerRequest *request) {
    // handle file|dir create
    log_msg(request);
    if (!request->hasParam("path")) {
        return request->send(400, "text/plain", "No filename specified.");
    }
    String 
        path = request->getParam("path")->value(),
        type = request->hasParam("type") ? \
               request->getParam("type")->value() : "file";
    if (type == "file") {
        if (FFS.exists(path)) 
            return request->send(403, "text/plain", "File already exists.");
        File file = FFS.open(path, "w");
        if (!file) {
            return request->send(500, "text/plain", "Create failed.");
        } else { file.close(); }
    } else if (type == "folder") {
        File dir = FFS.open(path);
        if (dir.isDirectory()) {
            dir.close();
            return request->send(403, "text/plain", "Dir already exists.");
        } else if (!FFS.mkdir(path)) {
            return request->send(500, "text/plain", "Create failed.");
        }
    }
    request->send(200);
}

void onDelete(AsyncWebServerRequest *request) {
    // handle file|dir delete
    log_msg(request);
    if (!request->hasParam("path")) {
        return request->send(400, "text/plain", "No path specified");
    }
    String path = request->getParam("path")->value();
    File f = FFS.open(path);
    if (!f) return request->send(403, "text/plain", "File/dir does not exist");
    bool isdir = f.isDirectory(); f.close();
    if (isdir && !FFS.rmdir(path)) {
        request->send(500, "text/plain", "Delete directory failed");
    } else if (!isdir && !FFS.remove(path)) {
        request->send(500, "text/plain", "Delete file failed");
    } else if (request->hasParam("from")) {
        request->redirect(request->getParam("from")->value());
    } else {
        request->send(200);
    }
}
    
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File file;
    if (!index) {
        log_msg(request);
        if (file) return request->send(400, "text/plain", "Busy uploading");
        if (!filename.startsWith("/")) filename = "/" + filename;
        if (FFS.exists(filename) && !request->hasParam("overwrite")) {
            return request->send(403, "text/plain", "File already exists.");
        }
        ESP_LOGW(TAG, "Uploading file: %s\n", filename.c_str());
        file = FFS.open(filename, "w");
    }
    if (file) {
        LIGHTON();
        file.write(data, len);
        ESP_LOGI(TAG, "\rProgress: %s", format_size(index));
        LIGHTOFF();
    }
    if (final && file) {
        file.flush();
        file.close();
        ESP_LOGW(TAG, "Update success: %s\n", format_size(index + len));
    }
}

void onUploadStrict(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!filename.startsWith(Config.web.DIR_DATA)) {
        log_msg(request, "400");
        return request->send(400, "text/plain", "No access to upload.");
    }
    onUpload(request, filename, index, data, len, final);
}

void onError(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) return request->send(200);
    return request->send(404, "text/html", ERROR_HTML);
}

void onErrorFileManager(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) return request->send(200);
    String path = request->url();
    File file = FFS.open(path);
    if (!file || !file.isDirectory()) {
        log_msg(request, " 404");
        file.close();
        return request->send(404, "text/html", ERROR_HTML);
    } else {
        log_msg(request, " is directory, goto file manager.");
        file.close();
    }
    request->send(FFS, Config.web.VIEW_FILE, String(), false,
        [&path](const String& var){
            if (var == "ROOT") return path;
            if (var == "FILELIST") {
                File root = FFS.open(path);
                char *json = jsonify_files(root);
                root.close();
                return String(json);
            }
        }
    );
}

/******************************************************************************
 * WebSocket message parser and callbacks
 */

void handle_websocket_message(AsyncWebSocketClient *client, char *data, uint8_t type) {
    // TODO: implement json callback
    printf(data);
    if (type == WS_TEXT) client->text("Got ID:");
    else                 client->binary("Got ID:");
}

void onWebSocket(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t datalen) {
    static char header[32];
    snprintf(header, 32, "ws#%u %s:%d", client->id(),
             client->remoteIP().toString().c_str(), client->remotePort());
    switch (type) {
    case WS_EVT_CONNECT:
        ESP_LOGV(WS, "%s connected", header);
        client->ping();
        // server.cleanupClients();
        break;
    case WS_EVT_DISCONNECT:
        ESP_LOGV(WS, "%s disconnected", header);
        break;
    case WS_EVT_ERROR:
        ESP_LOGI(WS, "%s error(%u) %s", header, *((uint16_t*)arg), (char*)data);
        break;
    case WS_EVT_DATA:
        uint32_t cid = client->id();

        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        static char * msg = NULL;
        static uint32_t wsid = -1;
        static size_t idx = 0, buflen = 0;

        if (wsid != -1 && wsid != cid) {
            ESP_LOGD(WS, "%s error: message buffer busy. Skip", header);
            break;
        } else wsid = cid;
        
        if (info->final && info->num == 0) {
            // Message is not fragmented so there's only one frame
            ESP_LOGD(WS, "%s message[%llu]", header, info->len);
            if (info->index == 0) {
                // We are starting this only frame
                buflen = ((info->opcode == WS_TEXT) ? 1 : 2) * info->len + 1;
                msg = (char *)malloc(buflen);
                if (msg == NULL) break;
                else idx = 0;
            } else if (msg != NULL) {
                // This frame is splitted into packets
                // do nothing
            } else {
                ESP_LOGD(WS, "%s error: lost first packets. Skip", header);
            }
        } else if (info->index == 0) {
            // Message is fragmented. Starting a new frame
            ESP_LOGD(WS, "%s frame%d[%llu]", header, info->num, info->len);
            size_t l = ((info->message_opcode == WS_TEXT) ? 1 : 2) * info->len;
            if (info->num == 0) {
                // Starting the first frame
                msg = (char *)malloc(l + 1);
                if (msg == NULL) break;
                idx = 0; buflen = l + 1;
            } else if (msg != NULL) {
                // Extend buffer for the comming frame
                char *tmp = (char *)realloc(msg, buflen + l);
                if (tmp == NULL) break;
                msg = tmp; buflen += l;
            } else {
                ESP_LOGD(WS, "%s error: lost first frame. Skip", header);
                break;
            }
        } else if (msg != NULL) {
            // Message is fragmented. Current frame is splitted
            // do nothing
        } else {
            ESP_LOGD(WS, "%s error: lost message head. Skip", header);
            break;
        }

        // Save/append packets to message buffer
        if (info->opcode == WS_TEXT) {
            idx += snprintf(msg + idx, buflen - idx, (char *)data);
        } else {
            for (size_t i = 0; i < datalen; i++) {
                idx += snprintf(msg + idx, buflen - idx, "%02x", data[i]);
            }
        }
        ESP_LOGV(WS, "%s >packets[%llu-%llu]",
                 header, info->index, info->index + datalen);

        if (info->index + datalen == info->len) {
            // Current frame end
            if (info->final) {
                // All message buffered
                wsid = -1; idx = buflen = 0;
                handle_websocket_message(client, msg, info->message_opcode);
                free(msg);
            } else {
                // Waiting for next frame
                // do nothing
            }
        }
        break;
    }
}

/* AsyncWebSocket Frame Information struct contains
 * {
 *   uint8_t  message_opcode // WS_TEXT | WS_BINARY
 *   uint8_t  opcode         // WS_CONTINUATION if fragmented
 *
 *   uint32_t num            // frame number of a fragmented message
 *   uint8_t  final          // whether this is the last frame
 *   
 *   uint64_t len            // length of the current frame
 *   uint64_t index          // data offset in current frame
 * }
 *
 * Assuming that a Message from the ws client is fragmented as follows:
 *   | Frame0__ | Frame1___ | Frame2____ |
 *     ^   ^      ^           ^  ^
 *     A   B      C           D  E
 * A: num=0, final=false, opcode=message_opcode,  index=0, len=8,  datalen=4
 * B: num=0, final=false, opcode=message_opcode,  index=4, len=8,  datalen=4
 * C: num=1, final=false, opcode=WS_CONTINUATION, index=0, len=9,  datalen=9
 * D: num=2, final=true,  opcode=WS_CONTINUATION, index=0, len=10, datalen=3
 * E: num=2, final=true,  opcode=WS_CONTINUATION, index=3, len=10, datalen=7
 */
void onWebSocketData(uint32_t cid, AwsFrameInfo *info, char *data, size_t datalen) {

}


/******************************************************************************
 * Class public members
 */

void WebServerClass::begin() {
    if (_started) return _server.begin();
    FFS.begin();
    _server.reset();
    register_statics();
    register_ws_api();
    register_ap_api();
    register_sta_api();
    DefaultHeaders::Instance()
        .addHeader("Access-Control-Allow-Origin", "*");
    _server.begin();
    _started = true;
}

void WebServerClass::register_sta_api() {
    _server.on("/cmd",    HTTP_POST, onCommand);

    _server.serveStatic("/sta", FFS, Config.web.DIR_STA)
        .setDefaultFile("index.html")
#ifndef CONFIG_DEBUG
        .setFilter(ON_STA_FILTER)
#endif
        ;
}

void WebServerClass::register_ap_api() {
    _server.on("/update", HTTP_GET, onUpdate).setFilter(ON_AP_FILTER);
    _server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        const char *error = ota_updation_error();
        AsyncWebServerResponse *response = request->beginResponse(
            200, "text/plain", error ? error : "OK. Rebooting");
        response->addHeader("Connection", "close");
        request->send(response);
        if (error == NULL) esp_restart();
    }, onUpdatePost).setFilter(ON_AP_FILTER);

    _server.on("/config", HTTP_GET, onConfig).setFilter(ON_AP_FILTER);
    _server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
    }, onConfigPost).setFilter(ON_AP_FILTER);

    // Use HTTP_ANY for compatibility with HTTP_PUT/HTTP_DELETE
    _server.on("/edit", HTTP_GET, onEdit).setFilter(ON_AP_FILTER);
    _server.on("/editc", HTTP_ANY, onCreate).setFilter(ON_AP_FILTER);
    _server.on("/editd", HTTP_ANY, onDelete).setFilter(ON_AP_FILTER);
    _server.on("/editu", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Uploaded");
    }, onUpload).setFilter(ON_AP_FILTER);

    // _server.rewrite("/", "index.html");
    _server.serveStatic("/ap/", FFS, Config.web.DIR_AP)
        .setDefaultFile("index.html")
        .setCacheControl("max-age=3600")
        .setAuthentication(Config.web.HTTP_NAME, Config.web.HTTP_PASS)
        .setFilter(ON_AP_FILTER);
}

void WebServerClass::register_ws_api() {
    _wsocket.onEvent(onWebSocket);
    _wsocket.setAuthentication(Config.web.WS_NAME, Config.web.WS_PASS);
    _server.addHandler(&_wsocket);
}

void WebServerClass::register_statics() {
    _server.serveStatic("/", FFS, Config.web.DIR_ROOT);
    _server.serveStatic("/assets/", FFS, Config.web.DIR_ASSET);
    _server.onNotFound(onErrorFileManager);
    _server.onFileUpload(onUploadStrict);
}

bool WebServerClass::logging() { return log_request; }
void WebServerClass::logging(bool log_request) { log_request = log_request; }

WebServerClass WebServer;
