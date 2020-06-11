/* 
 * File: server.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2019-05-27 15:29:05
 *
 * WebServerClass wraps on AsyncWebServer to provide only `begin` and `end`
 * function for easier usage.
 *
 * This component (AsyncServer framework & APIs) occupy about 113KB in firmware.
 *
 * API list:
 *  Name    Method  Description
 *  /ws     POST    Websocket connection point: messages are parsed as JSON
 *  /cmd    POST    Manually send in command string just like using console
 *
 * softAP only:
 *  Name    Method  Description
 *  /config GET     Get JSON string of configuration entries
 *  /config POST    Overwrite configuration options
 *  /update GET     Updation guide page
 *  /update POST    Upload compiled binary firmware to OTA flash partition
 *  /edit   ANY     Online Editor page: create/delete/edit
 */

#ifndef _SERVER_H_
#define _SERVER_H_

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void server_loop_begin();   // entry point (i.e. WebServer.begin)
void server_loop_end();

class WebServerClass {
private:
    AsyncWebServer _server  = AsyncWebServer(80);
    AsyncWebSocket _wsocket = AsyncWebSocket("/ws");
    bool _started;

public:
    WebServerClass(): _started(false) {}
    ~WebServerClass() { _started = false; end(); }

    void begin();                   // run server in LWIP thread
    void end() { _server.end(); }   // stop AsyncWebServer

    void register_sta_api();
    void register_ap_api();
    void register_ws_api();
    void register_statics();
    bool logging();
    void logging(bool);             // enable/disable http request logging
};

extern WebServerClass WebServer;

#endif // _SERVER_H_
