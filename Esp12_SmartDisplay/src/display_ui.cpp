#include "display_ui.h"

#include <math.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <ESP8266WiFi.h>

#include "app_state.h"
#include "config.h"
#include "display_assets.h"
#include "metrics.h"
#include "smooth_clock_font.h"
#include "theme.h"
#include "weather_location.h"

static void displayCentered(const String& text, int y, int size, uint16_t color)
{
  smoothClockFontUnload(appState.tft);
  appState.tft.setTextSize(size);
  appState.tft.setTextColor(color, TFT_BLACK);
  int16_t textWidth = appState.tft.textWidth(text);
  int16_t x = (DISPLAY_WIDTH - textWidth) / 2;
  if (x < 0) x = 0;
  appState.tft.setCursor(x, y);
  appState.tft.print(text);
}

static void displayTinyDashBrand(int titleY)
{
  appState.tft.setTextFont(4);
  displayCentered("TinyDash", titleY, 2, TFT_ORANGE);

  const String byText = "by";
  const String vendorText = "VargasTec";
  appState.tft.setTextFont(1);
  appState.tft.setTextSize(2);
  int byWidth = appState.tft.textWidth(byText);
  appState.tft.setTextFont(2);
  appState.tft.setTextSize(1);
  int vendorWidth = appState.tft.textWidth(vendorText);
  int totalWidth = byWidth + 5 + vendorWidth;
  int startX = (DISPLAY_WIDTH - totalWidth) / 2;
  appState.tft.setTextFont(1);
  appState.tft.setTextSize(2);
  appState.tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  appState.tft.setCursor(startX, titleY + 62); appState.tft.print(byText);
  appState.tft.setTextFont(2);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  appState.tft.setCursor(startX + byWidth + 5, titleY + 59); appState.tft.print(vendorText);
}

void displayUiInit()
{
  pinMode(DISPLAY_BL_PIN, OUTPUT);
  digitalWrite(DISPLAY_BL_PIN, DISPLAY_BL_ON);
  delay(100);
  appState.tft.init();
  appState.tft.setRotation(0);
  appState.tft.invertDisplay(DISPLAY_INVERT);
  appState.tft.fillScreen(CLEAN_TFT_THEME.background);
}

void displayUiInitSprites()
{
  appState.metricSprite.setColorDepth(8);
  appState.metricSpriteReady = appState.metricSprite.createSprite(METRIC_W, METRIC_H) != nullptr;
}

void displayUiDrawBootScreen()
{
  appState.tft.fillScreen(TFT_BLACK);
  displayTinyDashBrand(15);
  appState.tft.setTextFont(2);
  displayCentered("Iniciando Wi-Fi...", 112, 1, TFT_WHITE);
  displayCentered("OTA: /update", 184, 1, TFT_ORANGE);
}

void displayUiDrawStartupInfo(const String& ipAddress)
{
  appState.tft.fillRect(0, 100, DISPLAY_WIDTH, 54, TFT_BLACK);
  appState.tft.setTextFont(2);
  displayCentered("REDE CONECTADA", 104, 1, TFT_GREEN);
  appState.tft.setTextFont(1);
  displayCentered("IP: " + ipAddress, 126, 2, TFT_WHITE);
}

void displayUiDrawConnectingNetwork(const String& ssid)
{
  appState.tft.fillRect(0, 150, DISPLAY_WIDTH, 40, TFT_BLACK);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  String text = "Rede: " + ssid;
  int w = appState.tft.textWidth(text);
  appState.tft.setCursor((DISPLAY_WIDTH - w) / 2, 158);
  appState.tft.print(text);
}

void displayUiDrawApScreen()
{
  appState.tft.fillScreen(TFT_BLACK);
  displayCentered("Wi-Fi Setup", 12, 2, TFT_YELLOW);
  appState.tft.setTextSize(1); appState.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  appState.tft.setCursor(10, 47); appState.tft.print("Conecte nesta rede:");
  appState.tft.setCursor(10, 70); appState.tft.setTextSize(2); appState.tft.setTextColor(TFT_GREEN, TFT_BLACK); appState.tft.print(AP_SSID);
  appState.tft.setTextSize(1); appState.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  appState.tft.setCursor(10, 103); appState.tft.print("Senha:");
  appState.tft.setCursor(10, 124); appState.tft.setTextSize(2); appState.tft.setTextColor(TFT_GREEN, TFT_BLACK); appState.tft.print(AP_PASS);
  appState.tft.setTextSize(1); appState.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  displayCentered("Depois acesse:", 162, 1, TFT_WHITE);
  displayCentered("192.168.4.1", 184, 2, TFT_CYAN);
  displayCentered("OTA: /update", 218, 1, TFT_ORANGE);
}

void displayUiDrawConnectionFailedScreen()
{
  appState.tft.fillScreen(TFT_BLACK);
  displayCentered("Falhou!", 80, 2, TFT_RED);
  displayCentered("Iniciando setup...", 120, 1, TFT_WHITE);
  delay(1500);
}

void displayUiDrawSafeModeScreen(const String& ipAddress)
{
  appState.tft.fillScreen(TFT_BLACK);
  displayCentered("MODO SEGURO", 54, 2, TFT_ORANGE);
  displayCentered("Inicializacao instavel", 93, 1, TFT_LIGHTGREY);
  appState.tft.drawLine(18, 116, 222, 116, TFT_DARKGREY);
  displayCentered("IP: " + ipAddress, 139, 2, TFT_WHITE);
  displayCentered("OTA: /update", 178, 2, TFT_GREEN);
  displayCentered("Dashboard desativado", 214, 1, TFT_LIGHTGREY);
}

void displayUiDrawOtaMaintenanceScreen(const String& ipAddress)
{
  appState.tft.fillScreen(CLEAN_TFT_THEME.background);
  displayCentered("OTA MODE", 46, 2, TFT_ORANGE);
  displayCentered("Atualizacao segura", 86, 1, CLEAN_TFT_THEME.secondaryText);
  displayCentered("IP: " + ipAddress, 154, 1, TFT_WHITE);
  displayCentered("/update", 180, 2, TFT_GREEN);
  displayUiUpdateOtaMaintenanceProgress();
}

void displayUiUpdateOtaMaintenanceProgress()
{
  const int x = 34;
  const int y = 122;
  const int w = 172;
  const int h = 16;
  const int segmentW = 44;
  const int travel = w - segmentW;
  int frame = appState.otaMaintenanceFrame % 48;
  int offset = frame < 24
    ? (travel * frame) / 23
    : (travel * (47 - frame)) / 23;

  appState.tft.fillRoundRect(x, y, w, h, h / 2, CLEAN_TFT_THEME.barTrack);
  appState.tft.fillRoundRect(x + offset, y, segmentW, h, h / 2, CLEAN_TFT_THEME.cpu);
  appState.otaMaintenanceFrame++;
}

static String getTimeText()
{
  time_t now = time(nullptr);
  if (now < 100000) return "--:--";
  struct tm* timeInfo = localtime(&now);
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buffer);
}

template <typename TCanvas>
static void drawWeatherIcon34(TCanvas& spr, int x, int y, int code)
{
  const uint16_t cloud = 0xBDF7;
  const uint16_t cloudShade = 0x8410;
  const uint16_t sun = 0xFDC0;
  const uint16_t rain = 0x4DDF;

  if (code == 0)
  {
    spr.fillCircle(x + 17, y + 17, 8, sun);
    spr.drawCircle(x + 17, y + 17, 9, sun);
    spr.drawLine(x + 17, y + 2, x + 17, y + 6, sun);
    spr.drawLine(x + 17, y + 28, x + 17, y + 32, sun);
    spr.drawLine(x + 2, y + 17, x + 6, y + 17, sun);
    spr.drawLine(x + 28, y + 17, x + 32, y + 17, sun);
    spr.drawLine(x + 6, y + 6, x + 9, y + 9, sun);
    spr.drawLine(x + 25, y + 25, x + 28, y + 28, sun);
    spr.drawLine(x + 6, y + 28, x + 9, y + 25, sun);
    spr.drawLine(x + 25, y + 9, x + 28, y + 6, sun);
    return;
  }

  spr.fillCircle(x + 11, y + 18, 7, cloudShade);
  spr.fillCircle(x + 20, y + 14, 10, cloud);
  spr.fillCircle(x + 27, y + 19, 7, cloud);
  spr.fillRoundRect(x + 5, y + 19, 26, 10, 5, cloud);

  if (code == 45 || code == 48)
  {
    spr.drawFastHLine(x + 4, y + 28, 28, cloud);
    spr.drawFastHLine(x + 7, y + 32, 23, cloudShade);
  }
  else if (code >= 95)
  {
    spr.fillTriangle(x + 19, y + 24, x + 12, y + 33, x + 19, y + 30, sun);
    spr.fillTriangle(x + 19, y + 30, x + 26, y + 23, x + 20, y + 25, sun);
  }
  else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82))
  {
    spr.drawLine(x + 10, y + 29, x + 8, y + 33, rain);
    spr.drawLine(x + 19, y + 29, x + 17, y + 33, rain);
    spr.drawLine(x + 28, y + 29, x + 26, y + 33, rain);
  }
}

static void drawMonoIcon16(int x, int y, const uint8_t* icon, uint16_t color)
{
  for (int row = 0; row < 16; ++row)
  {
    uint16_t bits = (uint16_t(pgm_read_byte(icon + row * 2)) << 8) | pgm_read_byte(icon + row * 2 + 1);
    for (int column = 0; column < 16; ++column)
      if (bits & (uint16_t(0x8000) >> column)) appState.tft.drawPixel(x + column, y + row, color);
  }
}

static String formatPercent2Digits(int value)
{
  if (value < 0) return "--";
  if (value > 100) value = 100;
  uint8_t percent = static_cast<uint8_t>(value);
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02u%%", percent);
  return String(buffer);
}

static void drawHeader()
{
  smoothClockFontUnload(appState.tft);
  appState.tft.fillRect(0, 0, DISPLAY_WIDTH, TOP_LABEL_Y, CLEAN_TFT_THEME.background);
  String timeText = getTimeText();
  bool useSmoothFont = smoothClockFontEnsureLoaded(appState.tft);

  if (useSmoothFont)
  {
    smoothClockDrawText(appState.tft, timeText, 6, 5, CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
    smoothClockFontUnload(appState.tft);
  }
  else
  {
    appState.tft.setTextFont(4);
    appState.tft.setTextSize(2);
    appState.tft.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
    appState.tft.setCursor(6, 3);
    appState.tft.print(timeText);
  }

  if (appState.hasWeather)
  {
    drawWeatherIcon34(appState.tft, 155, 12, appState.weatherCode);
    appState.tft.setTextFont(4);
    appState.tft.setTextSize(1);
    appState.tft.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
    String temperature = String((int)round(appState.weatherTemp));
    appState.tft.setCursor(195, 9);
    appState.tft.print(temperature);
    int width = appState.tft.textWidth(temperature);
    appState.tft.drawCircle(197 + width, 11, 2, CLEAN_TFT_THEME.primaryText);
  }
  else
  {
    appState.tft.setTextFont(4);
    appState.tft.setTextSize(1);
    appState.tft.setTextColor(CLEAN_TFT_THEME.secondaryText, CLEAN_TFT_THEME.background);
    appState.tft.setCursor(195, 9);
    appState.tft.print("--");
  }

  appState.lastTimeDrawn = timeText;
  appState.lastWeatherDrawn = appState.hasWeather ? String(appState.weatherCode) + String(appState.weatherTemp, 1) + appState.weatherText : "none";
}

static void drawProgressBarOnSprite(TFT_eSprite& spr, int x, int y, int w, int h, int value, uint16_t color)
{
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  int fillWidth = (w * value) / 100;
  spr.fillRoundRect(x, y, w, h, h / 2, CLEAN_TFT_THEME.barTrack);
  if (fillWidth > 0) spr.fillRoundRect(x, y, fillWidth, h, h / 2, color);
}

static uint16_t getMetricBarColor(int value)
{
  static const uint16_t warningOrange = 0xB2C5;
  static const uint16_t criticalRed = 0xB800;
  if (value > 90) return criticalRed;
  if (value > 80) return warningOrange;
  return CLEAN_TFT_THEME.cpu;
}

void displayUiDrawMetricRow(int y, const String& label, int value, uint16_t baseColor)
{
  (void)baseColor;
  uint16_t barColor = getMetricBarColor(value);
  if (appState.metricSpriteReady)
  {
    appState.metricSprite.fillSprite(CLEAN_TFT_THEME.background);
    appState.metricSprite.setTextFont(4); appState.metricSprite.setTextSize(1); appState.metricSprite.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
    appState.metricSprite.setCursor(0, 0); appState.metricSprite.print(label);
    String percentText = formatPercent2Digits(value);
    appState.metricSprite.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
    int percentWidth = appState.metricSprite.textWidth(percentText);
    appState.metricSprite.setCursor(METRIC_PERCENT_X + METRIC_PERCENT_W - percentWidth, 0); appState.metricSprite.print(percentText);
    drawProgressBarOnSprite(appState.metricSprite, METRIC_BAR_X, 3, METRIC_BAR_W, 16, value, barColor);
    appState.metricSprite.pushSprite(METRIC_X, y);
    return;
  }
  appState.tft.fillRect(METRIC_X, y, METRIC_W, METRIC_H, CLEAN_TFT_THEME.background);
  appState.tft.setTextFont(4); appState.tft.setTextSize(1); appState.tft.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
  appState.tft.setCursor(METRIC_X, y); appState.tft.print(label);
  String percentText = formatPercent2Digits(value);
  appState.tft.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
  int percentWidth = appState.tft.textWidth(percentText);
  appState.tft.setCursor(METRIC_X + METRIC_PERCENT_X + METRIC_PERCENT_W - percentWidth, y); appState.tft.print(percentText);
  int barValue = constrain(value, 0, 100);
  int fillWidth = (METRIC_BAR_W * barValue) / 100;
  appState.tft.fillRoundRect(METRIC_X + METRIC_BAR_X, y + 3, METRIC_BAR_W, 16, 8, CLEAN_TFT_THEME.barTrack);
  if (fillWidth > 0) appState.tft.fillRoundRect(METRIC_X + METRIC_BAR_X, y + 3, fillWidth, 16, 8, barColor);
}

void displayUiDrawFooter()
{
  appState.tft.fillRect(0, FOOTER_Y, DISPLAY_WIDTH, DISPLAY_HEIGHT - FOOTER_Y, CLEAN_TFT_THEME.background);
  if (appState.isApMode)
  {
    appState.tft.setTextFont(2); appState.tft.setTextSize(1);
    appState.tft.setTextColor(CLEAN_TFT_THEME.offline, CLEAN_TFT_THEME.background);
    appState.tft.setCursor(12, 214); appState.tft.print("SETUP WI-FI");
    return;
  }

  unsigned long now = millis();
  if (appState.lastFooterStatusUpdate == 0) appState.lastFooterStatusUpdate = now;
  else if (now - appState.lastFooterStatusUpdate >= TOP_LABEL_INTERVAL_MS)
  {
    appState.lastFooterStatusUpdate = now;
    appState.footerStatusIndex++;
  }

  bool pcOnline = metricsHasRecentPcMetrics();
  String status;
  String detail;
  uint16_t statusColor;
  switch (appState.footerStatusIndex % 3)
  {
    case 0:
      status = pcOnline ? "PC ONLINE" : "PC WAIT";
      detail = pcOnline ? WiFi.localIP().toString() : "SEM DADOS";
      statusColor = pcOnline ? CLEAN_TFT_THEME.online : CLEAN_TFT_THEME.offline;
      break;
    case 1:
      status = "WIFI OK";
      detail = String(WiFi.RSSI()) + " dBm";
      statusColor = CLEAN_TFT_THEME.online;
      break;
    default:
      status = "OTA READY";
      detail = "/update";
      statusColor = CLEAN_TFT_THEME.cpu;
      break;
  }

  appState.tft.setTextFont(2); appState.tft.setTextSize(1);
  int statusWidth = appState.tft.textWidth(status);
  int detailWidth = appState.tft.textWidth(detail);
  bool useSmallDetail = statusWidth + detailWidth + 12 > DISPLAY_WIDTH - 24;
  if (useSmallDetail)
  {
    appState.tft.setTextFont(1); appState.tft.setTextSize(1);
    detailWidth = appState.tft.textWidth(detail);
  }
  int totalWidth = statusWidth + detailWidth + 12;
  int startX = (DISPLAY_WIDTH - totalWidth) / 2;
  if (startX < 12) startX = 12;

  appState.tft.setTextFont(2); appState.tft.setTextSize(1);
  appState.tft.setTextColor(statusColor, CLEAN_TFT_THEME.background);
  appState.tft.setCursor(startX, 214); appState.tft.print(status);
  appState.tft.fillCircle(startX + statusWidth + 6, 223, 1, CLEAN_TFT_THEME.secondaryText);
  if (useSmallDetail)
  {
    appState.tft.setTextFont(1); appState.tft.setTextSize(1);
  }
  appState.tft.setTextColor(CLEAN_TFT_THEME.secondaryText, CLEAN_TFT_THEME.background);
  appState.tft.setCursor(startX + statusWidth + 12, useSmallDetail ? 219 : 214); appState.tft.print(detail);
}

void displayUiDrawDashboardBase()
{
  appState.tft.fillScreen(CLEAN_TFT_THEME.background);
  appState.lastTimeDrawn = ""; appState.lastWeatherDrawn = "";
  appState.lastCpuDrawn = -1; appState.lastRamDrawn = -1; appState.lastGpuDrawn = -1; appState.lastDiskDrawn = -1; appState.lastDiskLabelDrawn = "";
  bool pcOnline = metricsHasRecentPcMetrics();
  drawHeader();
  appState.lastTopLabelDrawn = "";
  displayUiUpdateTopLabelIfNeeded();
  int cpuValue = pcOnline ? appState.cpuCurrent : -1;
  int ramValue = pcOnline ? appState.ramCurrent : -1;
  int diskValue = pcOnline ? appState.diskCurrent : -1;
  int gpuValue = pcOnline ? appState.gpuCurrent : -1;
  displayUiDrawMetricRow(CPU_Y, "CPU", cpuValue, CLEAN_TFT_THEME.cpu);
  displayUiDrawMetricRow(RAM_Y, "RAM", ramValue, CLEAN_TFT_THEME.ram);
  displayUiDrawMetricRow(GPU_Y, appState.diskLabel, diskValue, CLEAN_TFT_THEME.cpu);
  displayUiDrawMetricRow(DISK_Y, "GPU", gpuValue, CLEAN_TFT_THEME.gpu);
  appState.lastCpuDrawn = cpuValue; appState.lastRamDrawn = ramValue; appState.lastGpuDrawn = gpuValue;
  appState.lastDiskDrawn = diskValue; appState.lastDiskLabelDrawn = appState.diskLabel;
  displayUiDrawFooter();
}

void displayUiUpdateHeaderIfNeeded()
{
  String timeText = getTimeText();
  String weatherState = appState.hasWeather ? String(appState.weatherCode) + String(appState.weatherTemp, 1) + appState.weatherText : "none";
  if (timeText != appState.lastTimeDrawn || weatherState != appState.lastWeatherDrawn) drawHeader();
}

static String getDateLabel()
{
  time_t now = time(nullptr);
  if (now < 100000) return "";
  struct tm* timeInfo = localtime(&now);
  static const char* const weekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
  static const char* const months[] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
  return String(weekdays[timeInfo->tm_wday]) + ", " + String(timeInfo->tm_mday) + " " + months[timeInfo->tm_mon];
}

void displayUiUpdateTopLabelIfNeeded()
{
  unsigned long now = millis();
  if (appState.lastTopLabelUpdate != 0 && now - appState.lastTopLabelUpdate < TOP_LABEL_INTERVAL_MS) return;
  appState.lastTopLabelUpdate = now;

  const uint8_t statusCount = 3;
  uint8_t statusIndex = appState.topLabelIndex % statusCount;
  const uint8_t* icon = STATUS_PIN_16;
  uint16_t iconColor = CLEAN_TFT_THEME.secondaryText;
  String label;
  if (statusIndex == 0)
  {
    label = weatherLocationCityName();
  }
  else if (statusIndex == 1)
  {
    icon = STATUS_CALENDAR_16;
    label = getDateLabel();
    if (label.length() == 0) label = "DATA INDISPONIVEL";
  }
  else
  {
    icon = STATUS_WEATHER_16;
    iconColor = CLEAN_TFT_THEME.secondaryText;
    label = appState.hasWeather ? appState.weatherText + " " + String((int)round(appState.weatherTemp)) + "C" : "CLIMA INDISPONIVEL";
  }
  appState.topLabelIndex++;

  appState.tft.setTextFont(1); appState.tft.setTextSize(2);
  appState.tft.fillRect(0, TOP_LABEL_Y, DISPLAY_WIDTH, TOP_LABEL_H, CLEAN_TFT_THEME.background);
  drawMonoIcon16(12, TOP_LABEL_Y + 4, icon, iconColor);
  appState.tft.setTextColor(CLEAN_TFT_THEME.secondaryText, CLEAN_TFT_THEME.background);
  appState.tft.setCursor(36, TOP_LABEL_Y + 4);
  appState.tft.print(label);
  for (uint8_t dot = 0; dot < statusCount; ++dot)
    appState.tft.fillCircle(210 + dot * 9, TOP_LABEL_Y + 12, 2, dot == statusIndex ? CLEAN_TFT_THEME.cpu : CLEAN_TFT_THEME.divider);
  appState.lastTopLabelDrawn = String(statusIndex) + label;
}
