#include "ota_update.h"

#include <ESP8266HTTPUpdateServer.h>

#include "config.h"
#include "web_server.h"

void otaUpdateSetup()
{
  static ESP8266HTTPUpdateServer httpUpdater;
  httpUpdater.setup(&webServerGet(), "/update", OTA_USER, OTA_PASS);
}
