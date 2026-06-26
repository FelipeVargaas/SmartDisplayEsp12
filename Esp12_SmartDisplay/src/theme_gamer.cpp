#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdio.h>
#include <string.h>

#include "app_state.h"
#include "config.h"
#include "metrics.h"
#include "reset_marker.h"

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
const unsigned long GAMER_VALUES_INTERVAL_MS = 500;
unsigned long gamerLastValuesDrawAt = 0;

void gamerCard(int x, int y)
{
  appState.tft.fillRoundRect(x, y, CARD_W, CARD_H, 9, GAMER_CARD);
  appState.tft.drawRoundRect(x, y, CARD_W, CARD_H, 9, GAMER_BORDER);
}

void percentOrDash(int value, char* buffer, size_t bufferSize)
{
  if (value < 0) snprintf(buffer, bufferSize, "--");
  else snprintf(buffer, bufferSize, "%d%%", value);
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

int gamerCenteredValueX(int cardX, const char* value)
{
  int x = cardX + (CARD_W - appState.tft.textWidth(value)) / 2;
  return x < cardX + 8 ? cardX + 8 : x;
}

void gamerUsageCard(int x, int y, const char* label, int value, uint16_t color)
{
  appState.tft.fillRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, GAMER_CARD);
  gamerLabel(label, x + 9, y + 8, color);
  appState.tft.setTextFont(2); appState.tft.setTextSize(2); appState.tft.setTextColor(GAMER_TEXT, GAMER_CARD);
  char text[16];
  percentOrDash(value, text, sizeof(text));
  appState.tft.setCursor(gamerCenteredValueX(x, text), y + 21); appState.tft.print(text);
  gamerBar(x + 9, y + 54, value, color);
}

void gamerValueCard(int x, int y, const char* label, const char* value, uint16_t color)
{
  appState.tft.fillRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, GAMER_CARD);
  gamerLabel(label, x + 9, y + 8, color);
  // Mantem a tipografia nativa nesta POC; esta e a troca isolada para uma
  // futura fonte gamer (Oxanium/Rajdhani) sem tocar no restante do layout.
  appState.tft.setTextFont(2); appState.tft.setTextSize(2); appState.tft.setTextColor(GAMER_TEXT, GAMER_CARD);
  appState.tft.setCursor(gamerCenteredValueX(x, value), y + 21); appState.tft.print(value);
}

void gamerFrameCard(int x, int y, const char* value, const char* unit)
{
  appState.tft.fillRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, GAMER_CARD);
  gamerLabel("FRAME", x + 9, y + 8, GAMER_TEXT);
  appState.tft.setTextFont(2); appState.tft.setTextSize(2); appState.tft.setTextColor(GAMER_TEXT, GAMER_CARD);
  int valueWidth = appState.tft.textWidth(value);
  appState.tft.setTextSize(1);
  int unitWidth = appState.tft.textWidth(unit);
  int startX = x + (CARD_W - valueWidth - unitWidth - 4) / 2;
  appState.tft.setTextSize(2);
  appState.tft.setCursor(startX, y + 21); appState.tft.print(value);
  appState.tft.setTextSize(1);
  appState.tft.setCursor(startX + valueWidth + 4, y + 30); appState.tft.print(unit);
}

void gamerTemperatureCard(int x, int y, int temperature)
{
  appState.tft.fillRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, GAMER_CARD);
  gamerLabel("GPU TEMP", x + 9, y + 8, GAMER_ORANGE);
  char value[16];
  if (temperature < 0) snprintf(value, sizeof(value), "--\xB0");
  else snprintf(value, sizeof(value), "%d\xB0", temperature);
  appState.tft.setTextFont(2); appState.tft.setTextSize(2); appState.tft.setTextColor(GAMER_ORANGE, GAMER_CARD);
  appState.tft.setCursor(gamerCenteredValueX(x, value), y + 21); appState.tft.print(value);
}

void gamerCopyStatus(char* target, size_t targetSize, const String& value, const char* fallback)
{
  const char* source = value.length() ? value.c_str() : fallback;
  strncpy(target, source, targetSize - 1);
  target[targetSize - 1] = '\0';
}

void gamerTrimStatusToWidth(char* status, size_t statusSize, int maxWidth)
{
  while (strlen(status) > 3 && appState.tft.textWidth(status) > maxWidth)
  {
    size_t len = strlen(status);
    status[len - 2] = '.';
    status[len - 1] = '\0';
  }
}

void gamerDrawHeader()
{
  appState.tft.fillRect(0, 0, DISPLAY_WIDTH, 32, TFT_BLACK);
  bool showSource = appState.gamerShowSource;
  bool sourceOnline = metricsHasRecentPcMetrics() && appState.gamerSource.length() > 0;
  char status[56];
  if (showSource)
  {
    char source[16];
    gamerCopyStatus(source, sizeof(source), appState.gamerSource, "RTSS");
    snprintf(status, sizeof(status), "%s %s", source, sourceOnline ? "ON" : "OFF");
  }
  else
  {
    gamerCopyStatus(status, sizeof(status), appState.gamerGame, "GAME HUD");
  }
  appState.tft.setTextFont(2); appState.tft.setTextSize(1);
  gamerTrimStatusToWidth(status, sizeof(status), 180);
  int statusWidth = appState.tft.textWidth(status);
  int groupWidth = statusWidth + (showSource ? 9 : 0);
  int startX = (DISPLAY_WIDTH - groupWidth) / 2;
  if (showSource) appState.tft.fillCircle(startX + 2, 13, 2, sourceOnline ? GAMER_GREEN : TFT_RED);
  appState.tft.setTextColor(showSource ? GAMER_TEXT : GAMER_MUTED, TFT_BLACK);
  int textX = startX + (showSource ? 9 : 0);
  appState.tft.setCursor(textX, 8); appState.tft.print(status);
  appState.tft.setCursor(textX + 1, 8); appState.tft.print(status);
}

void gamerDrawValues()
{
  resetMarkerCheckpoint("gamer_draw_values");
  gamerDrawHeader();
  bool pcOnline = metricsHasRecentPcMetrics();
  char fpsText[16];
  if (!pcOnline || appState.gamerFps < 0) snprintf(fpsText, sizeof(fpsText), "--");
  else snprintf(fpsText, sizeof(fpsText), "%d", appState.gamerFps);
  gamerValueCard(LEFT_X, ROW1_Y, "FPS", fpsText, GAMER_CYAN);
  yield();
  char frameText[8];
  if (!pcOnline || appState.gamerFrametime < 0) snprintf(frameText, sizeof(frameText), "--");
  else snprintf(frameText, sizeof(frameText), "%.1f", appState.gamerFrametime);
  gamerFrameCard(RIGHT_X, ROW1_Y, frameText, "ms");
  yield();
  int gpu = pcOnline && appState.gamerDataVersion != 0 ? appState.gpuCurrent : -1;
  int cpu = pcOnline && appState.gamerDataVersion != 0 ? appState.cpuCurrent : -1;
  int ram = pcOnline && appState.gamerDataVersion != 0 ? appState.ramCurrent : -1;
  gamerUsageCard(LEFT_X, ROW2_Y, "GPU", gpu, GAMER_GREEN);
  yield();
  gamerTemperatureCard(RIGHT_X, ROW2_Y, pcOnline ? appState.gamerGpuTemp : -1);
  yield();
  gamerUsageCard(LEFT_X, ROW3_Y, "RAM", ram, GAMER_PURPLE);
  yield();
  gamerUsageCard(RIGHT_X, ROW3_Y, "CPU", cpu, GAMER_BLUE);
  yield();
  gamerLastValuesDrawAt = millis();
  resetMarkerCheckpoint("gamer_draw_done");
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

  if ((appState.gamerDrawnVersion != appState.gamerDataVersion || appState.gamerLastPcOnline != pcOnline) &&
      now - gamerLastValuesDrawAt >= GAMER_VALUES_INTERVAL_MS)
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
