#pragma once

#include <Arduino.h>

struct AnimationLineAsset
{
  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
  uint16_t color;
};

struct AnimationDotAsset
{
  int16_t x;
  int16_t y;
  uint8_t radius;
  uint16_t color;
};

static const AnimationLineAsset ANIMATION_LINES[] PROGMEM = {
  { 0, 42, 72, 12, 0x0398 },
  { 22, 222, 94, 178, 0x0398 },
  { 146, 18, 239, 62, 0xFB20 },
  { 164, 226, 239, 190, 0xFB20 },
  { 8, 147, 58, 147, 0x22B2 },
  { 182, 92, 232, 92, 0x9280 },
  { 76, 30, 103, 52, 0x22B2 },
  { 136, 188, 164, 211, 0x9280 }
};

static const AnimationDotAsset ANIMATION_DOTS[] PROGMEM = {
  { 31, 30, 2, 0x4D9F },
  { 54, 201, 2, 0x4D9F },
  { 79, 71, 1, 0xFB20 },
  { 96, 218, 1, 0xFB20 },
  { 166, 37, 2, 0x4D9F },
  { 195, 205, 2, 0x4D9F },
  { 212, 115, 1, 0xFB20 },
  { 224, 159, 1, 0xFB20 },
  { 18, 121, 1, 0x3186 },
  { 219, 25, 1, 0x3186 },
  { 32, 83, 1, 0x3186 },
  { 202, 71, 1, 0x3186 }
};

