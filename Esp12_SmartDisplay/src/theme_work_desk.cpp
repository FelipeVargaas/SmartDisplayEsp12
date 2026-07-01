#include <math.h>
#include <string.h>
#include <time.h>

#include <TFT_eSPI.h>

#include "app_state.h"
#include "config.h"
#include "numeric_clock_font.h"
#include "smooth_clock_font.h"
#include "theme.h"
#include "theme_work_desk.h"
#include "weather_location.h"

namespace
{
const uint16_t COLOR_BG = TFT_BLACK;
const uint16_t COLOR_CARD = 0x1082;
const uint16_t COLOR_CARD_ALT = 0x18C3;
const uint16_t COLOR_BORDER = 0x3186;
const uint16_t COLOR_TEXT = 0xEF7D;
const uint16_t COLOR_MUTED = 0x8410;
const uint16_t COLOR_SOFT = 0xBDF7;
const uint16_t COLOR_CYAN = 0x55DF;
const uint16_t COLOR_SUN = 0xFDA0;
const uint16_t COLOR_RAIN = 0x4DDF;
const uint16_t COLOR_STORM = 0xFEA0;
const unsigned long NOTIFICATION_MIN_VISIBLE_MS = 1000;
const unsigned long NOTIFICATION_MAX_VISIBLE_MS = 15000;

struct WorkDeskNotification
{
  char appName[24];
  char sender[36];
  char title[64];
  char timeText[8];
  uint16_t accent;
  unsigned long startedAt;
  unsigned long visibleMs;
  bool drawn;
  bool active;
};

String lastClockText;
String lastSecondsText;
String lastDateText;
String lastWeatherState;
unsigned long lastWorkDeskCheck = 0;
WorkDeskNotification notification = {};

bool workDeskGetTimeInfo(struct tm& timeInfo)
{
  time_t now = time(nullptr);
  if (now < 100000) return false;
  struct tm* localTime = localtime(&now);
  if (localTime == nullptr) return false;
  timeInfo = *localTime;
  return true;
}

String workDeskFormatTime(const struct tm& timeInfo)
{
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
  return String(buffer);
}

String workDeskFormatSeconds(const struct tm& timeInfo)
{
  char buffer[3];
  snprintf(buffer, sizeof(buffer), "%02d", timeInfo.tm_sec);
  return String(buffer);
}

String workDeskFormatDate(const struct tm& timeInfo)
{
  static const char* const weekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%s  %02d/%02d", weekdays[timeInfo.tm_wday], timeInfo.tm_mday, timeInfo.tm_mon + 1);
  return String(buffer);
}

String workDeskFormatForecastDate(uint8_t daysAhead, const char* fallback)
{
  time_t now = time(nullptr);
  if (now < 100000) return String(fallback);
  now += static_cast<time_t>(daysAhead) * 86400;
  struct tm* localTime = localtime(&now);
  if (localTime == nullptr) return String(fallback);

  char buffer[6];
  uint8_t day = localTime->tm_mday >= 1 && localTime->tm_mday <= 31 ? localTime->tm_mday : 0;
  uint8_t month = localTime->tm_mon >= 0 && localTime->tm_mon < 12 ? localTime->tm_mon + 1 : 0;
  snprintf(buffer, sizeof(buffer), "%02u/%02u", day, month);
  return String(buffer);
}

void drawTextClipped(const String& text, int x, int y, int maxWidth, int font, int size, uint16_t color, uint16_t bg)
{
  smoothClockFontUnload(appState.tft);
  appState.tft.setTextFont(font);
  appState.tft.setTextSize(size);
  appState.tft.setTextColor(color, bg);
  String clipped = text;
  while (clipped.length() > 0 && appState.tft.textWidth(clipped) > maxWidth) clipped.remove(clipped.length() - 1);
  appState.tft.setCursor(x, y);
  appState.tft.print(clipped);
}

void copyBounded(char* target, size_t targetSize, const char* value)
{
  if (targetSize == 0) return;
  if (value == nullptr) value = "";

  size_t start = 0;
  while (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n') start++;

  size_t len = 0;
  while (len < targetSize - 1 && value[start + len] != '\0') len++;
  while (len > 0)
  {
    char c = value[start + len - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    len--;
  }

  memcpy(target, value + start, len);
  target[len] = '\0';
}

uint8_t hexNibble(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  return 0;
}

uint16_t parseAccentColor(const char* accent)
{
  if (accent == nullptr) return COLOR_CYAN;
  if (accent[0] == '#') accent++;
  for (uint8_t i = 0; i < 6; i++)
  {
    char c = accent[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return COLOR_CYAN;
  }

  uint8_t r = (hexNibble(accent[0]) << 4) | hexNibble(accent[1]);
  uint8_t g = (hexNibble(accent[2]) << 4) | hexNibble(accent[3]);
  uint8_t b = (hexNibble(accent[4]) << 4) | hexNibble(accent[5]);
  return appState.tft.color565(r, g, b);
}

bool isRainCode(int code)
{
  return (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
}

void drawWeatherIcon(int x, int y, int code, uint8_t size)
{
  uint8_t half = size / 2;
  if (code == 0)
  {
    uint8_t radius = size / 4;
    appState.tft.fillCircle(x + half, y + half, radius, COLOR_SUN);
    appState.tft.drawCircle(x + half, y + half, radius + 1, COLOR_SUN);
    appState.tft.drawLine(x + half, y + 1, x + half, y + 5, COLOR_SUN);
    appState.tft.drawLine(x + half, y + size - 5, x + half, y + size - 1, COLOR_SUN);
    appState.tft.drawLine(x + 1, y + half, x + 5, y + half, COLOR_SUN);
    appState.tft.drawLine(x + size - 5, y + half, x + size - 1, y + half, COLOR_SUN);
    appState.tft.drawLine(x + 5, y + 5, x + 8, y + 8, COLOR_SUN);
    appState.tft.drawLine(x + size - 8, y + size - 8, x + size - 5, y + size - 5, COLOR_SUN);
    appState.tft.drawLine(x + 5, y + size - 5, x + 8, y + size - 8, COLOR_SUN);
    appState.tft.drawLine(x + size - 8, y + 8, x + size - 5, y + 5, COLOR_SUN);
    return;
  }

  uint16_t cloud = code >= 95 ? COLOR_STORM : COLOR_SOFT;
  appState.tft.fillCircle(x + size / 3, y + half + 2, size / 5, COLOR_MUTED);
  appState.tft.fillCircle(x + half, y + half - 1, size / 4, cloud);
  appState.tft.fillCircle(x + size * 2 / 3, y + half + 2, size / 5, cloud);
  appState.tft.fillRoundRect(x + size / 5, y + half + 3, size * 3 / 5, size / 4, 3, cloud);

  if (isRainCode(code))
  {
    appState.tft.drawLine(x + size / 3, y + size - 6, x + size / 3 - 2, y + size - 1, COLOR_RAIN);
    appState.tft.drawLine(x + half, y + size - 6, x + half - 2, y + size - 1, COLOR_RAIN);
    appState.tft.drawLine(x + size * 2 / 3, y + size - 6, x + size * 2 / 3 - 2, y + size - 1, COLOR_RAIN);
  }
  else if (code >= 95)
  {
    appState.tft.fillTriangle(x + half, y + size - 8, x + half - 5, y + size - 1, x + half, y + size - 3, COLOR_SUN);
    appState.tft.fillTriangle(x + half, y + size - 3, x + half + 5, y + size - 10, x + half + 1, y + size - 8, COLOR_SUN);
  }
}

void drawHeader()
{
  appState.tft.fillRect(0, 0, DISPLAY_WIDTH, 56, COLOR_BG);
  drawTextClipped(weatherLocationCityName(), 10, 10, 150, 2, 1, COLOR_TEXT, COLOR_BG);

  String updated = appState.hasWeather ? String("Atual ") + appState.weatherUpdatedAt : String("Syncing weather");
  drawTextClipped(updated, 10, 29, 128, 1, 1, COLOR_MUTED, COLOR_BG);

  int iconX = 176;
  int iconY = 8;
  drawWeatherIcon(iconX, iconY, appState.hasWeather ? appState.weatherCode : -1, 28);

  String condition = appState.hasWeather ? appState.weatherText : "Weather";
  int pillW = 58;
  appState.tft.fillRoundRect(170, 38, pillW, 14, 7, COLOR_CARD_ALT);
  drawTextClipped(condition, 176, 41, pillW - 10, 1, 1, COLOR_CYAN, COLOR_CARD_ALT);
}

void drawClock(const String& clockText, const String& secondsText, const String& dateText)
{
  smoothClockFontUnload(appState.tft);
  appState.tft.fillRect(0, 56, DISPLAY_WIDTH, 90, COLOR_BG);

  const uint8_t clockScale = 2;
  const uint8_t secondsScale = 1;
  String secondsSuffix = String(":") + secondsText;
  appState.tft.setTextFont(2);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(COLOR_MUTED, COLOR_BG);
  int dateX = (DISPLAY_WIDTH - appState.tft.textWidth(dateText)) / 2;
  if (dateX < 0) dateX = 0;
  appState.tft.setCursor(dateX, 119);
  appState.tft.print(dateText);

  bool useSmoothFont = smoothClockFontEnsureLoaded(appState.tft);
  int clockWidth = useSmoothFont ? smoothClockTextWidth(appState.tft, clockText) : numericClockTextWidth(clockText, clockScale);
  int secondsWidth = numericClockTextWidth(secondsSuffix, secondsScale);

  int groupW = clockWidth + 7 + secondsWidth;
  int clockX = (DISPLAY_WIDTH - groupW) / 2;
  if (clockX < 0) clockX = 0;

  if (useSmoothFont)
  {
    const int clockY = 61;
    int secondsY = clockY + (smoothClockTextHeight(appState.tft) - numericClockTextHeight(secondsScale)) / 2;
    smoothClockDrawText(appState.tft, clockText, clockX, clockY, COLOR_TEXT, COLOR_BG);
    numericClockDrawText(appState.tft, secondsSuffix, clockX + clockWidth + 7, secondsY, COLOR_SOFT, COLOR_BG, secondsScale);
    return;
  }

  numericClockDrawText(appState.tft, clockText, clockX, 65, COLOR_TEXT, COLOR_BG, clockScale);
  numericClockDrawText(appState.tft, secondsSuffix, clockX + clockWidth + 7, 75, COLOR_SOFT, COLOR_BG, secondsScale);
}

void drawClockSeconds(const String& clockText, const String& secondsText)
{
  const uint8_t clockScale = 2;
  const uint8_t secondsScale = 1;
  String secondsSuffix = String(":") + secondsText;
  bool useSmoothFont = smoothClockFontEnsureLoaded(appState.tft);
  int clockWidth = useSmoothFont ? smoothClockTextWidth(appState.tft, clockText) : numericClockTextWidth(clockText, clockScale);
  int secondsWidth = numericClockTextWidth(secondsSuffix, secondsScale);
  int groupW = clockWidth + 7 + secondsWidth;
  int clockX = (DISPLAY_WIDTH - groupW) / 2;
  if (clockX < 0) clockX = 0;

  int secondsX = clockX + clockWidth + 7;
  if (useSmoothFont)
  {
    const int clockY = 61;
    int secondsY = clockY + (smoothClockTextHeight(appState.tft) - numericClockTextHeight(secondsScale)) / 2;
    appState.tft.fillRect(secondsX - 2, secondsY - 2, secondsWidth + 4, numericClockTextHeight(secondsScale) + 4, COLOR_BG);
    numericClockDrawText(appState.tft, secondsSuffix, secondsX, secondsY, COLOR_SOFT, COLOR_BG, secondsScale);
    return;
  }

  appState.tft.fillRect(secondsX - 2, 73, secondsWidth + 4, numericClockTextHeight(secondsScale) + 4, COLOR_BG);
  numericClockDrawText(appState.tft, secondsSuffix, secondsX, 75, COLOR_SOFT, COLOR_BG, secondsScale);
}

void drawDegreeSymbol(int x, int y, uint16_t color)
{
  appState.tft.drawCircle(x, y, 2, color);
}

void drawCardFrame(int x, int y, int w, int h)
{
  appState.tft.fillRoundRect(x, y, w, h, 6, COLOR_CARD);
  appState.tft.drawRoundRect(x, y, w, h, 6, COLOR_BORDER);
}

void drawTemperatureValue(int x, int y, int value, uint16_t bg)
{
  if (value < -80)
  {
    drawTextClipped("--", x, y, 38, 4, 1, COLOR_TEXT, bg);
    return;
  }

  String text = String(value);
  appState.tft.setTextFont(4);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(COLOR_TEXT, bg);
  appState.tft.setCursor(x, y);
  appState.tft.print(text);
  drawDegreeSymbol(x + appState.tft.textWidth(text) + 3, y + 3, COLOR_TEXT);
}

void drawNowCard(int x, int y, int w, int h)
{
  drawCardFrame(x, y, w, h);
  drawTextClipped("Agora", x + 7, y + 7, w - 14, 1, 1, COLOR_MUTED, COLOR_CARD);
  drawWeatherIcon(x + w - 25, y + 6, appState.hasWeather ? appState.weatherCode : -1, 18);

  int temp = appState.hasWeather ? static_cast<int>(round(appState.weatherTemp)) : -100;
  drawTemperatureValue(x + 8, y + 26, temp, COLOR_CARD);

  String low = appState.hasWeather && appState.weatherTodayLow > -80 ? String(appState.weatherTodayLow) : "--";
  appState.tft.setTextFont(2);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(COLOR_SOFT, COLOR_CARD);
  appState.tft.setCursor(x + 10, y + 59);
  appState.tft.print(low);
  if (appState.hasWeather && appState.weatherTodayLow > -80) drawDegreeSymbol(x + 12 + appState.tft.textWidth(low), y + 61, COLOR_SOFT);
}

void drawForecastCard(int x, int y, int w, int h, const String& title, const WeatherForecastDay& forecast)
{
  drawCardFrame(x, y, w, h);
  drawTextClipped(title, x + 7, y + 7, w - 14, 1, 1, COLOR_MUTED, COLOR_CARD);
  drawWeatherIcon(x + w - 28, y + 22, forecast.valid ? forecast.code : -1, 22);

  drawTemperatureValue(x + 8, y + 27, forecast.valid ? forecast.high : -100, COLOR_CARD);

  String low = forecast.valid ? String(forecast.low) : "--";
  appState.tft.setTextFont(2);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(COLOR_SOFT, COLOR_CARD);
  appState.tft.setCursor(x + 10, y + 59);
  appState.tft.print(low);
  if (forecast.valid) drawDegreeSymbol(x + 12 + appState.tft.textWidth(low), y + 61, COLOR_SOFT);
}

void drawWeatherCards()
{
  appState.tft.fillRect(0, 146, DISPLAY_WIDTH, 94, COLOR_BG);
  if (!appState.hasWeather)
  {
    appState.tft.fillRoundRect(10, 158, 220, 58, 6, COLOR_CARD);
    appState.tft.drawRoundRect(10, 158, 220, 58, 6, COLOR_BORDER);
    drawTextClipped("Syncing weather", 24, 174, 180, 2, 1, COLOR_TEXT, COLOR_CARD);
    drawTextClipped(appState.weatherStatus, 24, 193, 180, 1, 1, COLOR_MUTED, COLOR_CARD);
    return;
  }

  const int cardY = 152;
  const int cardH = 78;
  const int cardW = 70;
  const int gap = 6;
  const int card0X = 8;
  drawNowCard(card0X, cardY, cardW, cardH);
  drawForecastCard(card0X + cardW + gap, cardY, cardW, cardH, workDeskFormatForecastDate(1, "Aman"), appState.weatherForecast[0]);
  drawForecastCard(card0X + (cardW + gap) * 2, cardY, cardW, cardH, workDeskFormatForecastDate(2, "Depois"), appState.weatherForecast[1]);
}

void drawNotificationCard()
{
  const int x = 22;
  const int y = 62;
  const int w = 196;
  const int h = 82;
  const uint16_t bg = 0x0861;
  const uint16_t border = 0x3A08;

  appState.tft.fillRect(0, 56, DISPLAY_WIDTH, 90, COLOR_BG);
  appState.tft.fillRoundRect(x, y, w, h, 8, bg);
  appState.tft.drawRoundRect(x, y, w, h, 8, border);
  appState.tft.fillRoundRect(x + 5, y + 8, 5, h - 16, 3, notification.accent);

  drawTextClipped(notification.appName, x + 18, y + 10, 102, 1, 1, notification.accent, bg);
  drawTextClipped(notification.timeText, x + w - 40, y + 10, 28, 1, 1, COLOR_MUTED, bg);
  drawTextClipped(notification.sender, x + 18, y + 29, w - 32, 2, 1, COLOR_TEXT, bg);
  drawTextClipped(notification.title, x + 18, y + 52, w - 32, 2, 1, COLOR_SOFT, bg);
}

void updateNotificationOverlay(unsigned long now)
{
  if (!notification.active) return;

  unsigned long elapsed = now - notification.startedAt;
  if (elapsed >= notification.visibleMs)
  {
    notification.active = false;
    notification.drawn = false;
    lastClockText = "";
    lastDateText = "";
    lastWorkDeskCheck = 0;
    return;
  }

  if (notification.drawn) return;
  drawNotificationCard();
  notification.drawn = true;
}

String buildWeatherStateKey()
{
  String key = appState.hasWeather ? "1" : "0";
  key += weatherLocationCityName();
  key += appState.weatherStatus;
  key += appState.weatherUpdatedAt;
  key += String(appState.weatherCode);
  key += String(appState.weatherTemp, 1);
  key += String(appState.weatherTodayLow);
  key += String(appState.weatherHumidity);
  key += workDeskFormatForecastDate(1, "Aman");
  key += workDeskFormatForecastDate(2, "Depois");
  for (uint8_t i = 0; i < 2; i++)
  {
    key += appState.weatherForecast[i].valid ? "v" : "x";
    key += String(appState.weatherForecast[i].code);
    key += String(appState.weatherForecast[i].high);
    key += String(appState.weatherForecast[i].low);
  }
  return key;
}
}

void themeWorkDeskUpdateIfNeeded();

void themeWorkDeskDrawBase()
{
  appState.tft.fillScreen(COLOR_BG);
  lastClockText = "";
  lastSecondsText = "";
  lastDateText = "";
  lastWeatherState = "";
  lastWorkDeskCheck = 0;
  drawHeader();
  drawWeatherCards();
  themeWorkDeskUpdateIfNeeded();
}

bool themeWorkDeskShowNotification(
  const char* appName,
  const char* sender,
  const char* title,
  const char* timeText,
  const char* accent,
  unsigned long durationMs)
{
  copyBounded(notification.appName, sizeof(notification.appName), appName);
  copyBounded(notification.sender, sizeof(notification.sender), sender);
  copyBounded(notification.title, sizeof(notification.title), title);
  copyBounded(notification.timeText, sizeof(notification.timeText), timeText);

  if (notification.appName[0] == '\0') copyBounded(notification.appName, sizeof(notification.appName), "App");
  if (notification.sender[0] == '\0') copyBounded(notification.sender, sizeof(notification.sender), "TinyDash Agent");
  if (notification.title[0] == '\0') copyBounded(notification.title, sizeof(notification.title), "Nova notificacao");
  if (notification.timeText[0] == '\0') copyBounded(notification.timeText, sizeof(notification.timeText), "--:--");

  notification.accent = parseAccentColor(accent);
  notification.startedAt = millis();
  notification.visibleMs = constrain(durationMs, NOTIFICATION_MIN_VISIBLE_MS, NOTIFICATION_MAX_VISIBLE_MS);
  notification.drawn = false;
  notification.active = true;
  return true;
}

void themeWorkDeskUpdateIfNeeded()
{
  unsigned long now = millis();
  updateNotificationOverlay(now);

  if (lastWorkDeskCheck != 0 && now - lastWorkDeskCheck < 1000) return;
  lastWorkDeskCheck = now;

  struct tm timeInfo;
  String clockText = "--:--";
  String secondsText = "--";
  String dateText = "";
  if (workDeskGetTimeInfo(timeInfo))
  {
    clockText = workDeskFormatTime(timeInfo);
    secondsText = workDeskFormatSeconds(timeInfo);
    dateText = workDeskFormatDate(timeInfo);
  }

  if (!notification.active && (clockText != lastClockText || dateText != lastDateText))
  {
    drawClock(clockText, secondsText, dateText);
    lastClockText = clockText;
    lastSecondsText = secondsText;
    lastDateText = dateText;
  }
  else if (!notification.active && secondsText != lastSecondsText)
  {
    drawClockSeconds(clockText, secondsText);
    lastSecondsText = secondsText;
  }

  String weatherState = buildWeatherStateKey();
  if (weatherState != lastWeatherState)
  {
    drawHeader();
    drawWeatherCards();
    lastWeatherState = weatherState;
  }
}
