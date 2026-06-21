#pragma once

#include <ESP8266WebServer.h>

ESP8266WebServer& webServerGet();
void webServerSetup();
void webServerHandleClient();
