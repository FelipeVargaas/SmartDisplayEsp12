#include "theme_render.h"

#include "app_state.h"
#include "smooth_clock_font.h"
#include "theme_manager.h"

void themePcMonitorDrawBase();
void themePcMonitorUpdateIfNeeded();
void themeWorkDeskDrawBase();
void themeWorkDeskUpdateIfNeeded();
void themeGamerDrawBase();
void themeGamerUpdateIfNeeded();
void themeMinimalClockDrawBase();
void themeMinimalClockUpdateIfNeeded();
void themeAnimationDrawBase();
void themeAnimationUpdateIfNeeded();

void themeDrawBase()
{
  if (themeManagerGetActive() != THEME_WORK_DESK)
    smoothClockFontUnload(appState.tft);

  switch (themeManagerGetActive())
  {
    case THEME_WORK_DESK: themeWorkDeskDrawBase(); break;
    case THEME_GAMER: themeGamerDrawBase(); break;
    case THEME_MINIMAL_CLOCK: themeMinimalClockDrawBase(); break;
    case THEME_ANIMATION: themeAnimationDrawBase(); break;
    case THEME_PC_MONITOR:
    default: themePcMonitorDrawBase(); break;
  }
}

void themeUpdateIfNeeded()
{
  switch (themeManagerGetActive())
  {
    case THEME_WORK_DESK: themeWorkDeskUpdateIfNeeded(); break;
    case THEME_MINIMAL_CLOCK: themeMinimalClockUpdateIfNeeded(); break;
    case THEME_GAMER: themeGamerUpdateIfNeeded(); break;
    case THEME_ANIMATION: themeAnimationUpdateIfNeeded(); break;
    case THEME_PC_MONITOR: themePcMonitorUpdateIfNeeded(); break;
    default: break;
  }
}

void themeForceFullRedraw()
{
  themeDrawBase();
}
