#include "weather.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>

#include "app_state.h"
#include "config.h"
#include "reset_marker.h"

namespace
{
const int WEATHER_MIN_FREE_HEAP = 28000;
const int WEATHER_MIN_MAX_BLOCK = 20000;
}

static String weatherCodeToText(int code)
{
  if (code == 0) return "Limpo";
  if (code == 1 || code == 2 || code == 3) return "Nublado";
  if (code == 45 || code == 48) return "Neblina";
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "Chuva";
  if (code >= 71 && code <= 77) return "Neve";
  if (code >= 95) return "Tempest";
  return "Clima";
}

static String buildWeatherUrl()
{
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(WEATHER_LAT, 4);
  url += "&longitude="; url += String(WEATHER_LON, 4);
  url += "&current=temperature_2m,weather_code";
  url += "&timezone="; url += WEATHER_TIMEZONE;
  return url;
}

bool weatherUpdate()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    appState.weatherStatus = "wifi_offline";
    return false;
  }

  if (ESP.getFreeHeap() < WEATHER_MIN_FREE_HEAP || ESP.getMaxFreeBlockSize() < WEATHER_MIN_MAX_BLOCK)
  {
    appState.weatherStatus = "low_heap";
    resetMarkerCheckpoint("weather_heap_skip");
    return false;
  }

  resetMarkerCheckpoint("weather_begin");
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  https.setTimeout(5000);
  resetMarkerCheckpoint("weather_http_begin");
  if (!https.begin(client, buildWeatherUrl()))
  {
    appState.weatherStatus = "begin_failed";
    return false;
  }
  resetMarkerCheckpoint("weather_get");
  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    https.end();
    appState.weatherStatus = String("http_") + String(httpCode);
    return false;
  }

  resetMarkerCheckpoint("weather_parse");
  String payload = https.getString();
  https.end();

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    appState.weatherStatus = String("json_") + error.c_str();
    return false;
  }

  resetMarkerCheckpoint("weather_apply");
  appState.weatherTemp = doc["current"]["temperature_2m"] | 0.0f;
  appState.weatherCode = doc["current"]["weather_code"] | -1;
  appState.weatherText = weatherCodeToText(appState.weatherCode);
  appState.weatherStatus = "ok";
  appState.hasWeather = true;
  return true;
}
