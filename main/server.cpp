/* 
 * File: server.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2019-05-27 15:29:05
 */

#include "server.h"
#include "config.h"
#include "update.h"
#include "globals.h"
#include "drivers.h"
#include "filesys.h"
#include "console.h"

#include "esp_log.h"
#include "esp_system.h"

static const char
*TAG = "Server",
*ERROR_HTML =
"<!DOCTYPE html>"
"<html>"
"<head>"
    "<title>Page not found</title>"
"</head>"
"<body>"
    "<h1>404: Page not found</h1><hr>"
    "<p>Sorry. The page you requested could not be found.</p>"
    "<p>Go back to <a href='/index.html'>homepage</a></p>"
"</body>"
"</html>",
*UPDATE_HTML =
"<!DOCTYPE html>"
"<html>"
"<head>"
    "<title>Basic OTA Updation</title>"
"</head>"
"<body>"
    "<h4>You should always try Advanced OTA Updation page first !</h4>"
    "<form action='/update' method='post' enctype='multipart/form-data'>"
        "<input type='file' name='update'>"
        "<input type='submit' value='Upload'>"
    "</form>"
"<body>"
"</html>";

void server_loop_begin() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    WebServer.begin();
}

void server_loop_end() { WebServer.end(); }

static bool log_request = true;

void log_msg(AsyncWebServerRequest *req, const char *msg = "") {
    if (!log_request) return;
    printf("%4s %s %s\n", req->methodToString(), req->url().c_str(), msg);
}

void log_param(AsyncWebServerRequest *req) {
    if (!log_request) return;
    size_t len = req->params();
    if (!len) printf("No params to log\n");
    AsyncWebParameter *p;
    for (uint8_t i = 0; (p = req->getParam(i)) && (i < len); i++) {
        printf("Param[%d] key:%s, post:%d, file:%d[%d], `%s`\n",
               i, p->name().c_str(), p->isPost(), p->isFile(), p->size(),
               p->isFile() ? "skip binary" : p->value().c_str());
    }
}

/******************************************************************************
 * HTTP & static files API
 */

void onCommand(AsyncWebServerRequest *req) {
    log_msg(req);
    if (req->hasParam("exec", true)) {
        log_param(req);
        const char *cmd = req->getParam("exec", true)->value().c_str();
        char *ret = console_handle_command(cmd);
        if (ret) {
            req->send(200, "text/plain", ret);
            free(ret);
        } else {
            req->send(200);
        }
    } else if (req->hasParam("gcode", true)) {
        const char *gcode = req->getParam("gcode", true)->value().c_str();
        printf("GCode parser: `%s`\n", gcode);
        req->send(500, "text/plain", "GCode parser not implemented yet");
    } else {
        req->send(400, "text/plain", "Invalid parameter");
    }
}

void onUpdate(AsyncWebServerRequest *req) {
    log_msg(req);
    String update = Config.web.VIEW_OTA;
    if (!FFS.exists(update) && !FFS.exists(update + ".gz")) {
        req->send(200, "text/html", UPDATE_HTML);
    } else {
        req->send(FFS, Config.web.VIEW_OTA);
    }
}

void onUpdateHelper(AsyncWebServerRequest *req) {
    log_param(req);
    if (req->hasParam("reset", true)) {
        log_msg(req, "reset");
        ota_updation_reset();
        req->send(200, "text/plain", "OTA Updation reset done");
    } else if (req->hasParam("size", true)) {
        String size = req->getParam("size", true)->value();
        log_msg(req, ("size: " + size).c_str());
        // erase OTA target partition for preparing
        if (!ota_updation_begin(size.toInt())) {
            req->send(400, "text/plain", ota_updation_error());
        } else {
            req->send(200, "text/plain", "OTA Updation ready for upload");
        }
    } else {
        const char *error = ota_updation_error();
        if (error) return req->send(400, "text/plain", error);
        req->send(200, "text/plain", "OTA Updation success - reboot");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
}

void onUpdatePost(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        if (!ota_updation_begin()) {
            AsyncWebServerResponse *res = request->beginResponse(
                400, "text/plain", ota_updation_error());
            res->addHeader("Connection", "close");
            return request->send(res);
        }
        printf("Updating file: %s\n", filename.c_str());
        led_blink(0, 50, 3);
    }
    if (!ota_updation_error()) {
        led_on();
        ota_updation_write(data, len);
        led_off();
    }
    if (final) {
        if (!ota_updation_end()) {
            request->send(400, "text/plain", ota_updation_error());
        } else {
            printf("Updation success: %s\n", format_size(index + len));
        }
    }
}

void onConfig(AsyncWebServerRequest *req) {
    log_msg(req);
    if (req->hasParam("json", true)) {
        if (!config_loads(req->getParam("json", true)->value().c_str())) {
            req->send(500, "text/plain", "Load config from JSON failed");
        } else {
            req->send(200);
        }
    } else {
        char *json = config_dumps();
        if (!json) {
            req->send(500, "text/plain", "Dump configs into JSON failed");
        } else {
            req->send(200, "application/json", json);
            free(json);
        }
    }
}

void onEdit(AsyncWebServerRequest *req) {
    static const char * buildTime = __DATE__ " " __TIME__ " GMT";
    log_msg(req);
    if (req->hasParam("list")) { // listdir
        String path = req->getParam("list")->value();
        if (!path.startsWith("/")) path = "/" + path;
        File root = FFS.open(path);
        if (!root) {
            req->send(404, "text/plain", path + " dir does not exists");
        } else if (!root.isDirectory()) {
            req->send(400, "text/plain", "No file entries under " + path);
        } else {
            root.close();
            char *json = FFS.list(path.c_str());
            req->send(200, "application/json", json ? json : "failed");
            if (json) free(json);
        }
    } else if (req->hasParam("path")) { // serve static files for editor
        String path = req->getParam("path")->value();
        if (!path.startsWith("/")) path = "/" + path;
        File file = FFS.open(path);
        if (!file) {
            req->send(404, "text/plain", path + " file does not exists");
        } else if (file.isDirectory()) {
            req->send(400, "text/plain", "Cannot download dir " + path);
        } else {
            req->send(file, path, String(), req->hasParam("download"));
        }
    } else if (req->header("If-Modified-Since") != buildTime) {
        AsyncWebServerResponse *res = \
            req->beginResponse(FFS, Config.web.VIEW_EDIT);
        res->addHeader("Content-Encoding", "gzip");
        res->addHeader("Last-Modified", buildTime);
        req->send(res);
    } else {
        req->send(304); // editor page not changed
    }
}

void onCreate(AsyncWebServerRequest *req) {
    // handle file|dir create
    log_msg(req);
    if (!req->hasParam("path")) {
        return req->send(400, "text/plain", "No filename specified.");
    }
    String
        path = req->getParam("path")->value(),
        type = req->hasParam("type") ? \
               req->getParam("type")->value() : "file";
    if (type == "file") {
        if (FFS.exists(path)) {
            return req->send(403, "text/plain", "File already exists.");
        }
        File file = FFS.open(path, "w");
        if (!file) {
            return req->send(500, "text/plain", "Create failed.");
        }
    } else if (type == "folder") {
        File dir = FFS.open(path);
        if (dir.isDirectory()) {
            dir.close();
            return req->send(403, "text/plain", "Dir already exists.");
        } else if (!FFS.mkdir(path)) {
            return req->send(500, "text/plain", "Create failed.");
        }
    }
    req->send(200);
}

void onDelete(AsyncWebServerRequest *req) {
    // handle file|dir delete
    log_msg(req);
    if (!req->hasParam("path"))
        return req->send(400, "text/plain", "No path specified");
    String path = req->getParam("path")->value();
    if (!FFS.exists(path))
        return req->send(403, "text/plain", "File/dir does not exist");
    if (!FFS.remove(path)) {
        req->send(500, "text/plain", "Delete file/dir failed");
    } else if (req->hasParam("from")) {
        req->redirect(req->getParam("from")->value());
    } else {
        req->send(200);
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
        led_on();
        file.write(data, len);
        ESP_LOGI(TAG, "\rProgress: %s", format_size(index));
        led_off();
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

void onError(AsyncWebServerRequest *req) {
    if (req->method() == HTTP_OPTIONS) return req->send(200);
    return req->send(404, "text/html", ERROR_HTML);
}

void onErrorFileManager(AsyncWebServerRequest *req) {
    if (req->method() == HTTP_OPTIONS) return req->send(200);
    String path = req->url(), fman = Config.web.VIEW_FILE;
    File file = FFS.open(path);
    if ((!path.endsWith("/") && (!file || !file.isDirectory())) ||
        (!FFS.exists(fman) && !FFS.exists(fman + ".gz")))
    {
        return req->send(404, "text/html", ERROR_HTML);
    }
    log_msg(req, " is directory, goto file manager.");
    req->send(FFS, fman, "text/html", false,
        [&path](const String& var){
            if (var == "ROOT") return path;
            if (var == "FILELIST") return String(FFS.list(path.c_str()));
            return var;
        }
    );
}

/******************************************************************************
 * WebSocket message parser and callbacks
 */

void handle_websocket_message(AsyncWebSocketClient *client, char *data) {
    char *ret = console_handle_rpc(data);
    if (ret) {
        client->text(ret);
        free(ret);
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
 * A: num=0, final=false, opcode=message_opcode,  index=0, len=8,  size=4
 * B: num=0, final=false, opcode=message_opcode,  index=4, len=8,  size=4
 * C: num=1, final=false, opcode=WS_CONTINUATION, index=0, len=9,  size=9
 * D: num=2, final=true,  opcode=WS_CONTINUATION, index=0, len=10, size=3
 * E: num=2, final=true,  opcode=WS_CONTINUATION, index=3, len=10, size=7
 */
void onWebSocketData(
    AsyncWebSocketClient *client, AwsFrameInfo *info, char *data, size_t size)
{
    static char * msg = NULL;
    static uint32_t wsid = -1;
    static size_t idx = 0, buflen = 0;
    uint32_t cid = client->id();
    if (wsid != cid) {
        if (wsid != -1) {
            ESP_LOGW(TAG, "ws#%d error: message buffer busy. Skip\n", cid);
            // TODO: trigger client error to stop its messaging
            return;
        }
        wsid = cid;
    }
    if (info->final && info->num == 0) {
        // Message is not fragmented so there's only one frame
        ESP_LOGI(TAG, "ws#%d message[%llu]\n", wsid, info->len);
        if (info->index == 0) {
            // We are starting this only frame
            buflen = ((info->opcode == WS_TEXT) ? 1 : 2) * info->len + 1;
            msg = (char *)malloc(buflen);
            if (msg == NULL) goto clean;
            else idx = 0;
        } else if (msg == NULL) {
            ESP_LOGW(TAG, "ws#%d error: lost first packets. Skip\n", wsid);
            goto clean;
        } else {
            // This frame is splitted into packets
            // do nothing
        }
    } else if (info->index == 0) {
        // Message is fragmented. Starting a new frame
        ESP_LOGI(TAG, "ws#%d frame%d[%llu]\n", wsid, info->num, info->len);
        size_t l = ((info->message_opcode == WS_TEXT) ? 1 : 2) * info->len;
        if (info->num == 0) {
            // Starting the first frame
            msg = (char *)malloc(l + 1);
            if (msg == NULL) goto clean;
            idx = 0; buflen = l + 1;
        } else if (msg == NULL) {
            ESP_LOGW(TAG, "ws#%d error: lost first frame. Skip\n", wsid);
            goto clean;
        } else {
            // Extend buffer for the comming frame
            char *tmp = (char *)realloc(msg, buflen + l);
            if (tmp == NULL) goto clean;
            msg = tmp; buflen += l;
        }
    } else if (msg == NULL) {
        ESP_LOGW(TAG, "ws#%d error: lost message head. Skip", wsid);
        goto clean;
    } else {
        // Message is fragmented. Current frame is splitted
        // do nothing
    }
    // Save/append packets to message buffer
    if (info->opcode == WS_TEXT) {
        idx += snprintf(msg + idx, buflen - idx, (char *)data);
    } else {
        for (size_t i = 0; i < size; i++) {
            idx += snprintf(msg + idx, buflen - idx, "%02X", data[i]);
        }
    }
    ESP_LOGD(TAG, "ws#%d >packets[%llu-%llu]\n",
             wsid, info->index, info->index + size);
    if (info->index + size == info->len) {
        // Current frame end
        if (info->final) {
            // All message buffered
            handle_websocket_message(client, msg);
            goto clean;
        } else {
            // Waiting for next frame
            // do nothing
        }
    }
clean:
    wsid = -1;
    idx = buflen = 0;
    if (msg) {
        free(msg);
        msg = NULL;
    }
    return;
}

void onWebSocket(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t datalen) {
    static char header[32];
    snprintf(header, 32, "ws#%u %s:%d", client->id(),
             client->remoteIP().toString().c_str(), client->remotePort());
    switch (type) {
    case WS_EVT_CONNECT:
        ESP_LOGD(TAG, "%s connected", header);
        client->ping();
        server->cleanupClients();
        break;
    case WS_EVT_DISCONNECT:
        ESP_LOGD(TAG, "%s disconnected", header);
        break;
    case WS_EVT_ERROR:
        ESP_LOGW(TAG, "%s error(%u)", header, *((uint16_t *)arg));
        break;
    case WS_EVT_DATA:
        ESP_LOGD(TAG, "%s message(%u)", header, datalen);
        onWebSocketData(client, (AwsFrameInfo *)arg, (char *)data, datalen);
        break;
    default:;
    }
}


/******************************************************************************
 * Class public members
 */

void WebServerClass::begin() {
    if (_started) return _server.begin();
    _server.reset();
    register_statics();
    register_ws_api();
    register_ap_api();
    register_sta_api();
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    _server.begin();
    _started = true;
}

void WebServerClass::register_sta_api() {
    _server.on("/cmd", HTTP_POST, onCommand);

    _server.serveStatic("/sta", FFS, Config.web.DIR_STA)
        .setDefaultFile("index.html")
#ifndef CONFIG_DEBUG
        .setFilter(ON_STA_FILTER)
#endif
        ;
}

void WebServerClass::register_ap_api() {
    _server.on("/config", HTTP_ANY, onConfig).setFilter(ON_AP_FILTER);

    _server.on("/update", HTTP_GET, onUpdate).setFilter(ON_AP_FILTER);
    _server.on("/update", HTTP_POST, onUpdateHelper, onUpdatePost)
        .setFilter(ON_AP_FILTER);

    // Use HTTP_ANY for compatibility with HTTP_PUT/HTTP_DELETE
    _server.on("/edit", HTTP_GET, onEdit).setFilter(ON_AP_FILTER);
    _server.on("/editc", HTTP_ANY, onCreate).setFilter(ON_AP_FILTER);
    _server.on("/editd", HTTP_ANY, onDelete).setFilter(ON_AP_FILTER);
    _server.on("/editu", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
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
    _server.serveStatic("/", FFS, Config.web.DIR_ROOT)
        .setDefaultFile("index.html");
    _server.serveStatic("/assets/", FFS, Config.web.DIR_ASSET);
    _server.serveStatic("/upload/", FFS, Config.web.DIR_DATA);
    _server.onNotFound(onErrorFileManager);
    _server.onFileUpload(onUploadStrict);
}

bool WebServerClass::logging() { return log_request; }
void WebServerClass::logging(bool log_request) { log_request = log_request; }

WebServerClass WebServer;
