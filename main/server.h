/* 
 * File: server.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2019-05-27 15:29:05
 *
 * WebServerClass wraps on AsyncWebServer to provide only `begin` and `end` 
 * function for easier usage.
 */

#ifndef _SERVER_H_
#define _SERVER_H_

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void server_initialize();        // entry point (i.e. WebServer.begin)

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

    void register_http_api();
    void register_websocket();      // WebSocket messages are parsed as JSON
    void register_statics();        // host static files like HTML/JS/CSS
    bool logging();                 // get logging level
    void logging(bool);             // set logging or not
};

extern WebServerClass WebServer;

#endif // _SERVER_H_
