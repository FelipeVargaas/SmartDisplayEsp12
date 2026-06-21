#include "app.h"

#include <ESP8266WiFi.h>
#include <time.h>

#include "app_state.h"
#include "config.h"
#include "display_ui.h"
#include "metrics.h"
#include "weather.h"
#include "web_server.h"
#include "wifi_manager.h"

void appSetup()
{
  Serial.begin(115200);
  delay(300);
  randomSeed(ESP.getCycleCount());
  displayUiInit();
  displayUiInitSprites();
  displayUiDrawBootScreen();
  if (!wifiConnect()) wifiStartApMode();
  webServerSetup();
  if (!appState.isApMode)
  {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
    displayUiDrawDashboardBase();
    weatherUpdate();
    appState.lastWeatherUpdate = millis();
    displayUiUpdateHeaderIfNeeded();
  }
  Serial.println(); Serial.println("=====================================");
  Serial.println("PC Monitor + Sprite + OTA iniciado"); Serial.println("=====================================");
  if (appState.isApMode)
  {
    Serial.println("Modo: AP Setup"); Serial.print("Wi-Fi: "); Serial.println(AP_SSID);
    Serial.print("Senha: "); Serial.println(AP_PASS); Serial.println("Pagina: http://192.168.4.1/"); Serial.println("OTA:    http://192.168.4.1/update");
  }
  else
  {
    Serial.println("Modo: Wi-Fi conectado"); Serial.print("SSID: "); Serial.println(WiFi.SSID());
    Serial.print("IP: "); Serial.println(WiFi.localIP()); Serial.print("Pagina: http://"); Serial.print(WiFi.localIP()); Serial.println("/");
    Serial.print("OTA:    http://"); Serial.print(WiFi.localIP()); Serial.println("/update");
  }
  Serial.print("Flash real: "); Serial.println(ESP.getFlashChipRealSize());
  Serial.print("Heap livre: "); Serial.println(ESP.getFreeHeap());
  Serial.print("Metric sprite: "); Serial.println(appState.metricSpriteReady ? "OK" : "FALHOU");
}

void appLoop()
{
  webServerHandleClient();
  if (appState.isApMode) return;
  unsigned long now = millis();
  if (!metricsHasRecentPcMetrics() && USE_FAKE_METRICS_WHEN_PC_OFFLINE) metricsUpdateFakeTargets();
  metricsAnimateFakeValues();
  metricsUpdateDisplayIfNeeded();
  if (now - appState.lastClockCheck >= CLOCK_CHECK_INTERVAL_MS) { appState.lastClockCheck = now; displayUiUpdateHeaderIfNeeded(); }
  if (now - appState.lastFooterUpdate >= FOOTER_UPDATE_INTERVAL_MS) { appState.lastFooterUpdate = now; displayUiDrawFooter(); }
  if (now - appState.lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL_MS)
  {
    appState.lastWeatherUpdate = now;
    if (weatherUpdate()) displayUiUpdateHeaderIfNeeded();
  }
}
