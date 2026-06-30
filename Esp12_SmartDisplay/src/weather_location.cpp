#include "weather_location.h"

#include <EEPROM.h>
#include <math.h>
#include <string.h>

#include "config.h"

namespace
{
WeatherLocation currentLocation = {
  "Rio de Janeiro, Rio de Janeiro, Brasil",
  WEATHER_LAT,
  WEATHER_LON,
  "America/Sao_Paulo"
};

void writeFloat(int address, float value)
{
  byte bytes[sizeof(float)];
  memcpy(bytes, &value, sizeof(float));
  for (uint8_t i = 0; i < sizeof(float); i++) EEPROM.write(address + i, bytes[i]);
}

float readFloat(int address)
{
  byte bytes[sizeof(float)];
  for (uint8_t i = 0; i < sizeof(float); i++) bytes[i] = EEPROM.read(address + i);

  float value;
  memcpy(&value, bytes, sizeof(float));
  return value;
}

void writeFixedString(int address, int maxLength, const String& value)
{
  for (int i = 0; i < maxLength; i++)
    EEPROM.write(address + i, i < static_cast<int>(value.length()) ? value[i] : 0);
}

String readFixedString(int address, int maxLength)
{
  char buffer[WEATHER_LOCATION_MAX_LABEL + 1];
  int limit = maxLength < WEATHER_LOCATION_MAX_LABEL ? maxLength : WEATHER_LOCATION_MAX_LABEL;
  for (int i = 0; i < limit; i++) buffer[i] = static_cast<char>(EEPROM.read(address + i));
  buffer[limit] = '\0';
  return String(buffer);
}

String compactString(String value, uint8_t maxLength)
{
  value.trim();
  if (value.length() > maxLength) value = value.substring(0, maxLength);
  return value;
}

char latinFallback(uint8_t lead, uint8_t trail)
{
  if (lead == 0xC3)
  {
    switch (trail)
    {
      case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: return 'A';
      case 0x87: return 'C';
      case 0x88: case 0x89: case 0x8A: case 0x8B: return 'E';
      case 0x8C: case 0x8D: case 0x8E: case 0x8F: return 'I';
      case 0x91: return 'N';
      case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: return 'O';
      case 0x99: case 0x9A: case 0x9B: case 0x9C: return 'U';
      case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: return 'a';
      case 0xA7: return 'c';
      case 0xA8: case 0xA9: case 0xAA: case 0xAB: return 'e';
      case 0xAC: case 0xAD: case 0xAE: case 0xAF: return 'i';
      case 0xB1: return 'n';
      case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: return 'o';
      case 0xB9: case 0xBA: case 0xBB: case 0xBC: return 'u';
      default: return 0;
    }
  }

  return 0;
}

String displaySafeText(const String& value)
{
  String output;
  output.reserve(value.length());

  for (unsigned int i = 0; i < value.length(); i++)
  {
    uint8_t c = static_cast<uint8_t>(value[i]);
    if (c < 0x80)
    {
      output += static_cast<char>(c);
      continue;
    }

    if (i + 1 < value.length())
    {
      uint8_t next = static_cast<uint8_t>(value[i + 1]);
      char fallback = latinFallback(c, next);
      if (fallback != 0) output += fallback;
      i++;
    }
  }

  output.trim();
  return output;
}

bool isValidCoordinate(float latitude, float longitude)
{
  return !isnan(latitude) && !isinf(latitude) &&
         !isnan(longitude) && !isinf(longitude) &&
         latitude >= -90.0f && latitude <= 90.0f &&
         longitude >= -180.0f && longitude <= 180.0f;
}
}

void weatherLocationLoad()
{
  EEPROM.begin(EEPROM_SIZE);
  uint8_t magic = EEPROM.read(WEATHER_LOCATION_MAGIC_ADDR);

  if (magic == WEATHER_LOCATION_MAGIC)
  {
    float latitude = readFloat(WEATHER_LOCATION_LAT_ADDR);
    float longitude = readFloat(WEATHER_LOCATION_LON_ADDR);
    String label = readFixedString(WEATHER_LOCATION_LABEL_ADDR, WEATHER_LOCATION_MAX_LABEL);
    String timezone = readFixedString(WEATHER_LOCATION_TZ_ADDR, WEATHER_LOCATION_MAX_TZ);

    if (isValidCoordinate(latitude, longitude) && label.length() > 0 && timezone.length() > 0)
    {
      currentLocation.label = label;
      currentLocation.latitude = latitude;
      currentLocation.longitude = longitude;
      currentLocation.timezone = timezone;
    }
  }

  EEPROM.end();
}

bool weatherLocationSave(const String& label, float latitude, float longitude, const String& timezone)
{
  if (!isValidCoordinate(latitude, longitude))
    return false;

  String compactLabel = compactString(label, WEATHER_LOCATION_MAX_LABEL - 1);
  String compactTimezone = compactString(timezone, WEATHER_LOCATION_MAX_TZ - 1);
  if (compactLabel.length() == 0 || compactTimezone.length() == 0)
    return false;

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(WEATHER_LOCATION_MAGIC_ADDR, WEATHER_LOCATION_MAGIC);
  writeFloat(WEATHER_LOCATION_LAT_ADDR, latitude);
  writeFloat(WEATHER_LOCATION_LON_ADDR, longitude);
  writeFixedString(WEATHER_LOCATION_LABEL_ADDR, WEATHER_LOCATION_MAX_LABEL, compactLabel);
  writeFixedString(WEATHER_LOCATION_TZ_ADDR, WEATHER_LOCATION_MAX_TZ, compactTimezone);
  bool saved = EEPROM.commit();
  EEPROM.end();

  if (saved)
  {
    currentLocation.label = compactLabel;
    currentLocation.latitude = latitude;
    currentLocation.longitude = longitude;
    currentLocation.timezone = compactTimezone;
  }

  return saved;
}

const WeatherLocation& weatherLocationGet()
{
  return currentLocation;
}

String weatherLocationCityName()
{
  String city = currentLocation.label;
  int commaIndex = city.indexOf(',');
  if (commaIndex > 0) city = city.substring(0, commaIndex);
  city.trim();
  city = displaySafeText(city);
  return city.length() > 0 ? city : String("--");
}
