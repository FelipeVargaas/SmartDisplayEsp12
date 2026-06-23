#pragma once

#include <Arduino.h>

// Paleta enxuta do Tema 1 - Clean TFT.
struct Theme
{
  uint16_t background;
  uint16_t primaryText;
  uint16_t secondaryText;
  uint16_t divider;
  uint16_t barTrack;
  uint16_t cpu;
  uint16_t ram;
  uint16_t gpu;
  uint16_t online;
  uint16_t offline;
};

static const Theme CLEAN_TFT_THEME = {
  TFT_BLACK, 0xEF7D, 0xBDF7, 0x2104, 0x18C3,
  0x243E, 0x243E, 0x243E, 0x5DCA, 0xFC00
};
