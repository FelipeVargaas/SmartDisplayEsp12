#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

bool numericClockCanDraw(const String& text);
int numericClockTextWidth(const String& text);
int numericClockTextWidth(const String& text, uint8_t scale);
int numericClockTextWidth(const String& text, uint8_t xNumerator, uint8_t xDenominator);
int numericClockTextHeight(uint8_t scale);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t scale);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t xScale, uint8_t yScale);
void numericClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg, uint8_t xNumerator, uint8_t xDenominator, uint8_t yScale);
