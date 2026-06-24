#include <math.h>
#include <time.h>

#include <TFT_eSPI.h>

#include "app_state.h"
#include "config.h"
#include "theme.h"

static String lastClockText;
static String lastSecondsText;
static String lastWeatherState;
static String lastDateText;
static unsigned long lastMinimalClockCheck = 0;

void themeMinimalClockUpdateIfNeeded();

static bool minimalClockGetTimeInfo(struct tm& timeInfo)
{
  time_t now = time(nullptr);
  if (now < 100000) return false;
  struct tm* localTime = localtime(&now);
  if (localTime == nullptr) return false;
  timeInfo = *localTime;
  return true;
}

static String minimalClockFormatTime(const struct tm& timeInfo)
{
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
  return String(buffer);
}

static String minimalClockFormatSeconds(const struct tm& timeInfo)
{
  char buffer[3];
  snprintf(buffer, sizeof(buffer), "%02d", timeInfo.tm_sec);
  return String(buffer);
}

static String minimalClockFormatDate(const struct tm& timeInfo)
{
  static const char* const weekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%s  %02d/%02d", weekdays[timeInfo.tm_wday], timeInfo.tm_mday, timeInfo.tm_mon + 1);
  return String(buffer);
}

static void minimalClockDrawCentered(const String& text, int y, int font, int size, uint16_t color)
{
  appState.tft.setTextFont(font);
  appState.tft.setTextSize(size);
  appState.tft.setTextColor(color, CLEAN_TFT_THEME.background);
  int x = (DISPLAY_WIDTH - appState.tft.textWidth(text)) / 2;
  if (x < 0) x = 0;
  appState.tft.setCursor(x, y);
  appState.tft.print(text);
}

static void minimalClockDrawWeatherIcon32(int x, int y, int code)
{
  const uint16_t cloud = 0xBDF7;
  const uint16_t cloudShade = 0x8410;
  const uint16_t sun = 0xFDC0;
  const uint16_t rain = 0x4DDF;

  if (code == 0)
  {
    appState.tft.fillCircle(x + 16, y + 16, 7, sun);
    appState.tft.drawCircle(x + 16, y + 16, 8, sun);
    appState.tft.drawLine(x + 16, y + 2, x + 16, y + 6, sun);
    appState.tft.drawLine(x + 16, y + 26, x + 16, y + 30, sun);
    appState.tft.drawLine(x + 2, y + 16, x + 6, y + 16, sun);
    appState.tft.drawLine(x + 26, y + 16, x + 30, y + 16, sun);
    appState.tft.drawLine(x + 6, y + 6, x + 9, y + 9, sun);
    appState.tft.drawLine(x + 23, y + 23, x + 26, y + 26, sun);
    appState.tft.drawLine(x + 6, y + 26, x + 9, y + 23, sun);
    appState.tft.drawLine(x + 23, y + 9, x + 26, y + 6, sun);
    return;
  }

  appState.tft.fillCircle(x + 11, y + 17, 7, cloudShade);
  appState.tft.fillCircle(x + 19, y + 13, 9, cloud);
  appState.tft.fillCircle(x + 25, y + 18, 6, cloud);
  appState.tft.fillRoundRect(x + 5, y + 18, 23, 9, 4, cloud);

  if (code == 45 || code == 48)
  {
    appState.tft.drawFastHLine(x + 4, y + 26, 25, cloud);
    appState.tft.drawFastHLine(x + 7, y + 30, 21, cloudShade);
  }
  else if (code >= 95)
  {
    appState.tft.fillTriangle(x + 18, y + 23, x + 12, y + 31, x + 18, y + 29, sun);
    appState.tft.fillTriangle(x + 18, y + 29, x + 24, y + 22, x + 19, y + 24, sun);
  }
  else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82))
  {
    appState.tft.drawLine(x + 10, y + 27, x + 8, y + 31, rain);
    appState.tft.drawLine(x + 18, y + 27, x + 16, y + 31, rain);
    appState.tft.drawLine(x + 26, y + 27, x + 24, y + 31, rain);
  }
}

static void minimalClockDrawTime(const String& clockText)
{
  appState.tft.fillRect(0, 18, DISPLAY_WIDTH, 96, CLEAN_TFT_THEME.background);
  String hours = clockText.substring(0, 2);
  String minutes = clockText.substring(3, 5);
  appState.tft.setTextFont(8);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
  int hoursWidth = appState.tft.textWidth(hours);
  int minutesWidth = appState.tft.textWidth(minutes);
  const int colonGap = 12;
  int startX = (DISPLAY_WIDTH - hoursWidth - minutesWidth - colonGap) / 2;
  appState.tft.setCursor(startX, 24);
  appState.tft.print(hours);
  appState.tft.setCursor(startX + hoursWidth + colonGap, 24);
  appState.tft.print(minutes);
  int colonX = startX + hoursWidth + colonGap / 2;
  appState.tft.fillCircle(colonX, 50, 3, CLEAN_TFT_THEME.primaryText);
  appState.tft.fillCircle(colonX, 78, 3, CLEAN_TFT_THEME.primaryText);
}

static void minimalClockDrawSeconds(const String& secondsText)
{
  appState.tft.fillRect(0, 104, DISPLAY_WIDTH, 42, CLEAN_TFT_THEME.background);
  appState.tft.setTextFont(8);
  appState.tft.setTextSize(1);
  String hours = lastClockText.substring(0, 2);
  String minutes = lastClockText.substring(3, 5);
  int timeWidth = appState.tft.textWidth(hours) + appState.tft.textWidth(minutes) + 12;
  int timeRightEdge = (DISPLAY_WIDTH + timeWidth) / 2;

  appState.tft.setTextFont(2);
  appState.tft.setTextSize(2);
  appState.tft.setTextColor(CLEAN_TFT_THEME.secondaryText, CLEAN_TFT_THEME.background);
  int secondsX = timeRightEdge - appState.tft.textWidth(secondsText);
  appState.tft.setCursor(secondsX, 104);
  appState.tft.print(secondsText);
}

static void minimalClockDrawWeather()
{
  appState.tft.fillRect(0, 150, DISPLAY_WIDTH, 55, CLEAN_TFT_THEME.background);
  if (!appState.hasWeather)
  {
    minimalClockDrawCentered("Clima indisponivel", 174, 2, 1, CLEAN_TFT_THEME.secondaryText);
    return;
  }

  String temperature = String((int)round(appState.weatherTemp));
  appState.tft.setTextFont(4);
  appState.tft.setTextSize(1);
  int groupWidth = 32 + 8 + appState.tft.textWidth(temperature) + 6;
  int groupX = (DISPLAY_WIDTH - groupWidth) / 2;
  minimalClockDrawWeatherIcon32(groupX, 156, appState.weatherCode);
  appState.tft.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
  appState.tft.setCursor(groupX + 40, 159);
  appState.tft.print(temperature);
  int degreeX = groupX + 42 + appState.tft.textWidth(temperature);
  appState.tft.drawCircle(degreeX, 161, 2, CLEAN_TFT_THEME.primaryText);
  minimalClockDrawCentered(appState.weatherText, 193, 2, 1, CLEAN_TFT_THEME.secondaryText);
}

static void minimalClockDrawDate(const String& dateText)
{
  appState.tft.fillRect(0, 208, DISPLAY_WIDTH, 24, CLEAN_TFT_THEME.background);
  minimalClockDrawCentered(dateText, 214, 2, 1, CLEAN_TFT_THEME.secondaryText);
}

void themeMinimalClockDrawBase()
{
  appState.tft.fillScreen(CLEAN_TFT_THEME.background);
  lastClockText = "";
  lastSecondsText = "";
  lastWeatherState = "";
  lastDateText = "";
  lastMinimalClockCheck = 0;
  themeMinimalClockUpdateIfNeeded();
}

void themeMinimalClockUpdateIfNeeded()
{
  unsigned long now = millis();
  if (lastMinimalClockCheck != 0 && now - lastMinimalClockCheck < 1000) return;
  lastMinimalClockCheck = now;

  struct tm timeInfo;
  String clockText = "--:--";
  String secondsText = "--";
  String dateText = "";
  if (minimalClockGetTimeInfo(timeInfo))
  {
    clockText = minimalClockFormatTime(timeInfo);
    secondsText = minimalClockFormatSeconds(timeInfo);
    dateText = minimalClockFormatDate(timeInfo);
  }

  if (clockText != lastClockText)
  {
    minimalClockDrawTime(clockText);
    lastClockText = clockText;
  }
  if (secondsText != lastSecondsText)
  {
    minimalClockDrawSeconds(secondsText);
    lastSecondsText = secondsText;
  }
  String weatherState = appState.hasWeather ? String(appState.weatherCode) + String(appState.weatherTemp, 1) + appState.weatherText : "none";
  if (weatherState != lastWeatherState)
  {
    minimalClockDrawWeather();
    lastWeatherState = weatherState;
  }
  if (dateText != lastDateText)
  {
    minimalClockDrawDate(dateText);
    lastDateText = dateText;
  }
}
