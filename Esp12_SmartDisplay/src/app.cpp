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
#include "weather_location.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace
{
bool weatherCrashCooldownPending = false;

void reserveRuntimeStrings()
{
  appState.gamerGame.reserve(48);
  appState.gamerSource.reserve(12);
  appState.diskLabel.reserve(4);
  appState.lastDiskLabelDrawn.reserve(4);
  appState.weatherText.reserve(12);
  appState.weatherStatus.reserve(18);
  appState.weatherUpdatedAt.reserve(6);
  appState.lastRestartIntent.reserve(24);
  appState.lastResetCheckpoint.reserve(24);
}

bool activeThemeUsesWeather()
{
  ThemeId theme = themeManagerGetActive();
  return theme == THEME_PC_MONITOR || theme == THEME_MINIMAL_CLOCK || theme == THEME_WORK_DESK;
}

unsigned long weatherRetryInterval()
{
  if (appState.hasWeather) return WEATHER_UPDATE_INTERVAL_MS;
  return appState.weatherRetryCount < WEATHER_FAST_RETRY_LIMIT ? WEATHER_FIRST_RETRY_INTERVAL_MS : WEATHER_RETRY_INTERVAL_MS;
}

bool timeReached(unsigned long now, unsigned long scheduledAt)
{
  return static_cast<long>(now - scheduledAt) >= 0;
}

bool otaMaintenanceActive(unsigned long now)
{
  return appState.otaMaintenanceMode && !timeReached(now, appState.otaMaintenanceUntil);
}

void updateOtaMaintenance(unsigned long now)
{
  if (!otaMaintenanceActive(now))
  {
    if (appState.otaMaintenanceMode)
    {
      appState.otaMaintenanceMode = false;
      appState.otaMaintenanceLastFrame = 0;
      appState.otaMaintenanceFrame = 0;
      themeDrawBase();
    }
    return;
  }

  if (appState.otaMaintenanceLastFrame == 0 ||
      now - appState.otaMaintenanceLastFrame >= OTA_MAINTENANCE_FRAME_MS)
  {
    appState.otaMaintenanceLastFrame = now;
    displayUiUpdateOtaMaintenanceProgress();
  }
}

bool hasFreshWeather(unsigned long now)
{
  return appState.hasWeather &&
    appState.lastWeatherSuccess != 0 &&
    now - appState.lastWeatherSuccess < WEATHER_UPDATE_INTERVAL_MS;
}

void scheduleWeatherForThemeStart(unsigned long now)
{
  if (hasFreshWeather(now))
  {
    appState.nextWeatherUpdate = appState.lastWeatherSuccess + WEATHER_UPDATE_INTERVAL_MS;
    return;
  }

  if (weatherCrashCooldownPending)
  {
    appState.weatherStatus = "weather_cooldown";
    appState.nextWeatherUpdate = now + WEATHER_CRASH_COOLDOWN_MS;
    weatherCrashCooldownPending = false;
    return;
  }

  appState.nextWeatherUpdate = now + WEATHER_THEME_START_DELAY_MS;
}

void recordWeatherUpdateResult(bool success, unsigned long now)
{
  if (success)
  {
    appState.weatherRetryCount = 0;
    appState.lastWeatherSuccess = now;
    appState.nextWeatherUpdate = now + WEATHER_UPDATE_INTERVAL_MS;
    appState.lastClockCheck = 0;
    return;
  }

  if (appState.weatherRetryCount < 255) appState.weatherRetryCount++;
  appState.nextWeatherUpdate = now + weatherRetryInterval();
}

void runWeatherUpdate(unsigned long now)
{
  appState.lastWeatherUpdate = now;
  resetMarkerCheckpoint("weather_update");
  recordWeatherUpdateResult(weatherUpdate(), now);
  resetMarkerCheckpoint("weather_done");
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
  weatherCrashCooldownPending = appState.lastResetCheckpoint == "weather_get";
  resetMarkerCheckpoint("boot_setup");
  weatherLocationLoad();
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
        scheduleWeatherForThemeStart(millis());
      }
      else
      {
        appState.lastWeatherUpdate = 0;
        appState.nextWeatherUpdate = 0;
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
  unsigned long now = millis();
  if (appState.otaMaintenanceMode)
  {
    updateOtaMaintenance(now);
    if (appState.otaMaintenanceMode) return;
  }
  if (appState.safeMode) return;
  if (appState.isApMode) return;
  if (!metricsHasRecentPcMetrics() && USE_FAKE_METRICS_WHEN_PC_OFFLINE) metricsUpdateFakeTargets();
  metricsAnimateFakeValues();
  bool usesWeather = activeThemeUsesWeather();
  if (usesWeather && !appState.lastThemeUsesWeather)
  {
    appState.weatherRetryCount = 0;
    scheduleWeatherForThemeStart(now);
  }
  else if (!usesWeather)
  {
    appState.nextWeatherUpdate = 0;
  }
  appState.lastThemeUsesWeather = usesWeather;

  if (usesWeather && appState.nextWeatherUpdate != 0 && timeReached(now, appState.nextWeatherUpdate))
  {
    runWeatherUpdate(now);
  }
  themeUpdateIfNeeded();
}
