#include <TFT_eSPI.h>

#include "app_state.h"
#include "config.h"
#include "metrics.h"

namespace
{
const uint16_t GAMER_CARD = 0x1082;
const uint16_t GAMER_BORDER = 0x18E3;
const uint16_t GAMER_TEXT = 0xEF7D;
const uint16_t GAMER_MUTED = 0x9CF3;
const uint16_t GAMER_CYAN = 0x04FF;
const uint16_t GAMER_GREEN = 0x5DCA;
const uint16_t GAMER_BLUE = 0x3D9F;
const uint16_t GAMER_PURPLE = 0x917F;
const uint16_t GAMER_ORANGE = 0xFC00;
const uint16_t GAMER_TRACK = 0x18C3;

const int CARD_W = 104;
const int CARD_H = 61;
const int LEFT_X = 12;
const int RIGHT_X = 124;
const int ROW1_Y = 36;
const int ROW2_Y = 102;
const int ROW3_Y = 168;
const unsigned long GAMER_STATUS_INTERVAL_MS = 3000;

void gamerCard(int x, int y)
{
  appState.tft.fillRoundRect(x, y, CARD_W, CARD_H, 9, GAMER_CARD);
  appState.tft.drawRoundRect(x, y, CARD_W, CARD_H, 9, GAMER_BORDER);
}

String percentOrDash(int value)
{
  return value < 0 ? "--" : String(value) + "%";
}

void gamerLabel(const char* label, int x, int y, uint16_t color)
{
  appState.tft.setTextFont(2); appState.tft.setTextSize(1);
  appState.tft.setTextColor(color, GAMER_CARD);
  appState.tft.setCursor(x, y); appState.tft.print(label);
}

void gamerBar(int x, int y, int value, uint16_t color)
{
  const int width = 82;
  appState.tft.fillRoundRect(x, y, width, 3, 2, GAMER_TRACK);
  if (value >= 0) appState.tft.fillRoundRect(x, y, (width * constrain(value, 0, 100)) / 100, 3, 2, color);
}

int gamerCenteredValueX(int cardX, const String& value)
{
  int x = cardX + (CARD_W - appState.tft.textWidth(value)) / 2;
  return x < cardX + 8 ? cardX + 8 : x;
}

void gamerUsageCard(int x, int y, const char* label, int value, uint16_t color)
{
  appState.tft.fillRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, GAMER_CARD);
  gamerLabel(label, x + 9, y + 8, color);
  appState.tft.setTextFont(2); appState.tft.setTextSize(2); appState.tft.setTextColor(GAMER_TEXT, GAMER_CARD);
  String text = percentOrDash(value);
  appState.tft.setCursor(gamerCenteredValueX(x, text), y + 21); appState.tft.print(text);
  gamerBar(x + 9, y + 54, value, color);
}

void gamerValueCard(int x, int y, const char* label, const String& value, uint16_t color)
{
  appState.tft.fillRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, GAMER_CARD);
  gamerLabel(label, x + 9, y + 8, color);
  // Mantem a tipografia nativa nesta POC; esta e a troca isolada para uma
  // futura fonte gamer (Oxanium/Rajdhani) sem tocar no restante do layout.
  appState.tft.setTextFont(2); appState.tft.setTextSize(2); appState.tft.setTextColor(GAMER_TEXT, GAMER_CARD);
  appState.tft.setCursor(gamerCenteredValueX(x, value), y + 21); appState.tft.print(value);
}

void gamerTemperatureCard(int x, int y, int temperature)
{
  appState.tft.fillRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, GAMER_CARD);
  gamerLabel("GPU TEMP", x + 9, y + 8, GAMER_ORANGE);
  String value = temperature < 0 ? "--\xB0" : String(temperature) + "\xB0";
  appState.tft.setTextFont(2); appState.tft.setTextSize(2); appState.tft.setTextColor(GAMER_ORANGE, GAMER_CARD);
  appState.tft.setCursor(gamerCenteredValueX(x, value), y + 21); appState.tft.print(value);
}

void gamerDrawHeader()
{
  appState.tft.fillRect(0, 0, DISPLAY_WIDTH, 32, TFT_BLACK);
  bool showSource = appState.gamerShowSource;
  bool sourceOnline = metricsHasRecentPcMetrics() && appState.gamerSource.length() > 0;
  String status;
  if (showSource)
  {
    String source = appState.gamerSource.length() ? appState.gamerSource : "RTSS";
    status = source + (sourceOnline ? " ON" : " OFF");
  }
  else
  {
    status = appState.gamerGame.length() ? appState.gamerGame : "GAME HUD";
  }
  appState.tft.setTextFont(2); appState.tft.setTextSize(1);
  while (status.length() > 3 && appState.tft.textWidth(status) > 180)
    status = status.substring(0, status.length() - 2) + ".";
  int statusWidth = appState.tft.textWidth(status);
  int groupWidth = statusWidth + (showSource ? 9 : 0);
  int startX = (DISPLAY_WIDTH - groupWidth) / 2;
  if (showSource) appState.tft.fillCircle(startX + 2, 13, 2, sourceOnline ? GAMER_GREEN : TFT_RED);
  appState.tft.setTextColor(showSource ? GAMER_TEXT : GAMER_MUTED, TFT_BLACK);
  appState.tft.setCursor(startX + (showSource ? 9 : 0), 8); appState.tft.print(status);
}

void gamerDrawValues()
{
  gamerDrawHeader();
  bool pcOnline = metricsHasRecentPcMetrics();
  gamerValueCard(LEFT_X, ROW1_Y, "FPS", !pcOnline ? "144" : appState.gamerFps < 0 ? "--" : String(appState.gamerFps), GAMER_CYAN);
  gamerValueCard(RIGHT_X, ROW1_Y, "FRAME", !pcOnline ? "6.9 ms" : appState.gamerFrametime < 0 ? "-- ms" : String(appState.gamerFrametime, 1) + " ms", GAMER_TEXT);
  int gpu = appState.gamerDataVersion == 0 ? -1 : appState.gpuCurrent;
  int cpu = appState.gamerDataVersion == 0 ? -1 : appState.cpuCurrent;
  int ram = appState.gamerDataVersion == 0 ? -1 : appState.ramCurrent;
  gamerUsageCard(LEFT_X, ROW2_Y, "GPU", gpu, GAMER_GREEN);
  gamerTemperatureCard(RIGHT_X, ROW2_Y, !pcOnline ? 68 : appState.gamerGpuTemp);
  gamerUsageCard(LEFT_X, ROW3_Y, "RAM", ram, GAMER_PURPLE);
  gamerUsageCard(RIGHT_X, ROW3_Y, "CPU", cpu, GAMER_BLUE);
}
}

void themeGamerDrawBase()
{
  appState.tft.fillScreen(TFT_BLACK);
  gamerCard(LEFT_X, ROW1_Y); gamerCard(RIGHT_X, ROW1_Y);
  gamerCard(LEFT_X, ROW2_Y); gamerCard(RIGHT_X, ROW2_Y);
  gamerCard(LEFT_X, ROW3_Y); gamerCard(RIGHT_X, ROW3_Y);
  appState.gamerShowSource = true;
  appState.gamerLastStatusUpdate = millis();
  appState.gamerLastPcOnline = metricsHasRecentPcMetrics();
  gamerDrawValues();
  appState.gamerDrawnVersion = appState.gamerDataVersion;
}

void themeGamerUpdateIfNeeded()
{
  unsigned long now = millis();
  bool pcOnline = metricsHasRecentPcMetrics();
  bool redrawHeader = false;
  if (now - appState.gamerLastStatusUpdate >= GAMER_STATUS_INTERVAL_MS)
  {
    appState.gamerLastStatusUpdate = now;
    appState.gamerShowSource = !appState.gamerShowSource;
    redrawHeader = true;
  }

  if (appState.gamerDrawnVersion != appState.gamerDataVersion || appState.gamerLastPcOnline != pcOnline)
  {
    appState.gamerLastPcOnline = pcOnline;
    gamerDrawValues();
    appState.gamerDrawnVersion = appState.gamerDataVersion;
  }
  else if (redrawHeader)
  {
    gamerDrawHeader();
  }
}
