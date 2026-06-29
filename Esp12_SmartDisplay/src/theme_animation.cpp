#include <TFT_eSPI.h>

#include "app_state.h"
#include "animation_image.h"
#include "config.h"
#include "theme.h"
#include "theme_animation_assets.h"

namespace
{
const uint16_t ANIMATION_BG = 0x0021;
const uint16_t ANIMATION_BG_BAND_1 = 0x0063;
const uint16_t ANIMATION_BG_BAND_2 = 0x0084;
const uint16_t ANIMATION_PANEL = 0x0842;
const uint16_t ANIMATION_PANEL_EDGE = 0x10C5;
const uint16_t ANIMATION_CYAN = 0x04FF;
const uint16_t ANIMATION_BLUE = 0x22B2;
const uint16_t ANIMATION_ORANGE = 0xFB20;
const uint16_t ANIMATION_MUTED = 0x8C71;
const uint16_t ANIMATION_TEXT = 0xEF7D;

void drawBackground()
{
  appState.tft.fillScreen(ANIMATION_BG);
  for (int y = 0; y < DISPLAY_HEIGHT; y += 8)
  {
    uint16_t color = (y < 80 || y > 176) ? ANIMATION_BG_BAND_1 : ANIMATION_BG_BAND_2;
    appState.tft.fillRect(0, y, DISPLAY_WIDTH, 4, color);
  }
  appState.tft.drawRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0x10A4);
}

void drawAssetLines()
{
  const size_t count = sizeof(ANIMATION_LINES) / sizeof(ANIMATION_LINES[0]);
  for (size_t i = 0; i < count; i++)
  {
    AnimationLineAsset line;
    memcpy_P(&line, &ANIMATION_LINES[i], sizeof(line));
    appState.tft.drawLine(line.x0, line.y0, line.x1, line.y1, line.color);
  }
}

void drawAssetDots()
{
  const size_t count = sizeof(ANIMATION_DOTS) / sizeof(ANIMATION_DOTS[0]);
  for (size_t i = 0; i < count; i++)
  {
    AnimationDotAsset dot;
    memcpy_P(&dot, &ANIMATION_DOTS[i], sizeof(dot));
    appState.tft.fillCircle(dot.x, dot.y, dot.radius, dot.color);
  }
}

void drawStaticScreensaver()
{
  appState.tft.drawCircle(120, 120, 83, 0x1188);
  appState.tft.drawCircle(120, 120, 65, 0x1A29);
  appState.tft.drawCircle(120, 120, 46, 0x3AEB);
  appState.tft.drawArc(120, 120, 89, 86, 23, 132, ANIMATION_ORANGE, ANIMATION_BG);
  appState.tft.drawArc(120, 120, 74, 71, 210, 318, ANIMATION_CYAN, ANIMATION_BG);

  appState.tft.fillRoundRect(54, 92, 132, 56, 10, ANIMATION_PANEL);
  appState.tft.drawRoundRect(54, 92, 132, 56, 10, ANIMATION_PANEL_EDGE);
  appState.tft.fillRoundRect(64, 102, 112, 4, 2, ANIMATION_BLUE);
  appState.tft.fillRoundRect(64, 134, 112, 4, 2, ANIMATION_ORANGE);

  appState.tft.setTextFont(2);
  appState.tft.setTextSize(2);
  appState.tft.setTextColor(ANIMATION_TEXT, ANIMATION_PANEL);
  appState.tft.setCursor(75, 109);
  appState.tft.print("TinyDash");

  appState.tft.setTextSize(1);
  appState.tft.setTextColor(ANIMATION_MUTED, ANIMATION_BG);
  appState.tft.setCursor(75, 160);
  appState.tft.print("ANIMATION V1");
}
}

void themeAnimationDrawBase()
{
  if (animationImageRender()) return;

  drawBackground();
  drawAssetLines();
  drawStaticScreensaver();
  drawAssetDots();
}
