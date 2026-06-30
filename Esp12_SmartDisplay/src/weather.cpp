#include "weather.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <math.h>
#include <time.h>

#include "app_state.h"
#include "config.h"
#include "reset_marker.h"
#include "weather_location.h"

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
  const WeatherLocation& location = weatherLocationGet();
  String timezone = location.timezone;
  timezone.replace("/", "%2F");

  String url = "http://api.open-meteo.com/v1/forecast?latitude=";
  url += String(location.latitude, 4);
  url += "&longitude="; url += String(location.longitude, 4);
  url += "&current=temperature_2m,relative_humidity_2m,weather_code";
  url += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
  url += "&forecast_days=3";
  url += "&timezone="; url += timezone;
  return url;
}

static String formatWeatherUpdatedAt()
{
  time_t now = time(nullptr);
  if (now < 100000) return "--:--";
  struct tm* localTime = localtime(&now);
  if (localTime == nullptr) return "--:--";

  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", localTime->tm_hour, localTime->tm_min);
  return String(buffer);
}

static int roundedJsonInt(JsonVariantConst value, int fallback)
{
  if (value.isNull()) return fallback;
  float numeric = value.as<float>();
  if (isnan(numeric) || isinf(numeric)) return fallback;
  return static_cast<int>(round(numeric));
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
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(5000);
  http.useHTTP10(true);
  resetMarkerCheckpoint("weather_http_begin");
  if (!http.begin(client, buildWeatherUrl()))
  {
    appState.weatherStatus = "begin_failed";
    return false;
  }

  resetMarkerCheckpoint("weather_get");
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    http.end();
    appState.weatherStatus = String("http_") + String(httpCode);
    return false;
  }

  resetMarkerCheckpoint("weather_parse");
  StaticJsonDocument<1536> doc;
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error)
  {
    appState.weatherStatus = String("json_") + error.c_str();
    return false;
  }

  JsonVariantConst currentTemp = doc["current"]["temperature_2m"];
  JsonVariantConst currentCode = doc["current"]["weather_code"];
  JsonVariantConst todayLow = doc["daily"]["temperature_2m_min"][0];
  if (currentTemp.isNull() || currentCode.isNull() || todayLow.isNull())
  {
    appState.weatherStatus = "json_missing";
    return false;
  }

  resetMarkerCheckpoint("weather_apply");
  appState.weatherTemp = currentTemp.as<float>();
  appState.weatherTodayLow = roundedJsonInt(todayLow, -1);
  appState.weatherHumidity = doc["current"]["relative_humidity_2m"] | -1;
  appState.weatherCode = currentCode.as<int>();
  appState.weatherText = weatherCodeToText(appState.weatherCode);
  appState.weatherUpdatedAt = formatWeatherUpdatedAt();
  for (uint8_t i = 0; i < 2; i++)
  {
    JsonVariantConst high = doc["daily"]["temperature_2m_max"][i + 1];
    JsonVariantConst low = doc["daily"]["temperature_2m_min"][i + 1];
    JsonVariantConst code = doc["daily"]["weather_code"][i + 1];
    appState.weatherForecast[i].high = roundedJsonInt(high, -1);
    appState.weatherForecast[i].low = roundedJsonInt(low, -1);
    appState.weatherForecast[i].code = code.isNull() ? -1 : code.as<int>();
    appState.weatherForecast[i].valid = !high.isNull() && !low.isNull() && !code.isNull();
  }
  appState.weatherStatus = "ok";
  appState.hasWeather = true;
  return true;
}
