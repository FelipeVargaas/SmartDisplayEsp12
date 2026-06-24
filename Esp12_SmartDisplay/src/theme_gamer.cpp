#include <TFT_eSPI.h>

#include "app_state.h"
#include "config.h"
#include "theme.h"

void themeGamerDrawBase()
{
  appState.tft.fillScreen(CLEAN_TFT_THEME.background);
  appState.tft.setTextFont(4);
  appState.tft.setTextSize(2);
  appState.tft.setTextColor(CLEAN_TFT_THEME.primaryText, CLEAN_TFT_THEME.background);
  String title = "GAMER";
  int x = (DISPLAY_WIDTH - appState.tft.textWidth(title)) / 2;
  if (x < 0) x = 0;
  appState.tft.setCursor(x, 94);
  appState.tft.print(title);
}
