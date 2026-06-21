#include "weather.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>

#include "app_state.h"
#include "config.h"

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
  if (WiFi.status() != WL_CONNECTED) return false;
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  https.setTimeout(2500);
  if (!https.begin(client, buildWeatherUrl())) return false;
  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) { https.end(); return false; }
  String payload = https.getString();
  https.end();
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, payload)) return false;
  appState.weatherTemp = doc["current"]["temperature_2m"] | 0.0f;
  appState.weatherText = weatherCodeToText(doc["current"]["weather_code"] | -1);
  appState.hasWeather = true;
  return true;
}
