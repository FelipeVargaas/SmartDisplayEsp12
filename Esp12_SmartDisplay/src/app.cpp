#include "app.h"

#include <ESP8266WiFi.h>
#include <time.h>

#include "app_state.h"
#include "boot_guard.h"
#include "config.h"
#include "display_ui.h"
#include "metrics.h"
#include "reset_marker.h"
#include "theme_manager.h"
#include "theme_render.h"
#include "weather.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace
{
void reserveRuntimeStrings()
{
  appState.gamerGame.reserve(48);
  appState.gamerSource.reserve(12);
  appState.diskLabel.reserve(4);
  appState.lastDiskLabelDrawn.reserve(4);
  appState.weatherText.reserve(12);
  appState.weatherStatus.reserve(18);
  appState.lastRestartIntent.reserve(24);
  appState.lastResetCheckpoint.reserve(24);
}

bool activeThemeUsesWeather()
{
  ThemeId theme = themeManagerGetActive();
  return theme == THEME_PC_MONITOR || theme == THEME_MINIMAL_CLOCK;
}

unsigned long weatherRetryInterval()
{
  if (appState.hasWeather) return WEATHER_UPDATE_INTERVAL_MS;
  return appState.weatherRetryCount < WEATHER_FAST_RETRY_LIMIT ? WEATHER_FIRST_RETRY_INTERVAL_MS : WEATHER_RETRY_INTERVAL_MS;
}

void recordWeatherUpdateResult(bool success)
{
  if (success)
  {
    appState.weatherRetryCount = 0;
    appState.lastClockCheck = 0;
    return;
  }

  if (appState.weatherRetryCount < 255) appState.weatherRetryCount++;
}
}

void appSetup()
{
  Serial.begin(115200);
  delay(300);
  randomSeed(ESP.getCycleCount());
  reserveRuntimeStrings();
  appState.lastRestartIntent = resetMarkerConsume();
  appState.lastResetCheckpoint = resetMarkerReadCheckpoint();
  resetMarkerCheckpoint("boot_setup");
  appState.safeMode = bootGuardBegin();
  displayUiInit();
  if (!appState.safeMode) displayUiInitSprites();
  displayUiDrawBootScreen();
  themeManagerLoad();
  if (!wifiConnect()) wifiStartApMode();
  webServerSetup();
  resetMarkerCheckpoint("web_ready");
  if (!appState.isApMode)
  {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
    String ipAddress = WiFi.localIP().toString();
    if (appState.safeMode)
    {
      displayUiDrawSafeModeScreen(ipAddress);
      Serial.println("Modo seguro ativo: dashboard desativado, OTA disponivel.");
    }
    else
    {
      displayUiDrawStartupInfo(ipAddress);
      delay(900);
      themeDrawBase();
      resetMarkerCheckpoint("theme_ready");
      appState.lastThemeUsesWeather = activeThemeUsesWeather();
      if (appState.lastThemeUsesWeather)
      {
        recordWeatherUpdateResult(weatherUpdate());
        appState.lastWeatherUpdate = millis();
      }
      else
      {
        appState.lastWeatherUpdate = 0;
      }
      themeUpdateIfNeeded();
      resetMarkerCheckpoint("setup_done");
    }
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
  resetMarkerCheckpoint("loop_start");
}

void appLoop()
{
  webServerHandleClient();
  bootGuardUpdate();
  if (appState.safeMode) return;
  if (appState.isApMode) return;
  unsigned long now = millis();
  if (!metricsHasRecentPcMetrics() && USE_FAKE_METRICS_WHEN_PC_OFFLINE) metricsUpdateFakeTargets();
  metricsAnimateFakeValues();
  bool usesWeather = activeThemeUsesWeather();
  if (usesWeather && !appState.lastThemeUsesWeather)
  {
    appState.weatherRetryCount = 0;
    appState.lastWeatherUpdate = 0;
  }
  appState.lastThemeUsesWeather = usesWeather;

  unsigned long weatherInterval = weatherRetryInterval();
  if (usesWeather && (appState.lastWeatherUpdate == 0 || now - appState.lastWeatherUpdate >= weatherInterval))
  {
    appState.lastWeatherUpdate = now;
    resetMarkerCheckpoint("weather_update");
    recordWeatherUpdateResult(weatherUpdate());
    resetMarkerCheckpoint("weather_done");
  }
  themeUpdateIfNeeded();
}
