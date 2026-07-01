#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

bool smoothClockFontEnsureLoaded(TFT_eSPI& tft);
bool smoothClockFontIsLoaded();
void smoothClockFontUnload(TFT_eSPI& tft);
int smoothClockTextWidth(TFT_eSPI& tft, const String& text);
int smoothClockTextHeight(TFT_eSPI& tft);
void smoothClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg);
