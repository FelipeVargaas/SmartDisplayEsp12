#include "smooth_clock_font.h"

#include <LittleFS.h>

namespace
{
const char* const FONT_NAME = "numeric_clock";
const char* const FONT_PATH = "/numeric_clock.vlw";
bool littleFsReady = false;
bool littleFsTried = false;
bool fontLoaded = false;
bool fontFailed = false;

bool ensureLittleFs()
{
  if (littleFsReady) return true;
  if (littleFsTried) return false;
  littleFsTried = true;

  if (!LittleFS.begin())
  {
    Serial.println("[SmoothClock] LittleFS falhou. Usando fonte bitmap.");
    return false;
  }

  littleFsReady = true;
  return true;
}

bool hasRequiredGlyphs(TFT_eSPI& tft)
{
  static const char* const required = "0123456789:%C";
  uint16_t index = 0;
  for (const char* c = required; *c != '\0'; c++)
  {
    if (!tft.getUnicodeIndex(static_cast<uint16_t>(*c), &index)) return false;
  }
  return true;
}
}

bool smoothClockFontEnsureLoaded(TFT_eSPI& tft)
{
  if (fontLoaded && tft.fontLoaded) return true;
  if (fontFailed) return false;
  if (!ensureLittleFs())
  {
    fontFailed = true;
    return false;
  }

  if (!LittleFS.exists(FONT_PATH))
  {
    Serial.println("[SmoothClock] /numeric_clock.vlw nao encontrado. Usando fonte bitmap.");
    fontFailed = true;
    return false;
  }

  File file = LittleFS.open(FONT_PATH, "r");
  if (!file)
  {
    Serial.println("[SmoothClock] Falha ao abrir /numeric_clock.vlw. Usando fonte bitmap.");
    fontFailed = true;
    return false;
  }

  size_t fileSize = file.size();
  file.close();

  uint32_t heapBefore = ESP.getFreeHeap();
  Serial.print("[SmoothClock] Heap antes loadFont: ");
  Serial.println(heapBefore);
  Serial.print("[SmoothClock] Arquivo .vlw bytes: ");
  Serial.println(fileSize);

  tft.loadFont(FONT_NAME, LittleFS);
  uint32_t heapAfter = ESP.getFreeHeap();
  Serial.print("[SmoothClock] Heap depois loadFont: ");
  Serial.println(heapAfter);

  if (!tft.fontLoaded || !hasRequiredGlyphs(tft))
  {
    Serial.println("[SmoothClock] loadFont falhou ou glifo obrigatorio ausente. Usando fonte bitmap.");
    if (tft.fontLoaded) tft.unloadFont();
    fontFailed = true;
    fontLoaded = false;
    return false;
  }

  fontLoaded = true;
  return true;
}

bool smoothClockFontIsLoaded()
{
  return fontLoaded;
}

void smoothClockFontUnload(TFT_eSPI& tft)
{
  if (!tft.fontLoaded)
  {
    fontLoaded = false;
    return;
  }

  tft.unloadFont();
  fontLoaded = false;
}

int smoothClockTextWidth(TFT_eSPI& tft, const String& text)
{
  return tft.textWidth(text);
}

int smoothClockTextHeight(TFT_eSPI& tft)
{
  return tft.fontHeight();
}

void smoothClockDrawText(TFT_eSPI& tft, const String& text, int x, int y, uint16_t color, uint16_t bg)
{
  tft.setTextColor(color, bg, true);
  tft.drawString(text, x, y);
}
