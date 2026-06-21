#include "display_ui.h"

#include <math.h>
#include <time.h>
#include <TFT_eSPI.h>

#include "app_state.h"
#include "config.h"
#include "metrics.h"

static void displayCentered(const String& text, int y, int size, uint16_t color)
{
  appState.tft.setTextSize(size);
  appState.tft.setTextColor(color, TFT_BLACK);
  int16_t textWidth = appState.tft.textWidth(text);
  int16_t x = (DISPLAY_WIDTH - textWidth) / 2;
  if (x < 0) x = 0;
  appState.tft.setCursor(x, y);
  appState.tft.print(text);
}

void displayUiInit()
{
  pinMode(DISPLAY_BL_PIN, OUTPUT);
  digitalWrite(DISPLAY_BL_PIN, DISPLAY_BL_ON);
  delay(100);
  appState.tft.init();
  appState.tft.setRotation(0);
  appState.tft.invertDisplay(DISPLAY_INVERT);
  appState.tft.fillScreen(TFT_BLACK);
}

void displayUiInitSprites()
{
  appState.metricSprite.setColorDepth(8);
  appState.metricSpriteReady = appState.metricSprite.createSprite(METRIC_W, METRIC_H) != nullptr;
}

void displayUiDrawBootScreen()
{
  appState.tft.fillScreen(TFT_BLACK);
  displayCentered("Ola Vargas", 68, 3, TFT_ORANGE);
  displayCentered("Conectando...", 125, 2, TFT_WHITE);
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

static String getTimeText()
{
  time_t now = time(nullptr);
  if (now < 100000) return "--:--";
  struct tm* timeInfo = localtime(&now);
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buffer);
}

static void drawDegreeTempSprite(TFT_eSprite& spr, int x, int y, int temp, uint16_t color)
{
  spr.setTextSize(2); spr.setTextColor(color, TFT_BLACK);
  String value = String(temp);
  spr.setCursor(x, y); spr.print(value);
  int numberWidth = spr.textWidth(value);
  spr.drawCircle(x + numberWidth + 5, y + 4, 2, color);
  spr.setCursor(x + numberWidth + 11, y); spr.print("C");
}

static void drawCloudIconSprite(TFT_eSprite& spr, int x, int y, uint16_t color)
{
  spr.drawCircle(x + 8, y + 9, 7, color); spr.drawCircle(x + 17, y + 7, 9, color);
  spr.drawCircle(x + 27, y + 11, 7, color); spr.drawLine(x + 7, y + 16, x + 28, y + 16, color);
}

static void drawHeader()
{
  TFT_eSprite header(&appState.tft);
  header.setColorDepth(8);
  if (header.createSprite(DISPLAY_WIDTH, HEADER_H) == nullptr)
  {
    appState.tft.fillRect(0, 0, DISPLAY_WIDTH, HEADER_H, TFT_BLACK);
    displayCentered(getTimeText(), 18, 5, TFT_WHITE);
    appState.tft.drawLine(12, 96, 228, 96, TFT_DARKGREY);
    return;
  }
  header.fillSprite(TFT_BLACK);
  String timeText = getTimeText();
  header.setTextSize(5); header.setTextColor(TFT_WHITE, TFT_BLACK);
  header.setCursor((DISPLAY_WIDTH - header.textWidth(timeText)) / 2, 10); header.print(timeText);
  if (appState.hasWeather)
  {
    drawDegreeTempSprite(header, 36, 66, (int)round(appState.weatherTemp), TFT_ORANGE);
    drawCloudIconSprite(header, 91, 67, TFT_WHITE);
    header.setTextSize(2); header.setTextColor(TFT_WHITE, TFT_BLACK);
    header.setCursor(132, 66); header.print(appState.weatherText);
  }
  else
  {
    header.setTextSize(2); header.setTextColor(TFT_DARKGREY, TFT_BLACK);
    String text = "--C  Clima";
    header.setCursor((DISPLAY_WIDTH - header.textWidth(text)) / 2, 66); header.print(text);
  }
  header.drawLine(12, 96, 228, 96, TFT_DARKGREY);
  header.pushSprite(0, 0); header.deleteSprite();
  appState.lastTimeDrawn = timeText;
  appState.lastWeatherDrawn = appState.hasWeather ? String(appState.weatherTemp, 1) + appState.weatherText : "none";
}

static uint16_t colorByValue(int value, uint16_t normalColor)
{
  if (value >= 90) return TFT_RED;
  if (value >= 75) return TFT_ORANGE;
  return normalColor;
}

static void drawProgressBarOnSprite(TFT_eSprite& spr, int x, int y, int w, int h, int value, uint16_t color)
{
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  int fillWidth = (w * value) / 100;
  spr.fillRoundRect(x, y, w, h, 4, TFT_DARKGREY);
  if (fillWidth > 0) spr.fillRoundRect(x, y, fillWidth, h, 4, color);
}

void displayUiDrawMetricRow(int y, const String& label, int value, uint16_t baseColor)
{
  uint16_t barColor = colorByValue(value, baseColor);
  if (appState.metricSpriteReady)
  {
    appState.metricSprite.fillSprite(TFT_BLACK);
    appState.metricSprite.setTextSize(2); appState.metricSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    appState.metricSprite.setCursor(0, 4); appState.metricSprite.print(label);
    String percentText = String(value) + "%";
    appState.metricSprite.setCursor(METRIC_W - appState.metricSprite.textWidth(percentText), 4); appState.metricSprite.print(percentText);
    drawProgressBarOnSprite(appState.metricSprite, 64, 13, 112, 10, value, barColor);
    appState.metricSprite.pushSprite(METRIC_X, y);
    return;
  }
  appState.tft.fillRect(METRIC_X, y, METRIC_W, METRIC_H, TFT_BLACK);
  appState.tft.setTextSize(2); appState.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  appState.tft.setCursor(METRIC_X, y + 4); appState.tft.print(label);
  String percentText = String(value) + "%";
  appState.tft.setCursor(DISPLAY_WIDTH - appState.tft.textWidth(percentText) - 12, y + 4); appState.tft.print(percentText);
  int fillWidth = (112 * value) / 100;
  appState.tft.fillRoundRect(METRIC_X + 64, y + 13, 112, 10, 4, TFT_DARKGREY);
  appState.tft.fillRoundRect(METRIC_X + 64, y + 13, fillWidth, 10, 4, barColor);
}

void displayUiDrawFooter()
{
  appState.tft.fillRect(0, 218, DISPLAY_WIDTH, 22, TFT_BLACK);
  appState.tft.drawLine(12, 220, 228, 220, TFT_DARKGREY);
  appState.tft.setTextSize(1); appState.tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  appState.tft.setCursor(12, 228); appState.tft.print(metricsHasRecentPcMetrics() ? "PC ONLINE" : "PC WAIT");
  String wifiText = appState.isApMode ? "SETUP" : "WiFi OK";
  appState.tft.setCursor(DISPLAY_WIDTH - appState.tft.textWidth(wifiText) - 12, 228); appState.tft.print(wifiText);
}

void displayUiDrawDashboardBase()
{
  appState.tft.fillScreen(TFT_BLACK);
  appState.lastTimeDrawn = ""; appState.lastWeatherDrawn = "";
  appState.lastCpuDrawn = -1; appState.lastRamDrawn = -1; appState.lastGpuDrawn = -1;
  drawHeader();
  displayUiDrawMetricRow(CPU_Y, "CPU", appState.cpuCurrent, TFT_BLUE);
  displayUiDrawMetricRow(RAM_Y, "RAM", appState.ramCurrent, TFT_GREEN);
  displayUiDrawMetricRow(GPU_Y, "GPU", appState.gpuCurrent, TFT_CYAN);
  appState.lastCpuDrawn = appState.cpuCurrent; appState.lastRamDrawn = appState.ramCurrent; appState.lastGpuDrawn = appState.gpuCurrent;
  displayUiDrawFooter();
}

void displayUiUpdateHeaderIfNeeded()
{
  String timeText = getTimeText();
  String weatherState = appState.hasWeather ? String(appState.weatherTemp, 1) + appState.weatherText : "none";
  if (timeText != appState.lastTimeDrawn || weatherState != appState.lastWeatherDrawn) drawHeader();
}
