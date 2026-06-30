#pragma once

#include <Arduino.h>

struct WeatherLocation
{
  String label;
  float latitude;
  float longitude;
  String timezone;
};

void weatherLocationLoad();
bool weatherLocationSave(const String& label, float latitude, float longitude, const String& timezone);
const WeatherLocation& weatherLocationGet();
String weatherLocationCityName();
