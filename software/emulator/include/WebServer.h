#pragma once

// The web server of the firmware, without a network: the page of the emulator hands every request
// of the web interface to emurequest() in hardware.cpp, which calls the handler the firmware registered.

#include <functional>
#include <map>

#include "Arduino.h"

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };

class WebServer;
extern WebServer *emuserver;

class WebServer {
 public:
  typedef std::function<void(void)> Handler;

  WebServer(int) { emuserver = this; }
  void on(const char *path, HTTPMethod method, Handler handler) { routes[path + std::string(method == HTTP_POST ? " POST" : " GET")] = handler; }
  void onNotFound(Handler handler) { notfound = handler; }
  void begin() {}
  void handleClient() {}

  void send(int code, const char *type, const String &body) { reply.code = code; reply.type = type; reply.body = body; }
  void send(int code, const char *type, const char *body) { send(code, type, String(body)); }
  void send_P(int code, const char *type, const char *body) { send(code, type, String(body)); }

  bool hasArg(const char *name) const { return args.count(name) > 0; }
  String arg(const char *name) const { auto a = args.find(name); return a == args.end() ? String() : a->second; }

  // For hardware.cpp
  struct Reply { int code = 404; String type = "text/plain"; String body = "Not found"; };
  Reply request(const char *method, const char *path, const char *query, const char *body);

 private:
  std::map<std::string, Handler> routes;
  Handler notfound;
  std::map<std::string, String> args;
  Reply reply;
};
