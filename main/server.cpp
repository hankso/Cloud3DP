/* 
 * File: server.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2019-05-27 15:29:05
 *
 * STA API list:
 *  Name    Method  Description
 *  /ws     POST    websocket connection point
 *  /temp   GET     return json string containing temprature values
 *  /cmd    POST    TODO
 *
 * AP API list:
 *  Name    Method  Description
 *  /config GET     TODO
 *  /config POST    TODO
 *  /update GET     Updation guide page
 *  /update POST    upload compiled binary firmware to OTA flash partition
 *  /list   GET     do SPIFFS listdir with parameter `?dir=/pathname`
 *  /edit   GET     Online editor page
 *  /edit   POST    upload file to SPIFFS flash partition
 *  /edit   PUT     create file/dir
 *  /edit   DELETE  delete file/dir
 */

#include "server.h"
#include "gpio.h"
#include "config.h"
#include "globals.h"

#include <FS.h>
// #include <FFat.h>
// #define FFS FFat
#include <SPIFFS.h>  // TODO
#define FFS SPIFFS

#include <Update.h>  // TODO

void server_initialize() { WebServer.begin(); }

static const char
*TAG = "Server", *WS = "WebSocket",
*ERROR_HTML =
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
*CONFIG_HTML =
    "<html>"
    "<head>"
        "<title>Configuration</title>"
    "</head>"
    "<body>"
        "TODO"
    "</body>"
    "</html>",
*UPDATE_HTML =
    "<html>"
    "<head>"
        "<title>OTA Updation</title>"
    "</head>"
    "<body>"
        "<form action='/update' method='post' enctype='multipart/form-data'>"
            "<input type='file' name='update'>"
            "<input type='submit' value='Update'>"
        "</form>"
    "<body>"
    "</html>";

static bool log_request = true;

inline String jsonify_dir(File dir) {
    String path, type, msg = "";
    File file;
    while (file = dir.openNextFile()) {
        path = file.name();
        path = path.substring(path.lastIndexOf('/') + 1);
        type = file.isDirectory() ? "dir" : "file";
        msg += 
            String(msg.length() ? "," : "") + 
            "{"
                "\"name\":\"" + path + "\","
                "\"type\":\"" + type + "\","
                "\"time\":" + String(file.getLastWrite()) + ""
            "}";
    }
    file.close();
    return "[" + msg + "]";
}

void log_msg(AsyncWebServerRequest *req, const char *msg = "") {
    if (!log_request) return;
    ESP_LOGD(TAG, "%4s %s%s", req->methodToString(), req->url().c_str(), msg);
}

/******************************************************************************
 * HTTP & static files API
 */

void onUpdate(AsyncWebServerRequest *request) {
    log_msg(request);
    request->send(200, "text/html", UPDATE_HTML);
}

void onUpdatePost(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        log_msg(request);
        ESP_LOGW(TAG, "Updating file: %s\n", filename.c_str());
        // Update.runAsync(true);
        if (!Update.begin()) {
            ESP_LOGE(TAG, "Update error: %s", Update.errorString());
            return;
        }
        Update.onProgress([](uint32_t progress, size_t size){
            float rate = (float)progress / size * 100;
            progress /= 1024; size /= 1024;
            ESP_LOGI(TAG, "\rProgress: %.2f%% %d/%d KB", rate, progress, size);
        });
        LIGHTBLK(3);
    }
    LIGHTON();
    if (!Update.hasError() && Update.write(data, len) != len) {
        ESP_LOGE(TAG, "Update error: %s", Update.errorString());
    }
    if (final) {
        ESP_LOGI(TAG, "");
        if (Update.end(true)) {
            ESP_LOGW(TAG, "Update success: %.2f KB\n", (index + len) / 1024.0);
        } else {
            ESP_LOGE(TAG, "Update error: %s", Update.errorString());
        }
    }
    LIGHTOFF();
}

void onUpdateDone(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain", Update.hasError() ? "FAIL" : "OK. Rebooting...");
    response->addHeader("Connection", "close");
    request->send(response);
    if (!Update.hasError()) ESP.restart();
}

void onList(AsyncWebServerRequest *request) {
    log_msg(request);
    String path = request->hasParam("path") ? \
                  request->getParam("path")->value() : "/";
    if (!path.startsWith("/")) path = '/' + path;
    File root = FFS.open(path);
    if (!root) {
        request->send(404, "text/plain", "Dir does not exists.");
    } else if (!root.isDirectory()) {
        request->send(400, "text/plain", "No more files under a file.");
        // request->redirect(path);
    } else {
        request->send(200, "application/json", jsonify_dir(root));
    }
    root.close();
}

void onEdit(AsyncWebServerRequest *request) {
    log_msg(request);
    if (!request->hasParam("path")) {
        return request->send(400, "text/plain", "No filename specified.");
    }
    String path = request->getParam("path")->value();
    request->send(FFS, Config.web.VIEW_EDIT, String(), false,
        [&path](const String& var){
            if (var == "TITLE") return path;
        }
    );
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
        if (file) {
            file.close();
        } else {
            return request->send(500, "text/plain", "Create failed.");
        }
    } else if (type == "dir") {
        File dir = FFS.open(path);
        if (dir.isDirectory()) {
            dir.close();
            return request->send(403, "text/plain", "Dir already exists.");
        }
        if (!FFS.mkdir(path)) {
            return request->send(500, "text/plain", "Create failed.");
        }
    }
    request->send(200);
}

void onDelete(AsyncWebServerRequest *request) {
    // handle file|dir delete
    log_msg(request);
    if (!request->hasParam("path")) {
        return request->send(400, "text/plain", "No path specified.");
    }
    String path = request->getParam("path")->value();
    File f = FFS.open(path);
    if (!f) return request->send(403, "text/plain", "File/dir does not exist");
    bool dir = f.isDirectory(); f.close();
    if (dir) {
        if (!FFS.rmdir(path)) {
            return request->send(500, "text/plain", "Delete path failed.");
        }
    } else {
        if (!FFS.remove(path)) {
            return request->send(500, "text/plain", "Delete file failed.");
        }
    }
}
    
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // handle file upload
    LIGHTON();
    static File file;
    if (!index) {
        log_msg(request);
        ESP_LOGW(TAG, "Uploading file: %s\n", filename.c_str());
        if (!filename.startsWith("/")) {
            filename = Config.web.DIR_DATA + filename;
        }
        if (FFS.exists(filename) && !request->hasParam("overwrite")) {
            LIGHTOFF();
            return request->send(403, "text/plain", "File already exists.");
        }
        file = FFS.open(filename, "w");
    }
    file.write(data, len);
    ESP_LOGI(TAG, "\rProgress: %d KB", index / 1024.0);
    if (final) {
        if (file) {
            file.flush();
            file.close();
            ESP_LOGW(TAG, "Update success: %.2f KB\n", (index + len) / 1024.0);
        } else {
            request->send(403, "text/plain", "Upload failed.");
        }
    }
    LIGHTOFF();
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
    String path = request->url();
    File file = FFS.open(path);
    if (!file || !file.isDirectory()) {
        log_msg(request, " 404");
        file.close();
        return request->send(404, "text/html", ERROR_HTML);
    } else {
        log_msg(request, " is directory, goto file manager.");
    }
    request->send(FFS, Config.web.VIEW_FILE, String(), false,
        [&path](const String& var){
            if (var == "TITLE") return path;
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
    const char *host = client->remoteIP().toString().c_str();
    uint16_t port = client->remotePort();
    uint32_t cid = client->id();
    switch (type) {
    case WS_EVT_CONNECT:
        ESP_LOGV(WS, "ws#%u %s:%d connected", cid, host, port);
        client->ping();
        // server.cleanupClients();
        break;
    case WS_EVT_DISCONNECT:
        ESP_LOGV(WS, "ws#%u %s:%d disconnected", cid, host, port);
        break;
    case WS_EVT_ERROR:
        ESP_LOGI(WS, "ws#%u %s:%d error(%u) %s",
                 cid, host, port, *((uint16_t *)arg), (char *)data);
        break;
    case WS_EVT_DATA:
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        static char * msg = NULL; static uint64_t idx = 0, buflen;
        
        if (info->final && info->num == 0) {
            // Message is not fragmented so there's only one frame
            ESP_LOGD(WS, "ws#%u %s:%d message[%llu]", cid, host, port, info->len);
            if (info->index == 0) {
                // We are starting this only frame
                buflen = ((info->opcode == WS_TEXT) ? 1: 2) * info->len;
                msg = (char *)malloc(buflen + 1);
                if (msg == NULL) break;
                else idx = 0;
            } else {
                // This frame is splitted into packets
                // do nothing
            }
        } else if (info->index == 0) {
            // Message is fragmented. Starting a new frame
            ESP_LOGD(WS, "ws#%u %s:%d frame%d[%llu]", cid, host, port, info->num, info->len);
            buflen = ((info->message_opcode == WS_TEXT) ? 1 : 2) * info->len;
            if (info->num == 0) {
                // Starting the first frame
                msg = (char *)malloc(buflen + 1);
                if (msg == NULL) break;
                else idx = 0;
            } else {
                // Extend buffer for the comming frame
                char *tmp = (char *)realloc(msg, sizeof(msg) + buflen);
                if (tmp == NULL) break;
                else msg = tmp;
            }
        } else {
            // Message is fragmented. Current frame is splitted
            // do nothing
        }

        // Save/append packets to message buffer
        if (info->opcode == WS_TEXT) {
            idx += sprintf(msg + idx, (char *)data);
        } else {
            for (size_t i = 0; i < datalen; i++) {
                idx += sprintf(msg + idx, "%02x", data[i]);
            }
        }
        ESP_LOGV(WS, "ws#%u %s:%d *packets[%llu-%llu]", cid, host, port, info->index, info->index + datalen);

        if (info->index + datalen == info->len) {
            // Current frame end
            if (info->final) {
                // All message buffered
                handle_websocket_message(client, msg, info->message_opcode);
                free(msg); idx = 0;
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
void onWebSocketData(AwsFrameInfo *info, char *data, size_t datalen) {

}


/******************************************************************************
 * Class public members
 */

void WebServerClass::begin() {
    if (_started) return _server.begin();
    FFS.begin();
    _server.reset();
    register_http_api();
    register_websocket();
    register_statics();
    DefaultHeaders::Instance()
        .addHeader("Access-Control-Allow-Origin", "*");
    _server.begin();
    _started = true;
}

#include "max6675.h"
inline void max6675_value_wrapper(AsyncWebServerRequest *request) {
    // Testing function. May be deleted in the future.
    LIGHTON();
    log_msg(request);
    static char values[50]; max6675_json(values);
    request->send(200, "application/json", String(values));
    LIGHTOFF();
}

void WebServerClass::register_http_api() {
    _server.on("/temp",   HTTP_GET,  max6675_value_wrapper);

    _server.on("/update", HTTP_GET,  onUpdate);
    _server.on("/update", HTTP_POST, onUpdateDone, onUpdatePost);
    
    _server.on("/list",   HTTP_GET,  onList);
    _server.on("/edit",   HTTP_GET,  onEdit);
    _server.on("/edit",   HTTP_ANY,  onCreate);  // Fix::HTTP_PUT
    _server.on("/edit",   HTTP_ANY,  onDelete);  // Fix::HTTP_DELETE
    _server.on("/edit",   HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Uploaded");
    }, onUpload);
}

void WebServerClass::register_websocket() {
    _wsocket.onEvent(onWebSocket);
    _wsocket.setAuthentication(Config.web.WS_NAME, Config.web.WS_PASS);
    _server.addHandler(&_wsocket);
}

void WebServerClass::register_statics() {
    // _server.rewrite("/", "index.html");

    _server.rewrite("/index.html", "/ap/index.html").setFilter(ON_AP_FILTER);
    _server.serveStatic("/ap/", FFS, Config.web.DIR_AP)
        .setDefaultFile("index.html")
        .setCacheControl("max-age=3600")
        .setAuthentication(Config.web.HTTP_NAME, Config.web.HTTP_PASS)
        .setFilter(ON_AP_FILTER);

    _server.serveStatic("/", FFS, Config.web.DIR_STA)
        .setDefaultFile("index.html")
        .setFilter(ON_STA_FILTER);
    _server.onNotFound(onError);
    _server.onFileUpload(onUploadStrict);
}

bool WebServerClass::logging() { return log_request; }
void WebServerClass::logging(bool log_request) { log_request = log_request; }

WebServerClass WebServer;
