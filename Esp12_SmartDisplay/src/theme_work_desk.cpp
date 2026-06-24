#include <TFT_eSPI.h>

#include "app_state.h"
#include "config.h"
#include "theme.h"

void themeWorkDeskDrawBase()
{
  appState.tft.fillScreen(CLEAN_TFT_THEME.background);
  appState.tft.setTextFont(2);
  appState.tft.setTextSize(1);
  appState.tft.setTextColor(CLEAN_TFT_THEME.secondaryText, CLEAN_TFT_THEME.background);
  String title = "WORK DESK";
  appState.tft.setCursor((DISPLAY_WIDTH - appState.tft.textWidth(title)) / 2, 112);
  appState.tft.print(title);
}
