#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

int numericClockTextWidth(const String& text);
int numericClockTextWidth(const String& text, uint8_t scale);
int numericClockTextHeight(uint8_t scale);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t scale);
